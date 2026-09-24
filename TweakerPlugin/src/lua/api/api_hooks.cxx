#include "pch.hxx"

#include "lua/api/api_hooks.hxx"

#include "engine/channel_kind.hxx"
#include "engine/channel_ref.hxx"
#include "engine/engine_groups.hxx"

#include "framework/channel_shim.hxx"

#include "lua/api/api_channels.hxx"
#include "lua/lua_diag.hxx"
#include "lua/lua_sched.hxx"

namespace
{
// One per tw.on_call / tw.mute subscription. Only the id crosses back into Lua: C never holds a Lua
// value, so there is nothing here for the collector to interact with and nothing to unregister in
// the right order.
struct subscription {
    int id;
    // Which script asked for this. Every subscription is attributable, because disabling a script
    // has to take exactly its own hooks back out and leave everyone else's in place - see
    // unsubscribe_owner.
    int owner;

    // Null while detached, which is a normal state and not an error: a subscription is accepted the
    // moment a script asks for it, and attached whenever its group turns up. A script that hooks a
    // channel in the `Renderer` pool detaches at the end of every run and re-attaches at the start
    // of the next one without noticing, because its id never changes.
    A3d_Channel* channel;
    A3d_ChannelGroup* group;
    tw::engine::groups::generation gen;

    // How the script asked, kept so the subscription can be resolved again after the group comes
    // back. `index` is -1 when it was addressed by name.
    std::string group_name;
    std::string channel_name;
    int index;

    bool after;
    // A mute answers "do not run the original" from C and never enters the VM. Kept in the same
    // table as the Lua subscriptions so one clear releases both, and so a script cannot mute and
    // observe the same channel through two vtable copies.
    bool mute;
    bool enabled;
};

std::vector<subscription*> g_subscriptions;
int g_next_subscription_id = 0;

subscription* find_subscription(int id) noexcept
{
    for(subscription* record : g_subscriptions) {
        if(record->id == id) {
            return record;
        }
    }

    return nullptr;
}

// The bridge from the engine's call stack back into Lua. Runs on the engine thread, inside whatever
// the game was doing - not inside an ImGui frame, so unlike on_frame there is no draw state to
// protect here; lua_sched::dispatch_call does the pcall and the error containment.
//
// Returns whether the engine's own handler should still run. Only the "before" phase can say no.
//
// **The owner's health is asked first, in C, before anything else.** It is one indexed load, and it is
// what keeps a suspended script from reaching the game through the one path that does not go through
// the prelude: a mute never enters the VM, so the guard in the dispatcher would never see it.
bool dispatch_channel_call(A3d_Channel* /*channel*/, void* user, tw::framework::channel_shim::phase when)
{
    const auto* record = static_cast<const subscription*>(user);

    if(!tw::lua::sched::runnable(record->owner)) [[unlikely]] {
        return true;
    }

    if(record->mute) [[unlikely]] {
        // The whole reason a mute is not just an on_call handler that returns false: this is the
        // entire cost of one, on a node the game calls every frame for the rest of the session.
        return !record->enabled;
    }

    const bool wanted = record->after ? (when == tw::framework::channel_shim::phase::after)
                                      : (when == tw::framework::channel_shim::phase::before);
    if(!wanted) {
        return true;
    }

    // A script only gets to cancel from a "before" handler. Letting an "after" one return false
    // would be meaningless - the call it would be declining has already happened.
    const bool proceed = tw::lua::sched::dispatch_call(record->id);

    return record->after || proceed;
}

// Said with the name the script used, and only for the answer that will never change - an error in
// the script's diagnostics, so once per shape and into the notefeed only once the game is up.
//
// **The distinction this rests on is the whole reason waiting stopped being polling.** "The group is
// not loaded" is transient and says nothing - the menu does not have a run's groups, and never will
// until there is a run. "The group is loaded and has no such channel" is final: a group's channel
// list is fixed when it loads, so no amount of waiting will produce one. That is a typo, and a typo
// that stays silent forever is worse than a noisy one.
void report_missing_target(const subscription& record, tw::lua::api::resolve_status status) noexcept
{
    const char* const what = record.mute ? "tw.mute" : "tw.on_call";

    if(status == tw::lua::api::resolve_unusable) {
        tw::lua::diag::report(record.owner, tw::lua::diag::level::error, what,
            std::format("the target in {} could not be hooked", record.group_name));
        return;
    }

    if(record.index >= 0) {
        tw::lua::diag::report(record.owner, tw::lua::diag::level::error, what,
            std::format("{}.#{}: no such channel in group", record.group_name, record.index));
        return;
    }

    tw::lua::diag::report(record.owner, tw::lua::diag::level::error, what,
        std::format("{}.{}: no such channel in group", record.group_name, record.channel_name));
}

// Tries to bind one record to a live channel. No kind check on purpose: CallChannel is slot 1 of the
// *base* vtable, so every channel of every family has it - this is the one thing that does not care
// what the channel is.
//
// Returns the status a script would want to hear about, and leaves the record detached on anything
// but `resolve_ok`. It is called twice for most subscriptions: once when the script registers, and
// again from the group registry when the group it was waiting for turns up.
tw::lua::api::resolve_status attach_subscription(subscription* record) noexcept
{
    if(record->channel != nullptr) {
        return tw::lua::api::resolve_ok;
    }

    if(!tw::engine::channels::available()) {
        return tw::lua::api::resolve_engine_pending;
    }

    A3d_ChannelGroup* const group = tw::engine::channels::find_group(record->group_name.c_str());
    if(group == nullptr) {
        return tw::lua::api::resolve_no_group;
    }

    A3d_Channel* const channel = record->index < 0 ? tw::engine::channels::find_channel(group, record->channel_name.c_str())
                                                   : tw::engine::channels::find_channel_at(group, record->index);
    if(channel == nullptr) {
        return tw::lua::api::resolve_no_channel;
    }

    // The record doubles as the subscriber key: it is unique per subscription, which is exactly what
    // the shim needs to tell two scripts watching the same channel apart. The group goes with it so
    // that the shim can be torn off the whole group at once when it is destroyed.
    if(!tw::framework::channel_shim::subscribe(channel, group, &dispatch_channel_call, record)) {
        return tw::lua::api::resolve_unusable;
    }

    record->channel = channel;
    record->group = group;
    record->gen = tw::engine::groups::generation_of(group);

    return tw::lua::api::resolve_ok;
}

// Registers a subscription, shared by the by-name and by-index forms of on_call and mute.
//
// **A group that is not loaded is not a failure any more.** It used to be: the call returned -1 and
// Lua re-tried it every 60 dispatches, forever, which cost a full group scan per attempt and never
// stopped for a name that would never resolve. Now the record is kept and the group registry
// attaches it the moment that group appears, so waiting costs nothing at all.
//
// -1 still means "this will not happen": either the group is loaded and has no such channel - a
// final answer, because a group's channel list does not change once it is loaded - or the channel
// could not be hooked. Both are reported here rather than left for Lua to count misses.
int resolve_and_subscribe(
    const char* group_name, const char* channel_name, int index, int owner, int after, bool mute, int* out) noexcept
{
    const auto set = [out](tw::lua::api::resolve_status status) {
        if(out != nullptr) {
            out[0] = status;
            out[1] = static_cast<int>(tw::engine::kind::unknown);
        }
    };

    auto* record = new(std::nothrow) subscription {};
    if(record == nullptr) {
        set(tw::lua::api::resolve_unusable);
        return -1;
    }

    record->id = g_next_subscription_id;
    record->owner = owner;
    record->channel = nullptr;
    record->group = nullptr;
    record->gen = tw::engine::groups::no_generation;
    record->group_name = group_name != nullptr ? group_name : "";
    record->channel_name = channel_name != nullptr ? channel_name : "";
    record->index = channel_name != nullptr ? -1 : index;
    record->after = after != 0;
    record->mute = mute;
    record->enabled = true;

    const tw::lua::api::resolve_status status = attach_subscription(record);

    if(status == tw::lua::api::resolve_no_channel || status == tw::lua::api::resolve_unusable) {
        report_missing_target(*record, status);
        delete record;
        set(status);
        return -1;
    }

    g_subscriptions.push_back(record);
    ++g_next_subscription_id;

    set(status);

    return record->id;
}

// A group is being destroyed. The shim copies are already off its channels by the time this runs -
// engine_groups does that first, before any listener - so all that is left is to forget the
// pointers, which puts the subscription back into the waiting state it started in.
void on_group_unloading(A3d_ChannelGroup* group, tw::engine::groups::generation /*gen*/) noexcept
{
    for(subscription* record : g_subscriptions) {
        if(record->group == group) {
            record->channel = nullptr;
            record->group = nullptr;
            record->gen = tw::engine::groups::no_generation;
        }
    }
}

// The roster moved. Cold path by construction: it runs only on the frames where a group actually
// appeared or disappeared, which during play is the start and the end of a run.
void on_groups_changed() noexcept
{
    for(subscription* record : g_subscriptions) {
        if(record->channel != nullptr) {
            continue;
        }

        if(attach_subscription(record) == tw::lua::api::resolve_no_channel) {
            report_missing_target(*record, tw::lua::api::resolve_no_channel);
        }
    }
}
} // namespace

namespace tw::lua::api
{
extern "C" {
int tw_on_call(int owner, const char* group_name, const char* channel_name, int after, int* out) noexcept
{
    return resolve_and_subscribe(group_name, channel_name, 0, owner, after, false, out);
}

int tw_on_call_at(int owner, const char* group_name, int index, int after, int* out) noexcept
{
    return resolve_and_subscribe(group_name, nullptr, index, owner, after, false, out);
}

int tw_mute(int owner, const char* group_name, const char* channel_name, int* out) noexcept
{
    return resolve_and_subscribe(group_name, channel_name, 0, owner, 0, true, out);
}

int tw_mute_at(int owner, const char* group_name, int index, int* out) noexcept
{
    return resolve_and_subscribe(group_name, nullptr, index, owner, 0, true, out);
}

void tw_mute_set(int id, int enable) noexcept
{
    subscription* record = find_subscription(id);
    if(record == nullptr || !record->mute) {
        return;
    }

    record->enabled = enable != 0;
}
}

void unsubscribe_owner(int owner) noexcept
{
    // Two passes, because the shim keys on the record pointer and the records have to outlive the
    // detach: collect first, detach, then free.
    std::vector<void*> doomed;
    for(subscription* record : g_subscriptions) {
        if(record->owner == owner) {
            doomed.push_back(record);
        }
    }

    if(doomed.empty()) {
        return;
    }

    tw::framework::channel_shim::unsubscribe_all_of(doomed);

    std::erase_if(g_subscriptions, [owner](const subscription* record) { return record->owner == owner; });

    for(void* record : doomed) {
        delete static_cast<subscription*>(record);
    }
}

void clear_subscriptions() noexcept
{
    tw::framework::channel_shim::remove_all();

    for(subscription* record : g_subscriptions) {
        delete record;
    }
    g_subscriptions.clear();
}

int subscription_count(int owner) noexcept
{
    int total = 0;
    for(const subscription* record : g_subscriptions) {
        if(owner < 0 || record->owner == owner) {
            ++total;
        }
    }

    return total;
}

int waiting_count(int owner, std::vector<std::string>* groups) noexcept
{
    int waiting = 0;
    for(const subscription* record : g_subscriptions) {
        if(record->owner != owner || record->channel != nullptr) {
            continue;
        }

        ++waiting;
        if(groups != nullptr && std::ranges::find(*groups, record->group_name) == groups->end()) {
            groups->push_back(record->group_name);
        }
    }

    return waiting;
}

int shared_channel_count() noexcept
{
    // Distinct channels more than one subscription is attached to. Surfaced in the Scripts tab
    // because it is the one thing about a pile of third-party scripts that stays invisible until it
    // misbehaves: two of them quietly watching - or muting - the same node.
    std::vector<const A3d_Channel*> seen;
    for(const subscription* record : g_subscriptions) {
        if(tw::framework::channel_shim::subscriber_count(record->channel) <= 1) {
            continue;
        }
        if(std::ranges::find(seen, record->channel) == seen.end()) {
            seen.push_back(record->channel);
        }
    }

    return static_cast<int>(seen.size());
}

void install_engine_listeners() noexcept
{
    tw::engine::groups::subscribe_unloading(&on_group_unloading);
    tw::engine::groups::subscribe_changed(&on_groups_changed);
}
} // namespace tw::lua::api
