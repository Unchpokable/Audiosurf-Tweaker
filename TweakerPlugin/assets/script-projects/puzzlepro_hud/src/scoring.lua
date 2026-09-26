-- ---------------------------------------------------------------------------------------------
-- Scoring
-- ---------------------------------------------------------------------------------------------

local colours = require("colours")
local ch = require("channels")
local scan = require("scan")

local YELLOW, RED = colours.YELLOW, colours.RED
local league_id, is_ninja, prevent_mono, perfect_ninja = ch.league_id, ch.is_ninja, ch.prevent_mono, ch.perfect_ninja
local scaler, best_match, hits = ch.scaler, ch.best_match, ch.hits
local totals = scan.totals

local function league()
    local id = league_id:get()
    if id == nil then
        return 1, "casual"
    end
    -- LeagueID is 0-based: XX_StartHere's Do_CalcMedalRequirements switches the medal scalers on it
    -- directly. The name is printed alongside the rating so a wrong reading is visible rather than
    -- silently folded into the number.
    local n = math.floor(id + 0.5) + 1
    if n < 1 then
        n = 1
    elseif n > 3 then
        n = 3
    end
    return n, ({ "casual", "pro", "elite" })[n]
end

-- Everything the game will add to Points at the end, as far as it is knowable mid-run, minus Clean
-- Finish - that one is shown separately as the second number, because it is only decided on the
-- last frame. Returns the multiplier and the feat names in the game's own order
-- (Do_FindAddBonusPoints #797).
local function earned_bonus()
    local mult, feats = 0, {}

    local ninja = (is_ninja:get() or 0) ~= 0
    local prevent = (prevent_mono:get() or 0) ~= 0

    -- Stealth. The whole Mono half of the scoring table used to be missing here, which made the
    -- prediction wrong by a third of the score for every Ninja character.
    if ninja and (perfect_ninja:get() or 0) ~= 0 and not prevent then
        mult = mult + (scaler.stealth:get() or 0.33)
        feats[#feats + 1] = "Stealth"
    end

    local lm = best_match:get() or 0
    if lm > 20 then
        mult = mult + (scaler.match21:get() or 0.21)
        feats[#feats + 1] = "Match21"
    elseif lm > 10 then
        mult = mult + (scaler.match11:get() or 0.11)
        feats[#feats + 1] = "Match11"
    elseif lm > 6 then
        mult = mult + (scaler.match7:get() or 0.07)
        feats[#feats + 1] = "Match7"
    end

    -- Both colour bonuses are gated on !Ninja? - a Ninja gets Stealth instead, and never these.
    --
    -- The 95% test uses Achievements::YellowsHit / RedsHit over Achievements' own totals, which is a
    -- different quantity from the de-duplicated table (tally.lua, DESIGN.md §1). This reads what the game
    -- will read; the table shows the truth. Where they disagree is itself worth seeing.
    if not ninja and totals.ready then
        local yellow_total = totals.by_row[YELLOW] or 0
        local yellow_hit = hits[YELLOW]:get()
        if yellow_total > 0 and yellow_hit and yellow_hit / yellow_total >= 0.95 then
            mult = mult + (scaler.yellow:get() or 0.05)
            feats[#feats + 1] = "Butter Ninja"
        end

        local red_total = totals.by_row[RED] or 0
        local red_hit = hits[RED]:get()
        if red_total > 0 and red_hit and red_hit / red_total >= 0.95 then
            mult = mult + (scaler.red:get() or 0.05)
            feats[#feats + 1] = "Seeing Red"
        end
    end

    -- Worth no points, but the game puts it in the Feat String, so a tracker that omits it
    -- disagrees with the results screen for no reason.
    if prevent then
        feats[#feats + 1] = "Mono Special"
    end

    return mult, feats
end

local FEAT_ICON = {
    ["Clean Finish"] = "feat_clean_finish",
    ["Stealth"] = "feat_stealth",
    ["Match7"] = "feat_match7",
    ["Match11"] = "feat_match11",
    ["Match21"] = "feat_match21",
    ["Butter Ninja"] = "feat_butter_ninja",
    ["Seeing Red"] = "feat_seeing_red",
    ["Mono Special"] = "feat_mono_special",
}

return {
    league = league,
    earned_bonus = earned_bonus,
    FEAT_ICON = FEAT_ICON,
}
