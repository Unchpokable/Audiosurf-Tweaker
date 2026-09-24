#include "pch.hxx"

#include "engine/channel_ref.hxx"

#include "engine/engine_symbols.hxx"

#include "plugin/quest3d_state.hxx"

namespace
{
using tw::engine::channel_ref;
using tw::engine::kind;
using tw::engine::resolve_status;

// A3d_Channel::channelCalculatedAtCount_ and ingoreTreeCountState_ (the CHIC chunk), the two fields
// CheckRenderCount consults (engine journal §4.3, boot journal §3.1). Object fields, not vtable
// offsets - the +0x10 here has nothing to do with slot 4 in channel_kind.cxx.
constexpr std::size_t k_calculated_at_offset = 0x10;
constexpr std::size_t k_ignore_tree_count_offset = 0x60;

// The tree count rings at 30000, so no value outside that range can ever compare equal to it -
// which is all it takes to make CheckRenderCount treat the channel as stale.
constexpr std::int32_t k_impossible_tree_count = -1;

bool reads_no_memo(A3d_Channel* channel) noexcept
{
    return *reinterpret_cast<const std::uint8_t*>(reinterpret_cast<const std::byte*>(channel) + k_ignore_tree_count_offset) != 0;
}

// The shared tail of both resolve forms: the family check, the slot check, and filling the ref.
resolve_status finish(A3d_ChannelGroup* group, A3d_Channel* channel, int index, kind want, channel_ref& out, kind* actual) noexcept
{
    // Before any family slot is touched. On the wrong family, slot 17 is not a wrong getter but a
    // different method altogether - InvalidateDeviceObjects, DrawSurfaces - and slot 19 can be a
    // Release() (boot journal §7.1).
    const kind found = tw::engine::channel_kind::of(channel);
    if(actual != nullptr) {
        *actual = found;
    }

    if(found != want) {
        return resolve_status::wrong_kind;
    }

    if(!tw::engine::channel_kind::callable_as(channel, found)) {
        return resolve_status::unusable;
    }

    out.channel = channel;
    out.group = group;
    out.gen = tw::engine::groups::generation_of(group);
    out.index = index;
    out.family = found;
    out.no_memo = reads_no_memo(channel);

    return resolve_status::ok;
}
} // namespace

namespace tw::engine::channels
{
bool available() noexcept
{
    return groups::available() && tw::plugin::quest3d::g_engine != nullptr;
}

A3d_ChannelGroup* find_group(const char* name) noexcept
{
    if(!available() || name == nullptr) {
        return nullptr;
    }

    return groups::find(name);
}

A3d_Channel* find_channel(A3d_ChannelGroup* group, const char* name) noexcept
{
    if(!available() || group == nullptr || name == nullptr) {
        return nullptr;
    }

    const auto get = reinterpret_cast<symbols::channel_by_name_fn>(symbols::groups().channel_by_name);
    return get(group, nullptr, name);
}

A3d_Channel* find_channel_at(A3d_ChannelGroup* group, int index) noexcept
{
    if(!available() || group == nullptr || index < 0) {
        return nullptr;
    }

    const auto get = reinterpret_cast<symbols::channel_by_index_fn>(symbols::groups().channel_by_index);
    return get(group, nullptr, index);
}

resolve_status resolve(const char* group, const char* name, kind want, channel_ref& out, kind* actual) noexcept
{
    out = {};
    if(actual != nullptr) {
        *actual = kind::unknown;
    }

    if(!available()) {
        return resolve_status::engine_pending;
    }

    A3d_ChannelGroup* const found_group = find_group(group);
    if(found_group == nullptr) {
        return resolve_status::no_group;
    }

    A3d_Channel* const channel = find_channel(found_group, name);
    if(channel == nullptr) {
        return resolve_status::no_channel;
    }

    return finish(found_group, channel, -1, want, out, actual);
}

resolve_status resolve_at(const char* group, int index, kind want, channel_ref& out, kind* actual) noexcept
{
    out = {};
    if(actual != nullptr) {
        *actual = kind::unknown;
    }

    if(!available()) {
        return resolve_status::engine_pending;
    }

    A3d_ChannelGroup* const found_group = find_group(group);
    if(found_group == nullptr) {
        return resolve_status::no_group;
    }

    A3d_Channel* const channel = find_channel_at(found_group, index);
    if(channel == nullptr) {
        return resolve_status::no_channel;
    }

    return finish(found_group, channel, index, want, out, actual);
}

channel_ref adopt(A3d_Channel* channel) noexcept
{
    channel_ref ref {};
    if(channel == nullptr) {
        return ref;
    }

    ref.channel = channel;
    ref.family = channel_kind::of(channel);
    ref.no_memo = reads_no_memo(channel);

    return ref;
}

bool valid(const channel_ref& ref) noexcept
{
    return ref.channel != nullptr && groups::alive(ref.group, ref.gen);
}

liveness live(const channel_ref& ref) noexcept
{
    if(ref.channel == nullptr || ref.group == nullptr) {
        return liveness::unknown;
    }

    if(ref.no_memo) {
        return liveness::unknown;
    }

    const auto group_count = reinterpret_cast<symbols::group_tree_count_fn>(symbols::groups().group_tree_count);
    const std::int32_t now = group_count(ref.group, nullptr);
    const std::int32_t at
        = *reinterpret_cast<const std::int32_t*>(reinterpret_cast<const std::byte*>(ref.channel) + k_calculated_at_offset);

    return at == now ? liveness::yes : liveness::no;
}

void bust_memo(const channel_ref& ref) noexcept
{
    if(ref.channel == nullptr || ref.no_memo) {
        return;
    }

    *reinterpret_cast<std::int32_t*>(reinterpret_cast<std::byte*>(ref.channel) + k_calculated_at_offset) = k_impossible_tree_count;
}

const char* name_of(A3d_Channel* channel) noexcept
{
    if(channel == nullptr || !symbols::groups_ready()) {
        return nullptr;
    }

    const auto get = reinterpret_cast<symbols::channel_name_fn>(symbols::groups().channel_name);
    return get(channel, nullptr);
}
} // namespace tw::engine::channels
