#include "pch.hpp"

#include "quest_wrappers.hpp"

namespace
{
struct ChannelGroupLayout
{
    void* vptr;
    bool unused;

    char list; // we only need an offset so let it be a char
};

struct A3dListLayout
{
    void** data;
    int count;
    int buffer_size;
};
}

namespace
{
using engine_interface_get_channel_group_idx_func = void*(__thiscall*)(void* this_, std::int32_t idx);
engine_interface_get_channel_group_idx_func engine_interface_get_channel_group_idx;

using engine_interface_get_channel_group_count_func = std::int32_t(__thiscall*)(void* this_);
engine_interface_get_channel_group_count_func engine_interface_get_channel_group_count;
} // namespace

namespace
{
using channel_group_get_filename_func = const char*(__thiscall*)(void* this_);
channel_group_get_filename_func channel_group_get_filename;

using channel_group_get_pool_name_func = const char*(__thiscall*)(void* this_);
channel_group_get_pool_name_func channel_group_get_pool_name;

using channel_group_get_channel_func = void*(__thiscall*)(void* this_, const char* name);
channel_group_get_channel_func channel_group_get_channel;

using channel_group_get_channel_count_func = std::int32_t(__thiscall*)(void* this_);
channel_group_get_channel_count_func channel_group_get_channel_count;

using channel_group_get_unique_channel_func = void*(__thiscall*)(void* this_, std::int32_t id);
channel_group_get_unique_channel_func channel_group_get_unique_channel;

using channel_group_get_channel_guid_func = void*(__thiscall*)(void* this_, GUID guid);
channel_group_get_channel_guid_func channel_group_get_guid_channel;
} // namespace

namespace
{
using channel_get_name_func = const char*(__thiscall*)(void* this_);
channel_get_name_func channel_get_name;

using channel_get_group_func = void*(__thiscall*)(void* this_);
channel_get_group_func channel_get_group;
} // namespace

tw::game::AcoChannel::AcoChannel(void* ptr)
{
    m_ptr = ptr;
}

std::string_view tw::game::AcoChannel::get_name() const
{
    return channel_get_name(m_ptr);
}

tw::game::AcoChannelGroup tw::game::AcoChannel::get_group() const
{
    auto ptr = channel_get_group(m_ptr);

    return { ptr };
}

void* tw::game::AcoChannel::offset_field(std::ptrdiff_t field_offset) const
{
    return static_cast<char*>(m_ptr) + field_offset;
}

tw::game::AcoChannelGroup::AcoChannelGroup(void* ptr)
{
    m_ptr = ptr;
}

std::string_view tw::game::AcoChannelGroup::get_file_name() const
{
    return channel_group_get_filename(m_ptr);
}

std::string_view tw::game::AcoChannelGroup::get_pool_name() const
{
    return channel_group_get_pool_name(m_ptr);
}

std::int32_t tw::game::AcoChannelGroup::get_channel_count() const
{
    auto list_addr = &static_cast<ChannelGroupLayout*>(m_ptr)->list;

    auto list = reinterpret_cast<A3dListLayout*>(list_addr);

    return list->count;
}

tw::game::AcoChannel tw::game::AcoChannelGroup::get_channel(const char* name) const
{
    auto ptr = channel_group_get_channel(m_ptr, name);

    return { ptr };
}

tw::game::AcoChannel tw::game::AcoChannelGroup::get_channel(std::int32_t id) const
{
    using get_channel_at_func = void*(__thiscall*)(void*, std::int32_t);
    // v5 = (A3d_Channel *)(*(int (__thiscall **)(A3d_ChannelGroup *, int))(*(_DWORD *)this + 72))(this, v4);
    // Actually - I have no fucking idea what is going on here, but it looks like a virtual call
    // Why this function is private - The Question are greater than a question about meaning of live, universe and things
    // Quest3D. Quest3D never changes.
    auto get_channel_at = reinterpret_cast<get_channel_at_func>(*static_cast<void**>(m_ptr) + 18);

    return get_channel_at(m_ptr, id);
}

tw::game::AcoChannel tw::game::AcoChannelGroup::get_unique_channel(std::int32_t id) const
{
    auto ptr = channel_group_get_unique_channel(m_ptr, id);

    return { ptr };
}

tw::game::AcoChannel tw::game::AcoChannelGroup::get_unique_channel(GUID guid) const
{
    auto ptr = channel_group_get_guid_channel(m_ptr, guid);

    return { ptr };
}

std::int32_t tw::game::AcoChannelGroup::get_unique_channel_count() const
{
    return channel_group_get_channel_count(m_ptr);
}

tw::game::AcoEngineInterface::AcoEngineInterface(void* ptr)
{
    //assert(ptr);
    m_ptr = ptr;
}

tw::game::AcoChannelGroup tw::game::AcoEngineInterface::get_channel_group(std::int32_t id) const
{
    auto ptr = engine_interface_get_channel_group_idx(m_ptr, id);

    return { ptr };
}

std::int32_t tw::game::AcoEngineInterface::get_channel_group_count() const
{
    return engine_interface_get_channel_group_count(m_ptr);
}

void tw::game::initialize()
{
    static constexpr char module[] = "HighPoly.dll";

    engine_interface_get_channel_group_idx = reinterpret_cast<engine_interface_get_channel_group_idx_func>(
        DetourFindFunction(module, "?GetChannelGroup@EngineInterface@@UAEPAVA3d_ChannelGroup@@H@Z"));

    engine_interface_get_channel_group_count = reinterpret_cast<engine_interface_get_channel_group_count_func>(
        DetourFindFunction(module, "?GetChannelGroupCount@EngineInterface@@UAEHXZ"));

    channel_group_get_filename = reinterpret_cast<channel_group_get_filename_func>(
        DetourFindFunction(module, "?GetChannelGroupFileName@A3d_ChannelGroup@@UAEPBDXZ"));

    channel_group_get_pool_name =
        reinterpret_cast<channel_group_get_pool_name_func>(DetourFindFunction(module, "?GetPoolName@A3d_ChannelGroup@@UAEPBDXZ"));

    channel_group_get_channel = reinterpret_cast<channel_group_get_channel_func>(
        DetourFindFunction(module, "?GetChannel@A3d_ChannelGroup@@UAEPAVA3d_Channel@@PBD@Z"));

    channel_group_get_unique_channel = reinterpret_cast<channel_group_get_unique_channel_func>(
        DetourFindFunction(module, "?GetUniqueChannel@A3d_ChannelGroup@@UAEPAVA3d_Channel@@H@Z"));

    channel_group_get_channel_count = reinterpret_cast<channel_group_get_channel_count_func>(
        DetourFindFunction(module, "?GetUniqueChannelCount@A3d_ChannelGroup@@UAEHXZ"));

    channel_get_name = reinterpret_cast<channel_get_name_func>(DetourFindFunction(module, "?GetChannelName@A3d_Channel@@QAEPBDXZ"));

    channel_get_group =
        reinterpret_cast<channel_get_group_func>(DetourFindFunction(module, "?GetChannelGroup@A3d_Channel@@QAEPAVA3d_ChannelGroup@@XZ"));

    channel_group_get_guid_channel = reinterpret_cast<channel_group_get_channel_guid_func>(
        DetourFindFunction(module, "?GetUniqueChannel@A3d_ChannelGroup@@UAEPAVA3d_Channel@@U_GUID@@@Z"));

    assert(engine_interface_get_channel_group_idx);
    assert(engine_interface_get_channel_group_count);
    assert(channel_group_get_filename);
    assert(channel_group_get_pool_name);
    assert(channel_group_get_channel);
    assert(channel_get_name);
    assert(channel_get_group);
}
