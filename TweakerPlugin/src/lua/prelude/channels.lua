-- ---------------------------------------------------------------------------------------------------
-- tw.channels - handles onto the game's channels, one accessor per family, and Array Table columns.
-- ---------------------------------------------------------------------------------------------------

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

function Channel:_resolve()
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
    if not self:_resolve() then return nil end
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
    if not self:_resolve() then return nil end
    return C_get(self.h)
end

function FloatChannel:set(value)
    if not self:_resolve() then return false end
    C_set(self.h, value)
    return true
end

local TextChannel = setmetatable({}, { __index = Channel })
TextChannel.__index = TextChannel

function TextChannel:get()
    if not self:_resolve() then return nil end
    return ffi.string(C_text(self.h))
end

-- Vector channels return three numbers rather than a table: a table per read would be garbage on the
-- HUD path, and every caller so far wants the components immediately anyway.
local VectorChannel = setmetatable({}, { __index = Channel })
VectorChannel.__index = VectorChannel

function VectorChannel:get()
    if not self:_resolve() then return nil end
    if C_vector(self.h, vec_out) == 0 then return nil end
    return vec_out[0], vec_out[1], vec_out[2]
end

-- Writing a vector channel is not the local store that float_ch:set is. The engine sets the three
-- components and then writes each one through into the numeric channel wired to that component, if
-- one is wired - so this can reach further into the graph than it looks. See
-- Docs/scripting/channels.md.
function VectorChannel:set(x, y, z)
    if not self:_resolve() then return false end
    C_set_vector(self.h, x, y, z)
    return true
end

-- Matrix channels: sixteen numbers, row-major (_11 _12 _13 _14 _21 ... _44), as multiple returns for
-- the same reason vectors are - a table per read is garbage on the drawing path. For writing, a table
-- of sixteen is accepted as well as sixteen arguments, because nobody wants to type the second form.
local MatrixChannel = setmetatable({}, { __index = Channel })
MatrixChannel.__index = MatrixChannel

function MatrixChannel:get()
    if not self:_resolve() then return nil end
    if C_matrix(self.h, mat_out) == 0 then return nil end
    return mat_out[0], mat_out[1], mat_out[2], mat_out[3],
           mat_out[4], mat_out[5], mat_out[6], mat_out[7],
           mat_out[8], mat_out[9], mat_out[10], mat_out[11],
           mat_out[12], mat_out[13], mat_out[14], mat_out[15]
end

function MatrixChannel:set(first, ...)
    if not self:_resolve() then return false end
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
    if not self.column:_resolve() or not self.cursor:_resolve() then return nil end
    return C_array_read(self.column.h, self.cursor.h, index)
end

-- Writing a row is NOT symmetrical with reading one, and the asymmetry is deliberate: an index the
-- table does not have is refused here, because the engine's own write path would create that row
-- instead of rejecting it - lengthening a table the rest of the game reads. So `false` from this
-- means either "could not resolve" or "no such row", and both are worth checking.
function Array:set(index, value)
    if not self.column:_resolve() or not self.cursor:_resolve() then return false end
    return C_array_write(self.column.h, self.cursor.h, index, value) ~= 0
end

-- Row count of the underlying table, or nil when it cannot be asked. Valid indices are 0..rows()-1.
function Array:rows()
    if not self.column:_resolve() then return nil end
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
    if not self.column:_resolve() or not self.cursor:_resolve() then return nil end
    if C_array_vector(self.column.h, self.cursor.h, index, vec_out) == 0 then return nil end
    return vec_out[0], vec_out[1], vec_out[2]
end

-- Same contract as Array:set. Note this goes through the array type's own SetVector, which writes
-- the row - not the component-propagating store a plain vector_ch:set performs.
function VectorArray:set(index, x, y, z)
    if not self.column:_resolve() or not self.cursor:_resolve() then return false end
    return C_array_write_vec(self.column.h, self.cursor.h, index, x, y, z) ~= 0
end

function VectorArray:rows()
    if not self.column:_resolve() then return nil end
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

-- For tooling only: what each kind of handle answers to, and which constructor hands back which.
-- harness/lua reads these to describe the API in Docs/scripting/api.json - the names a script can
-- call on a handle are the functions of these tables and of what they inherit, less the ones that
-- start with "_", which are the prelude's own (Channel:_resolve). Nothing at runtime looks.
S.handle_types = {
    FloatHandle = FloatChannel,
    TextHandle = TextChannel,
    VectorHandle = VectorChannel,
    MatrixHandle = MatrixChannel,
    ArrayHandle = Array,
    VectorArrayHandle = VectorArray,
}
S.constructors = {
    float_ch = "FloatHandle",
    channel = "FloatHandle",
    string_ch = "TextHandle",
    vector_ch = "VectorHandle",
    matrix_ch = "MatrixHandle",
    array = "ArrayHandle",
    array_vec = "VectorArrayHandle",
}
