#include "pch.hxx"

#include "engine/engine_symbols.hxx"

#include "plugin/diagnostics.hxx"

namespace
{
constexpr const wchar_t* k_highpoly_module = L"HighPoly.dll";

// What missing() reports when the module itself is not mapped, as opposed to an export being absent.
constexpr std::string_view k_module_missing = "HighPoly.dll (module not mapped)";

struct required_symbol {
    const char* name;
    std::size_t offset; // into the table being filled
};

// One list per tier, so "which one is missing" needs no parallel bookkeeping. Offsets rather than
// member pointers because the members are of several different pointer types and both tables are
// plain PODs - the cast happens in one place, below.
constexpr std::array<required_symbol, 4> k_spine { {
    { "?EngineLoop@EngineControl@@UAEXXZ", offsetof(tw::engine::symbols::table, engine_loop) },
    { "??_7EngineControl@@6B@", offsetof(tw::engine::symbols::table, engine_control_vtable) },
    { "?GetEngineInterface@EngineControl@@UAEPAVEngineInterface@@XZ", offsetof(tw::engine::symbols::table, get_engine_interface) },
    { "?GetTreeCalculateCount@EngineInterface@@UAEHXZ", offsetof(tw::engine::symbols::table, get_tree_calculate_count) },
} };

constexpr std::array<required_symbol, 13> k_groups { {
    { "?GetChannelGroupCount@EngineInterface@@UAEHXZ", offsetof(tw::engine::symbols::group_table, get_group_count) },
    { "?GetChannelGroup@EngineInterface@@UAEPAVA3d_ChannelGroup@@H@Z", offsetof(tw::engine::symbols::group_table, get_group_at) },
    { "?GetChannelGroupFileName@A3d_ChannelGroup@@UAEPBDXZ", offsetof(tw::engine::symbols::group_table, get_group_file_name) },
    { "?GetPoolName@A3d_ChannelGroup@@UAEPBDXZ", offsetof(tw::engine::symbols::group_table, get_pool_name) },
    { "?GetGroupIndex@A3d_ChannelGroup@@UAEHXZ", offsetof(tw::engine::symbols::group_table, get_group_index) },
    { "??_7EngineInterfaceExt@@6B@", offsetof(tw::engine::symbols::group_table, engine_interface_ext_vtable) },
    { "?GetStartGroup@EngineInterfaceExt@@UAEHXZ", offsetof(tw::engine::symbols::group_table, get_start_group) },
    { "?Release@A3d_ChannelGroup@@UAEXXZ", offsetof(tw::engine::symbols::group_table, group_release) },
    { "?DeleteChannelGroup@EngineInterface@@UAEXH@Z", offsetof(tw::engine::symbols::group_table, delete_channel_group) },
    { "?GetTreeCalculateCount@A3d_ChannelGroup@@QAEHXZ", offsetof(tw::engine::symbols::group_table, group_tree_count) },
    { "?GetChannel@A3d_ChannelGroup@@UAEPAVA3d_Channel@@PBD@Z", offsetof(tw::engine::symbols::group_table, channel_by_name) },
    { "?GetChannel@A3d_ChannelGroup@@UAEPAVA3d_Channel@@H@Z", offsetof(tw::engine::symbols::group_table, channel_by_index) },
    { "?GetChannelName@A3d_Channel@@QAEPBDXZ", offsetof(tw::engine::symbols::group_table, channel_name) },
} };

tw::engine::symbols::table g_table {};
tw::engine::symbols::group_table g_group_table {};
bool g_ready = false;
bool g_groups_ready = false;
std::string_view g_missing {};

// Fills `base` from one list, or reports the first name that is not there. Nothing is written into
// the caller's table until every symbol in the list resolved, so a half-filled table is not a state
// anything downstream has to consider.
bool resolve_into(HMODULE module_handle, std::span<const required_symbol> list, void* base) noexcept
{
    auto* const bytes = static_cast<std::byte*>(base);

    for(const required_symbol& symbol : list) {
        void* const address = reinterpret_cast<void*>(::GetProcAddress(module_handle, symbol.name));
        if(address == nullptr) {
            g_missing = symbol.name;
            return false;
        }

        *reinterpret_cast<void**>(bytes + symbol.offset) = address;
    }

    return true;
}
} // namespace

namespace tw::engine::symbols
{
bool initialize() noexcept
{
    if(g_ready) {
        return true;
    }

    // GetModuleHandleW rather than DetourFindFunction: the latter LoadLibrary's the module and falls
    // back to dbghelp for symbols. HighPoly is mapped long before anything here runs - it is the
    // module that loaded us.
    const HMODULE highpoly = ::GetModuleHandleW(k_highpoly_module);
    if(highpoly == nullptr) {
        g_missing = k_module_missing;
        return false;
    }

    table resolved {};
    if(!resolve_into(highpoly, k_spine, &resolved)) {
        TW_LOG_ERROR("engine_symbols: HighPoly.dll does not export {}", g_missing);
        return false;
    }

    g_table = resolved;
    g_ready = true;
    g_missing = {};

    // The group tier is resolved in the same pass but reported separately: losing it costs the
    // registry, not the spine.
    group_table groups_resolved {};
    if(resolve_into(highpoly, k_groups, &groups_resolved)) {
        g_group_table = groups_resolved;
        g_groups_ready = true;
    }
    else {
        TW_LOG_WARNING("engine_symbols: HighPoly.dll does not export {} - group registry unavailable", g_missing);
        g_missing = {};
    }

    return true;
}

bool is_ready() noexcept
{
    return g_ready;
}

bool groups_ready() noexcept
{
    return g_groups_ready;
}

const table& get() noexcept
{
    return g_table;
}

const group_table& groups() noexcept
{
    return g_group_table;
}

std::string_view missing() noexcept
{
    return g_missing;
}
} // namespace tw::engine::symbols
