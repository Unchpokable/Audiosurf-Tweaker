#include "pch.hxx"

#include "lua/lua_prelude.hxx"

namespace
{
// ---------------------------------------------------------------------------------------------------
// tw.abi - every C entry point, bound once, by name.
//
// This is where the whole FFI story of Docs/Internal/lua-scripting.md §2.2 actually happens: every
// call a script makes into the plugin goes through one of the ffi.cast'ed pointers below, so it
// compiles into the trace as a direct call instead of aborting it the way a lua_CFunction would. The
// host then deletes `ffi` from the environment (lua_vm: strip_sandbox) - the already-cast pointers
// keep working because they are values, not names, which is exactly the property §6 relies on to keep
// the fast path without handing scripts arbitrary memory access.
//
// **By name, not by position.** The table used to be positional - P[1]..P[57] - and an entry
// inserted in the middle of the C list silently shifted every function after it onto the wrong
// signature. Now both sides are checked against each other here, at startup, in both directions: a
// name C offers that has no signature below, or a signature below whose name C does not offer, stops
// the prelude with that name in the message. Order no longer means anything.
// ---------------------------------------------------------------------------------------------------
constexpr std::string_view k_abi = R"LUA(
local ffi = require("ffi")
local S = ...

local P = S.ptrs
S.ptrs = nil

-- Signatures must match the declarations in src/lua/api/*.hxx. extern "C" on x86 MSVC is __cdecl,
-- which is also the FFI's default, so no calling-convention decoration is needed.
local SIG = {
    -- api_core
    tw_script_enter       = "int (*)(int, int)",
    tw_script_leave       = "void (*)(int, int, const char*)",
    tw_diag               = "void (*)(int, int, const char*, const char*)",
    tw_log                = "void (*)(const char*)",
    tw_notify             = "void (*)(int, const char*)",
    tw_engine_ready       = "int (*)(void)",
    tw_can_write          = "int (*)(void)",
    tw_state              = "int (*)(void)",
    tw_state_name         = "const char* (*)(void)",
    tw_ready              = "int (*)(void)",
    tw_graph_revision     = "int (*)(void)",
    tw_group_count        = "int (*)(void)",
    tw_group_name         = "const char* (*)(int)",
    tw_group_loaded       = "int (*)(const char*)",
    tw_frame              = "int (*)(void)",
    tw_dt                 = "float (*)(void)",
    tw_ease               = "float (*)(int, float)",
    tw_ease_count         = "int (*)(void)",
    tw_ease_name          = "const char* (*)(int)",

    -- api_channels
    tw_channel_resolve    = "void* (*)(const char*, const char*, int, int*)",
    tw_channel_resolve_at = "void* (*)(const char*, int, int, int*)",
    tw_ref_free           = "void (*)(void*)",
    tw_kind_name          = "const char* (*)(int)",
    tw_channel_get        = "float (*)(void*)",
    tw_channel_set        = "void (*)(void*, float)",
    tw_channel_text       = "const char* (*)(void*)",
    tw_channel_vector     = "int (*)(void*, float*)",
    tw_channel_set_vector = "void (*)(void*, float, float, float)",
    tw_channel_matrix     = "int (*)(void*, float*)",
    tw_channel_set_matrix = "void (*)(void*, const float*)",
    tw_channel_live       = "int (*)(void*)",
    tw_array_read         = "float (*)(void*, void*, float)",
    tw_array_read_vector  = "int (*)(void*, void*, float, float*)",
    tw_array_write        = "int (*)(void*, void*, float, float)",
    tw_array_write_vector = "int (*)(void*, void*, float, float, float, float)",
    tw_array_rows         = "int (*)(void*)",

    -- api_hooks
    tw_on_call            = "int (*)(int, const char*, const char*, int, int*)",
    tw_on_call_at         = "int (*)(int, const char*, int, int, int*)",
    tw_mute               = "int (*)(int, const char*, const char*, int*)",
    tw_mute_at            = "int (*)(int, const char*, int, int*)",
    tw_mute_set           = "void (*)(int, int)",

    -- api_hud
    tw_theme_count        = "int (*)(void)",
    tw_theme_name         = "const char* (*)(int)",
    tw_theme_color        = "unsigned int (*)(int)",
    tw_font_count         = "int (*)(void)",
    tw_font_name          = "const char* (*)(int)",
    tw_hud_text           = "void (*)(float, float, unsigned int, const char*)",
    tw_hud_text_sized     = "void (*)(float, float, unsigned int, const char*, float)",
    tw_hud_text_font      = "void (*)(float, float, unsigned int, const char*, float, int)",
    tw_hud_text_glow      = "void (*)(float, float, unsigned int, unsigned int, const char*, float, int, float)",
    tw_hud_measure        = "void (*)(const char*, float, float*)",
    tw_hud_measure_font   = "void (*)(const char*, float, int, float*)",
    tw_hud_rect           = "void (*)(float, float, float, float, unsigned int, float, float)",
    tw_hud_rect_corners   = "void (*)(float, float, float, float, unsigned int, float, float, int)",
    tw_hud_rect_glow      = "void (*)(float, float, float, float, unsigned int, float, float)",
    tw_hud_rect_gradient  = "void (*)(float, float, float, float, unsigned int, unsigned int, int)",
    tw_hud_line           = "void (*)(float, float, float, float, unsigned int, float)",
    tw_hud_icon           = "void (*)(const char*, float, float, float, unsigned int)",
    tw_hud_metric         = "float (*)(int)",
    tw_hud_widget_rect    = "int (*)(int, float*)",
}

local C = {}
for name, sig in pairs(SIG) do
    local p = P[name]
    if p == nil then
        error("prelude: the host offers no entry point '" .. name .. "'", 0)
    end
    C[name] = ffi.cast(sig, p)
end

for name in pairs(P) do
    if SIG[name] == nil then
        error("prelude: entry point '" .. name .. "' has no signature in the prelude", 0)
    end
end

S.C = C
)LUA";

// ---------------------------------------------------------------------------------------------------
// tw.core - the tw table itself: who is calling, what to say about it, the clock, the lifecycle.
// ---------------------------------------------------------------------------------------------------
constexpr std::string_view k_core = R"LUA(
local ffi = require("ffi")
local S = ...
local C = S.C

local C_log, C_notify, C_diag = C.tw_log, C.tw_notify, C.tw_diag
local C_group_count, C_group_name, C_engine_ready = C.tw_group_count, C.tw_group_name, C.tw_engine_ready
local C_dt, C_ease, C_ease_count, C_ease_name = C.tw_dt, C.tw_ease, C.tw_ease_count, C.tw_ease_name
local C_can_write, C_state_name, C_ready = C.tw_can_write, C.tw_state_name, C.tw_ready

-- Captured here, before the host strips `debug` from the environment. The prelude keeps using both:
-- traceback for every guarded callback, getinfo for where a diagnostic was said from.
local traceback, getinfo = debug.traceback, debug.getinfo
S.traceback = traceback

local tw = {}
S.tw = tw

-- Which script is calling right now.
--
-- The host writes __tw_owner into each script's own environment table before running it, so this is
-- correct both for top-level code and for anything the script registers from a closure later. It has
-- to be an environment lookup rather than a "currently loading" global on the host side, because a
-- script's closures run long after its body has returned.
--
-- Level 3 is the script: 1 is this function, 2 is the tw.* entry point that called it, 3 is whoever
-- called that. So this must be called directly from a tw.* function, never through pcall or another
-- helper - each would occupy a level of its own and silently shift the count, which is how the first
-- version of this credited every subscription to nobody.
--
-- -1 is "not a script": the harness driving the prelude, or the prelude itself. Owner -1 is never
-- suspended, never budgeted and never held to the registration window - there is nobody to hold.
local function caller_owner()
    local env = getfenv(3)
    if type(env) == "table" and type(env.__tw_owner) == "number" then
        return env.__tw_owner
    end
    return -1
end
S.caller_owner = caller_owner

-- Where in the script the call came from - "hud.lua:88" - under the same level rule as above. It is
-- the key a diagnostic is deduplicated on, so a message said from two places is two records.
local function caller_place()
    local info = getinfo(3, "Sl")
    if info == nil or info.currentline == nil or info.currentline < 0 then return "" end
    return info.short_src .. ":" .. info.currentline
end
S.caller_place = caller_place

-- Diagnostic levels, same numbers as tw::lua::diag::level.
local D_PENDING, D_INFO, D_WARN, D_ERROR = 0, 1, 2, 3
S.D_PENDING, S.D_INFO, S.D_WARN, S.D_ERROR = D_PENDING, D_INFO, D_WARN, D_ERROR

-- The frame number, and it counts **engine** frames - one per evaluation of the game's channel
-- graph - rather than frames of the overlay.
--
-- The difference is not cosmetic. The overlay's counter advances in Present, which does not happen
-- while the window is minimised, does not happen before there is a device, and advances at whatever
-- rate the machine reaches: `tw.frame % 60` meant "once a second" on the machine it was written on
-- and "three times a second" on a 180 Hz one. The engine's counter advances exactly when the graph
-- is evaluated, which is what "per frame" means for anything that reads a channel.
--
-- Set from C at the top of every dispatcher rather than incremented here, so they all agree and so
-- the value keeps moving even if the engine spine never comes up - see tw_frame().
tw.frame = 0

-- Frames of the overlay, for the rare thing that really is about drawing rate. Incremented by the
-- draw dispatcher and by nothing else.
tw.draw_frame_count = 0

function tw.log(msg) C_log(tostring(msg)) end

-- A toast in the notefeed, for something the player should see. Capped per script: a handful a
-- minute, and the rest land in the script's diagnostics in the Scripts tab instead.
function tw.notify(msg) C_notify(caller_owner(), tostring(msg)) end

-- Diagnostics, for things the script's author needs to see - in a release build too, where tw.log
-- goes nowhere. All three are deduplicated: saying the same thing from the same place a thousand
-- times is one record with a count of a thousand, in the Scripts tab.
--
--   tw.warn    - something is off, the script carries on. Tab and log, never the notefeed.
--   tw.error   - something is wrong. Also the notefeed, once, and only once the game is up.
--                Reports; does not raise - use Lua's error() to stop.
--   tw.pending - "I am waiting for something", e.g. a run to start. Shows the script as waiting for
--                as long as it keeps saying it, so say it every frame you are waiting.
function tw.warn(msg)    C_diag(caller_owner(), D_WARN,    caller_place(), tostring(msg)) end
function tw.error(msg)   C_diag(caller_owner(), D_ERROR,   caller_place(), tostring(msg)) end
function tw.pending(msg) C_diag(caller_owner(), D_PENDING, caller_place(), tostring(msg)) end

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

-- Seconds since the previous frame, from the overlay's own frame clock. Use this rather than a
-- fixed per-frame step: the game does not run at a fixed frame rate, and a widget that fades by a
-- constant amount each frame fades at whatever speed the machine happens to run at.
function tw.dt() return C_dt() end

-- An easing curve evaluated at t (clamped to 0..1, mapped to 0..1). Pure - a script keeps its own
-- progress and asks this to shape it:
--
--     t = math.min(1, t + tw.dt() / 0.25)          -- 250 ms
--     local k = tw.ease("cubicOut", t)
--
-- There is deliberately no tween object. One would have to own state and be torn down with the
-- script; a curve you sample owns nothing and disappears on its own.
local EASE = {}
for i = 0, C_ease_count() - 1 do
    EASE[ffi.string(C_ease_name(i))] = i
end

function tw.ease(curve, t)
    local i = EASE[curve]
    if i == nil then error("unknown easing curve: " .. tostring(curve), 2) end
    return C_ease(i, t)
end

-- Every curve name tw.ease accepts, sorted.
function tw.ease_names()
    local out = {}
    for name in pairs(EASE) do out[#out + 1] = name end
    table.sort(out)
    return out
end

-- Whether writes to the graph are being accepted yet.
--
-- Writes are refused while the game is still assembling itself, because writing then does not crash
-- anything - it silently corrupts the game, and the symptom (a broken track generator, characters
-- shuffled in the menu) looks nothing like its cause. A refused write returns false like any other
-- failed write; this is here so a script can wait deliberately instead of wondering.
function tw.can_write() return C_can_write() ~= 0 end

-- The lifecycle, as scripts see it.
--
-- `tw.state()` is one of "detached", "booting", "starting", "ready", "busy"; `tw.ready()` is the
-- predicate that matters - the graph is up, and reading, writing and hooking all work. Every callback
-- is held behind it, which is the whole point of Ф2: a script no longer has to guess whether the game
-- has finished loading, and no longer runs while it has not.
function tw.state() return ffi.string(C_state_name()) end
function tw.ready() return C_ready() ~= 0 end
)LUA";

// ---------------------------------------------------------------------------------------------------
// tw.channels - handles onto the game's channels, one accessor per family, and Array Table columns.
// ---------------------------------------------------------------------------------------------------
constexpr std::string_view k_channels = R"LUA(
local ffi = require("ffi")
local S = ...
local C = S.C
local tw = S.tw
local caller_owner, caller_place = S.caller_owner, S.caller_place
local D_ERROR = S.D_ERROR

local C_resolve, C_resolve_at, C_ref_free = C.tw_channel_resolve, C.tw_channel_resolve_at, C.tw_ref_free
local C_kind_name, C_engine_ready, C_graph_revision = C.tw_kind_name, C.tw_engine_ready, C.tw_graph_revision
local C_get, C_set, C_text = C.tw_channel_get, C.tw_channel_set, C.tw_channel_text
local C_vector, C_set_vector = C.tw_channel_vector, C.tw_channel_set_vector
local C_matrix, C_set_matrix, C_live = C.tw_channel_matrix, C.tw_channel_set_matrix, C.tw_channel_live
local C_array_read, C_array_vector = C.tw_array_read, C.tw_array_read_vector
local C_array_write, C_array_write_vec, C_array_rows = C.tw_array_write, C.tw_array_write_vector, C.tw_array_rows
local C_diag = C.tw_diag

-- One reusable out-buffer for resolve results: [0] = status, [1] = the kind the channel actually is.
-- Allocated once here rather than per call, so a resolve costs no garbage.
local resolve_out = ffi.new("int[2]")

-- Same idea for the out-buffers reads fill. These are read every frame, so allocating one per call
-- would put the HUD path on the collector's critical list.
local vec_out = ffi.new("float[3]")
local mat_out = ffi.new("float[16]")
local mat_in = ffi.new("float[16]")

local KIND_NUMBER, KIND_TEXT, KIND_VECTOR, KIND_MATRIX = 0, 1, 2, 3

local STATUS_OK = 0
local STATUS_NO_CHANNEL, STATUS_WRONG_KIND, STATUS_UNUSABLE = 3, 4, 5

-- Resolution is lazy, because it has to be: a handle is created at load time, when the group it
-- names may not exist and in some cases (a run's groups, asked for from the menu) does not exist
-- yet by design.
--
-- **Retrying is driven by the graph, not by a timer.** It used to be "every 60 dispatches, forever",
-- which cost a linear _stricmp scan over a group of several thousand channels each time and never
-- stopped, not even for a name that could never resolve. `C_graph_revision()` changes exactly when a
-- group appeared or went away - the only two events that can change the answer - so an unresolved
-- handle now costs one integer comparison per frame and one scan per actual change.
--
-- **The same number also expires a resolved handle**, and that is not an optimisation. Groups are
-- destroyed during play: the `Renderer` pool is dropped and rebuilt on every single run. A cached
-- channel pointer into a destroyed group is not stale data, it is freed memory, so when the graph
-- has moved the handle is re-resolved before it is used again.
--
-- The failure taxonomy matters more than it looks:
--   - engine not up, or group not loaded -> transient, silent, waits for the graph to change;
--   - group loaded but no such channel -> **final**, because a group's channel list is fixed once it
--     is loaded. Said once, as an error of the script that made the handle, and the handle stops
--     trying;
--   - asked through the wrong accessor -> a bug in the script, and it can never fix itself, so it
--     is raised as a Lua error rather than silently yielding nil forever.
local Channel = {}
Channel.__index = Channel

function Channel:resolve()
    local rev = C_graph_revision()

    if self.h ~= nil then
        if self.rev == rev then return true end
        -- The set of loaded groups changed. Whatever this handle points at may have been destroyed
        -- with its group, so it goes back through the resolve rather than being trusted.
        self.h = nil
    end

    if self.dead then return false end

    -- Before the revision check, not after, and that order is the whole correctness of it: "the
    -- engine pointer is not captured yet" is not an answer about this handle, so it must not consume
    -- the one attempt this revision allows. Spending it here would leave the handle waiting for the
    -- next group to load before it ever tried - which, in a session already past loading, could be
    -- the rest of the run.
    if C_engine_ready() == 0 then return false end

    if self.tried == rev then return false end
    self.tried = rev

    local h
    if type(self.name) == "number" then
        h = C_resolve_at(self.group, self.name, self.kind, resolve_out)
    else
        h = C_resolve(self.group, self.name, self.kind, resolve_out)
    end
    local status = resolve_out[0]

    if status == STATUS_OK then
        -- The handle is a channel_ref the host allocated for us; its lifetime is this cdata's. When
        -- the graph moves and the handle re-resolves, the old one simply becomes garbage and the
        -- finalizer hands it back - nothing on this side has to remember to free anything.
        self.h = ffi.gc(h, C_ref_free)
        self.rev = rev
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

    -- The group is there and the channel is not. Nothing about that changes with time, so it is said
    -- once - with the name the script used, filed against the place the handle was made - and the
    -- handle retires instead of rescanning the group for the rest of the session.
    if status == STATUS_NO_CHANNEL then
        self.dead = true
        C_diag(self.owner, D_ERROR, self.place, string.format("%s.%s: no such channel in group", self.group, tostring(self.name)))
    end

    return false
end

-- Whether the engine evaluated this channel in the current frame: true, false, or nil when that
-- cannot be known (the channel never memoises, so the field that would say is never written).
--
-- "Current frame" is literal, and it is what makes this useful rather than confusing: read from
-- on_post_tick it answers "did the graph run this branch just now", which is how you tell a part of
-- the game that is active from one that is merely loaded. Read from on_tick, which runs before the
-- graph, it is false for everything - the frame has not been evaluated yet.
function Channel:live()
    if not self:resolve() then return nil end
    local v = C_live(self.h)
    if v < 0 then return nil end
    return v ~= 0
end

-- Whether this handle has given up. False for one that is merely waiting - the distinction a script
-- needs to tell "the run has not started" from "I typed the name wrong".
function Channel:dead_end() return self.dead == true end

function Channel:valid() return self.h ~= nil end

-- nil rather than a default when unavailable: a script showing "--" until the graph is reachable is
-- correct, one showing 0.0 or "" is lying.
local FloatChannel = setmetatable({}, { __index = Channel })
FloatChannel.__index = FloatChannel

function FloatChannel:get()
    if not self:resolve() then return nil end
    return C_get(self.h)
end

function FloatChannel:set(value)
    if not self:resolve() then return false end
    C_set(self.h, value)
    return true
end

local TextChannel = setmetatable({}, { __index = Channel })
TextChannel.__index = TextChannel

function TextChannel:get()
    if not self:resolve() then return nil end
    return ffi.string(C_text(self.h))
end

-- Vector channels return three numbers rather than a table: a table per read would be garbage on the
-- HUD path, and every caller so far wants the components immediately anyway.
local VectorChannel = setmetatable({}, { __index = Channel })
VectorChannel.__index = VectorChannel

function VectorChannel:get()
    if not self:resolve() then return nil end
    if C_vector(self.h, vec_out) == 0 then return nil end
    return vec_out[0], vec_out[1], vec_out[2]
end

-- Writing a vector channel is not the local store that float_ch:set is. The engine sets the three
-- components and then writes each one through into the numeric channel wired to that component, if
-- one is wired - so this can reach further into the graph than it looks. See
-- Docs/scripting/channels.md.
function VectorChannel:set(x, y, z)
    if not self:resolve() then return false end
    C_set_vector(self.h, x, y, z)
    return true
end

-- Matrix channels: sixteen numbers, row-major (_11 _12 _13 _14 _21 ... _44), as multiple returns for
-- the same reason vectors are - a table per read is garbage on the drawing path. For writing, a table
-- of sixteen is accepted as well as sixteen arguments, because nobody wants to type the second form.
local MatrixChannel = setmetatable({}, { __index = Channel })
MatrixChannel.__index = MatrixChannel

function MatrixChannel:get()
    if not self:resolve() then return nil end
    if C_matrix(self.h, mat_out) == 0 then return nil end
    return mat_out[0], mat_out[1], mat_out[2], mat_out[3],
           mat_out[4], mat_out[5], mat_out[6], mat_out[7],
           mat_out[8], mat_out[9], mat_out[10], mat_out[11],
           mat_out[12], mat_out[13], mat_out[14], mat_out[15]
end

function MatrixChannel:set(first, ...)
    if not self:resolve() then return false end
    if type(first) == "table" then
        for i = 1, 16 do mat_in[i - 1] = first[i] or 0 end
    else
        mat_in[0] = first or 0
        local rest = { ... }
        for i = 1, 15 do mat_in[i] = rest[i] or 0 end
    end
    C_set_matrix(self.h, mat_in)
    return true
end

-- `tried` and `rev` are graph revisions, not frame numbers: -1 is "never", and C_graph_revision()
-- starts at 0, so a handle tries once before anything has happened and then waits for a change.
--
-- `owner` and `place` are who made the handle and where, taken by the public constructor: a typo in
-- a channel name is reported against the line that has the typo, not against wherever the handle
-- happened to be read first.
local function new_handle(mt, kind, group, name, owner, place)
    return setmetatable({ h = nil, group = group, name = name, kind = kind, tried = -1, rev = -1,
                          owner = owner, place = place }, mt)
end

-- One accessor per channel family. Deliberately not a single generic tw.channel(): the engine's
-- vtable slots mean different things per family, so the accessor is what carries the type, and
-- asking through the wrong one is a mistake worth reporting rather than papering over.
--
-- `name` may be a number, meaning "the channel at this index in the group". Channel names are NOT
-- unique - TrafficCommander has two Values called "TrafficType", and a by-name lookup finds the
-- wrong one - so sometimes the index is the only way to be precise. It is also more brittle across
-- game versions, so prefer the name where it is unambiguous.
function tw.float_ch(group, name)  return new_handle(FloatChannel,  KIND_NUMBER, group, name, caller_owner(), caller_place()) end
function tw.string_ch(group, name) return new_handle(TextChannel,   KIND_TEXT,   group, name, caller_owner(), caller_place()) end
function tw.vector_ch(group, name) return new_handle(VectorChannel, KIND_VECTOR, group, name, caller_owner(), caller_place()) end
function tw.matrix_ch(group, name) return new_handle(MatrixChannel, KIND_MATRIX, group, name, caller_owner(), caller_place()) end

-- Kept as an alias so scripts written against the first cut keep working. New code should say what
-- it means.
tw.channel = tw.float_ch

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

-- Writing a row is NOT symmetrical with reading one, and the asymmetry is deliberate: an index the
-- table does not have is refused here, because the engine's own write path would create that row
-- instead of rejecting it - lengthening a table the rest of the game reads. So `false` from this
-- means either "could not resolve" or "no such row", and both are worth checking.
function Array:set(index, value)
    if not self.column:resolve() or not self.cursor:resolve() then return false end
    return C_array_write(self.column.h, self.cursor.h, index, value) ~= 0
end

-- Row count of the underlying table, or nil when it cannot be asked. Valid indices are 0..rows()-1.
function Array:rows()
    if not self.column:resolve() then return nil end
    local n = C_array_rows(self.column.h)
    if n < 0 then return nil end
    return n
end

function tw.array(group, column, cursor)
    local owner, place = caller_owner(), caller_place()
    return setmetatable({
        column = new_handle(FloatChannel, KIND_NUMBER, group, column, owner, place),
        cursor = new_handle(FloatChannel, KIND_NUMBER, group, cursor, owner, place),
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

-- Same contract as Array:set. Note this goes through the array type's own SetVector, which writes
-- the row - not the component-propagating store a plain vector_ch:set performs.
function VectorArray:set(index, x, y, z)
    if not self.column:resolve() or not self.cursor:resolve() then return false end
    return C_array_write_vec(self.column.h, self.cursor.h, index, x, y, z) ~= 0
end

function VectorArray:rows()
    if not self.column:resolve() then return nil end
    local n = C_array_rows(self.column.h)
    if n < 0 then return nil end
    return n
end

function tw.array_vec(group, column, cursor)
    local owner, place = caller_owner(), caller_place()
    return setmetatable({
        column = new_handle(VectorChannel, KIND_VECTOR, group, column, owner, place),
        cursor = new_handle(FloatChannel,  KIND_NUMBER, group, cursor, owner, place),
    }, VectorArray)
end
)LUA";

// ---------------------------------------------------------------------------------------------------
// tw.hud - drawing, the overlay's palette and colour helpers.
// ---------------------------------------------------------------------------------------------------
constexpr std::string_view k_hud = R"LUA(
local ffi = require("ffi")
local S = ...
local C = S.C
local tw = S.tw

local C_hud_text, C_hud_text_sized, C_hud_text_font = C.tw_hud_text, C.tw_hud_text_sized, C.tw_hud_text_font
local C_hud_text_glow, C_hud_measure, C_hud_measure_fnt = C.tw_hud_text_glow, C.tw_hud_measure, C.tw_hud_measure_font
local C_hud_rect, C_hud_rect_corner, C_hud_rect_glow = C.tw_hud_rect, C.tw_hud_rect_corners, C.tw_hud_rect_glow
local C_hud_rect_grad, C_hud_line, C_hud_icon = C.tw_hud_rect_gradient, C.tw_hud_line, C.tw_hud_icon
local C_hud_metric, C_hud_widget_rect = C.tw_hud_metric, C.tw_hud_widget_rect
local C_font_count, C_font_name = C.tw_font_count, C.tw_font_name
local C_theme_count, C_theme_name, C_theme_color = C.tw_theme_count, C.tw_theme_name, C.tw_theme_color

local rect_out = ffi.new("float[4]")
local size_out = ffi.new("float[2]")

tw.hud = {}

-- The baked font faces, name -> index, built once here so the per-frame path is an array index
-- rather than a string compare. Same shape as the theme table below, for the same reason.
--
-- Faces are weights, not sizes: ImGui rasterizes at whatever size it is drawn, so `size` and `font`
-- are independent. nil means the default face, which is what every existing script gets.
local FONT = {}
for i = 0, C_font_count() - 1 do
    FONT[ffi.string(C_font_name(i))] = i
end

local function font_index(name)
    if name == nil then return 0 end
    local i = FONT[name]
    if i == nil then error("unknown font: " .. tostring(name), 3) end
    return i
end

-- Every face name tw.hud.text accepts.
function tw.hud.fonts()
    local out = {}
    for name in pairs(FONT) do out[#out + 1] = name end
    table.sort(out)
    return out
end

-- Colour is ImGui's packed IM_COL32 (0xAABBGGRR). Default is opaque white.
--
-- `size` is optional and in pixels; omitted means the overlay's own text size. `font` is optional
-- and names a face from tw.hud.fonts(); omitted means the default one.
function tw.hud.text(x, y, text, color, size, font)
    if font then
        C_hud_text_font(x, y, color or 0xFFFFFFFF, tostring(text), size or 0, font_index(font))
    elseif size then
        C_hud_text_sized(x, y, color or 0xFFFFFFFF, tostring(text), size)
    else
        C_hud_text(x, y, color or 0xFFFFFFFF, tostring(text))
    end
end

-- Width and height the same text would occupy. The measurement comes from ImGui, so it matches what
-- tw.hud.text actually draws - which is what makes centering and right-alignment exact rather than
-- approximate.
--
-- Pass the same `font` you will draw with. Measuring one face and drawing another is off by enough
-- to be visible in anything right-aligned, and nothing can catch that for you.
function tw.hud.measure(text, size, font)
    if font then
        C_hud_measure_fnt(tostring(text), size or 0, font_index(font), size_out)
    else
        C_hud_measure(tostring(text), size or 0, size_out)
    end
    return size_out[0], size_out[1]
end

-- Text with a soft glow behind it, in the overlay's own glow style. Draws the text as well - the
-- glow is offset copies of the same glyphs, so splitting it in two would rasterize them twice.
--
-- `glow` defaults to the text colour, which is the common case: a colour that glows in its own hue.
function tw.hud.glow_text(x, y, text, color, glow, size, font, strength)
    color = color or 0xFFFFFFFF
    C_hud_text_glow(x, y, color, glow or color, tostring(text), size or 0, font_index(font), strength or 1)
end

-- The overlay's default text height. Layouts should scale off this instead of assuming a pixel size.
function tw.hud.font_size()
    return C_hud_metric(6)
end

-- Which corners tw.hud.rect rounds. A plain bitmask, so `0` honestly means "none" - unlike ImGui's
-- own flags, where zero means "all" and "none" is a set bit.
tw.hud.corners = {
    none = 0,
    top_left = 1, top_right = 2, bottom_left = 4, bottom_right = 8,
    top = 3, bottom = 12, left = 5, right = 10,
    all = 15,
}

-- A rectangle. `rounding` is the corner radius; `thickness` <= 0 (the default) fills it, anything
-- else strokes an outline. `corners` selects which corners the radius applies to and defaults to
-- all of them - it is what a bar built out of several abutting rectangles needs, so the outer ends
-- round and the internal joins stay square.
function tw.hud.rect(x0, y0, x1, y1, color, rounding, thickness, corners)
    if corners then
        C_hud_rect_corner(x0, y0, x1, y1, color or 0xFFFFFFFF, rounding or 0, thickness or 0, corners)
    else
        C_hud_rect(x0, y0, x1, y1, color or 0xFFFFFFFF, rounding or 0, thickness or 0)
    end
end

-- A soft glow around a rounded rect, in the overlay's own style. Draws only the glow - fill first,
-- then glow, which is also what lets a shape glow in a different colour than it is filled with.
function tw.hud.glow_rect(x0, y0, x1, y1, color, rounding, strength)
    C_hud_rect_glow(x0, y0, x1, y1, color or 0xFFFFFFFF, rounding or 0, strength or 1)
end

-- A rectangle filled with a two-stop linear gradient: `from` at the left edge and `to` at the right,
-- or top and bottom when `vertical` is true. Both colours carry their own alpha, so a backdrop that
-- fades out to nothing is the same call as one colour fading into another.
--
-- No rounding: the primitive underneath is one quad with per-corner colours and has no rounded form.
-- A gradient that wants a soft end gets it from the gradient.
function tw.hud.gradient_rect(x0, y0, x1, y1, from, to, vertical)
    C_hud_rect_grad(x0, y0, x1, y1, from or 0xFFFFFFFF, to or 0, vertical and 1 or 0)
end

function tw.hud.line(x0, y0, x1, y1, color, thickness)
    C_hud_line(x0, y0, x1, y1, color or 0xFFFFFFFF, thickness or 1)
end

-- One of the plugin's packed SVG icons, in a square box of `size` pixels at (x, y).
--
-- `name` is a bare stem ("feat_stealth"); the icons/ prefix and .svg extension are attached on the
-- other side of the boundary, so this reaches the icon set and nothing else. Icons are monochrome
-- and take their colour from `colour`. An unknown name draws nothing rather than raising - an icon
-- is decoration, and a script should not die because a build dropped one.
function tw.hud.icon(name, x, y, size, colour)
    C_hud_icon(tostring(name), x, y, size, colour or 0xFFFFFFFF)
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
)LUA";

// ---------------------------------------------------------------------------------------------------
// tw.callbacks - registering callbacks, and the dispatchers that run them, one script at a time.
// ---------------------------------------------------------------------------------------------------
constexpr std::string_view k_callbacks = R"LUA(
local S = ...
local C = S.C
local tw = S.tw
local caller_owner, traceback = S.caller_owner, S.traceback
local D_ERROR = S.D_ERROR

local C_enter, C_leave, C_diag = C.tw_script_enter, C.tw_script_leave, C.tw_diag
local C_frame, C_group_loaded = C.tw_frame, C.tw_group_loaded
local C_on_call, C_on_call_at = C.tw_on_call, C.tw_on_call_at
local C_mute, C_mute_at, C_mute_set = C.tw_mute, C.tw_mute_at, C.tw_mute_set

local resolve_out = require("ffi").new("int[2]")

-- Callback kinds, same numbers as tw::lua::callback.
local K_FRAME, K_TICK, K_POST_TICK, K_CALL, K_READY, K_STATE, K_GROUP, K_UNLOAD = 0, 1, 2, 3, 4, 5, 6, 7

-- **The unit of failure is the script.** Every callback any script registered runs through this, one
-- at a time, each under its own xpcall - so one that throws loses its own call and nothing else: the
-- next handler in the loop, which is usually somebody else's, runs as if nothing happened.
--
-- The two C calls around it are the rest of the story (lua_sched.cxx). `enter` says whether this
-- script may run at all - no, once it has been suspended - and starts its clock; `leave` stops the
-- clock and, when there was an error, files it against this script's diagnostics and its own error
-- budget. Running out of either budget is what suspends a script; nothing a script does can suspend
-- another one.
--
-- Returns whether it ran, and the handler's first result. A script that did not run is how a
-- caller tells "suspended" from "ran and returned nothing".
local function guarded(owner, kind, fn, arg)
    if C_enter(owner, kind) == 0 then return false end
    local ok, result = xpcall(fn, traceback, arg)
    if ok then
        C_leave(owner, kind, nil)
        return true, result
    end
    C_leave(owner, kind, tostring(result))
    return true, nil
end

-- **Registration happens while a script loads, or from its on_ready, and nowhere else.** Order of
-- registration is order of calling, and registering from a callback that runs every frame is a list
-- that grows every frame. So a registration from anywhere else is refused, and said once - against
-- the API that was misused, which dedups it however often it happens.
--
-- Only scripts are held to this. Owner -1 is not a script (see caller_owner in tw.core).
local loading = false  -- a script's body is running right now (exports.run_script)
local in_ready = false -- on_ready handlers are running right now

local function window_open(owner, what)
    if owner < 0 or loading or in_ready then return true end
    C_diag(owner, D_ERROR, what, what .. " called from a callback - register while the script loads or in tw.on_ready; ignored")
    return false
end

-- The same, for the registrations that take a handler. A handler that is not a function is a mistake
-- made right here, so it is raised here - level 3 is the script's line - rather than surfacing later
-- as an error inside a dispatcher, every frame, against a callback that looks innocent.
local function may_register(owner, what, fn)
    if type(fn) ~= "function" then
        error(what .. " expects a function", 3)
    end
    return window_open(owner, what)
end

local handlers = {}
local before_ready_count = 0

-- Drawing. Runs every frame of the overlay, inside its ImGui frame, and only once the game is up -
-- unless `opts.before_ready` is set, which is for the rare script that really does want to put
-- something over the loading screen.
function tw.on_frame(fn, opts)
    local owner = caller_owner()
    if not may_register(owner, "tw.on_frame", fn) then return end
    local early = type(opts) == "table" and opts.before_ready == true
    handlers[#handlers + 1] = { fn = fn, owner = owner, before_ready = early }
    if early then before_ready_count = before_ready_count + 1 end
end

-- Per-engine-frame work, which is a different thing from per-overlay-frame drawing.
--
-- on_tick runs immediately before the game evaluates its channel graph, on the engine's own thread,
-- inside its call stack; on_post_tick runs immediately after, which is where this frame's results
-- are readable. Neither may draw: there is no ImGui frame open around them, and the tw.hud.* calls
-- refuse outside one rather than corrupting anything.
--
-- The rule of thumb for authors: compute in on_tick, draw in on_frame. A script that reads channels
-- from on_frame is reading them at the overlay's rate, which is not the rate the values change at.
local ticks = {}
function tw.on_tick(fn)
    local owner = caller_owner()
    if not may_register(owner, "tw.on_tick", fn) then return end
    ticks[#ticks + 1] = { fn = fn, owner = owner }
end

local post_ticks = {}
function tw.on_post_tick(fn)
    local owner = caller_owner()
    if not may_register(owner, "tw.on_post_tick", fn) then return end
    post_ticks[#post_ticks + 1] = { fn = fn, owner = owner }
end

-- Runs once, on the first frame where everything is allowed.
--
-- **Also runs for a script enabled in the middle of a session**, on its next frame, because the
-- contract is "once, when it can" and not "once, at startup". That is what removes the pattern
-- bundled scripts had to invent for themselves - a `tw.frame % 60 == 0` poll whose only job was to
-- notice that the script had been switched on mid-run.
--
-- The one callback other than the script's body that may register more callbacks.
local ready_waiting = {}
function tw.on_ready(fn)
    local owner = caller_owner()
    if not may_register(owner, "tw.on_ready", fn) then return end
    ready_waiting[#ready_waiting + 1] = { fn = fn, owner = owner }
end

-- Every transition, with the new state as a string. For a script that wants to put something on
-- screen while the game is still coming up, or to reset itself when a run starts loading.
local state_handlers = {}
function tw.on_state(fn)
    local owner = caller_owner()
    if not may_register(owner, "tw.on_state", fn) then return end
    state_handlers[#state_handlers + 1] = { fn = fn, owner = owner }
end

-- `fn(loaded)` whenever that group appears or disappears - by pool name ("Renderer") or bare file
-- name ("Puzzle"), the same two spellings every other call here accepts.
--
-- This is the event a script that lives inside a run actually wants: the `Renderer` pool is
-- destroyed and rebuilt every single time, and until now nothing said so.
local group_handlers = {}
function tw.on_group(name, fn)
    local owner = caller_owner()
    if not may_register(owner, "tw.on_group", fn) then return end
    group_handlers[#group_handlers + 1] = { name = name, fn = fn, owner = owner, loaded = C_group_loaded(name) ~= 0 }
end

-- The script is being switched off or reloaded: put back whatever it changed. Runs before its hooks
-- are taken out, so a script can still reach the channels it held; writes follow the usual rule and
-- are refused while the game is not up. Not run when the game itself exits - there is nothing left to
-- restore by then.
local unloads = {}
function tw.on_unload(fn)
    local owner = caller_owner()
    if not may_register(owner, "tw.on_unload", fn) then return end
    unloads[#unloads + 1] = { fn = fn, owner = owner }
end

-- Channel-call subscriptions. The engine calls back with a numeric id, which is looked up here -
-- C never holds a Lua value, so there is nothing on that side for the collector to trip over.
--
-- **There is no pending queue on this side.** A subscription is accepted immediately, id and all,
-- and the host attaches it to a real channel whenever the group turns up - and re-attaches it when a
-- group that had gone away comes back, which is what every run does to the `Renderer` pool. The id
-- never changes, so this table needs no maintenance across any of that.
local call_handlers = {}

-- `when` is "after" (default) or "before". After is what an event consumer wants: the Do_* handler
-- has run, so whatever it wrote is readable.
--
-- A "before" handler may return false to stop the engine's own handler from running at all. Any
-- other return - including none, and including an error - lets it run, so an observer never
-- suppresses anything by accident.
function tw.on_call(group, name, when, fn)
    if fn == nil then
        fn, when = when, "after"
    end

    local owner = caller_owner()
    if not may_register(owner, "tw.on_call", fn) then return end

    local after = (when ~= "before") and 1 or 0
    local id
    if type(name) == "number" then
        id = C_on_call_at(owner, group, name, after, resolve_out)
    else
        id = C_on_call(owner, group, name, after, resolve_out)
    end

    -- id >= 0 covers "attached" and "waiting for the group" alike; -1 is the final answer, and the
    -- host has already said what was wrong with the name.
    if id >= 0 then
        call_handlers[id] = { fn = fn, owner = owner }
    end
end

-- Takes a channel out of the graph entirely: the engine keeps calling it, and it keeps doing
-- nothing. The suppression lives in C, so a muted node costs one predicted branch per call rather
-- than a trip into the VM - which matters, because the things worth muting are render nodes the game
-- calls every frame.
--
-- Returns a handle with :on(), :off() and :set(bool). It starts on. A mute belongs to its script: if
-- the script is suspended, the game gets the node back until it is resumed.
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

function tw.mute(group, name)
    local owner = caller_owner()
    local m = setmetatable({ group = group, name = name, want = true, owner = owner }, Mute)

    -- Refused registrations still hand back a handle - an inert one - so code that goes on to call
    -- :on() does not fail a second time over the first mistake.
    if not window_open(owner, "tw.mute") then return m end

    local id
    if type(name) == "number" then
        id = C_mute_at(owner, group, name, resolve_out)
    else
        id = C_mute(owner, group, name, resolve_out)
    end

    if id >= 0 then
        m.id = id
        C_mute_set(id, 1)
    end

    return m
end

_G.tw = tw
_G.print = tw.log

-- What C calls. Kept here, in the prelude's private table, and not as globals: a script that could
-- call the unload dispatcher could unload somebody else.
local exports = {}
S.exports = exports

-- One entry point per dispatch site, so the C side never has to walk a Lua table on the hot path.
--
-- **The host calls none of the per-frame ones before the state machine says `ready`** - with one
-- exception, on_frame handlers registered with before_ready, which is why the draw dispatcher takes
-- a flag instead of simply not being called. The lifecycle dispatchers run in every state, because
-- their entire job is to say which state it is.
function exports.dispatch_frame(loading_screen)
    if loading_screen and before_ready_count == 0 then return end
    tw.frame = C_frame()
    tw.draw_frame_count = tw.draw_frame_count + 1
    for i = 1, #handlers do
        local h = handlers[i]
        if not loading_screen or h.before_ready then
            guarded(h.owner, K_FRAME, h.fn)
        end
    end
end

function exports.dispatch_tick()
    tw.frame = C_frame()

    -- Drained here rather than at registration: everything a script is handed runs inside a guarded
    -- dispatcher, so a script enabled mid-session gets its on_ready on the next frame instead of
    -- inside its own body, where an error would look like a load failure.
    --
    -- A handler whose script is suspended did not run, so it is kept for when the script resumes:
    -- "once, when it can" includes "not while it cannot".
    if #ready_waiting > 0 then
        local waiting = ready_waiting
        ready_waiting = {}
        in_ready = true
        for i = 1, #waiting do
            local r = waiting[i]
            if not guarded(r.owner, K_READY, r.fn) then
                ready_waiting[#ready_waiting + 1] = r
            end
        end
        in_ready = false
    end

    for i = 1, #ticks do
        local t = ticks[i]
        guarded(t.owner, K_TICK, t.fn)
    end
end

function exports.dispatch_post_tick()
    for i = 1, #post_ticks do
        local t = post_ticks[i]
        guarded(t.owner, K_POST_TICK, t.fn)
    end
end

-- Called on a transition, in any state.
function exports.dispatch_state(name)
    for i = 1, #state_handlers do
        local h = state_handlers[i]
        guarded(h.owner, K_STATE, h.fn, name)
    end
end

-- Called when the set of loaded groups changed, in any state. One roster lookup per registered
-- watcher, on a path taken twice a run - not per frame.
function exports.dispatch_graph()
    for i = 1, #group_handlers do
        local g = group_handlers[i]
        local now = C_group_loaded(g.name) ~= 0
        if now ~= g.loaded then
            g.loaded = now
            guarded(g.owner, K_GROUP, g.fn, now)
        end
    end
end

-- The return value travels back to framework/channel_shim: false from a "before" handler cancels the
-- engine's call. Falling off the end, an error and a suspended script all return nil, which proceeds.
function exports.dispatch_call(id)
    local rec = call_handlers[id]
    if rec == nil then return nil end
    local _, result = guarded(rec.owner, K_CALL, rec.fn)
    return result
end

-- Runs one script's body, with the registration window open. `chunk` already carries the script's
-- environment. Returns nil, or the error with its traceback.
function exports.run_script(chunk)
    loading = true
    local ok, err = xpcall(chunk, traceback)
    loading = false
    if ok then return nil end
    return tostring(err)
end

local function drop(list, id)
    for i = #list, 1, -1 do
        if list[i].owner == id then table.remove(list, i) end
    end
end

-- Forgets everything one script registered: its frame handlers, its lifecycle handlers and its
-- channel callbacks - after giving its on_unload handlers their one chance.
--
-- The C side does the other half - it takes the script's subscriptions out and puts the hooked
-- channels' original vtables back - and this half makes sure nothing is left pointing at a callback
-- from a script that is no longer running. Between them, a disabled script costs the game nothing:
-- no vtable copy, no dispatch, no VM entry. Its environment table becomes unreachable and the
-- collector takes the rest.
function exports.unload_owner(id)
    for i = 1, #unloads do
        local u = unloads[i]
        if u.owner == id then guarded(id, K_UNLOAD, u.fn) end
    end

    drop(handlers, id)
    drop(ticks, id)
    drop(post_ticks, id)
    drop(ready_waiting, id)
    drop(state_handlers, id)
    drop(group_handlers, id)
    drop(unloads, id)
    for sid, rec in pairs(call_handlers) do
        if rec.owner == id then call_handlers[sid] = nil end
    end

    before_ready_count = 0
    for i = 1, #handlers do
        if handlers[i].before_ready then before_ready_count = before_ready_count + 1 end
    end
end
)LUA";

const tw::lua::prelude::section k_sections[] = {
    { "=tw.abi", k_abi },
    { "=tw.core", k_core },
    { "=tw.channels", k_channels },
    { "=tw.hud", k_hud },
    { "=tw.callbacks", k_callbacks },
};
} // namespace

namespace tw::lua::prelude
{
std::span<const section> sections() noexcept
{
    return k_sections;
}
} // namespace tw::lua::prelude
