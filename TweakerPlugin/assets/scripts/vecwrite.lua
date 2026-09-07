-- @name        Vector Write Probe
-- @author      Audiosurf Tweaker
-- @version     1.2
-- @description Writes a vector channel and reads it back, to show whether the write stuck
--
-- A measuring script, not an effect. It answers one question: when you write a vector channel, does
-- the value stay?
--
--
-- WHY THAT IS A REAL QUESTION
--
-- Writing a vector channel is not the local store that writing a number is. The engine sets the
-- three components and then writes each one through into whatever numeric channel is wired to that
-- component. What happens next depends entirely on what those channels are:
--
--   - plain values      -> the write sticks
--   - computed channels -> the next evaluation overwrites it, usually on the next frame
--
-- And separately from whether the value stays, whether you *see* anything depends on what reads the
-- channel. Those are two different failure modes and this script separates them: the numbers below
-- tell you about the first, your eyes tell you about the second.
--
--
-- A WORKED EXAMPLE OF GETTING THE TARGET WRONG
--
-- Version 1.0 of this script pointed at RenderCommon::GridColor, on the assumption that it colours
-- the puzzle grid. It does not. Tracing what actually consumes it: two Materials on a Surface called
-- `ro_01 1` under the 3D Object `static parts`, and two more under `CarHitBlurBar`. Road furniture
-- and the hit flash - not tiles. The puzzle tiles are drawn per-cell down a different path whose
-- Surface carries no Material at all, and they take their colour from the block occupying the cell.
--
-- So writing GridColor changed nothing visible, and looked like the write had failed when it had
-- not. Hence the target below, chosen for the opposite property.
--
--
-- THE TARGET
--
-- XX_gui::Color1 is the red entry of the block palette **that the game actually reads**.
--
-- Version 1.1 pointed at StartGroup #1526 instead, and that produced the most instructive result of
-- the whole exercise: the write landed and held - a script reading that channel saw the new colour -
-- but the blocks on the road stayed red. The channel was real, the write was real, and the game was
-- not looking at it.
--
-- FetchColorByID walks three switches before it reaches a palette:
--
--     (XX_gui::Color5 is all-zero?)  no -> (isMechMode?)  no -> (PortalMode?)  no -> XX_gui::Color1..5
--                                    yes -> StartGroup #1542..#1526          <- the fallback
--
-- So #1526 is the palette used only when no colours are configured. Reading it gives a plausible
-- number either way, because a Value Vector carries no "somebody uses me" flag. The lesson is worth
-- more than the colour: **a channel reading sensibly is not evidence the game reads it.**
--
-- Numbering is reversed between the two palettes: ColorID 0 (purple) is Color5, ColorID 4 (red) is
-- Color1. See reversing-journal-gameplay.md §3.7.

local GROUP  = "XX_gui"
local TARGET = "Color1"

-- Off by default. This writes a colour the player really sees, and a script that repainted the road
-- the moment it was installed would be a rude default. Flip it and press Reload in the Scripts tab.
local WRITE = false

-- Components are 0..1.
local VALUE = { 0.1, 1.0, 0.2 }

-- The write goes into plain values, which means **it persists after this script stops writing**.
-- Turning the script off does not put the old colour back. Set this instead to write the original
-- back and leave the game as it was.
local RESTORE = false

local target = tw.vector_ch(GROUP, TARGET)

local original = nil    -- captured before the first write, so there is something to restore to
local wrote = false
local last_write = { 0, 0, 0 }

-- Statistics over a window rather than a verdict per frame.
--
-- The first version printed "the write stuck" or "something is pulling it back" from *this frame's*
-- drift, which was useless for the thing it was measuring: when a write survives on some frames and
-- not others, that line changes every frame, and at 100+ Hz two different strings at the same
-- position blur into unreadable overlap. The instrument was destroying its own reading.
--
-- Counting instead. A ratio is stable to look at and says strictly more: not just "does it hold" but
-- "how often".
local WINDOW = 120
local samples, held = 0, 0
local drift_min, drift_max = math.huge, 0

tw.on_frame(function()
    if not tw.engine_ready() then return end

    local r, g, b = target:get()

    -- Captured on the first frame the channel is readable, before anything is written to it.
    if original == nil and r ~= nil then
        original = { r, g, b }
    end

    if RESTORE and original then
        wrote = target:set(original[1], original[2], original[3])
        last_write = original
    elseif WRITE then
        -- Every frame rather than once: whether that is necessary is exactly what the drift line
        -- below is here to answer. If drift reads zero, once would have been enough.
        wrote = target:set(VALUE[1], VALUE[2], VALUE[3])
        last_write = VALUE
    end

    -- Bottom-left. Deliberately not the top-left of the safe area: that corner is where the default
    -- "somewhere uncluttered" formula puts things, and _groups.lua is already there - two scripts
    -- using the same anchor draw straight through each other.
    local base = tw.hud.font_size()
    local row = base + 5
    local x0, _, _, y1 = tw.hud.safe()
    local x = x0 + 24
    local y = y1 - 24 - row * 4

    tw.hud.text(x, y, string.format("vector write probe - %s::%s", GROUP, tostring(TARGET)), tw.theme("text_primary"))

    if r == nil then
        tw.hud.text(x, y + row, "channel not reachable yet", tw.theme("text_faint"))
        return
    end

    local function swatch(cy, cr, cg, cb)
        tw.hud.rect(x, cy + 3, x + 18, cy + 3 + base * 0.7, tw.rgb(cr, cg, cb), 3)
    end

    swatch(y + row, r, g, b)
    tw.hud.text(x + 26, y + row, string.format("read   %.3f %.3f %.3f", r, g, b), tw.theme("text_secondary"))

    if not (WRITE or RESTORE) then
        tw.hud.text(x, y + row * 2, "not writing - set WRITE = true in vecwrite.lua and Reload", tw.theme("text_muted"))
        if original then
            tw.hud.text(x, y + row * 3, string.format("original %.3f %.3f %.3f", original[1], original[2], original[3]),
                tw.theme("text_faint"))
        end
        return
    end

    swatch(y + row * 2, last_write[1], last_write[2], last_write[3])
    tw.hud.text(x + 26, y + row * 2, string.format("%s %.3f %.3f %.3f%s",
        RESTORE and "restore" or "wrote  ", last_write[1], last_write[2], last_write[3],
        wrote and "" or "  (not resolved)"), tw.theme("accent_text"))

    -- `r,g,b` was read at the top of this frame, before this frame's write - so it is what survived
    -- from last frame's write, which is exactly the question.
    local drift = math.abs(r - last_write[1]) + math.abs(g - last_write[2]) + math.abs(b - last_write[3])

    samples = samples + 1
    if drift < 0.001 then held = held + 1 end
    if drift < drift_min then drift_min = drift end
    if drift > drift_max then drift_max = drift end

    -- Restart the window rather than sliding it: a sliding average hides a regime change, and going
    -- in and out of a run is exactly when the answer is expected to change.
    if samples >= WINDOW then
        samples, held = 0, 0
        drift_min, drift_max = math.huge, 0
    end

    local ratio = samples > 0 and held / samples or 0
    local verdict
    if samples < 10 then
        verdict = "sampling"
    elseif ratio > 0.99 then
        verdict = "holds"
    elseif ratio < 0.01 then
        verdict = "always overwritten"
    else
        verdict = "contested - the value is re-derived on some frames"
    end

    tw.hud.text(x, y + row * 3, string.format("held %3d/%-3d  %3.0f%%   drift %.2f..%.2f   %s",
        held, samples, ratio * 100, drift_min == math.huge and 0 or drift_min, drift_max, verdict),
        ratio > 0.99 and tw.theme("text_secondary") or tw.theme("text_warning"))
end)
