-- Runs the real spectrum_calibration.lua against a simulated game, and checks that the conclusions
-- it prints are the ones the simulation was built to produce.
--
--     uv run --with lupa python run_tests.py     (from this directory)
--
-- The point is not that the Lua parses. It is that `band_range` and `bin_fit` measure what is
-- actually in front of them. Those two functions turn a screen full of numbers into a sentence the
-- next decision rests on ("band 0 is 0-1.8 kHz, so the 12-band split is linear"), and a reader that
-- quietly echoed its own assumptions would produce exactly the answer being looked for.
--
-- So the simulation is run twice: once with linearly spaced bands and once with logarithmic ones. A
-- reader worth trusting has to report two different answers. If both runs agree, it is not measuring.
--
-- No game and no plugin build: the script under test only ever touches `tw`, and `tw` is stubbed
-- below. It is the same file the plugin loads, read off disk, not a copy.

local SCRIPT = "../../TweakerPlugin/assets/scripts/spectrum_calibration.lua"

-- Has to match the schedule inside the script under test and inside make_calibration.py.
local LEAD_IN, STEP, FIRST_HZ, PER_OCTAVE, STEPS = 3.0, 0.9, 20.0, 6, 61
local TAIL = 3.0
local SAMPLE_RATE, FFT_BINS = 44100, 256
local HZ_PER_BIN = (SAMPLE_RATE / 2) / FFT_BINS   -- 86.13, what an unweighted FFT hands back
local BANDS = 12
local FPS = 60

local function step_hz(i)
    return FIRST_HZ * 2.0 ^ (i / PER_OCTAVE)
end

---------------------------------------------------------------------------- the fake game

-- One tone at a time, smeared over a couple of bins the way a real FFT smears it.
local function bins_for(hz)
    local out = {}
    local centre = hz / HZ_PER_BIN
    for i = 0, FFT_BINS - 1 do
        local d = (i - centre) / 1.2
        out[i] = math.exp(-d * d)
    end
    return out
end

-- Two candidate shapes for the 12-band split. `linear` is the hypothesis under test: 256 bins cut
-- into 12 equal slices, so band 0 alone covers 0-1837 Hz. `log` is what a band splitter designed
-- for music would do.
local function band_edges(shape)
    local edges = {}
    if shape == "linear" then
        for b = 0, BANDS do edges[b] = b * FFT_BINS / BANDS end
    else
        -- Starting at 100 Hz rather than 20: with 86 Hz bins the whole 20-80 Hz region lives inside
        -- bin 0, so log bands below that are empty by arithmetic and the simulation would be testing
        -- the reader against a signal no FFT of this size could produce.
        local lo, hi = 100.0, 20000.0
        for b = 0, BANDS do
            local hz = lo * (hi / lo) ^ (b / BANDS)
            edges[b] = hz / HZ_PER_BIN
        end
    end
    return edges
end

local function bands_for(bins, edges)
    local out = {}
    for b = 0, BANDS - 1 do
        local sum = 0.0
        local a, z = math.floor(edges[b] + 0.5), math.floor(edges[b + 1] + 0.5)
        for i = a, math.min(z, FFT_BINS) - 1 do
            sum = sum + (bins[i] or 0.0)
        end
        out[b] = sum
    end
    return out
end

---------------------------------------------------------------------------- the tw stub

local state = { t = 0.0, bins = {}, bands = {} }
local drawn = {}
local handlers = {}

local function make_float(name)
    return {
        get = function()
            if name == "RunningTime_Seconds" then return state.t end
            if name == "SongLength" then return LEAD_IN + STEPS * STEP + TAIL end
            return nil
        end,
    }
end

local function make_array(column)
    return {
        get = function(_, i)
            if column == "SpectrumHit256" then return state.bins[i] end
            return state.bands[i]
        end,
    }
end

tw = {
    frame = 0,
    engine_ready = function() return true end,
    float_ch = function(_, name) return make_float(name) end,
    array = function(_, column) return make_array(column) end,
    on_frame = function(fn) handlers[#handlers + 1] = fn end,
    hud = {
        size = function() return 2560, 1440 end,
        font_size = function() return 18 end,
        text = function(_, _, s) drawn[#drawn + 1] = s end,
        rect = function() end,
        line = function() end,
    },
}

---------------------------------------------------------------------------- the run

local function run(shape)
    handlers, drawn = {}, {}

    local chunk, err = loadfile(SCRIPT)
    if not chunk then
        error("could not load " .. SCRIPT .. ": " .. tostring(err))
    end
    chunk()

    local edges = band_edges(shape)
    local total = LEAD_IN + STEPS * STEP + TAIL
    local frames = math.floor(total * FPS)

    for f = 0, frames do
        state.t = f / FPS
        local into = state.t - LEAD_IN
        local index = math.floor(into / STEP)

        -- Silence outside a tone, exactly as the generated file has it.
        local playing = into >= 0.0 and index >= 0 and index < STEPS
            and (into - index * STEP) >= 0.15
        if playing then
            state.bins = bins_for(step_hz(index))
            state.bands = bands_for(state.bins, edges)
        else
            state.bins, state.bands = {}, {}
            for i = 0, FFT_BINS - 1 do state.bins[i] = 0.0 end
            for b = 0, BANDS - 1 do state.bands[b] = 0.0 end
        end

        tw.frame = f
        drawn = {}
        for i = 1, #handlers do handlers[i]() end
    end

    return drawn
end

---------------------------------------------------------------------------- the checks

local failures = 0

local function check(name, ok, detail)
    if ok then
        io.write("  ok    ", name, "\n")
    else
        io.write("  FAIL  ", name, "   ", tostring(detail), "\n")
        failures = failures + 1
    end
end

local function find(lines, pattern)
    for i = 1, #lines do
        local a, b, c = string.match(lines[i], pattern)
        if a then return a, b, c end
    end
    return nil
end

-- The reader prints "band 0   20 Hz .. 1.79 kHz   peak at ...". Pull the upper edge of band 0 back
-- out as a number of Hz, whichever unit it chose to print it in.
local function band0_upper(lines)
    for i = 1, #lines do
        local hi, unit = string.match(lines[i], "^%s*0%s+[%d%.]+ .?Hz%s+%.%.%s+([%d%.]+) (k?)Hz")
        if hi then
            return tonumber(hi) * (unit == "k" and 1000.0 or 1.0)
        end
    end
    return nil
end

for _, shape in ipairs({ "linear", "log" }) do
    io.write("== ", shape, " bands\n")
    local lines = run(shape)

    -- 1. The wide spectrum is linear in frequency in both simulations, so the fit must recover the
    --    bin width regardless of how the 12 bands were built.
    local per_bin, residual = find(lines, "([%d%.]+) Hz per bin, worst residual ([%d%.]+)")
    check("recovers the FFT bin width",
        per_bin ~= nil and math.abs(tonumber(per_bin) - HZ_PER_BIN) < 2.0,
        "got " .. tostring(per_bin) .. " Hz, wanted " .. string.format("%.2f", HZ_PER_BIN))
    check("the linear fit is tight",
        residual ~= nil and tonumber(residual) < 2.0,
        "worst residual " .. tostring(residual) .. " bins")

    -- 2. Every step contributed.
    local recorded = find(lines, "recorded (%d+) / %d+")
    check("recorded every step",
        recorded ~= nil and tonumber(recorded) == STEPS,
        "recorded " .. tostring(recorded) .. " of " .. STEPS)

    -- 3. The band shape itself. This is the discriminating check: the same reader must give two
    --    different answers for the two simulations.
    local upper = band0_upper(lines)
    if shape == "linear" then
        -- Band 0 spans bins 0..21, i.e. up to 1837 Hz. Half-power lands one 1/6-octave step either
        -- side of that, so anything from ~1.2 to ~2.2 kHz is the right answer.
        check("band 0 reaches roughly 1.8 kHz",
            upper ~= nil and upper > 1200.0 and upper < 2300.0,
            "band 0 upper edge came out at " .. tostring(upper))
    else
        -- Log bands put band 0 at 100-153 Hz - an order of magnitude narrower than the linear case,
        -- which is the whole point of running both.
        check("band 0 stops well below 300 Hz",
            upper ~= nil and upper < 300.0,
            "band 0 upper edge came out at " .. tostring(upper))
    end
end

io.write("\n")
if failures == 0 then
    io.write("PASS\n")
else
    io.write(failures, " FAILURES\n")
end

-- The launcher turns this into the process exit code; the VM is embedded, so os.exit here would
-- take the host down with it.
return failures
