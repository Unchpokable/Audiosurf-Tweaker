-- @name        UI Probe
-- @description Every kind of control a script tab has, and settings kept in tw.storage
-- @api         3
--
-- A probe for script tabs and storage (Docs/Internal/script-tabs.md), not a script anyone
-- wants running while they play. Copy it flat into Scripts\ and open the overlay: its tab, "UI Probe", is
-- after Scripts.
--
--  - every control on the tab says what it did in the Events list, live while a slider is dragged and
--    "committed" when it is let go; what is committed is saved to ScriptData\ui_probe\settings.json and
--    comes back after a restart;
--  - the card it draws on the HUD follows the Look group: shown or not, its scale, its corner, its accent,
--    its number of rows;
--  - "Fail five times" throws from on_frame five frames running, which is the error budget: the probe is
--    suspended, its tab stays and is switched off as a whole, and Resume in the Scripts tab brings it back;
--  - a settings.json spoiled by hand is reported once in the notefeed, and the probe starts from its own
--    initial values; a missing one is the first run and says nothing.
--
-- Not shipped (dev/ never is). The API it exercises: Docs/scripting/menus.md and Docs/scripting/storage.md; the
-- walk-through: Docs/scripting/examples.md, "A settings tab that survives a restart".

local DEFAULTS = { show = true, scale = 1, corner = "Top right", accent = tw.theme("accent_text"), rows = 3, extra = false }

-- The first run has no file; a file that does not read is worth one line to the user, not a failure.
local cfg = {}
if tw.storage.exists("settings.json") then
    local loaded, why = tw.storage.read_json("settings.json")
    if loaded then
        cfg = loaded
    else
        tw.notify("UI Probe: starting from defaults - " .. why)
    end
end

local function setting(key)
    if cfg[key] == nil then return DEFAULTS[key] end
    return cfg[key]
end

local tab = tw.ui.tab{ label = "UI Probe" }

local look = tab:group("Look")
local show = look:toggle{ label = "Show the card", value = setting("show"), tooltip = "The probe's card on the HUD" }
local scale = look:slider{ label = "Card scale", min = 0.5, max = 2, value = setting("scale") }
local corner = look:choice{ label = "Corner", options = { "Top left", "Top right", "Bottom" }, value = setting("corner") }
local accent = look:color{ label = "Accent", value = setting("accent") }
local rows = look:number{ label = "Rows on the card", min = 1, max = 8, value = setting("rows"), tooltip = "How many recent events the card lists" }

local more = look:group("More")
local extra = more:toggle{ label = "Show the hidden slider", value = setting("extra") }
local hidden = more:slider{ label = "Was hidden", min = 0, max = 10, value = 5, visible = setting("extra") }
more:toggle{ label = "Always off", enabled = false, tooltip = "Declared with enabled = false" }

tab:separator()
local events = tab:list{ label = "Events", height = 150 }
local status = tab:text("Nothing yet")

local actions = tab:group("Actions")
local reset = actions:button{ label = "Reset to defaults" }
local forget = actions:button{ label = "Forget saved settings" }
local fail = actions:button{ label = "Fail five times", tooltip = "Throws from on_frame five frames in a row - the error budget suspends the probe" }

-- The newest event first; the list and the card both show it.
local lines = {}
local function note(text)
    table.insert(lines, 1, string.format("%6d  %s", tw.frame, text))
    if #lines > 40 then lines[#lines] = nil end
    events:items(lines)
end

local saved_at = "never"
local function save()
    cfg.show, cfg.scale, cfg.corner = show:get(), scale:get(), corner:get().text
    cfg.accent, cfg.rows, cfg.extra = accent:get(), rows:get(), extra:get()
    local ok, why = tw.storage.write_json("settings.json", cfg)
    if ok then
        saved_at = "frame " .. tw.frame
    else
        tw.warn("settings not saved: " .. why)
    end
end

local function describe(value)
    if type(value) == "table" then return value.text .. " (#" .. value.index .. ")" end
    if type(value) == "number" and value > 65535 then return string.format("0x%08X", value) end
    return tostring(value)
end

local function watch(name, control)
    control:on_change(function(value, committed)
        note(name .. " -> " .. describe(value) .. (committed and "" or "  (dragging)"))
        if committed then save() end
    end)
end

watch("show", show)
watch("scale", scale)
watch("corner", corner)
watch("accent", accent)
watch("rows", rows)
watch("hidden", hidden)
watch("extra", extra)

extra:on_change(function(on) hidden:show(on) end)

events:on_change(function(row) note("picked row " .. tostring(row)) end)

reset:on_click(function()
    show:set(DEFAULTS.show)
    scale:set(DEFAULTS.scale)
    corner:set(DEFAULTS.corner)
    accent:set(DEFAULTS.accent)
    rows:set(DEFAULTS.rows)
    extra:set(DEFAULTS.extra)
    hidden:show(DEFAULTS.extra)
    save()
    note("reset - set() raised no on_change above")
end)

forget:on_click(function()
    local ok, why = tw.storage.remove("settings.json")
    note(ok and "settings.json removed" or ("not removed: " .. why))
end)

local failing = 0
fail:on_click(function()
    failing = 5
    note("failing five times")
end)

local CARD_W, LINE_H = 260, 18

tw.on_frame(function()
    if failing > 0 then
        failing = failing - 1
        error("UI Probe failing on purpose, " .. failing .. " to go")
    end

    -- Building the status line only while somebody can see it.
    if tab:visible() then
        status:set(string.format("Frame %d. Saved: %s. Files: %s.", tw.frame, saved_at, table.concat(tw.storage.list(), ", ")))
    end

    if not show:get() then return end

    local k = scale:get()
    local count = math.min(rows:get(), #lines)
    local w, h = CARD_W * k, (34 + LINE_H * count) * k
    local where = ({ "top_left", "top_right", "bottom_right" })[corner:get().index]
    local x, y = tw.hud.place(w, h, where)
    if x == nil then return end

    local tint = accent:get()
    tw.hud.rect(x, y, x + w, y + h, tw.alpha(tw.theme("surface"), 0.92), 8 * k)
    tw.hud.rect(x, y, x + 4 * k, y + h, tint, 2 * k)
    tw.hud.text(x + 14 * k, y + 8 * k, "UI Probe", tint, 16 * k)
    for i = 1, count do
        tw.hud.text(x + 14 * k, y + (30 + LINE_H * (i - 1)) * k, lines[i], tw.theme("text_primary"), 13 * k)
    end
end)
