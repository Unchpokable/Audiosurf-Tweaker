-- The minimap: the track from the side, coloured by what each stretch of it is made of, the part already
-- ridden dimmed, and a dot where the player is (DESIGN.md §11). It replaces the game's own minimap and
-- its song progress bar, both of which are muted.

local colours = require("colours")
local ch = require("channels")
local track = require("track")
local map_colours = require("map_colours")

local mix = colours.mix
local player_ring, player_ring_frac = ch.player_ring, ch.player_ring_frac
local POINTS = track.points

-- How much of its colour the stretch already ridden keeps. Enough to still show the shape behind you,
-- little enough that "where am I" is answered by where the bright part starts.
local PASSED = 0.35

-- The line's vertices for the box it was last laid out in. Rebuilt when the box or the track changes,
-- which is rarely: the panel moves only to make room for a toast.
local vx, vy = {}, {}
local laid = { x = nil, y = nil, w = nil, h = nil, inset = nil, revision = -1 }

-- The two lines drawn every frame, refilled in place. tw.hud.gradient_polyline reads their length, so
-- whatever is past the end of this frame's line is cleared.
local passed_x, passed_y, passed_c = {}, {}, {}
local ahead_x, ahead_y, ahead_c = {}, {}, {}
local passed_n, ahead_n = 0, 0

local function layout(x, y, w, h, inset)
    if laid.revision == track.revision and laid.x == x and laid.y == y and laid.w == w and laid.h == h and laid.inset == inset then
        return
    end
    local height = track.height
    for i = 1, POINTS do
        vx[i] = x + (i - 1) * w / (POINTS - 1)
        vy[i] = y + inset + (1 - height[i]) * (h - inset * 2)
    end
    laid.x, laid.y, laid.w, laid.h, laid.inset, laid.revision = x, y, w, h, inset, track.revision
end

local function trim(xs, ys, cs, n, was)
    for i = n + 1, was do
        xs[i], ys[i], cs[i] = nil, nil, nil
    end
    return n
end

-- Draws the map into the box and returns the seconds of song left, or nil when there is nothing to draw
-- yet - the track still being walked, or no run.
return function(L, x, y, w, h)
    if not track.ready then
        return nil
    end
    local line = map_colours.update(tw.theme("text_muted"))
    if line == nil then
        return nil
    end

    local fade, base = L.fade, L.base
    -- Whole pixels, so ImGui can draw it with its textured lines: crisper than a fractional width.
    local thickness = math.max(2, math.floor(base * 0.14 + 0.5))
    local dot = thickness * 2
    layout(x, y, w, h, dot + 1)

    -- Where the player is, as a fractional vertex index. Before the run has started the ring is the
    -- start; a channel that cannot be read leaves the dot at the start rather than hiding the map.
    local ring = player_ring:get()
    local at = ring and (ring + (player_ring_frac:get() or 0)) or 0
    local p = track.point_at(at)
    local k = math.min(math.floor(p), POINTS - 1)
    local t = p - k

    local sx = vx[k] + (vx[k + 1] - vx[k]) * t
    local sy = vy[k] + (vy[k + 1] - vy[k]) * t
    local sc = mix(line[k], line[k + 1], t)

    -- Behind: every vertex up to k, then the exact point under the dot, so the bright part starts at the
    -- dot rather than at the nearest vertex.
    local dim = fade * PASSED
    for i = 1, k do
        passed_x[i], passed_y[i], passed_c[i] = vx[i], vy[i], tw.fade(line[i], dim)
    end
    passed_x[k + 1], passed_y[k + 1], passed_c[k + 1] = sx, sy, tw.fade(sc, dim)
    passed_n = trim(passed_x, passed_y, passed_c, k + 1, passed_n)

    -- Ahead: from the dot to the end.
    ahead_x[1], ahead_y[1], ahead_c[1] = sx, sy, tw.fade(sc, fade)
    local n = 1
    for i = k + 1, POINTS do
        n = n + 1
        ahead_x[n], ahead_y[n], ahead_c[n] = vx[i], vy[i], tw.fade(line[i], fade)
    end
    ahead_n = trim(ahead_x, ahead_y, ahead_c, n, ahead_n)

    tw.hud.gradient_polyline(passed_x, passed_y, passed_c, thickness)
    tw.hud.gradient_polyline(ahead_x, ahead_y, ahead_c, thickness)

    -- The dot in the colour of the line under it, glowing, so it reads as the head of the bright part.
    local c = tw.fade(sc, fade)
    tw.hud.rect(sx - dot, sy - dot, sx + dot, sy + dot, c, dot)
    tw.hud.glow_rect(sx - dot, sy - dot, sx + dot, sy + dot, c, dot, 0.8 * fade)

    return track.time_left(at)
end
