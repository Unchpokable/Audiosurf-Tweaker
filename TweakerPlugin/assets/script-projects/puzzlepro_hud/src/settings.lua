-- ---------------------------------------------------------------------------------------------
-- The settings tab (DESIGN.md §14)
-- ---------------------------------------------------------------------------------------------
--
-- PuzzlePRO HUD's own tab in the overlay menu, and the file it is kept in:
-- engine\TweakerStuff\ScriptData\puzzlepro_hud\settings.json. The tab draws itself; this declares it,
-- reads the file once at load, hands every value to the module that uses it, and writes the file when
-- the player finishes a change.
--
-- Labels are English, like the rest of the HUD.

local map_colours = require("map_colours")
local song_badge = require("draw.song_badge")
local stats_panel = require("draw.stats_panel")

local FILE = "settings.json"

-- What a missing or broken file falls back to, field by field. The weights are the game's block values
-- (DESIGN.md §11), the rest what the HUD looked like before it had settings.
local DEFAULTS = {
    batch = 96,
    softness = 0.5,
    weights = { 1, 2, 3, 5, 8 },
    time_left = true,
    overflow = "Scroll",
    pause = 6,
}

local COLOUR_NAMES = { "Purple", "Blue", "Green", "Yellow", "Red" }

-- A value from the file if it is of the right kind, the default if it is not. A settings file written by
-- an older version, or edited by hand, must never stop the HUD from loading.
local function number_or(v, default)
    if type(v) == "number" and v == v and v ~= math.huge and v ~= -math.huge then
        return v
    end
    return default
end

local stored = tw.storage.read_json(FILE)
if type(stored) ~= "table" then
    stored = {}
end

local cfg = {
    batch = number_or(stored.batch, DEFAULTS.batch),
    softness = number_or(stored.softness, DEFAULTS.softness),
    weights = {},
    time_left = stored.time_left ~= false,
    overflow = (stored.overflow == "Ellipsis") and "Ellipsis" or "Scroll",
    pause = number_or(stored.pause, DEFAULTS.pause),
}
local stored_weights = type(stored.weights) == "table" and stored.weights or {}
for i = 1, #COLOUR_NAMES do
    cfg.weights[i] = number_or(stored_weights[i], DEFAULTS.weights[i])
end

-- ---- the tab ------------------------------------------------------------------------------------

local tab = tw.ui.tab{ label = "PuzzlePRO HUD" }

local map = tab:group("Minimap")
local batch = map:number{
    label = "Rings per colour stop",
    min = 16,
    max = 512,
    value = cfg.batch,
    tooltip = "How long a stretch of track gets one colour. About 48 rings is a second of song; "
        .. "rounded to a multiple of 16.",
}
local softness = map:slider{
    label = "Colour softness",
    min = 0,
    max = 1,
    value = cfg.softness,
    tooltip = "0: each stretch takes its leading colour outright. 1: colours blend with their neighbours.",
}
local time_left = map:toggle{ label = "Time left under the map", value = cfg.time_left }

local weights_group = tab:group("Minimap colour weights")
weights_group:text("How much one block of each colour counts towards the colour of its stretch. "
    .. "The defaults are what the game pays for a block: a red one is worth eight purple ones.")
local weights = {}
for i, name in ipairs(COLOUR_NAMES) do
    weights[i] = weights_group:slider{ label = name, min = 0, max = 10, value = cfg.weights[i] }
end
local reset_weights = weights_group:button{ label = "Reset to the game's values" }

local song = tab:group("Song")
local overflow = song:choice{
    label = "A title too long to fit",
    options = { "Scroll", "Ellipsis" },
    value = cfg.overflow,
}
local pause = song:slider{
    label = "Seconds before it scrolls",
    min = 2,
    max = 30,
    value = cfg.pause,
    enabled = cfg.overflow == "Scroll",
}

-- ---- applying and saving ------------------------------------------------------------------------

-- Every value straight from its control into the module that uses it. Cheap enough to run on every
-- change, drags included, so a slider previews on the map while it moves.
local function apply()
    local w = {}
    for i = 1, #weights do
        w[i] = weights[i]:get()
    end
    map_colours.configure(batch:get(), softness:get(), w)
    stats_panel.settings.time_left = time_left:get()
    song_badge.settings.overflow = (overflow:get().text == "Ellipsis") and "ellipsis" or "scroll"
    song_badge.settings.pause = pause:get()
    pause:enable(overflow:get().text == "Scroll")
end

local function save()
    local w = {}
    for i = 1, #weights do
        w[i] = weights[i]:get()
    end
    local ok, why = tw.storage.write_json(FILE, {
        batch = batch:get(),
        softness = softness:get(),
        weights = w,
        time_left = time_left:get(),
        overflow = overflow:get().text,
        pause = pause:get(),
    })
    if not ok then
        tw.warn("settings were not saved: " .. tostring(why))
    end
end

local function changed(_, committed)
    apply()
    if committed then
        save()
    end
end

for _, control in ipairs({ batch, softness, time_left, overflow, pause }) do
    control:on_change(changed)
end
for _, control in ipairs(weights) do
    control:on_change(changed)
end

-- set() does not raise on_change, so the button applies and saves itself.
reset_weights:on_click(function()
    for i = 1, #weights do
        weights[i]:set(DEFAULTS.weights[i])
    end
    apply()
    save()
end)

apply()

return {}
