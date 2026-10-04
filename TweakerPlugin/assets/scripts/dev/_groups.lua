-- @name        Group Inspector
-- @author      Audiosurf Tweaker
-- @version     1.0
-- @description Lists the channel groups the engine has loaded - the answer to "my group did not resolve"
--
-- Diagnostic: what channel groups the engine actually has loaded, right now.
--
-- Exists because "my group did not resolve" has no good answer from inside a script. The engine
-- stores a full path per group ("Environment\Puzzle.cgr"), the pool name is something else again,
-- and which groups are loaded changes as the game moves between the menu and a run. This just draws
-- the list.
--
-- Delete or rename it once you are done - it is a debugging tool, not a feature.

local DIM   = 0xFF9A9A9A
local WHITE = 0xFFFFFFFF

tw.on_frame(function()
    if not tw.engine_ready() then return end

    local groups = tw.groups()
    local _, y0, _, y1 = tw.hud.safe()
    local x = 24
    local y = y0 + 24

    tw.hud.text(x, y, string.format("loaded groups: %d", #groups), WHITE)

    -- Two columns, because there are usually more of these than fit down one side of the screen.
    local per_column = math.max(1, math.floor((y1 - y - 40) / 14))
    for i, name in ipairs(groups) do
        local col = math.floor((i - 1) / per_column)
        local row = (i - 1) % per_column
        tw.hud.text(x + col * 300, y + 20 + row * 14, name, DIM)
    end
end)
