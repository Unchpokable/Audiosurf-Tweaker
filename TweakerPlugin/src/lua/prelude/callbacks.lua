-- ---------------------------------------------------------------------------------------------------
-- tw.callbacks - registering callbacks, and the dispatchers that run them, one script at a time.
-- ---------------------------------------------------------------------------------------------------

local S = ...
local C = S.C
local tw = S.tw
local caller_owner, traceback = S.caller_owner, S.traceback
local D_ERROR = S.D_ERROR

local C_enter, C_leave, C_diag, C_order = C.tw_script_enter, C.tw_script_leave, C.tw_diag, C.tw_script_order
local C_frame, C_group_loaded = C.tw_frame, C.tw_group_loaded
local C_on_call, C_on_call_at = C.tw_on_call, C.tw_on_call_at
local C_mute, C_mute_at, C_mute_set = C.tw_mute, C.tw_mute_at, C.tw_mute_set

local resolve_out = require("ffi").new("int[2]")

-- Callback kinds, same numbers as tw::lua::callback.
local K_FRAME, K_TICK, K_POST_TICK, K_CALL, K_READY, K_STATE, K_GROUP, K_UNLOAD = 0, 1, 2, 3, 4, 5, 6, 7
S.K_UI = 8 -- tw.ui's on_change / on_click, run by the tw.ui section

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
-- caller tells "suspended" from "ran and returned nothing". Up to two arguments go to the handler.
local function guarded(owner, kind, fn, arg, arg2)
    if C_enter(owner, kind) == 0 then return false end
    local ok, result = xpcall(fn, traceback, arg, arg2)
    if ok then
        C_leave(owner, kind, nil)
        return true, result
    end
    C_leave(owner, kind, tostring(result))
    return true, nil
end

-- **Registration happens while a script loads, or from its on_ready, and nowhere else.** Order of
-- registration is order of calling within a script, and registering from a callback that runs every frame is a list
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

-- For the sections after this one (tw.ui): the same guard and the same window, not copies of them.
S.guarded, S.window_open, S.may_register = guarded, window_open, may_register

-- **Every list below runs in the user's order of scripts** (script-coexistence.md §3), and a
-- script's own handlers in the order it registered them. Not in the order of registration across
-- scripts: a script registers again every time it is reloaded or switched back on, and appending would
-- move it to the end of everything - the order would last only until the first Reload.
--
-- So each record carries where its script runs (`place`, asked of C once, here) and a registration
-- number (`seq`), and goes in at its place rather than at the end. The order changes only when the user
-- reorders, and exports.reorder re-sorts everything then - nothing on the per-frame path looks at
-- either field.
local seq = 0

local function runs_before(a, b)
    if a.place ~= b.place then return a.place < b.place end
    return a.seq < b.seq
end

-- After every record that runs before it. From the back, because the usual record is the newest of the
-- last script to load and belongs at the end.
local function place_in(list, rec)
    local i = #list
    while i > 0 and runs_before(rec, list[i]) do i = i - 1 end
    table.insert(list, i + 1, rec)
end

local function enlist(list, rec)
    seq = seq + 1
    rec.seq = seq
    rec.place = C_order(rec.owner)
    place_in(list, rec)
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
    enlist(handlers, { fn = fn, owner = owner, before_ready = early })
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
    enlist(ticks, { fn = fn, owner = owner })
end

local post_ticks = {}
function tw.on_post_tick(fn)
    local owner = caller_owner()
    if not may_register(owner, "tw.on_post_tick", fn) then return end
    enlist(post_ticks, { fn = fn, owner = owner })
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
    enlist(ready_waiting, { fn = fn, owner = owner })
end

-- Every transition, with the new state as a string. For a script that wants to put something on
-- screen while the game is still coming up, or to reset itself when a run starts loading.
local state_handlers = {}
function tw.on_state(fn)
    local owner = caller_owner()
    if not may_register(owner, "tw.on_state", fn) then return end
    enlist(state_handlers, { fn = fn, owner = owner })
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
    enlist(group_handlers, { name = name, fn = fn, owner = owner, loaded = C_group_loaded(name) ~= 0 })
end

-- The script is being switched off or reloaded: put back whatever it changed. Runs before its hooks
-- are taken out, so a script can still reach the channels it held; writes follow the usual rule and
-- are refused while the game is not up. Not run when the game itself exits - there is nothing left to
-- restore by then.
local unloads = {}
function tw.on_unload(fn)
    local owner = caller_owner()
    if not may_register(owner, "tw.on_unload", fn) then return end
    enlist(unloads, { fn = fn, owner = owner })
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

-- For tooling, like S.handle_types in tw.channels.
S.handle_types.MuteHandle = Mute
S.constructors.mute = "MuteHandle"

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
                place_in(ready_waiting, r)
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

-- The user changed the order. Every list is re-sorted against the registry's new answer; within a
-- script, registration order still decides. Channel subscribers are C's half (api_hooks), done by the
-- caller right after this.
function exports.reorder()
    local lists = { handlers, ticks, post_ticks, ready_waiting, state_handlers, group_handlers, unloads }
    for _, list in ipairs(lists) do
        for i = 1, #list do
            list[i].place = C_order(list[i].owner)
        end
        table.sort(list, runs_before)
    end
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
