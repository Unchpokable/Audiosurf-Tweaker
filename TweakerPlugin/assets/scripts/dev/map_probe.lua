-- @name        Map probe
-- @author      Audiosurf Tweaker
-- @version     1.0.0
-- @description Dev probe for Ф5: a bundled script that errors on purpose, to show errors pointing at its modules. Not for players.
-- @api         1
-- @bundle      twllink 1.1, 2 modules

local __twl_modules, __twl_cache, __twl_loading = {}, {}, {}

local function require(name)
    local hit = __twl_cache[name]
    if hit == __twl_loading then
        error("module '" .. tostring(name) .. "' is still loading - a require cycle, or it failed while loading", 2)
    end
    if hit ~= nil then return hit end
    local body = __twl_modules[name]
    if body == nil then error("module '" .. tostring(name) .. "' is not in this bundle", 2) end
    __twl_cache[name] = __twl_loading
    local value = body(require, name)
    if value == nil then value = true end
    __twl_cache[name] = value
    return value
end

__twl_modules["faulty.thrower"] = function(require, ...)

local M = {}

tw.warn("map probe: said at load")

local frames = 0

function M.tick()
    frames = frames + 1
    if frames % 120 ~= 0 then
        return
    end
    error("map probe: deliberate error")
end

return M
end

__twl_modules["main"] = function(require, ...)

local thrower = require("faulty.thrower")

tw.on_frame(function()
    thrower.tick()
end)
end

require("main")
