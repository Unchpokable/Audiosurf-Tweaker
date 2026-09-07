-- @name        Array Write Probe
-- @author      Audiosurf Tweaker
-- @version     1.0
-- @description Checks that writing a table row works, and that an out-of-range row is refused
--
-- A measuring script, not an effect. It exists because two claims about table writes can only be
-- confirmed inside the running game, and one of them is a safety guard whose failure is silent.
--
--
-- WHAT THE OFFLINE TESTS ALREADY COVER, AND WHY THAT IS NOT ENOUGH
--
-- harness/lua/arraytest.cxx drives the real write path against fake channel objects and pins down
-- every branch: the type gate, the per-type member offset, the bounds check going through GetRow
-- rather than GetRowOrCreate, the cursor being restored, the memo being invalidated after the write
-- and not before. It catches a wrong slot constant immediately.
--
-- What it cannot catch is the layout being wrong in the first place. The fakes are built to the same
-- offsets the code reads - +0xa4 / +0xb0 for the connect item, slots 0/1/13/18 on it - so if those
-- numbers are wrong about the real Aco_Array_Value, the harness agrees with the mistake. Only the
-- game can disagree.
--
--
-- THE TWO CLAIMS
--
--   1. Writing a row of an `Array Value` column works. Nothing in the shipped scripts does this yet -
--      particles.lua writes a vector column - so this path has never run against the game.
--
--   2. A row index past the end is REFUSED, not created. This is the one that matters. The engine's
--      own write path creates a missing row (engine journal §2.2.3), which would lengthen a table
--      the rest of the game reads, with no error and no crash and no visible link to the cause. The
--      check below is the guard against that, and this script is how we find out it is real.
--
--
-- SAFETY
--
-- Writing is off by default. Turn it on deliberately.
--
-- The in-range write is a **write and restore**: the original value is read, a sentinel is written,
-- read back, and the original put straight back the same frame. The out-of-range attempts should
-- write nothing at all - and if they do write something, the row count changing is exactly how we
-- find out.
--
-- The target is a summary column the game fills at the end of a run and nothing reads mid-run, which
-- is about as harmless as a real table gets. It is still a real table. Do not point this at
-- something the game is actively using without thinking about it first.

local WRITE = false -- set to true and Reload to actually write

local GROUP  = "StatCollector"
local COLUMN = "Stats: TrafficColorCounts"
local CURSOR = "Index_TrafficColorCounts"

local counts = tw.array(GROUP, COLUMN, CURSOR)

local SENTINEL = 1234.5

-- Results are latched, not recomputed per frame. A probe whose readout changes every frame is
-- unreadable at 180 Hz and destroys its own measurement - this project has made that mistake once.
local done = false
local lines = {}

local function record(ok, text)
    lines[#lines + 1] = { ok = ok, text = text }
end

local function run()
    local rows = counts:rows()
    if not rows then
        record(nil, "table not reachable yet")
        return false
    end

    record(nil, string.format("rows() = %d", rows))

    if rows < 1 then
        record(nil, "table is empty - nothing to write to yet")
        return false
    end

    if not WRITE then
        record(nil, "not writing - set WRITE = true and Reload")
        return true
    end

    if not tw.can_write() then
        record(nil, "write gate still shut - waiting")
        return false
    end

    -- Claim 1: an in-range write lands and reads back.
    local original = counts:get(0)
    if not original then
        record(false, "could not read row 0")
        return true
    end

    local wrote = counts:set(0, SENTINEL)
    local readback = counts:get(0)
    counts:set(0, original) -- put it back before anything else looks
    local restored = counts:get(0)

    record(wrote, wrote and "in-range write accepted" or "in-range write REFUSED - unexpected")
    record(readback == SENTINEL, string.format("read back %.1f (wanted %.1f)", readback or -1, SENTINEL))
    record(restored == original, string.format("original %.1f restored", original))

    -- Claim 2: out of range is refused, and - the part that actually matters - the table does not
    -- grow. A refusal that still lengthened the table would be worse than no check at all, because
    -- it would look like it worked.
    local past = counts:set(rows, SENTINEL)
    local negative = counts:set(-1, SENTINEL)
    local rows_after = counts:rows()

    record(not past, past and "row past the end was ACCEPTED - the guard is not working" or "row past the end refused")
    record(not negative, negative and "negative row was ACCEPTED" or "negative row refused")
    record(rows_after == rows,
        rows_after == rows and "table length unchanged" or string.format("TABLE GREW: %d -> %d", rows, rows_after or -1))

    return true
end

tw.on_frame(function()
    if not done then
        -- Retry until the group is loaded and the gate opens, then latch.
        if tw.frame % 30 == 0 then
            lines = {}
            done = run()
        end
        if #lines == 0 then return end
    end

    -- Top-right. Every other shipped script computes "somewhere uncluttered" and lands bottom-left or
    -- bottom-centre; two scripts sharing an anchor draw through each other.
    local base = tw.hud.font_size()
    local row = base + 5
    local _, y0, x1, _ = tw.hud.safe()

    local title = "array write probe"
    local w = tw.hud.measure(title)
    for _, line in ipairs(lines) do
        local lw = tw.hud.measure(line.text)
        if lw > w then w = lw end
    end

    local x = x1 - w - 24
    local y = y0 + 24

    tw.hud.text(x, y, title, tw.theme("text_primary"))

    for i, line in ipairs(lines) do
        local color = tw.theme("text_muted")
        if line.ok == true then color = tw.theme("accent_text") end
        if line.ok == false then color = tw.theme("text_error") end
        tw.hud.text(x, y + row * i, line.text, color)
    end
end)
