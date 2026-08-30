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
    tw::framework::channel_shim::call_hook_fn hook;
    void* user;
};

struct binding {
    A3d_Channel* channel;
    void** original_vtable;
    call_channel_fn original_call;
    // Small by construction - one or two in practice, and only ever as many as there are scripts
    // watching the same node. A flat vector beats anything cleverer at this size, and the whole
    // point of the per-object vtable copy is that channels nobody watches are not in this list at
    // all.
    std::vector<subscriber> subscribers;
    void** block;      // the allocation; block[k_prefix_slots] is what the object points at
    std::size_t slots; // copied entries, excluding the prefix
};

std::vector<binding*> g_bindings;

binding* find_binding(A3d_Channel* channel) noexcept
{
    for(binding* item : g_bindings) {
        if(item->channel == channel) {
            return item;
        }
    }

    return nullptr;
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

void __fastcall shim_call_channel(A3d_Channel* self, void* edx)
{
    // One load to recover the context: the vptr the engine just used points at our copy, and the
    // binding sits two words in front of it.
    binding* item = reinterpret_cast<binding**>(*reinterpret_cast<void***>(self))[-2];

    // Every `before` subscriber runs before any of them gets to cancel. Short-circuiting on the
    // first false would make whether an observer sees the event depend on the order two unrelated
    // scripts happened to be loaded in, which is not something a script author can reason about.
    bool proceed = true;
    for(const subscriber& sub : item->subscribers) {
        if(!sub.hook(self, sub.user, tw::framework::channel_shim::phase::before)) {
            proceed = false;
        }
    }

    // Suppression is an OR: any one subscriber can take the call away, and the `after` phase goes
    // with it - it means "the original has returned", which is not true here.
    if(!proceed) {
        return;
    }

    item->original_call(self, edx);

    for(const subscriber& sub : item->subscribers) {
        sub.hook(self, sub.user, tw::framework::channel_shim::phase::after);
    }
}

// Drops one subscriber from a binding and tears the binding down when it was the last. Returns
// whether anything was removed.
bool detach(binding* item, void* user, std::size_t binding_index) noexcept
{
    const auto it = std::find_if(item->subscribers.begin(), item->subscribers.end(),
        [user](const subscriber& sub) { return sub.user == user; });
    if(it == item->subscribers.end()) {
        return false;
    }

    item->subscribers.erase(it);

    if(!item->subscribers.empty()) {
        return true;
    }

    (void)restore(item);
    delete[] item->block;
    delete item;
    g_bindings.erase(g_bindings.begin() + static_cast<std::ptrdiff_t>(binding_index));

    return true;
}
} // namespace

namespace tw::framework::channel_shim
{
bool subscribe(A3d_Channel* channel, call_hook_fn hook, void* user) noexcept
{
    if(channel == nullptr || hook == nullptr) {
        return false;
    }

    // Second and later subscribers join the existing copy. This used to overwrite the hook instead,
    // which silently stole the first script's subscription the moment a second one asked for the
    // same channel - the exact case a user with several scripts installed runs into.
    if(binding* existing = find_binding(channel); existing != nullptr) {
        existing->subscribers.push_back(subscriber { hook, user });
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
    item->original_vtable = original;
    item->original_call = reinterpret_cast<call_channel_fn>(original[k_call_channel_slot]);
    item->subscribers.push_back(subscriber { hook, user });
    item->block = block;
    item->slots = slots;

    block[k_prefix_slots + k_call_channel_slot] = reinterpret_cast<void*>(&shim_call_channel);

    // Publishing the new vptr is a single aligned pointer store, which is atomic on x86. That
    // matters because this runs from inside EndScene while the engine owns the rest of the frame: a
    // call already in flight keeps using the function pointer it had loaded, and the next one picks
    // up ours. There is no window where a caller could read half a pointer.
    *reinterpret_cast<void***>(channel) = block + k_prefix_slots;

    g_bindings.push_back(item);

    return true;
}

void unsubscribe(A3d_Channel* channel, void* user) noexcept
{
    for(std::size_t i = 0; i < g_bindings.size(); ++i) {
        if(g_bindings[i]->channel == channel) {
            (void)detach(g_bindings[i], user, i);
            return;
        }
    }
}

void unsubscribe_all_of(std::span<void* const> users) noexcept
{
    // Backwards, because detach() can erase the binding it was given.
    for(std::size_t i = g_bindings.size(); i-- > 0;) {
        for(void* user : users) {
            if(detach(g_bindings[i], user, i)) {
                // The binding may be gone now; either way this channel has at most one subscription
                // per user, so there is nothing more to find here.
                break;
            }
        }
    }
}

void remove(A3d_Channel* channel) noexcept
{
    for(std::size_t i = 0; i < g_bindings.size(); ++i) {
        binding* item = g_bindings[i];
        if(item->channel != channel) {
            continue;
        }

        (void)restore(item);
        delete[] item->block;
        delete item;
        g_bindings.erase(g_bindings.begin() + static_cast<std::ptrdiff_t>(i));
        return;
    }
}

int subscriber_count(A3d_Channel* channel) noexcept
{
    const binding* item = find_binding(channel);
    return item != nullptr ? static_cast<int>(item->subscribers.size()) : 0;
}

void remove_all() noexcept
{
    for(binding* item : g_bindings) {
        (void)restore(item);
        delete[] item->block;
        delete item;
    }

    g_bindings.clear();
}

int active_count() noexcept
{
    return static_cast<int>(g_bindings.size());
}
} // namespace tw::framework::channel_shim
