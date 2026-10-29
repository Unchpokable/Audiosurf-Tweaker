-- Where the clouds go.
-- The generator API: https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/skyboxes/clouds.md
--
-- The engine calls place() whenever the layer is rebuilt: a knob moved, the sky was picked, or the package was
-- installed again. It owns the billboard basis, the poles and the vertex format; this decides only where each cloud
-- sits, how big it is and how it is turned.
--
-- Every tw.prop() name is a "prop" knob of the clouds layer in Config.json. Asking for one that is not declared
-- there is an error, not a nil - a manifest out of step with its script should say so at once.
--
-- The script runs in a sandbox of its own: no io, no os, no require. tw.random() is seeded from the layer's "seed",
-- so the same seed gives the same sky every time.

local DEG = math.pi / 180.0

function place()
    local count = math.floor(tw.prop("count"))
    local size = math.tan(math.max(tw.prop("size"), 0.1) * DEG)

    -- The band of sky to fill, in degrees above the horizon.
    local floor_deg = tw.prop("floor")
    local ceiling_deg = math.max(tw.prop("ceiling"), floor_deg + 1.0)

    for i = 1, count do
        -- Wider than tall, a little bigger or smaller each, and tilted a little.
        local scale = size * (0.6 + 0.8 * tw.random())
        local half_width, half_height = scale * 1.8, scale
        local roll = (tw.random() - 0.5) * 0.4

        -- How far the lowest corner hangs below the centre. The centre is kept that much above the floor, so no
        -- cloud dips under the horizon however big it is.
        local drop = math.atan(half_height * math.cos(roll) + half_width * math.abs(math.sin(roll))) / DEG
        local lowest = math.sin(math.min(floor_deg + drop, ceiling_deg) * DEG)
        local highest = math.sin(ceiling_deg * DEG)

        -- Uniform in height (y is up) and around the horizon: equal areas of sky get equal numbers of clouds. Plainly
        -- random directions bunch up and leave bald patches; the bundled sky shows a lattice that does not.
        local y = lowest + (highest - lowest) * tw.random()
        local ring = math.sqrt(math.max(0.0, 1.0 - y * y))
        local around = tw.random() * 2.0 * math.pi
        local x, z = math.cos(around) * ring, math.sin(around) * ring

        -- direction, roll in radians, half-width, half-height, which of the four tiles of the built-in bake
        tw.emit(x, y, z, roll, half_width, half_height, math.floor(tw.random() * 4))
    end
end
