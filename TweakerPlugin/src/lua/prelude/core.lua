-- ---------------------------------------------------------------------------------------------------
-- tw.core - the tw table itself: who is calling, what to say about it, the clock, the lifecycle.
-- ---------------------------------------------------------------------------------------------------

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

-- The version of this API, and the one number the whole toolchain agrees on: a built script carries
-- the version it was written against in its `-- @api` header line, and a plugin older than that
-- refuses to run it rather than let it fail on a nil (lua_registry.cxx); harness/lua writes it into
-- Docs/scripting/api.json, and twllink checks a project's `"api"` against that. Raise it when a name
-- is added - names are never taken away (Docs/scripting/limits.md, "Versions and stability") - and
-- lua_vm.cxx reads it from here, so this line is its only definition.
tw.api_version = 3

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
