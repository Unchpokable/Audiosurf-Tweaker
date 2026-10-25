#include "pch.hxx"

#include "framework/channel_shim.hxx"

#include "plugin/diagnostics.hxx"

namespace
{
// Slot 1 of every channel vtable (reversing-journal-engine.md §2.1). The base implementation is an
// empty `ret`; the 159 types that matter override it.
constexpr std::size_t k_call_channel_slot = 1;

// Two words of our own live in front of the copied table: [-2] the binding, [-1] whatever the
// original had there (MSVC puts the RTTI complete-object locator at [-1], and copying it verbatim
// keeps any dynamic_cast the engine might do behaving as before).
constexpr std::size_t k_prefix_slots = 2;

// How much of the original table to take. The real length is not knowable - a vtable is just an
// array of pointers with nothing marking its end - so this over-copies deliberately: extra entries
// past the real end are never called, whereas copying too few would hand the engine garbage the
// first time it used a later slot. The read is clamped to the end of the memory region the vtable
// lives in, so over-copying can never touch an unmapped page.
constexpr std::size_t k_max_slots = 128;

using call_channel_fn = void(__fastcall*)(A3d_Channel* self, void* edx);

struct subscriber {
    // Null once the subscriber is gone but its binding is still being dispatched - see `in_flight`.
    tw::framework::channel_shim::call_hook_fn hook;
    void* user;
    // Lower runs first; see subscribe() in the header.
    std::uint64_t order;
};

// After every subscriber whose key is not greater: equal keys keep the order they subscribed in.
void add_subscriber(std::vector<subscriber>& list, const subscriber& entry)
{
    const auto at = std::ranges::upper_bound(list, entry.order, {}, &subscriber::order);
    list.insert(at, entry);
}

struct binding {
    A3d_Channel* channel;
    // The group the channel was in when it was hooked. Recorded rather than derived, because the one
    // moment it is needed is inside that group's teardown - see remove_all_in() in the header.
    A3d_ChannelGroup* group;
    void** original_vtable;
    call_channel_fn original_call;
    // Small by construction - one or two in practice, and only ever as many as there are scripts
    // watching the same node. A flat vector beats anything cleverer at this size, and the whole
    // point of the per-object vtable copy is that channels nobody watches are not in this list at
    // all.
    std::vector<subscriber> subscribers;
    void** block;      // the allocation; block[k_prefix_slots] is what the object points at
    std::size_t slots; // copied entries, excluding the prefix

    // **How many shim_call_channel frames of this binding are on the stack right now.**
    //
    // The engine's call stack is ours to run code in, and that code can come back round to this
    // binding: a subscriber's handler, the original's own children, and - the path that actually
    // happens - Present, which the game calls from deep inside its graph and from which the overlay
    // switches scripts off and reloads them. Every channel above Present on the stack is mid-call.
    // Freeing a binding, or growing, shrinking or sorting its subscriber list, while a frame is
    // walking it was a use-after-free (Docs/Internal/lua-audit-journal.md, A2).
    //
    // So while this is non-zero `subscribers` never changes size or order: a subscriber that leaves is
    // marked (hook = null), one that joins waits in `pending`, a re-sort waits in `resort`, and a
    // binding that loses its last subscriber gives the channel its vtable back at once but is only
    // freed by the frame that brings this back to zero (settle()).
    int in_flight;
    bool compact;                   // a subscriber was marked gone while in flight
    bool resort;                    // reorder() came while in flight
    bool retired;                   // vptr already restored; waiting in g_retired for its last frame
    std::vector<subscriber> pending; // subscribed while in flight
};

// Bindings that are live: the only list find, subscribe and every removal look at.
std::vector<binding*> g_bindings;

// Bindings already taken off their channel whose shim_call_channel is still on the stack. Nothing
// reaches them but that frame.
std::vector<binding*> g_retired;

binding* find_binding(A3d_Channel* channel) noexcept
{
    for(binding* item : g_bindings) {
        if(item->channel == channel) {
            return item;
        }
    }

    return nullptr;
}

std::size_t index_of(const binding* item) noexcept
{
    for(std::size_t i = 0; i < g_bindings.size(); ++i) {
        if(g_bindings[i] == item) {
            return i;
        }
    }

    return g_bindings.size();
}

// How many pointer-sized entries can be read starting at `address` without leaving the committed
// region it belongs to.
std::size_t readable_slots(const void* address) noexcept
{
    MEMORY_BASIC_INFORMATION info {};
    if(::VirtualQuery(address, &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT) {
        return 0;
    }

    const auto* base = static_cast<const std::byte*>(info.BaseAddress);
    const auto* start = static_cast<const std::byte*>(address);
    const std::size_t consumed = static_cast<std::size_t>(start - base);
    if(consumed >= info.RegionSize) {
        return 0;
    }

    return (info.RegionSize - consumed) / sizeof(void*);
}

// Puts a channel's original vptr back, if the channel is still there to put it back into.
//
// It may not be. Groups load and unload as the game moves between the menu and a run, and nothing
// tells us when one goes - so by the time anything unhooks, the object may be freed and its block
// handed to something else entirely. Writing the original vtable pointer into that would be a
// corruption with no visible cause and a very long fuse.
//
// Two guards, neither of them a liveness proof: the object's page must still be committed, and the
// object must still be pointing at *our* copy. Together they turn the realistic failure - the
// allocator reused the block - into a no-op rather than a stray write. Freeing our copy is safe
// either way: if the vptr no longer points at it, nothing does.
bool restore(binding* item) noexcept
{
    MEMORY_BASIC_INFORMATION info {};
    if(::VirtualQuery(item->channel, &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT) {
        return false;
    }

    if(*reinterpret_cast<void***>(item->channel) != item->block + k_prefix_slots) {
        return false;
    }

    *reinterpret_cast<void***>(item->channel) = item->original_vtable;

    return true;
}

void free_binding(binding* item) noexcept
{
    delete[] item->block;
    delete item;
}

bool has_live_subscribers(const binding* item) noexcept
{
    if(!item->pending.empty()) {
        return true;
    }

    return std::ranges::any_of(item->subscribers, [](const subscriber& sub) { return sub.hook != nullptr; });
}

// Takes a binding off its channel: the original vptr goes back now, the subscribers are dropped, and
// the binding is freed - now, or when the last frame dispatching it returns. `index` is its place in
// g_bindings.
void retire(binding* item, std::size_t index) noexcept
{
    (void)restore(item);
    g_bindings.erase(g_bindings.begin() + static_cast<std::ptrdiff_t>(index));

    if(item->in_flight == 0) {
        free_binding(item);
        return;
    }

    for(subscriber& sub : item->subscribers) {
        sub.hook = nullptr;
    }
    item->pending.clear();
    item->retired = true;
    g_retired.push_back(item);
}

// Drops the subscriber keyed by `user`, wherever it is. Whether one was found.
bool drop_user(binding* item, void* user) noexcept
{
    if(const auto it = std::ranges::find(item->pending, user, &subscriber::user); it != item->pending.end()) {
        item->pending.erase(it);
        return true;
    }

    const auto it = std::ranges::find_if(item->subscribers, [user](const subscriber& sub) { return sub.hook != nullptr && sub.user == user; });
    if(it == item->subscribers.end()) {
        return false;
    }

    if(item->in_flight > 0) {
        it->hook = nullptr;
        it->user = nullptr;
        item->compact = true;
    }
    else {
        item->subscribers.erase(it);
    }

    return true;
}

// The last frame of a binding has returned: apply whatever was held back while it ran.
void settle(binding* item) noexcept
{
    if(item->retired) {
        if(const auto it = std::ranges::find(g_retired, item); it != g_retired.end()) {
            g_retired.erase(it);
        }
        free_binding(item);
        return;
    }

    if(item->compact) {
        std::erase_if(item->subscribers, [](const subscriber& sub) { return sub.hook == nullptr; });
        item->compact = false;
    }

    for(const subscriber& entry : item->pending) {
        add_subscriber(item->subscribers, entry);
    }
    item->pending.clear();

    if(item->resort) {
        std::ranges::stable_sort(item->subscribers, {}, &subscriber::order);
        item->resort = false;
    }

    if(item->subscribers.empty()) {
        retire(item, index_of(item));
    }
}

void __fastcall shim_call_channel(A3d_Channel* self, void* edx)
{
    // One load to recover the context: the vptr the engine just used points at our copy, and the
    // binding sits two words in front of it.
    binding* item = reinterpret_cast<binding**>(*reinterpret_cast<void***>(self))[-2];

    // Taken before anything can run: everything below may retire this binding.
    const call_channel_fn original = item->original_call;

    ++item->in_flight;

    // Every `before` subscriber runs before any of them gets to cancel. Short-circuiting on the
    // first false would make whether an observer sees the event depend on the order two unrelated
    // scripts happened to be loaded in, which is not something a script author can reason about.
    //
    // By index, and over a copy of each entry: while in flight the list keeps its size, but an entry
    // can be marked gone between two iterations.
    bool proceed = true;
    for(std::size_t i = 0; i < item->subscribers.size(); ++i) {
        const subscriber sub = item->subscribers[i];
        if(sub.hook != nullptr && !sub.hook(self, sub.user, tw::framework::channel_shim::phase::before)) {
            proceed = false;
        }
    }

    // Suppression is an OR: any one subscriber can take the call away, and the `after` phase goes
    // with it - it means "the original has returned", which is not true here.
    if(proceed) {
        original(self, edx);

        for(std::size_t i = 0; i < item->subscribers.size(); ++i) {
            const subscriber sub = item->subscribers[i];
            if(sub.hook != nullptr) {
                sub.hook(self, sub.user, tw::framework::channel_shim::phase::after);
            }
        }
    }

    if(--item->in_flight == 0 && (item->retired || item->compact || item->resort || !item->pending.empty())) [[unlikely]] {
        settle(item);
    }
}
} // namespace

namespace tw::framework::channel_shim
{
bool subscribe(A3d_Channel* channel, A3d_ChannelGroup* group, call_hook_fn hook, void* user, std::uint64_t order) noexcept
{
    if(channel == nullptr || hook == nullptr) {
        return false;
    }

    // Second and later subscribers join the existing copy. This used to overwrite the hook instead,
    // which silently stole the first script's subscription the moment a second one asked for the
    // same channel - the exact case a user with several scripts installed runs into.
    if(binding* existing = find_binding(channel); existing != nullptr) {
        const subscriber entry { hook, user, order };
        if(existing->in_flight > 0) {
            existing->pending.push_back(entry);
        }
        else {
            add_subscriber(existing->subscribers, entry);
        }
        return true;
    }

    auto** original = *reinterpret_cast<void***>(channel);
    if(original == nullptr) {
        return false;
    }

    // Include the [-1] prefix word in the readability check, since that is copied too.
    const std::size_t available = readable_slots(original - 1);
    if(available < k_call_channel_slot + 2) {
        TW_LOG_WARNING("channel_shim: vtable at {} is not readable - not hooking", static_cast<void*>(original));
        return false;
    }

    const std::size_t slots = std::min(k_max_slots, available - 1);

    auto* item = new(std::nothrow) binding {};
    if(item == nullptr) {
        return false;
    }

    auto** block = new(std::nothrow) void*[slots + k_prefix_slots];
    if(block == nullptr) {
        delete item;
        return false;
    }

    block[0] = item;         // [-2] from the object's point of view
    block[1] = original[-1]; // [-1] the RTTI locator, carried over untouched
    std::memcpy(block + k_prefix_slots, original, slots * sizeof(void*));

    item->channel = channel;
    item->group = group;
    item->original_vtable = original;
    item->original_call = reinterpret_cast<call_channel_fn>(original[k_call_channel_slot]);
    item->subscribers.push_back(subscriber { hook, user, order });
    item->block = block;
    item->slots = slots;

    block[k_prefix_slots + k_call_channel_slot] = reinterpret_cast<void*>(&shim_call_channel);

    // Publishing the new vptr is a single aligned pointer store, which is atomic on x86. That
    // matters because this runs from inside the game's frame: a call already in flight keeps using the
    // function pointer it had loaded, and the next one picks up ours. There is no window where a
    // caller could read half a pointer.
    *reinterpret_cast<void***>(channel) = block + k_prefix_slots;

    g_bindings.push_back(item);

    return true;
}

void unsubscribe(A3d_Channel* channel, void* user) noexcept
{
    binding* const item = find_binding(channel);
    if(item == nullptr || !drop_user(item, user)) {
        return;
    }

    if(!has_live_subscribers(item)) {
        retire(item, index_of(item));
    }
}

void unsubscribe_all_of(std::span<void* const> users) noexcept
{
    // **Every** subscriber in the set, off every binding. One script can hold several subscriptions on
    // one channel - PuzzlePRO HUD has two on Do_ResetStats - and stopping at the first one found used
    // to leave the rest pointing at subscription records their owner had already freed, which the
    // next records allocated at the same address then answered for (lua-audit-journal.md, A1).
    //
    // Backwards, because retire() erases the binding it was given.
    for(std::size_t i = g_bindings.size(); i-- > 0;) {
        binding* const item = g_bindings[i];

        bool dropped = false;
        for(void* user : users) {
            while(drop_user(item, user)) {
                dropped = true;
            }
        }

        if(dropped && !has_live_subscribers(item)) {
            retire(item, i);
        }
    }
}

void remove(A3d_Channel* channel) noexcept
{
    if(binding* const item = find_binding(channel); item != nullptr) {
        retire(item, index_of(item));
    }
}

void reorder(order_fn order_of) noexcept
{
    if(order_of == nullptr) {
        return;
    }

    for(binding* item : g_bindings) {
        // A subscriber that is gone has no key to ask for: its user may already be freed.
        for(subscriber& sub : item->subscribers) {
            if(sub.hook != nullptr) {
                sub.order = order_of(sub.user);
            }
        }
        for(subscriber& sub : item->pending) {
            sub.order = order_of(sub.user);
        }

        if(item->in_flight > 0) {
            item->resort = true;
        }
        else {
            std::ranges::stable_sort(item->subscribers, {}, &subscriber::order);
        }
    }
}

int subscriber_count(A3d_Channel* channel) noexcept
{
    const binding* item = find_binding(channel);
    if(item == nullptr) {
        return 0;
    }

    const auto live = std::ranges::count_if(item->subscribers, [](const subscriber& sub) { return sub.hook != nullptr; });

    return static_cast<int>(live + static_cast<std::ptrdiff_t>(item->pending.size()));
}

int remove_all_in(A3d_ChannelGroup* group) noexcept
{
    if(group == nullptr) {
        return 0;
    }

    int removed = 0;

    // Backwards, so erasing does not move anything still to be looked at.
    for(std::size_t i = g_bindings.size(); i-- > 0;) {
        if(g_bindings[i]->group != group) {
            continue;
        }

        retire(g_bindings[i], i);
        ++removed;
    }

    return removed;
}

void remove_all() noexcept
{
    for(std::size_t i = g_bindings.size(); i-- > 0;) {
        retire(g_bindings[i], i);
    }
}

int active_count() noexcept
{
    return static_cast<int>(g_bindings.size());
}

int retired_count() noexcept
{
    return static_cast<int>(g_retired.size());
}
} // namespace tw::framework::channel_shim
