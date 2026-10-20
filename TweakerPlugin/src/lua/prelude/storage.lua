-- ---------------------------------------------------------------------------------------------------
-- tw.storage - a script's own files: text and JSON, in a folder of its own.
--
-- TweakerStuff\ScriptData\<script file without .lua>\, flat names ([A-Za-z0-9_.-], up to 64, not
-- starting with a dot), with limits - a file, the number of files, the folder. Not a hole in the
-- sandbox but its lawful replacement: io and os stay gone. Docs/Internal/script-tabs.md §3.8.
--
-- A write goes to memory and returns; C's own thread puts it on disk within a tenth of a second,
-- through a temporary file, so writing from on_frame costs a copy and never a system call. A read of
-- a file on disk is a system call the first time and a copy after - read while the script loads.
--
-- Failures are results, not errors: nil and a reason, so `tw.storage.read_json("settings.json") or {}`
-- is the whole first-run case. Only a wrong argument - a name that is not a string - is a Lua error,
-- at the script's line, as everywhere in tw.
-- ---------------------------------------------------------------------------------------------------

local ffi = require("ffi")
local S = ...
local C = S.C
local tw = S.tw
local caller_owner = S.caller_owner

-- Same layout as tw::lua::storage::json_node.
ffi.cdef [[
typedef struct { int type; int count; double number; const char* str; int len; } tw_json_node;
]]

local C_read, C_write, C_read_json, C_write_json = C.tw_storage_read, C.tw_storage_write, C.tw_storage_read_json, C.tw_storage_write_json
local C_exists, C_remove, C_list, C_error = C.tw_storage_exists, C.tw_storage_remove, C.tw_storage_list, C.tw_storage_error

local data_out, size_out = ffi.new("const char*[1]"), ffi.new("int[1]")
local tape_out, count_out = ffi.new("const void*[1]"), ffi.new("int[1]")

-- JSON node types, same numbers as tw::lua::storage::json_type.
local J_NULL, J_FALSE, J_TRUE, J_NUMBER, J_STRING, J_ARRAY, J_OBJECT = 0, 1, 2, 3, 4, 5, 6
local MAX_DEPTH = 64

local huge, floor = math.huge, math.floor

local function last_error() return ffi.string(C_error()) end

-- Level 3 is the script: 1 this, 2 the tw.storage function, 3 its caller.
local function name_arg(name)
    if type(name) ~= "string" then error("tw.storage: a file name must be a string", 3) end
end

tw.storage = {}

-- The file's bytes as a string - text, or anything else: Lua strings hold any bytes.
function tw.storage.read_text(name)
    local owner = caller_owner()
    name_arg(name)
    if C_read(owner, name, data_out, size_out) == 0 then return nil, last_error() end
    return ffi.string(data_out[0], size_out[0])
end

function tw.storage.write_text(name, text)
    local owner = caller_owner()
    name_arg(name)
    if type(text) ~= "string" then error("tw.storage.write_text: the text must be a string", 2) end
    if C_write(owner, name, text, #text) == 0 then return nil, last_error() end
    return true
end

-- The tape C made of the file, back into Lua values: objects are tables with string keys, arrays
-- sequences from 1, null is nil - the key is simply not there.
local function from_tape(tape, i)
    local node = tape[i]
    local t = node.type
    if t == J_NUMBER then return node.number, i + 1 end
    if t == J_STRING then return ffi.string(node.str, node.len), i + 1 end
    if t == J_TRUE then return true, i + 1 end
    if t == J_FALSE then return false, i + 1 end
    if t == J_ARRAY then
        local out, j = {}, i + 1
        for k = 1, node.count do
            local v
            v, j = from_tape(tape, j)
            out[k] = v
        end
        return out, j
    end
    if t == J_OBJECT then
        local out, j = {}, i + 1
        for _ = 1, node.count do
            local key = ffi.string(tape[j].str, tape[j].len)
            local v
            v, j = from_tape(tape, j + 1)
            out[key] = v
        end
        return out, j
    end
    return nil, i + 1 -- J_NULL
end

function tw.storage.read_json(name)
    local owner = caller_owner()
    name_arg(name)
    if C_read_json(owner, name, tape_out, count_out) == 0 then return nil, last_error() end
    local value = from_tape(ffi.cast("const tw_json_node*", tape_out[0]), 0)
    if value == nil then return nil, name .. " holds null" end
    return value
end

-- A Lua value into tape records, { type, count, number, string }; nil, or the reason it has no JSON.
-- An object's keys go out sorted, so a file does not reshuffle itself between two writes.
local function encode(value, nodes, depth, visiting)
    local tv = type(value)
    if tv == "string" then
        nodes[#nodes + 1] = { J_STRING, 0, 0, value }
    elseif tv == "number" then
        if value ~= value or value == huge or value == -huge then return "NaN and infinity have no JSON" end
        nodes[#nodes + 1] = { J_NUMBER, 0, value }
    elseif tv == "boolean" then
        nodes[#nodes + 1] = { value and J_TRUE or J_FALSE, 0, 0 }
    elseif tv == "table" then
        if depth >= MAX_DEPTH then return "it nests deeper than " .. MAX_DEPTH .. " levels" end
        if visiting[value] then return "a table contains itself" end
        visiting[value] = true

        local n, strings, top = 0, 0, 0
        for k in pairs(value) do
            n = n + 1
            local tk = type(k)
            if tk == "string" then
                strings = strings + 1
            elseif tk == "number" and k >= 1 and k == floor(k) then
                if k > top then top = k end
            else
                return "a key is a " .. tk .. " (" .. tostring(k) .. ") - keys are strings, or 1..n for an array"
            end
        end

        if n == 0 or strings == n then
            local keys = {}
            for k in pairs(value) do keys[#keys + 1] = k end
            table.sort(keys)
            nodes[#nodes + 1] = { J_OBJECT, n, 0 }
            for i = 1, n do
                local k = keys[i]
                nodes[#nodes + 1] = { J_STRING, 0, 0, k }
                local err = encode(value[k], nodes, depth + 1, visiting)
                if err then return err end
            end
        elseif strings == 0 then
            if top ~= n then return "an array has a hole - its keys must be 1.." .. n end
            nodes[#nodes + 1] = { J_ARRAY, n, 0 }
            for i = 1, n do
                local err = encode(value[i], nodes, depth + 1, visiting)
                if err then return err end
            end
        else
            return "a table mixes string keys with numbered ones"
        end

        visiting[value] = nil
    else
        return "a " .. tv .. " has no JSON"
    end
end

function tw.storage.write_json(name, value)
    local owner = caller_owner()
    name_arg(name)
    if type(value) ~= "table" then error("tw.storage.write_json: the value must be a table", 2) end

    local nodes = {}
    local err = encode(value, nodes, 0, {})
    if err then return nil, name .. " was not written: " .. err end

    -- The strings stay referenced by `nodes`, and through it by this frame, for as long as C reads them.
    local tape = ffi.new("tw_json_node[?]", #nodes)
    for i = 1, #nodes do
        local n, out = nodes[i], tape[i - 1]
        out.type, out.count, out.number = n[1], n[2], n[3]
        local s = n[4]
        if s ~= nil then
            out.str, out.len = s, #s
        end
    end

    if C_write_json(owner, name, tape, #nodes) == 0 then return nil, last_error() end
    return true
end

function tw.storage.exists(name)
    local owner = caller_owner()
    name_arg(name)
    return C_exists(owner, name) ~= 0
end

function tw.storage.remove(name)
    local owner = caller_owner()
    name_arg(name)
    if C_remove(owner, name) == 0 then return nil, last_error() end
    return true
end

-- Every file of the script's, sorted.
function tw.storage.list()
    local owner = caller_owner()
    local out = {}
    for line in ffi.string(C_list(owner)):gmatch("[^\n]+") do
        out[#out + 1] = line
    end
    return out
end
