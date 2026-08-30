#include "pch.hxx"

#include "lua/lua_host.hxx"

#include "lua/lua_api.hxx"
#include "lua/lua_channels.hxx"
#include "lua/lua_config.hxx"

#include "plugin/diagnostics.hxx"
#include "plugin/globals.hxx"

#include "ui/plugins/static/notefeed.hxx"

#include <imgui.h>
#include <imgui_internal.h>

namespace
{
lua_State* g_lua = nullptr;
int g_traceback_ref = LUA_NOREF;
int g_dispatch_ref = LUA_NOREF;
int g_dispatch_call_ref = LUA_NOREF;
int g_unload_ref = LUA_NOREF;
int g_loaded_scripts = 0;
bool g_frame_dispatch_disabled = false;
bool g_call_dispatch_disabled = false;
std::string g_last_error;

// The prelude. Runs once, before any user script, with __tw_ptrs holding the addresses from
// lua_api::entry_points() as lightuserdata.
//
// This is where the whole FFI story of Docs/Internal/lua-scripting.md §2.2 actually happens: every
// call a script makes into the plugin goes through one of the ffi.cast'ed pointers below, so it
// compiles into the trace as a direct call instead of aborting it the way a lua_CFunction would.
// The host then deletes `ffi` from the environment (see strip_sandbox) - the already-cast pointers
// keep working because they are values, not names, which is exactly the property §6 relies on to
// keep the fast path without handing scripts arbitrary memory access.
constexpr const char* k_bootstrap = R"LUA(
local ffi = require("ffi")

local P = __tw_ptrs
__tw_ptrs = nil

-- Signatures must match src/lua/lua_api.hxx, positionally. extern "C" on x86 MSVC is __cdecl,
-- which is also the FFI's default, so no calling-convention decoration is needed.
local C_channel_resolve = ffi.cast("void* (*)(const char*, const char*, int, int*)",    P[1])
local C_channel_get     = ffi.cast("float (*)(void*)",                                  P[2])
local C_engine_ready    = ffi.cast("int (*)(void)",                                     P[3])
local C_log             = ffi.cast("void (*)(const char*)",                             P[4])
local C_notify          = ffi.cast("void (*)(const char*)",                             P[5])
local C_hud_text        = ffi.cast("void (*)(float, float, unsigned int, const char*)", P[6])
local C_hud_metric      = ffi.cast("float (*)(int)",                                    P[7])
local C_channel_text    = ffi.cast("const char* (*)(void*)",                            P[8])
local C_kind_name       = ffi.cast("const char* (*)(int)",                              P[9])
local C_array_read      = ffi.cast("float (*)(void*, void*, float)",                    P[10])
local C_on_call         = ffi.cast("int (*)(int, const char*, const char*, int, int*)", P[11])
local C_channel_set     = ffi.cast("void (*)(void*, float)",                            P[12])
local C_group_count     = ffi.cast("int (*)(void)",                                     P[13])
local C_group_name      = ffi.cast("const char* (*)(int)",                              P[14])
local C_resolve_at      = ffi.cast("void* (*)(const char*, int, int, int*)",            P[15])
local C_on_call_at      = ffi.cast("int (*)(int, const char*, int, int, int*)",         P[16])
local C_channel_vector  = ffi.cast("int (*)(void*, float*)",                            P[17])
local C_hud_text_sized  = ffi.cast("void (*)(float, float, unsigned int, const char*, float)", P[18])
local C_hud_measure     = ffi.cast("void (*)(const char*, float, float*)",              P[19])
local C_hud_rect        = ffi.cast("void (*)(float, float, float, float, unsigned int, float, float)", P[20])
local C_hud_line        = ffi.cast("void (*)(float, float, float, float, unsigned int, float)", P[21])
local C_hud_widget_rect = ffi.cast("int (*)(int, float*)",                              P[22])
local C_mute            = ffi.cast("int (*)(int, const char*, const char*, int*)",      P[23])
local C_mute_at         = ffi.cast("int (*)(int, const char*, int, int*)",             P[24])
local C_mute_set        = ffi.cast("void (*)(int, int)",                                P[25])
local C_array_vector    = ffi.cast("int (*)(void*, void*, float, float*)",              P[26])
local C_theme_count     = ffi.cast("int (*)(void)",                                     P[27])
local C_theme_name      = ffi.cast("const char* (*)(int)",                              P[28])
local C_theme_color     = ffi.cast("unsigned int (*)(int)",                             P[29])

-- One reusable out-buffer for resolve results: [0] = status, [1] = the kind the channel actually is.
-- Allocated once here rather than per call, so a resolve costs no garbage.
local resolve_out = ffi.new("int[2]")

-- Same idea for the two float out-buffers the drawing/geometry calls fill. These are read every
-- frame, so allocating one per call would put the HUD path on the collector's critical list.
local vec_out  = ffi.new("float[3]")
local rect_out = ffi.new("float[4]")
local size_out = ffi.new("float[2]")

local KIND_NUMBER, KIND_TEXT, KIND_VECTOR = 0, 1, 2

local STATUS_OK, STATUS_PENDING = 0, 1
local STATUS_NO_GROUP, STATUS_NO_CHANNEL, STATUS_WRONG_KIND, STATUS_UNUSABLE = 2, 3, 4, 5

local tw = {}
tw.frame = 0

function tw.log(msg)    C_log(tostring(msg))    end
function tw.notify(msg) C_notify(tostring(msg)) end

-- Same as notify, but also logged. For things a script author needs to see even in a release build,
-- where tw.log alone goes nowhere.
function tw.warn(msg)
    msg = tostring(msg)
    C_log(msg)
    C_notify("Lua: " .. msg)
end

-- What the engine actually has loaded right now, as "<pool> | <file>" strings. The answer to "why
-- did my group not resolve" - and it changes as the game moves between menu and run.
function tw.groups()
    local out = {}
    for i = 0, C_group_count() - 1 do
        out[#out + 1] = ffi.string(C_group_name(i))
    end
    return out
end
function tw.engine_ready() return C_engine_ready() ~= 0 end

-- Resolution is lazy and retried, because it has to be: the engine pointer is captured by a detour
-- that only fires once the game calls a channel which does not override CallChannel, which in
-- practice can mean "after the player clicks something". A channel handle asked for at load time
-- would simply not exist yet. Retries are throttled to once every 60 dispatches because a miss
-- costs a linear _stricmp scan over the whole group.
--
-- The failure taxonomy matters more than it looks. "Not yet" and "wrong" are different things:
--   - engine not up, group not loaded, channel absent -> transient, stay quiet, keep retrying;
--   - asked through the wrong accessor -> a bug in the script, and it can never fix itself, so it
--     is raised as a Lua error rather than silently yielding nil forever.
local Channel = {}
Channel.__index = Channel

function Channel:resolve()
    if self.h ~= nil then return true end
    if tw.frame < self.retry_at then return false end
    self.retry_at = tw.frame + 60

    -- Cheap short-circuit before crossing into C at all. The host checks this too and is the
    -- authority; this one only saves the boundary crossing during the startup window, which can last
    -- until the player touches a menu.
    if C_engine_ready() == 0 then return false end

    local h
    if type(self.name) == "number" then
        h = C_resolve_at(self.group, self.name, self.kind, resolve_out)
    else
        h = C_channel_resolve(self.group, self.name, self.kind, resolve_out)
    end
    local status = resolve_out[0]

    if status == STATUS_OK then
        self.h = h
        return true
    end

    if status == STATUS_WRONG_KIND then
        error(string.format("%s.%s is a '%s' channel, not '%s' - use the matching accessor",
            self.group, self.name,
            ffi.string(C_kind_name(resolve_out[1])),
            ffi.string(C_kind_name(self.kind))), 3)
    end

    if status == STATUS_UNUSABLE then
        error(string.format("%s.%s cannot be read: its vtable slot is not code", self.group, self.name), 3)
    end

    -- Transient, so this keeps retrying - but it is also how a typo looks, and a typo that stays
    -- silent forever is worse than a noisy one. Reported once per handle, and only after several
    -- attempts, so a group that genuinely has not loaded yet does not raise anything.
    --
    -- Deliberately through tw.warn (notefeed) and not tw.log: TW_LOG_* is compiled out of release
    -- builds, which is exactly where a user's script runs.
    self.misses = (self.misses or 0) + 1
    if not self.warned and self.misses >= 5 and (status == STATUS_NO_GROUP or status == STATUS_NO_CHANNEL) then
        self.warned = true
        tw.warn(string.format("%s.%s: %s", self.group, self.name,
            status == STATUS_NO_GROUP and "no such group loaded" or "no such channel in group"))
    end

    return false
end

function Channel:valid() return self.h ~= nil end

-- nil rather than a default when unavailable: a script showing "--" until the graph is reachable is
-- correct, one showing 0.0 or "" is lying.
local FloatChannel = setmetatable({}, { __index = Channel })
FloatChannel.__index = FloatChannel

function FloatChannel:get()
    if not self:resolve() then return nil end
    return C_channel_get(self.h)
end

local TextChannel = setmetatable({}, { __index = Channel })
TextChannel.__index = TextChannel

function TextChannel:get()
    if not self:resolve() then return nil end
    return ffi.string(C_channel_text(self.h))
end

-- Vector channels return three numbers rather than a table: a table per read would be garbage on the
-- HUD path, and every caller so far wants the components immediately anyway.
local VectorChannel = setmetatable({}, { __index = Channel })
VectorChannel.__index = VectorChannel

function VectorChannel:get()
    if not self:resolve() then return nil end
    if C_channel_vector(self.h, vec_out) == 0 then return nil end
    return vec_out[0], vec_out[1], vec_out[2]
end

local function make(mt, kind, group, name)
    return setmetatable({ h = nil, group = group, name = name, kind = kind, retry_at = 0 }, mt)
end

-- One accessor per channel family. Deliberately not a single generic tw.channel(): the engine's
-- vtable slots mean different things per family, so the accessor is what carries the type, and
-- asking through the wrong one is a mistake worth reporting rather than papering over.
--
-- `name` may be a number, meaning "the channel at this index in the group". Channel names are NOT
-- unique - TrafficCommander has two Values called "TrafficType", and a by-name lookup finds the
-- wrong one - so sometimes the index is the only way to be precise. It is also more brittle across
-- game versions, so prefer the name where it is unambiguous.
function tw.float_ch(group, name)  return make(FloatChannel,  KIND_NUMBER, group, name) end
function tw.string_ch(group, name) return make(TextChannel,   KIND_TEXT,   group, name) end
function tw.vector_ch(group, name) return make(VectorChannel, KIND_VECTOR, group, name) end

-- Kept as an alias so scripts written against the first cut keep working. New code should say what
-- it means.
tw.channel = tw.float_ch

function FloatChannel:set(value)
    if not self:resolve() then return false end
    C_channel_set(self.h, value)
    return true
end

-- An Array Table column, addressed the way the engine addresses one: a cursor channel carries the
-- row index, and the Array Value channel reads whatever row the cursor currently points at. The
-- save/set/read/restore around that lives in C, because the restore has to happen even if something
-- goes wrong - the cursor belongs to the game.
local Array = {}
Array.__index = Array

function Array:get(index)
    if not self.column:resolve() or not self.cursor:resolve() then return nil end
    return C_array_read(self.column.h, self.cursor.h, index)
end

function tw.array(group, column, cursor)
    return setmetatable({
        column = tw.float_ch(group, column),
        cursor = tw.float_ch(group, cursor),
    }, Array)
end

-- An `Array Vector` column. Same cursor mechanism, three numbers out instead of one.
local VectorArray = {}
VectorArray.__index = VectorArray

function VectorArray:get(index)
    if not self.column:resolve() or not self.cursor:resolve() then return nil end
    if C_array_vector(self.column.h, self.cursor.h, index, vec_out) == 0 then return nil end
    return vec_out[0], vec_out[1], vec_out[2]
end

function tw.array_vec(group, column, cursor)
    return setmetatable({
        column = tw.vector_ch(group, column),
        cursor = tw.float_ch(group, cursor),
    }, VectorArray)
end

tw.hud = {}

-- Colour is ImGui's packed IM_COL32 (0xAABBGGRR). Default is opaque white.
--
-- `size` is optional and in pixels; omitted means the overlay's own text size.
function tw.hud.text(x, y, text, color, size)
    if size then
        C_hud_text_sized(x, y, color or 0xFFFFFFFF, tostring(text), size)
    else
        C_hud_text(x, y, color or 0xFFFFFFFF, tostring(text))
    end
end

-- Width and height the same text would occupy. The measurement comes from ImGui, so it matches what
-- tw.hud.text actually draws - which is what makes centering and right-alignment exact rather than
-- approximate.
function tw.hud.measure(text, size)
    C_hud_measure(tostring(text), size or 0, size_out)
    return size_out[0], size_out[1]
end

-- The overlay's default text height. Layouts should scale off this instead of assuming a pixel size.
function tw.hud.font_size()
    return C_hud_metric(6)
end

-- A rectangle. `rounding` is the corner radius; `thickness` <= 0 (the default) fills it, anything
-- else strokes an outline.
function tw.hud.rect(x0, y0, x1, y1, color, rounding, thickness)
    C_hud_rect(x0, y0, x1, y1, color or 0xFFFFFFFF, rounding or 0, thickness or 0)
end

function tw.hud.line(x0, y0, x1, y1, color, thickness)
    C_hud_line(x0, y0, x1, y1, color or 0xFFFFFFFF, thickness or 1)
end

-- The overlay's own palette, by name: tw.theme("surface"), tw.theme("text_muted"), and so on.
--
-- Read live, not cached, because the Settings colour pickers edit the theme in place - a script that
-- sampled it once would drift out of the overlay's look the moment a theme changed. The name->index
-- table is built here, once, so the per-frame path is an array index rather than a string compare.
--
-- A script's own borders, backgrounds and muted text should come from here rather than from
-- invented constants: that is the difference between a HUD that belongs to Tweaker and one that
-- merely sits on top of it.
local THEME = {}
for i = 0, C_theme_count() - 1 do
    THEME[ffi.string(C_theme_name(i))] = i
end

function tw.theme(name)
    local i = THEME[name]
    if i == nil then error("unknown theme colour: " .. tostring(name), 2) end
    return C_theme_color(i)
end

-- Every name tw.theme accepts, for a script that wants to enumerate rather than guess.
function tw.theme_names()
    local out = {}
    for name in pairs(THEME) do out[#out + 1] = name end
    table.sort(out)
    return out
end

-- Same colour with a different alpha, since the palette entry usually carries the alpha the overlay
-- wants and a script often wants the same hue at a different weight.
function tw.alpha(colour, a)
    local v = math.floor(math.min(math.max(a, 0), 1) * 255 + 0.5)
    return (colour % 16777216) + v * 16777216
end

-- Scales the alpha a colour already has, rather than replacing it. This is the one a fade wants:
-- every colour in a widget keeps its relative weight while the whole thing dissolves, which is not
-- what happens if each is forced to the same absolute alpha.
function tw.fade(colour, k)
    local a = math.floor(colour / 16777216) * math.min(math.max(k, 0), 1)
    return (colour % 16777216) + math.floor(a + 0.5) * 16777216
end

-- Packs 0..1 floats into the colour format above. Written to take exactly what a vector channel
-- hands back, so `tw.rgb(colour_ch:get())` is the whole path from the game's live palette to a
-- drawing call.
function tw.rgb(r, g, b, a)
    local function q(v)
        v = math.floor((v or 0) * 255 + 0.5)
        if v < 0 then return 0 elseif v > 255 then return 255 else return v end
    end
    return q(r) + q(g) * 256 + q(b) * 65536 + q(a == nil and 1 or a) * 16777216
end

-- Viewport size in pixels. Scripts should position against this rather than hardcoding, since the
-- game runs at whatever resolution the player picked.
function tw.hud.size()
    return C_hud_metric(0), C_hud_metric(1)
end

-- x0, y0, x1, y1 of the area free of the overlay's own always-on chrome. The coarse answer, kept for
-- scripts that only want somewhere uncluttered to sit.
function tw.hud.safe()
    return C_hud_metric(2), C_hud_metric(3), C_hud_metric(4), C_hud_metric(5)
end

local WIDGETS = { notefeed = 0, pins = 1, watermark = 2, menu = 3 }

-- Exactly where one overlay widget is, as x0, y0, x1, y1 - or nil when it is not on screen (no pins
-- are showing, the menu is closed).
--
-- These are this frame's rectangles: scripts draw after every widget has laid itself out, so a HUD
-- can sit flush against the notefeed or beside an open menu and follow it while it is dragged. And
-- because scripts draw last, the geometry is for lining up against the overlay, not for staying out
-- of its way - script output goes on top of overlay chrome, not under it.
function tw.hud.widget(name)
    local id = WIDGETS[name]
    if id == nil then error("unknown overlay widget: " .. tostring(name), 2) end
    if C_hud_widget_rect(id, rect_out) == 0 then return nil end
    return rect_out[0], rect_out[1], rect_out[2], rect_out[3]
end

-- Which script is calling right now.
--
-- The host writes __tw_owner into each script's own environment table before running it, so this is
-- correct both for top-level code and for anything the script registers from a closure later. It has
-- to be an environment lookup rather than a "currently loading" global on the host side, because
-- registration is deferred: a tw.on_call issued at load time is not handed to C until the group it
-- names shows up, which can be minutes and several other scripts later.
--
-- Level 3 is the script: 1 is this function, 2 is the tw.* entry point that called it, 3 is whoever
-- called that. Called directly rather than through pcall on purpose - pcall would occupy a level of
-- its own and silently shift the count, which is how the first version of this credited every
-- subscription to nobody.
local function caller_owner()
    local env = getfenv(3)
    if type(env) == "table" and type(env.__tw_owner) == "number" then
        return env.__tw_owner
    end
    return -1
end

local handlers = {}
function tw.on_frame(fn) handlers[#handlers + 1] = { fn = fn, owner = caller_owner() } end

-- Channel-call subscriptions. The engine calls back with a numeric id, which is looked up here -
-- C never holds a Lua value, so there is nothing on that side for the collector to trip over.
--
-- Registration is retried the same way channel resolution is, and for the same reason: the group may
-- not be loaded yet. `pending` holds the ones still waiting.
local call_handlers = {}
local pending_calls = {}

-- `when` is "after" (default) or "before". After is what an event consumer wants: the Do_* handler
-- has run, so whatever it wrote is readable.
--
-- A "before" handler may return false to stop the engine's own handler from running at all. Any
-- other return - including none - lets it run, so an observer never suppresses anything by accident.
-- Returning false from an "after" handler does nothing; the call it would decline already happened.
function tw.on_call(group, name, when, fn)
    if fn == nil then
        fn, when = when, "after"
    end
    pending_calls[#pending_calls + 1] =
        { group = group, name = name, after = (when ~= "before"), fn = fn, retry_at = 0, owner = caller_owner() }
end

-- Takes a channel out of the graph entirely: the engine keeps calling it, and it keeps doing
-- nothing. The suppression lives in C, so a muted node costs one predicted branch per call rather
-- than a trip into the VM - which matters, because the things worth muting are render nodes the game
-- calls every frame.
--
-- Returns a handle with :on(), :off() and :set(bool). It starts on. Registration is retried like
-- on_call's, since the group may not be loaded yet.
--
-- Whether a given channel is safe to mute is a question about the game's graph, not about this API:
-- a node that only draws can go without consequence, while one whose value something else reads
-- leaves that reader on a stale number. Pick the node deliberately.
local Mute = {}
Mute.__index = Mute

function Mute:set(on)
    self.want = on and true or false
    if self.id then C_mute_set(self.id, self.want and 1 or 0) end
    return self
end

function Mute:on()  return self:set(true)  end
function Mute:off() return self:set(false) end
function Mute:active() return self.id ~= nil and self.want end

local pending_mutes = {}

function tw.mute(group, name)
    local m = setmetatable({ group = group, name = name, want = true, retry_at = 0, owner = caller_owner() }, Mute)
    pending_mutes[#pending_mutes + 1] = m
    return m
end

local function pump_pending_mutes()
    if #pending_mutes == 0 then return end
    if C_engine_ready() == 0 then return end

    for i = #pending_mutes, 1, -1 do
        local m = pending_mutes[i]
        if tw.frame >= m.retry_at then
            m.retry_at = tw.frame + 60
            local id
            if type(m.name) == "number" then
                id = C_mute_at(m.owner, m.group, m.name, resolve_out)
            else
                id = C_mute(m.owner, m.group, m.name, resolve_out)
            end
            if id >= 0 then
                m.id = id
                C_mute_set(id, m.want and 1 or 0)
                table.remove(pending_mutes, i)
            elseif resolve_out[0] == STATUS_NO_GROUP or resolve_out[0] == STATUS_NO_CHANNEL then
                m.misses = (m.misses or 0) + 1
                if not m.warned and m.misses >= 5 then
                    m.warned = true
                    tw.warn(string.format("mute target %s.%s not found", m.group, m.name))
                end
            end
        end
    end
end

local function pump_pending_calls()
    if #pending_calls == 0 then return end
    if C_engine_ready() == 0 then return end

    for i = #pending_calls, 1, -1 do
        local p = pending_calls[i]
        if tw.frame >= p.retry_at then
            p.retry_at = tw.frame + 60
            local id
            if type(p.name) == "number" then
                id = C_on_call_at(p.owner, p.group, p.name, p.after and 1 or 0, resolve_out)
            else
                id = C_on_call(p.owner, p.group, p.name, p.after and 1 or 0, resolve_out)
            end
            if id >= 0 then
                call_handlers[id] = { fn = p.fn, owner = p.owner }
                table.remove(pending_calls, i)
            elseif resolve_out[0] == STATUS_NO_GROUP or resolve_out[0] == STATUS_NO_CHANNEL then
                p.misses = (p.misses or 0) + 1
                if not p.warned and p.misses >= 5 then
                    p.warned = true
                    tw.warn(string.format("on_call target %s.%s not found", p.group, p.name))
                end
            end
        end
    end
end

-- The return value travels back to framework/channel_shim: false from a "before" handler cancels the
-- engine's call. Falling off the end returns nil, which proceeds.
function __tw_dispatch_call(id)
    local rec = call_handlers[id]
    if rec then return rec.fn() end
end

-- Forgets everything one script registered: its frame handlers, its channel callbacks, and anything
-- still queued waiting for a group to load.
--
-- The C side does the other half - tw_unsubscribe_owner puts the hooked channels' original vtables
-- back - and this half makes sure nothing is left pointing at a callback from a script that is no
-- longer running. Between them, a disabled script costs the game nothing: no vtable copy, no
-- dispatch, no VM entry. Its environment table becomes unreachable and the collector takes the rest.
function __tw_unload_owner(id)
    for i = #handlers, 1, -1 do
        if handlers[i].owner == id then table.remove(handlers, i) end
    end
    for i = #pending_calls, 1, -1 do
        if pending_calls[i].owner == id then table.remove(pending_calls, i) end
    end
    for i = #pending_mutes, 1, -1 do
        if pending_mutes[i].owner == id then table.remove(pending_mutes, i) end
    end
    for sid, rec in pairs(call_handlers) do
        if rec.owner == id then call_handlers[sid] = nil end
    end
end

_G.tw = tw
_G.print = tw.log

-- One entry point for the host, so the C side never has to walk a Lua table on the hot path.
function __tw_dispatch_frame()
    tw.frame = tw.frame + 1
    pump_pending_calls()
    pump_pending_mutes()
    for i = 1, #handlers do
        handlers[i].fn()
    end
end
)LUA";

void set_error(std::string_view where, const char* detail) noexcept
{
    g_last_error.assign(where);
    if(detail != nullptr) {
        g_last_error.append(": ");
        g_last_error.append(detail);
    }

    TW_LOG_ERROR("lua_host: {}", g_last_error);
}

// Pushes the traceback handler captured at bootstrap. Returns its stack index, or 0 when there is
// none - lua_pcall treats 0 as "no handler", so the caller needs no special case.
int push_traceback_handler(lua_State* lua) noexcept
{
    if(g_traceback_ref == LUA_NOREF) {
        return 0;
    }

    lua_rawgeti(lua, LUA_REGISTRYINDEX, g_traceback_ref);
    return lua_gettop(lua);
}

// Everything a script must not reach. Runs after the bootstrap chunk, which is the only code that
// legitimately needs `ffi` and `require`.
//
// This is the "good enough for a first cut" version of Docs/Internal/lua-scripting.md §6: it removes
// the obvious capabilities, but it is not a hardened sandbox, and loading a script is still an act
// of trust in its author.
void strip_sandbox(lua_State* lua) noexcept
{
    static constexpr const char* k_globals_to_remove[] = {
        "ffi",       // the whole point of §6 - already cast into closures above, no longer nameable
        "io",        //
        "package",   //
        "require",   //
        "dofile",    //
        "loadfile",  //
        "load",      //
        "loadstring",//
        "debug",     // captured for tracebacks before this runs
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

std::filesystem::path script_directory() noexcept
{
    wchar_t buffer[MAX_PATH] {};
    const DWORD length = ::GetModuleFileNameW(tw::plugin::globals::module_handle, buffer, MAX_PATH);
    if(length == 0 || length >= MAX_PATH) {
        return {};
    }

    std::error_code ec;
    std::filesystem::path directory = std::filesystem::path(buffer).parent_path() / L"scripts";
    if(!std::filesystem::is_directory(directory, ec)) {
        return {};
    }

    return directory;
}

// One entry per .lua file found, running or not.
struct script_entry {
    tw::lua::host::script_info info;
    std::filesystem::path path;
};

std::vector<script_entry> g_scripts;

script_entry* find_script(int id) noexcept
{
    for(script_entry& entry : g_scripts) {
        if(entry.info.id == id) {
            return &entry;
        }
    }

    return nullptr;
}

// Reads the `-- @key value` header annotations without executing anything.
//
// Executing is exactly what must not happen here: the tab lists scripts the user has turned off, and
// running one to find out what it calls itself would defeat the point of having turned it off.
//
// Scanning stops at the first line that is neither blank nor a comment - the header is a header, and
// a stray `@author` in a comment three hundred lines down is not metadata.
void read_header(script_entry& entry) noexcept
{
    entry.info.name = entry.path.stem().string();
    entry.info.author.clear();
    entry.info.version.clear();
    entry.info.description.clear();

    std::ifstream file { entry.path };
    if(!file.is_open()) {
        return;
    }

    std::string line;
    while(std::getline(file, line)) {
        std::string_view text { line };
        while(!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r')) {
            text.remove_prefix(1);
        }
        while(!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
            text.remove_suffix(1);
        }

        if(text.empty()) {
            continue;
        }
        if(!text.starts_with("--")) {
            break;
        }

        text.remove_prefix(2);
        while(!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '-')) {
            text.remove_prefix(1);
        }

        if(text.empty() || text.front() != '@') {
            continue;
        }
        text.remove_prefix(1);

        const auto space = text.find_first_of(" \t");
        if(space == std::string_view::npos) {
            continue;
        }

        const std::string_view key = text.substr(0, space);
        std::string_view value = text.substr(space + 1);
        while(!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
            value.remove_prefix(1);
        }

        if(value.empty()) {
            continue;
        }

        if(key == "name") {
            entry.info.name.assign(value);
        }
        else if(key == "author") {
            entry.info.author.assign(value);
        }
        else if(key == "version") {
            entry.info.version.assign(value);
        }
        else if(key == "description") {
            entry.info.description.assign(value);
        }
    }
}

// Takes one script's registrations back out, on both sides of the boundary.
//
// Lua first, then C. Ordering matters only in one direction: the C records are what the shim holds a
// pointer to, so they must be the last thing freed. Dropping the Lua callbacks first is harmless -
// a channel call landing in between finds no handler, and __tw_dispatch_call answers "proceed".
void unload_script(int owner) noexcept
{
    if(g_lua != nullptr && g_unload_ref != LUA_NOREF) {
        const int handler = push_traceback_handler(g_lua);
        lua_rawgeti(g_lua, LUA_REGISTRYINDEX, g_unload_ref);
        lua_pushinteger(g_lua, owner);
        if(lua_pcall(g_lua, 1, 0, handler) != 0) {
            set_error("unload", lua_tostring(g_lua, -1));
            lua_pop(g_lua, 1);
        }
        if(handler != 0) {
            lua_pop(g_lua, 1);
        }
    }

    tw::lua::api::tw_unsubscribe_owner(owner);
}

// Loads one .lua file and runs it. Each script gets its own globals table chained to _G, so two
// scripts declaring the same global do not overwrite each other; reads still fall through to the
// shared `tw`.
//
// `owner` is stamped into that environment as __tw_owner, which is how every registration the script
// makes - now or from a callback minutes later - is attributed back to it. See caller_owner() in the
// prelude.
bool run_script_file(lua_State* lua, const std::filesystem::path& path, int owner) noexcept
{
    const std::string narrow_path = path.string();

    // Handler first so its stack index stays valid for the whole sequence below.
    const int handler = push_traceback_handler(lua);

    if(luaL_loadfile(lua, narrow_path.c_str()) != 0) {
        set_error("load", lua_tostring(lua, -1));
        lua_pop(lua, handler == 0 ? 1 : 2);
        return false;
    }

    // Per-script globals chained to _G: writes stay local to the script, reads still find `tw`.
    lua_newtable(lua);                        // env
    lua_pushinteger(lua, owner);
    lua_setfield(lua, -2, "__tw_owner");
    lua_newtable(lua);                        // metatable
    lua_pushvalue(lua, LUA_GLOBALSINDEX);
    lua_setfield(lua, -2, "__index");
    lua_setmetatable(lua, -2);
    lua_setfenv(lua, -2);

    const bool failed = lua_pcall(lua, 0, 0, handler) != 0;
    if(failed) {
        set_error("run", lua_tostring(lua, -1));
        lua_pop(lua, 1);
    }

    if(handler != 0) {
        lua_pop(lua, 1);
    }

    return !failed;
}
} // namespace

namespace tw::lua::host
{
void initialize() noexcept
{
    if(g_lua != nullptr) {
        return;
    }

    // Best-effort and non-fatal: the graph entry points come from HighPoly.dll, which is certainly
    // mapped by the time the overlay exists, but the VM is useful (logging, HUD) even if it is not.
    tw::lua::channels::initialize();

    const std::filesystem::path directory = script_directory();
    if(directory.empty()) {
        TW_LOG_INFO("lua_host: no scripts/ directory next to the DLL - scripting stays idle");
        return;
    }

    lua_State* lua = luaL_newstate();
    if(lua == nullptr) {
        set_error("luaL_newstate", "out of memory");
        return;
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

    // Hand the C entry points over as lightuserdata for the bootstrap chunk to ffi.cast().
    const std::span<void* const> entries = tw::lua::api::entry_points();
    lua_createtable(lua, static_cast<int>(entries.size()), 0);
    for(std::size_t i = 0; i < entries.size(); ++i) {
        lua_pushlightuserdata(lua, entries[i]);
        lua_rawseti(lua, -2, static_cast<int>(i) + 1);
    }
    lua_setglobal(lua, "__tw_ptrs");

    if(luaL_loadstring(lua, k_bootstrap) != 0 || lua_pcall(lua, 0, 0, 0) != 0) {
        set_error("bootstrap", lua_tostring(lua, -1));
        lua_close(lua);
        return;
    }

    strip_sandbox(lua);

    lua_getglobal(lua, "__tw_dispatch_frame");
    if(!lua_isfunction(lua, -1)) {
        set_error("bootstrap", "__tw_dispatch_frame missing");
        lua_pop(lua, 1);
        lua_close(lua);
        return;
    }
    g_dispatch_ref = luaL_ref(lua, LUA_REGISTRYINDEX);

    lua_getglobal(lua, "__tw_dispatch_call");
    if(!lua_isfunction(lua, -1)) {
        set_error("bootstrap", "__tw_dispatch_call missing");
        lua_pop(lua, 1);
        lua_close(lua);
        return;
    }
    g_dispatch_call_ref = luaL_ref(lua, LUA_REGISTRYINDEX);

    lua_getglobal(lua, "__tw_unload_owner");
    if(!lua_isfunction(lua, -1)) {
        set_error("bootstrap", "__tw_unload_owner missing");
        lua_pop(lua, 1);
        lua_close(lua);
        return;
    }
    g_unload_ref = luaL_ref(lua, LUA_REGISTRYINDEX);

    g_lua = lua;

    tw::lua::config::load((directory.parent_path() / L"TweakerScripts.cfg").string());

    // Catalogue everything first, then run what is enabled. Two passes because the tab has to be
    // able to list a script the user turned off, and that listing comes from the file's header
    // rather than from anything the file does when it runs.
    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for(const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory, ec)) {
        if(entry.is_regular_file(ec) && entry.path().extension() == L".lua") {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);

    int next_id = 0;
    for(const std::filesystem::path& path : files) {
        script_entry entry;
        entry.path = path;
        entry.info.id = next_id++;
        entry.info.file = path.filename().string();
        read_header(entry);
        entry.info.enabled = tw::lua::config::enabled(entry.info.file);
        g_scripts.push_back(std::move(entry));
    }

    for(script_entry& entry : g_scripts) {
        if(!entry.info.enabled) {
            TW_LOG_INFO("lua_host: {} is disabled - not loading", entry.info.file);
            continue;
        }

        if(run_script_file(lua, entry.path, entry.info.id)) {
            entry.info.failed = false;
            entry.info.error.clear();
            ++g_loaded_scripts;
            TW_LOG_INFO("lua_host: loaded {}", entry.info.file);
        }
        else {
            entry.info.failed = true;
            entry.info.error = g_last_error;
            tw::ui::plugins::statics::notefeed::push("Lua: " + g_last_error);
        }
    }

    TW_LOG_INFO("lua_host: {} ({} of {} script(s) loaded)", LUAJIT_VERSION, g_loaded_scripts, g_scripts.size());
}

int script_count() noexcept
{
    return static_cast<int>(g_scripts.size());
}

const script_info* script_at(int index) noexcept
{
    if(index < 0 || index >= static_cast<int>(g_scripts.size())) {
        return nullptr;
    }

    // Refreshed here rather than tracked on every subscribe: this is read once per row per frame
    // while a tab is open, and the alternative is a counter that can drift out of step with the
    // thing it counts.
    g_scripts[static_cast<std::size_t>(index)].info.hooks =
        tw::lua::api::tw_subscription_count(g_scripts[static_cast<std::size_t>(index)].info.id);

    return &g_scripts[static_cast<std::size_t>(index)].info;
}

bool set_script_enabled(int id, bool enabled) noexcept
{
    script_entry* entry = find_script(id);
    if(entry == nullptr || g_lua == nullptr) {
        return false;
    }

    if(entry->info.enabled == enabled && !entry->info.failed) {
        return entry->info.enabled;
    }

    // Both dispatchers latch off on the first error, and until scripts could be toggled nothing
    // could ever clear that - one script throwing once took every script's on_frame down for the
    // rest of the session. Changing the set of running scripts is exactly the moment to try again:
    // the offending one may be the one just switched off, and if it is not, it will latch straight
    // back off on the next frame at no cost.
    g_frame_dispatch_disabled = false;
    g_call_dispatch_disabled = false;

    if(!enabled) {
        unload_script(entry->info.id);
        entry->info.enabled = false;
        entry->info.failed = false;
        entry->info.error.clear();

        tw::lua::config::set_enabled(entry->info.file, false);
        tw::lua::config::save();

        TW_LOG_INFO("lua_host: disabled {} ({} subscription(s) released)", entry->info.file,
            tw::lua::api::tw_subscription_count(entry->info.id));
        return false;
    }

    // Enabling is a fresh run from disk, not the resumption of anything - see the header. Unload
    // first anyway: a failed load can have registered handlers before it threw.
    unload_script(entry->info.id);
    read_header(*entry);

    if(!run_script_file(g_lua, entry->path, entry->info.id)) {
        unload_script(entry->info.id);
        entry->info.enabled = false;
        entry->info.failed = true;
        entry->info.error = g_last_error;
        tw::ui::plugins::statics::notefeed::push("Lua: " + g_last_error);
        return false;
    }

    entry->info.enabled = true;
    entry->info.failed = false;
    entry->info.error.clear();

    tw::lua::config::set_enabled(entry->info.file, true);
    tw::lua::config::save();

    TW_LOG_INFO("lua_host: enabled {}", entry->info.file);
    return true;
}

void reload_script(int id) noexcept
{
    const script_entry* entry = find_script(id);
    if(entry == nullptr || !entry->info.enabled) {
        return;
    }

    // Through the disable/enable path so there is exactly one teardown-and-run sequence to get right.
    (void)set_script_enabled(id, false);
    (void)set_script_enabled(id, true);
}

int shared_channel_count() noexcept
{
    return tw::lua::api::tw_shared_channel_count();
}

void shutdown() noexcept
{
    if(g_lua == nullptr) {
        return;
    }

    // Vtables first: the thunk they point at lives in this image, and a channel still routed
    // through it after the VM is gone would call into a dispatcher with no state behind it.
    tw::lua::api::tw_on_call_clear();

    lua_close(g_lua);
    g_lua = nullptr;
    g_traceback_ref = LUA_NOREF;
    g_dispatch_ref = LUA_NOREF;
    g_dispatch_call_ref = LUA_NOREF;
    g_call_dispatch_disabled = false;
    g_loaded_scripts = 0;
    g_frame_dispatch_disabled = false;
    g_unload_ref = LUA_NOREF;
    g_scripts.clear();
}

void draw_frame() noexcept
{
    if(g_lua == nullptr || g_dispatch_ref == LUA_NOREF || g_frame_dispatch_disabled) [[unlikely]] {
        return;
    }

    // ID scope first, then the recovery snapshot: recovery restores the ID stack to whatever depth
    // it was at when the snapshot was taken, so taking it *after* the push keeps our own PopID below
    // balanced on both the success and the failure path.
    ImGui::PushID("tw_lua");

    ImGuiErrorRecoveryState saved;
    ImGui::ErrorRecoveryStoreState(&saved);

    // A script's mistake is not a bug in the overlay, and the game is not a place to assert or to
    // pop a diagnostic tooltip over.
    ImGuiIO& io = ImGui::GetIO();
    const bool previous_assert = io.ConfigErrorRecoveryEnableAssert;
    const bool previous_tooltip = io.ConfigErrorRecoveryEnableTooltip;
    io.ConfigErrorRecoveryEnableAssert = false;
    io.ConfigErrorRecoveryEnableTooltip = false;

    const int handler = push_traceback_handler(g_lua);
    lua_rawgeti(g_lua, LUA_REGISTRYINDEX, g_dispatch_ref);

    const int status = lua_pcall(g_lua, 0, 0, handler);

    if(status != 0) {
        set_error("on_frame", lua_tostring(g_lua, -1));
        lua_pop(g_lua, 1);

        // One strike. Drawing runs every frame, so a script that throws would otherwise throw sixty
        // times a second - into the log, into the notefeed, and into whatever ImGui state recovery
        // has to undo each time.
        g_frame_dispatch_disabled = true;
        tw::ui::plugins::statics::notefeed::push("Lua script disabled: " + g_last_error);
    }

    if(handler != 0) {
        lua_pop(g_lua, 1);
    }

    io.ConfigErrorRecoveryEnableAssert = previous_assert;
    io.ConfigErrorRecoveryEnableTooltip = previous_tooltip;

    ImGui::ErrorRecoveryTryToRecoverState(&saved);
    ImGui::PopID();
}

bool dispatch_call(int subscription_id) noexcept
{
    if(g_lua == nullptr || g_dispatch_call_ref == LUA_NOREF || g_call_dispatch_disabled) [[unlikely]] {
        return true;
    }

    const int handler = push_traceback_handler(g_lua);
    lua_rawgeti(g_lua, LUA_REGISTRYINDEX, g_dispatch_call_ref);
    lua_pushinteger(g_lua, subscription_id);

    bool proceed = true;

    if(lua_pcall(g_lua, 1, 1, handler) != 0) {
        set_error("on_call", lua_tostring(g_lua, -1));
        lua_pop(g_lua, 1);

        // Same one-strike rule as on_frame, and for a stronger reason: this fires from inside the
        // game's own graph walk, several times a second during a run.
        g_call_dispatch_disabled = true;
        tw::ui::plugins::statics::notefeed::push("Lua channel hook disabled: " + g_last_error);
    }
    else {
        // Only a literal `false` cancels the engine's handler. A handler that returns nothing yields
        // nil here and proceeds - which is what keeps every observer written before suppression
        // existed behaving exactly as it did.
        proceed = !(lua_isboolean(g_lua, -1) && lua_toboolean(g_lua, -1) == 0);
        lua_pop(g_lua, 1);
    }

    if(handler != 0) {
        lua_pop(g_lua, 1);
    }

    return proceed;
}

bool is_running() noexcept
{
    return g_lua != nullptr;
}

int loaded_script_count() noexcept
{
    return g_loaded_scripts;
}

std::string_view last_error() noexcept
{
    return g_last_error;
}
} // namespace tw::lua::host
