#include "pch.hxx"

#include "lua/lua_vm.hxx"

#include "lua/api/api_channels.hxx"
#include "lua/api/api_core.hxx"
#include "lua/api/api_hooks.hxx"
#include "lua/api/api_hud.hxx"
#include "lua/lua_prelude.hxx"

#include "plugin/diagnostics.hxx"
#include "plugin/paths.hxx"

namespace
{
lua_State* g_lua = nullptr;
int g_traceback_ref = LUA_NOREF;
std::string g_last_error;

// Registry references to the prelude's exports, indexed by vm::fn.
constexpr const char* k_export_names[] = {
    "dispatch_frame",
    "dispatch_tick",
    "dispatch_post_tick",
    "dispatch_call",
    "dispatch_state",
    "dispatch_graph",
    "unload_owner",
    "run_script",
};

std::array<int, std::size(k_export_names)> g_exports = [] {
    std::array<int, std::size(k_export_names)> refs {};
    refs.fill(LUA_NOREF);
    return refs;
}();

// **The C side of the ABI, and the only list of it.** Every function a script can reach, by the name
// the prelude binds it under (tw.abi in lua_prelude.cxx), which is its C name.
//
// Keyed by name rather than by position, and checked in both directions when the prelude starts: a
// name here that the prelude has no signature for, or one the prelude binds that is missing here,
// stops the VM from starting, with the name in the error. What used to break silently - inserting an
// entry mid-list shifted everything after it onto the wrong signature - now cannot. Order is free;
// entries are grouped by the api/ file that defines them.
struct entry_point {
    const char* name;
    void* address;
};

#define TW_LUA_ENTRY(function) { #function, reinterpret_cast<void*>(&tw::lua::api::function) }

const entry_point k_entry_points[] = {
    // api_core
    TW_LUA_ENTRY(tw_script_enter),
    TW_LUA_ENTRY(tw_script_leave),
    TW_LUA_ENTRY(tw_diag),
    TW_LUA_ENTRY(tw_log),
    TW_LUA_ENTRY(tw_notify),
    TW_LUA_ENTRY(tw_engine_ready),
    TW_LUA_ENTRY(tw_can_write),
    TW_LUA_ENTRY(tw_state),
    TW_LUA_ENTRY(tw_state_name),
    TW_LUA_ENTRY(tw_ready),
    TW_LUA_ENTRY(tw_graph_revision),
    TW_LUA_ENTRY(tw_group_count),
    TW_LUA_ENTRY(tw_group_name),
    TW_LUA_ENTRY(tw_group_loaded),
    TW_LUA_ENTRY(tw_frame),
    TW_LUA_ENTRY(tw_dt),
    TW_LUA_ENTRY(tw_ease),
    TW_LUA_ENTRY(tw_ease_count),
    TW_LUA_ENTRY(tw_ease_name),

    // api_channels
    TW_LUA_ENTRY(tw_channel_resolve),
    TW_LUA_ENTRY(tw_channel_resolve_at),
    TW_LUA_ENTRY(tw_ref_free),
    TW_LUA_ENTRY(tw_kind_name),
    TW_LUA_ENTRY(tw_channel_get),
    TW_LUA_ENTRY(tw_channel_set),
    TW_LUA_ENTRY(tw_channel_text),
    TW_LUA_ENTRY(tw_channel_vector),
    TW_LUA_ENTRY(tw_channel_set_vector),
    TW_LUA_ENTRY(tw_channel_matrix),
    TW_LUA_ENTRY(tw_channel_set_matrix),
    TW_LUA_ENTRY(tw_channel_live),
    TW_LUA_ENTRY(tw_array_read),
    TW_LUA_ENTRY(tw_array_read_vector),
    TW_LUA_ENTRY(tw_array_write),
    TW_LUA_ENTRY(tw_array_write_vector),
    TW_LUA_ENTRY(tw_array_rows),

    // api_hooks
    TW_LUA_ENTRY(tw_on_call),
    TW_LUA_ENTRY(tw_on_call_at),
    TW_LUA_ENTRY(tw_mute),
    TW_LUA_ENTRY(tw_mute_at),
    TW_LUA_ENTRY(tw_mute_set),

    // api_hud
    TW_LUA_ENTRY(tw_theme_count),
    TW_LUA_ENTRY(tw_theme_name),
    TW_LUA_ENTRY(tw_theme_color),
    TW_LUA_ENTRY(tw_font_count),
    TW_LUA_ENTRY(tw_font_name),
    TW_LUA_ENTRY(tw_hud_text),
    TW_LUA_ENTRY(tw_hud_text_sized),
    TW_LUA_ENTRY(tw_hud_text_font),
    TW_LUA_ENTRY(tw_hud_text_glow),
    TW_LUA_ENTRY(tw_hud_measure),
    TW_LUA_ENTRY(tw_hud_measure_font),
    TW_LUA_ENTRY(tw_hud_rect),
    TW_LUA_ENTRY(tw_hud_rect_corners),
    TW_LUA_ENTRY(tw_hud_rect_glow),
    TW_LUA_ENTRY(tw_hud_rect_gradient),
    TW_LUA_ENTRY(tw_hud_line),
    TW_LUA_ENTRY(tw_hud_icon),
    TW_LUA_ENTRY(tw_hud_metric),
    TW_LUA_ENTRY(tw_hud_widget_rect),
};

#undef TW_LUA_ENTRY

// Everything a script must not reach. Runs after the prelude, which is the only code that
// legitimately needs `ffi`, `require` and `debug`.
//
// This is the "good enough for a first cut" version of Docs/Internal/lua-scripting.md §6: it removes
// the obvious capabilities, but it is not a hardened sandbox, and loading a script is still an act
// of trust in its author.
void strip_sandbox(lua_State* lua) noexcept
{
    static constexpr const char* k_globals_to_remove[] = {
        "ffi",       // the whole point of §6 - already cast into closures, no longer nameable
        "io",        //
        "package",   //
        "require",   //
        "dofile",    //
        "loadfile",  //
        "load",      //
        "loadstring",//
        "debug",     // captured by the prelude and for tracebacks before this runs
        "jit",       // jit.util is a memory-inspection surface; nothing here needs the rest
        "newproxy",  //
        "collectgarbage",
    };

    for(const char* name : k_globals_to_remove) {
        lua_pushnil(lua);
        lua_setglobal(lua, name);
    }

    // os keeps clock/time/date and loses the rest.
    lua_getglobal(lua, "os");
    if(lua_istable(lua, -1)) {
        static constexpr const char* k_os_fields_to_remove[] = { "execute", "remove", "rename", "tmpname", "exit", "getenv", "setlocale" };
        for(const char* field : k_os_fields_to_remove) {
            lua_pushnil(lua);
            lua_setfield(lua, -2, field);
        }
    }
    lua_pop(lua, 1);
}

// Runs every prelude section against one private table, which arrives holding the entry points and
// leaves holding the exports. Leaves nothing on the stack either way.
bool run_prelude(lua_State* lua) noexcept
{
    lua_newtable(lua); // S
    const int shared = lua_gettop(lua);

    lua_createtable(lua, 0, static_cast<int>(std::size(k_entry_points)));
    for(const entry_point& entry : k_entry_points) {
        lua_pushlightuserdata(lua, entry.address);
        lua_setfield(lua, -2, entry.name);
    }
    lua_setfield(lua, shared, "ptrs");

    for(const tw::lua::prelude::section& section : tw::lua::prelude::sections()) {
        if(luaL_loadbuffer(lua, section.source.data(), section.source.size(), section.chunk_name) != 0) {
            tw::lua::vm::set_error("prelude", lua_tostring(lua, -1));
            lua_settop(lua, shared - 1);
            return false;
        }

        lua_pushvalue(lua, shared);
        if(lua_pcall(lua, 1, 0, 0) != 0) {
            tw::lua::vm::set_error("prelude", lua_tostring(lua, -1));
            lua_settop(lua, shared - 1);
            return false;
        }
    }

    lua_getfield(lua, shared, "exports");
    if(!lua_istable(lua, -1)) {
        tw::lua::vm::set_error("prelude", "no exports table");
        lua_settop(lua, shared - 1);
        return false;
    }

    for(std::size_t i = 0; i < std::size(k_export_names); ++i) {
        lua_getfield(lua, -1, k_export_names[i]);
        if(!lua_isfunction(lua, -1)) {
            tw::lua::vm::set_error("prelude", (std::string("export missing: ") + k_export_names[i]).c_str());
            lua_settop(lua, shared - 1);
            return false;
        }
        g_exports[i] = luaL_ref(lua, LUA_REGISTRYINDEX);
    }

    lua_settop(lua, shared - 1);
    return true;
}

// The whole file, read through the wide path. luaL_loadfile would have been one call, but it opens the
// file with fopen and a narrow name - which cannot spell a path the ANSI code page does not cover, and
// the scripts folder now sits under the game's install, wherever that is.
bool read_script(const std::filesystem::path& path, std::string& out)
{
    std::ifstream file { path, std::ios::binary };
    if(!file.is_open()) {
        return false;
    }

    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if(size < 0) {
        return false;
    }
    file.seekg(0, std::ios::beg);

    out.resize(static_cast<std::size_t>(size));
    if(!out.empty()) {
        file.read(out.data(), size);
    }

    return static_cast<bool>(file) || file.eof();
}
} // namespace

namespace tw::lua::vm
{
bool initialize() noexcept
{
    if(g_lua != nullptr) {
        return true;
    }

    lua_State* lua = luaL_newstate();
    if(lua == nullptr) {
        set_error("luaL_newstate", "out of memory");
        return false;
    }

    luaL_openlibs(lua);

    // Capture debug.traceback before strip_sandbox() removes the table it lives in - error reports
    // without a traceback are much harder to act on, and the script never needs the capability.
    lua_getglobal(lua, "debug");
    if(lua_istable(lua, -1)) {
        lua_getfield(lua, -1, "traceback");
        if(lua_isfunction(lua, -1)) {
            g_traceback_ref = luaL_ref(lua, LUA_REGISTRYINDEX);
        }
        else {
            lua_pop(lua, 1);
        }
    }
    lua_pop(lua, 1);

    if(!run_prelude(lua)) {
        lua_close(lua);
        g_traceback_ref = LUA_NOREF;
        g_exports.fill(LUA_NOREF);
        return false;
    }

    strip_sandbox(lua);

    g_lua = lua;
    return true;
}

void shutdown() noexcept
{
    if(g_lua == nullptr) {
        return;
    }

    lua_close(g_lua);
    g_lua = nullptr;
    g_traceback_ref = LUA_NOREF;
    g_exports.fill(LUA_NOREF);
}

lua_State* state() noexcept
{
    return g_lua;
}

bool push(fn which) noexcept
{
    const int ref = g_exports[static_cast<std::size_t>(which)];
    if(g_lua == nullptr || ref == LUA_NOREF) [[unlikely]] {
        return false;
    }

    lua_rawgeti(g_lua, LUA_REGISTRYINDEX, ref);
    return true;
}

int push_traceback() noexcept
{
    if(g_lua == nullptr || g_traceback_ref == LUA_NOREF) {
        return 0;
    }

    lua_rawgeti(g_lua, LUA_REGISTRYINDEX, g_traceback_ref);
    return lua_gettop(g_lua);
}

// Each script gets its own globals table chained to _G, so two scripts declaring the same global do
// not overwrite each other; reads still fall through to the shared `tw`.
//
// `owner` is stamped into that environment as __tw_owner, which is how every registration the script
// makes - now or from a callback minutes later - is attributed back to it. See caller_owner in the
// prelude's tw.core.
bool run_file(const std::filesystem::path& path, int owner, std::string& error) noexcept
{
    if(g_lua == nullptr) {
        error = "the VM is not running";
        return false;
    }

    std::string source;
    if(!read_script(path, source)) {
        error = "cannot read " + tw::plugin::paths::to_utf8(path.filename());
        return false;
    }

    // "@name" is what luaL_loadfile would have used, minus the directory: every script lives in the same
    // folder, and an error line that repeats the whole install path in the Scripts tab is noise.
    const std::string chunk_name = "@" + tw::plugin::paths::to_utf8(path.filename());

    lua_State* const lua = g_lua;
    const int base = lua_gettop(lua);

    // The prelude runs the body (registration window open, its own traceback), so what is pushed here
    // is run_script and the chunk as its argument. The outer pcall only catches the prelude failing.
    if(!push(fn::run_script)) {
        error = "the prelude has no run_script";
        return false;
    }

    if(luaL_loadbuffer(lua, source.data(), source.size(), chunk_name.c_str()) != 0) {
        error = lua_tostring(lua, -1);
        lua_settop(lua, base);
        return false;
    }

    // Per-script globals chained to _G: writes stay local to the script, reads still find `tw`.
    lua_newtable(lua); // env
    lua_pushinteger(lua, owner);
    lua_setfield(lua, -2, "__tw_owner");
    lua_newtable(lua); // metatable
    lua_pushvalue(lua, LUA_GLOBALSINDEX);
    lua_setfield(lua, -2, "__index");
    lua_setmetatable(lua, -2);
    lua_setfenv(lua, -2);

    if(lua_pcall(lua, 1, 1, 0) != 0) {
        error = lua_tostring(lua, -1);
        lua_settop(lua, base);
        return false;
    }

    const bool ok = lua_isnil(lua, -1);
    if(!ok) {
        const char* const message = lua_tostring(lua, -1);
        error = message != nullptr ? message : "error";
    }

    lua_settop(lua, base);
    return ok;
}

void set_error(std::string_view where, const char* detail) noexcept
{
    g_last_error.assign(where);
    if(detail != nullptr) {
        g_last_error.append(": ");
        g_last_error.append(detail);
    }

    TW_LOG_ERROR("lua_vm: {}", g_last_error);
}

std::string_view last_error() noexcept
{
    return g_last_error;
}
} // namespace tw::lua::vm
