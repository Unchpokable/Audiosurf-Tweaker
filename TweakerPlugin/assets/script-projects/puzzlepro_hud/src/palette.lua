-- ---------------------------------------------------------------------------------------------
-- Palette
-- ---------------------------------------------------------------------------------------------
--
-- The colours the player actually sees come from XX_StartHere::FetchColorByID (#882), and there is
-- more than one palette behind it:
--
--     #1533  switch (XX_gui::Color5.x + .y + .z) == 0
--      +- 1 -> #1535  StartGroup #1542/#1538/#1534/#1530/#1526      "fallback"
--      +- 0 -> #7157  switch SpecialPurpose::isMechMode
--              +- 1 -> #7159  Highway::MechColor0..4                "mech"
--              +- 0 -> #7072  switch SpecialPurpose::PortalMode?
--                      +- 1 -> #7074  PlayerCar_Sword::PortalColor1..5   "portal"
--                      +- 0 -> #440   XX_gui::Color1..Color5             "normal"
--
-- Reading a channel and getting a plausible colour is not evidence the game is using it - a
-- Value Vector carries no "someone reads me" flag. The only way to tell a live branch from a spare
-- is to walk the switches, which is what palette_source() does.
--
-- Note the reversed numbering in the normal branch (ColorID 0 -> Color5) and the extra switch on
-- ids 2 and 3, which become GlobalMonoColor under Ninja or Freeride. Both are the game's, not a
-- transcription slip.

local colours = require("colours")
local ch = require("channels")

local COLOURS = colours.COLOURS
local is_ninja, is_freeride, is_mech, is_portal = ch.is_ninja, ch.is_freeride, ch.is_mech, ch.is_portal

-- Handles for palettes other than the normal one are created on demand. Two of those groups
-- (PlayerCar_Sword, and Highway before a run) are not always loaded, and a handle for an absent
-- group eventually reports itself in the notification feed - which would be noise, not information,
-- for a branch the player is not in.
local sources = {}

local function vectors(key, group, names)
    local made = sources[key]
    if made == nil then
        made = {}
        for i, name in ipairs(names) do
            made[i] = tw.vector_ch(group, name)
        end
        sources[key] = made
    end
    return made
end

-- ColorID -> channel, per branch. Index 6 in each list is the white/stone block, which is the same
-- StartGroup channel everywhere.
local function normal_palette()
    return vectors("normal", "XX_gui", { "Color5", "Color4", "Color3", "Color2", "Color1" })
end

local function stone()
    return vectors("stone", "StartGroup", { 2667 })[1]
end

local function mono_colour()
    return vectors("mono", "StartGroup", { 862 })[1]
end

-- Which of the four palettes FetchColorByID is actually reading right now.
local function palette_source()
    local r, g, b = normal_palette()[1]:get()
    -- The game's own test, verbatim: an unset Color5 means the player has configured no colours and
    -- the whole switch falls through to the spare palette in StartGroup.
    if r == nil then
        return "unknown"
    end
    if r + g + b == 0 then
        return "fallback"
    end
    if (is_mech:get() or 0) ~= 0 then
        return "mech"
    end
    if (is_portal:get() or 0) ~= 0 then
        return "portal"
    end
    return "normal"
end

local function palette_channels(source)
    if source == "fallback" then
        return vectors("fallback", "StartGroup", { 1542, 1538, 1534, 1530, 1526 })
    elseif source == "mech" then
        return vectors("mech", "Highway", { "MechColor4", "MechColor3", "MechColor2", "MechColor1", "MechColor0" })
    elseif source == "portal" then
        return vectors("portal", "PlayerCar_Sword", { "PortalColor5", "PortalColor4", "PortalColor3", "PortalColor2", "PortalColor1" })
    end
    return normal_palette()
end

-- The road colour of a Mono run, as last read. Kept apart from COLOURS because it is not a block colour of
-- its own: it is what ids 2 and 3 turn into, and the Stealth feat is drawn in it. nil outside Ninja and
-- Freeride, where the game does not use it.
local mono_now = nil

-- The palette is a player setting, so it is re-read periodically - but not every frame, because six
-- vector evaluations to answer a question that changes when someone opens the options screen is six
-- too many.
local function refresh_palette()
    local source = palette_source()
    if source == "unknown" then
        return
    end

    local channels = palette_channels(source)
    -- Only the normal branch has the mono override on ids 2 and 3 (#3602 / #4350); the spare and
    -- mech palettes wire those cases straight through.
    local mono = nil
    if source == "normal" and ((is_ninja:get() or 0) ~= 0 or (is_freeride:get() or 0) ~= 0) then
        local r, g, b = mono_colour():get()
        if r then
            mono = tw.rgb(r, g, b)
        end
    end
    mono_now = mono

    for i, c in ipairs(COLOURS) do
        local channel = (c.id == 7) and stone() or channels[i]
        if mono ~= nil and (c.id == 2 or c.id == 3) then
            c.colour = mono
        elseif channel ~= nil then
            local r, g, b = channel:get()
            if r then
                c.colour = tw.rgb(r, g, b)
            end
        end
    end
end

return {
    refresh = refresh_palette,
    mono = function()
        return mono_now
    end,
}
