-- Where this sky's clouds go.
--
-- The engine used to decide this. It no longer does: it owns the billboard basis, the pole handling,
-- the atlas rectangles and the vertex format - invariants rather than choices - and everything below
-- is the part that was only ever an opinion.
--
-- The engine calls place() whenever the layer is rebuilt: a knob moved, the sky was chosen, or this
-- file was saved. Every knob it reads is declared in Config.json under the `clouds` layer; asking for
-- one that is not declared is an error rather than a nil, because place() only ever runs at build
-- time and a manifest out of step with its own script should say so immediately.

local DEG = math.pi / 180.0

-- The distribution, and the one thing worth understanding before changing it.
--
-- A Fibonacci (golden angle) spiral is uniform and deterministic, and on its own it is also *visibly*
-- a spiral - the eye finds the arms at any count worth drawing. Independent random directions are not
-- the fix either: they clump and leave bald patches at these counts, which is the artefact the
-- lattice was chosen to avoid in the first place.
--
-- What works is neither: a lattice whose points have been let off their exact positions. `scatter` is
-- that displacement as a fraction of a cell radius, and a cell radius is derived rather than tuned -
-- N clumps sharing a band of solid angle 2*pi*(ceiling - floor) get pi*r^2 each. At scatter = 1 a
-- clump may reach anywhere in its own cell, which reads as random and cannot leave a hole.
local function clump_direction(index, total, floor_y, ceiling_y, scatter)
    local t = (index + 0.5) / total

    local y = floor_y + t * (ceiling_y - floor_y)
    local ring = math.sqrt(math.max(0.0, 1.0 - y * y))

    local theta = index * 2.39996323
    local ax, ay, az = math.cos(theta) * ring, y, math.sin(theta) * ring

    if scatter <= 0.0 then
        return ax, ay, az
    end

    local band = math.max(ceiling_y - floor_y, 1e-3)
    local cell = math.sqrt(2.0 * band / total)

    local angle = tw.random() * 2.0 * math.pi
    local radius = scatter * cell * math.sqrt(tw.random())

    -- The tangent basis at the anchor. The reference axis is swapped near the poles for the same
    -- reason the engine does it: no smooth non-vanishing tangent field exists on a sphere, so a fixed
    -- reference degenerates somewhere, and here is where that somewhere is handled.
    local rx, ry, rz = 0.0, 1.0, 0.0
    if math.abs(ay) > 0.99 then
        rx, ry, rz = 1.0, 0.0, 0.0
    end

    local tx, ty, tz = ry * az - rz * ay, rz * ax - rx * az, rx * ay - ry * ax
    local tl = math.sqrt(tx * tx + ty * ty + tz * tz)
    tx, ty, tz = tx / tl, ty / tl, tz / tl

    local bx, by, bz = ay * tz - az * ty, az * tx - ax * tz, ax * ty - ay * tx

    local offset = math.tan(math.min(radius, 1.4))
    local ca, sa = math.cos(angle), math.sin(angle)

    local x = ax + (tx * ca + bx * sa) * offset
    local y2 = ay + (ty * ca + by * sa) * offset
    local z = az + (tz * ca + bz * sa) * offset

    local len = math.sqrt(x * x + y2 * y2 + z * z)

    return x / len, y2 / len, z / len
end

-- Folds a direction back into the band, reflecting at each edge rather than clamping to it.
--
-- Not a refinement: the lattice reaches both edges, so half the jitter disc of an edge clump points
-- outside the band. Clamping would put every one of those on the boundary itself - a row of clumps in
-- a line along the horizon, which is a worse artefact than the spiral the jitter was added to break.
-- Reflection is measure-preserving: the density stays even right to the edge because what leaves
-- comes back.
local function fold_into(x, y, z, min_y, max_y)
    if y < min_y then
        y = min_y + (min_y - y)
    elseif y > max_y then
        y = max_y - (y - max_y)
    end

    y = math.max(min_y, math.min(max_y, y))

    local ring = math.sqrt(math.max(0.0, 1.0 - y * y))
    local horizontal = math.sqrt(x * x + z * z)

    if horizontal <= 1e-6 then
        return ring, y, 0.0
    end

    return x / horizontal * ring, y, z / horizontal * ring
end

function place()
    local count = math.floor(tw.prop("count"))
    local clumps = math.max(1, math.floor(tw.prop("clumps")))
    local spread = tw.prop("spread") * DEG
    local half_size = math.tan(math.max(tw.prop("size"), 0.05) * DEG)
    local scatter = tw.prop("scatter")
    local elongation = tw.prop("elongation")

    local floor_deg = tw.prop("floor")
    local ceiling_deg = math.max(tw.prop("ceiling"), floor_deg + 1.0)

    local floor_y = math.sin(floor_deg * DEG)
    local ceiling_y = math.sin(ceiling_deg * DEG)

    if clumps > count then
        clumps = count
    end

    -- Each clump's shape is drawn once and shared by every sprite in it, so the sprites agree about
    -- the shape they are part of. Area is preserved - one axis times `aspect`, the other divided by
    -- it - so stretching a clump does not also make it bigger.
    local clump = {}
    for c = 0, clumps - 1 do
        local aspect = 1.0 + math.max(elongation - 1.0, 0.0) * tw.random()
        local heading = tw.random() * 2.0 * math.pi
        local x, y, z = clump_direction(c, clumps, floor_y, ceiling_y, scatter)
        x, y, z = fold_into(x, y, z, floor_y, ceiling_y)

        clump[c] = { x = x, y = y, z = z, aspect = aspect, heading = heading }
    end

    for i = 0, count - 1 do
        -- Round-robin rather than contiguous blocks, so a count that does not divide evenly spreads
        -- the remainder across every clump instead of piling it into the last one.
        local anchor = clump[i % clumps]

        local ax, ay, az = anchor.x, anchor.y, anchor.z

        local rx, ry, rz = 0.0, 1.0, 0.0
        if math.abs(ay) > 0.99 then
            rx, ry, rz = 1.0, 0.0, 0.0
        end

        local tx, ty, tz = ry * az - rz * ay, rz * ax - rx * az, rx * ay - ry * ax
        local tl = math.sqrt(tx * tx + ty * ty + tz * tz)
        tx, ty, tz = tx / tl, ty / tl, tz / tl

        local bx, by, bz = ay * tz - az * ty, az * tx - ax * tz, ax * ty - ay * tx

        local hc, hs = math.cos(anchor.heading), math.sin(anchor.heading)
        local lx, ly, lz = tx * hc + bx * hs, ty * hc + by * hs, tz * hc + bz * hs
        local sx, sy, sz = ay * lz - az * ly, az * lx - ax * lz, ax * ly - ay * lx

        -- Uniform in the disc, not in the radius: sqrt() is what stops every clump having a dense
        -- core and a bare edge. It still reads as denser in the middle, because the sprites overlap
        -- more there - which is how a real one reads too.
        local angle = tw.random() * 2.0 * math.pi
        local radius = spread * math.sqrt(tw.random())

        local along = math.tan(radius * anchor.aspect) * math.cos(angle)
        local across = math.tan(radius / anchor.aspect) * math.sin(angle)

        local x = ax + lx * along + sx * across
        local y = ay + ly * along + sy * across
        local z = az + lz * along + sz * across

        local len = math.sqrt(x * x + y * y + z * z)
        x, y, z = x / len, y / len, z / len

        -- A uniform size reads as a pattern however well the positions are distributed.
        local scale = half_size * (0.55 + 0.9 * tw.random())

        -- The whole quad has to clear the floor, not just its centre - a sprite whose lower half
        -- hangs into the horizon haze is the cut-out-pasted-over-fog look the floor exists to
        -- prevent. The reach is the quad's *diagonal*, not its half-height: a corner sits at
        -- (+-scale, +-scale) in the tangent plane, so it is sqrt(2) further out than the edge.
        local margin = math.atan(scale * 1.41421356)
        local lo = math.sin(math.min(math.asin(floor_y) + margin, 1.4))
        local hi = math.sin(math.max(math.asin(ceiling_y) - margin, -1.4))

        if hi < lo then
            hi = lo
        end

        x, y, z = fold_into(x, y, z, lo, hi)

        tw.emit(x, y, z, tw.random() * 2.0 * math.pi, scale, scale, math.floor(tw.random() * 4))
    end
end

-- What the clouds are painted with.
--
-- The engine calls this once per rebuild, before place(), and hands it the tile count and size the
-- manifest declared. Every tile has to come back: a missing one would be drawn as a transparent hole
-- on whichever sprites happened to use it, which reads as a rendering bug rather than an unfinished
-- script.
--
-- What each tile carries: a *surface normal* in rgb and a *density* in alpha, both derived from the
-- same height field. The alpha gives the sprite a silhouette with lobes and gaps instead of a circle;
-- the normal makes light break over that shape instead of gliding across a smooth ball.
--
-- Three details here are load-bearing, and each is a bug that was made once:
--
--   * `radial` **subtracts** its falloff rather than scaling by it. Scaling raises the effective
--     threshold continuously towards the rim, so only the noise's peaks survive and the tile becomes
--     a ring of shreds with no body - measured at 10% covered and 0.1% solid.
--   * `normalize` cuts at a **quantile of this tile's own distribution**, not at a fixed height. The
--     field's mean floats between seeds; an absolute cut gave four tiles at 8.6 / 19.9 / 27.7 / 8.8
--     percent coverage.
--   * `normals` reads the **unclamped** height. A saturated field has no gradient, so normals taken
--     from the opacity are flat throughout the interior and violent along the outline - a flat cutout
--     with a glowing rim.
function fill(tiles, size)
    for i = 0, tiles - 1 do
        local height = tw.layer(size, size)

        tw.fbm(height, { octaves = 5, frequency = 2.8, warp = 0.6, seed = tw.seed() + i * 7919 })
        tw.radial(height, { inner = 0.60, feather = 0.36, depth = 0.50 })
        tw.normalize(height, { coverage = 0.42, band = 0.55 })

        local tile = tw.layer(size, size)
        tw.normals(tile, height, { span = 3, relief = 1.4 })
        tw.alpha(tile, height)

        tw.tile(i, tile)
    end
end
