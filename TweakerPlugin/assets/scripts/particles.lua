-- @name        Uncoloured Particles
-- @author      Audiosurf Tweaker
-- @version     1.0
-- @description Stops the engine tinting collision particles, so the atlas shows its own colours
--
-- The first script that writes to a table row, and the reason table writes exist at all.
--
--
-- WHAT IT DOES
--
-- When you hit a block, the game spawns particles and tints them. The tint is not a property of the
-- particle texture - it is a per-particle vertex colour the engine multiplies the texture by. This
-- script overwrites that colour with white as each particle is born, which makes the multiply a
-- no-op and leaves whatever is in the atlas exactly as the artist drew it.
--
--
-- WHY YOU WOULD WANT THAT
--
-- This looks cosmetic and is not. It changes what a texture pack is allowed to contain.
--
-- Look at the shipped atlases: particles1.png, particles2.jpg and particles3.jpg are strictly
-- monochrome - measured chroma of exactly 0.0 - while tiles.png sits at 18.1. That is not taste. It
-- is the only thing that works: any colour you put in a particle atlas gets multiplied by the
-- engine's tint, so a coloured atlas comes out as mud rather than as your palette. Docs/texturing.md
-- warns about precisely this.
--
-- With the tint gone, that warning stops applying to particles. You can paint the atlas and see what
-- you painted.
--
-- So expect **whitish, washed-out particles** the moment you enable this with the stock textures.
-- That is the success condition, not a bug: it means the modulation is off and the colour now comes
-- from the file. Pair it with a coloured particle atlas and you get your own palette instead.
--
--
-- HOW THE COLOUR IS PRODUCED
--
-- Inside Actors/Debris.cgr:
--
--   Set Vector #48   writes  Debris: Color[Index_Debris] := <switch on CollisionColor>
--   Debris: Color    #47     an Array Vector column, one row per live particle
--   Index_Debris     #15     the cursor saying which row #48 is currently filling
--
-- The Tune_Forest channels that draw the particles read that column through port 8. So the write has
-- already happened by the time anything looks at it, and the smallest correct intervention is to run
-- immediately after #48 and put the row back to white.
--
-- Everything here is addressed **by index**, and that is not laziness. `Set Vector` is a generic
-- name; Debris.cgr contains many channels called that, and a lookup by name finds whichever comes
-- first, which is not this one. Where the index is the only precise address, the index is what we
-- use - at the cost of being tied to this build of the game.
--
--
-- WHY NOT JUST MUTE #48
--
-- Muting the writer stops the column being updated at all. Rows for particles spawned afterwards are
-- never filled, so they stay at zero - and zero is black, not white. Muting gives black particles;
-- this gives untinted ones. Different outcomes, and only one of them is the goal.
--
-- The genuinely tidy fix is to unplug port 8 from the Tune_Forest channels entirely, at which point
-- the engine skips the vertex-colour buffer, drops D3DFVF_DIFFUSE and defaults the diffuse to white
-- on its own. That needs an API for detaching a channel's input, which does not exist yet. Until it
-- does, this costs one vector write per spawned particle - and particles are spawned per collision,
-- not per frame, so the cost is small and bounded by how hard you are playing.

local GROUP  = "Debris.cgr"
local COLUMN = 47  -- Debris: Color, Array Vector
local CURSOR = 15  -- Index_Debris
local WRITER = 48  -- Set Vector that fills the row

local colors = tw.array_vec(GROUP, COLUMN, CURSOR)
local cursor = tw.float_ch(GROUP, CURSOR)

-- Counters, so that "is this doing anything" has an answer that does not depend on squinting at
-- particles. Writes that were refused are counted separately, because the two failures mean
-- different things: refused means the row was not there (or the write gate is still shut), while
-- simply never being called means the hook did not attach.
local written = 0
local refused = 0
local calls = 0

tw.on_call(GROUP, WRITER, "after", function()
    calls = calls + 1

    local row = cursor:get()
    if not row then return end

    -- White, not "no colour". The engine multiplies, so 1,1,1 is the identity.
    if colors:set(row, 1, 1, 1) then
        written = written + 1
    else
        refused = refused + 1
    end
end)

-- One line, bottom-centre, only while something is actually happening.
--
-- Centred rather than corner-anchored on purpose: every other shipped script computes "somewhere
-- uncluttered" the same way and lands in the same place - the bottom-left block already holds
-- _groups.lua and vecwrite.lua, and two scripts sharing an anchor draw straight through each other.
-- This is a mistake this project has made once already.
--
-- Also deliberately not a per-frame readout of the last value written: a number that changes every
-- frame blurs into an unreadable smear at high frame rates. Totals stay legible.
tw.on_frame(function()
    if calls == 0 then return end

    local x0, _, x1, y1 = tw.hud.safe()

    local rows = colors:rows()
    local text = string.format("PARTICLES  untinted %d / %d%s   table %s", written, calls,
        refused > 0 and string.format("  (%d refused)", refused) or "",
        rows and tostring(rows) or "?")

    local w, h = tw.hud.measure(text)
    tw.hud.text((x0 + x1) * 0.5 - w * 0.5, y1 - h - 24, text, tw.theme("text_muted"))
end)
