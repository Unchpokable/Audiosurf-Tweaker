-- @name        Spectrum Calibration
-- @author      Audiosurf Tweaker
-- @version     1.0
-- @description Measures which frequencies the game's FFT bands actually cover. Needs the calibration track.
--
-- Answers one question: what frequency range is each of the game's spectrum bands?
--
-- Music cannot answer it. Every real track has energy everywhere at once, so "band 0 reacts to
-- everything" is equally consistent with band 0 being 0-2 kHz and with the game weighting it oddly.
-- A stepped tone can: one pure sine at a time, at a frequency we chose, with nothing else playing.
--
-- Play `Tools/SpectrumCalibration/Audiosurf Spectrum Calibration.wav` (generate it with
-- `make_calibration.py`) and leave this on. It records the response of all 12 bands and the peak bin
-- of the 256-bin array for every step, and prints the conclusion on screen: each band's passband in
-- Hz, and the Hz-per-bin of the wide spectrum.
--
-- Turn it off afterwards. It reads 256 array cells per frame while a step is being sampled, which is
-- cheap but pointless outside this measurement.

local GROUP = "VisMusic"

-- The schedule of the calibration track. `make_calibration.py` prints these; they must match, and
-- there is no way to hand them over automatically - this is a sandboxed script with no file access.
-- A mismatch is visible rather than silent: the heatmap goes diagonal or smears, and the step count
-- below stops lining up with the song length the game reports.
local CAL_LEAD_IN = 3.0
local CAL_STEP = 0.9
local CAL_SILENCE = 0.15
local CAL_FIRST_HZ = 20.0
local CAL_PER_OCTAVE = 6
local CAL_STEPS = 61

-- Sample the settled middle of each tone. The tone runs from CAL_SILENCE to CAL_STEP within the
-- step; staying well inside it absorbs both the raised-cosine edges and any disagreement between
-- BASS's reported play position and what the FFT is actually looking at.
local SAMPLE_FROM = CAL_SILENCE + 0.30
local SAMPLE_TO = CAL_STEP - 0.05

local BANDS = 12
local BINS = 256

-- Refuse to record against anything that is not the calibration track. Without this, one ordinary
-- song played with the script on would fill the table with nonsense that looks like data.
local EXPECTED_LENGTH = CAL_LEAD_IN + CAL_STEPS * CAL_STEP + 3.0
local LENGTH_TOLERANCE = 3.0

local ch = {
    time = tw.float_ch(GROUP, "RunningTime_Seconds"),
    length = tw.float_ch(GROUP, "SongLength"),
}

local spectrum = tw.array(GROUP, "New Table: SpectrumHit", "SpectrumIndex")
local spectrum256 = tw.array(GROUP, "SpectrumHit256", "Index_SpectrumHit256")

-- rows[step] = { bands = {…12…}, frames = n, bin = peak bin index, bin_value = its value }
local rows = {}
local recorded = 0
local wide_seen = false

local function step_hz(index)
    return CAL_FIRST_HZ * 2.0 ^ (index / CAL_PER_OCTAVE)
end

local function fmt_hz(hz)
    if hz == nil then return "--" end
    if hz >= 1000.0 then return string.format("%.2f kHz", hz / 1000.0) end
    return string.format("%.0f Hz", hz)
end

----------------------------------------------------------------------------- recording

local function record(step)
    local row = rows[step]
    if row == nil then
        row = { bands = {}, frames = 0, bin = nil, bin_value = 0.0 }
        for i = 0, BANDS - 1 do row.bands[i] = 0.0 end
        rows[step] = row
        recorded = recorded + 1
    end

    row.frames = row.frames + 1

    -- Max rather than mean across the frames of a step. The tone is steady, so the frames should
    -- agree; where they do not it is because the FFT window still held part of the silence, and the
    -- larger reading is the one that saw the whole tone.
    for i = 0, BANDS - 1 do
        local v = spectrum:get(i)
        if v ~= nil and v > row.bands[i] then row.bands[i] = v end
    end

    local best, best_at = nil, nil
    for i = 0, BINS - 1 do
        local v = spectrum256:get(i)
        if v ~= nil and (best == nil or v > best) then
            best, best_at = v, i
        end
    end
    if best ~= nil then
        wide_seen = true
        if best > row.bin_value then
            row.bin_value = best
            row.bin = best_at
        end
    end
end

----------------------------------------------------------------------------- conclusions

-- A band's passband, read off its own response curve: the steps where it reached at least half its
-- own peak. Half-power rather than a fixed threshold because the bands do not share a scale - the
-- question is where each one is sensitive, not which is loudest.
local function band_range(band)
    local peak, peak_step = 0.0, nil
    for step, row in pairs(rows) do
        local v = row.bands[band]
        if v > peak then peak, peak_step = v, step end
    end
    if peak_step == nil or peak <= 0.0 then return nil end

    local lo, hi = peak_step, peak_step
    for step, row in pairs(rows) do
        if row.bands[band] >= peak * 0.5 then
            if step < lo then lo = step end
            if step > hi then hi = step end
        end
    end
    return step_hz(lo), step_hz(hi), step_hz(peak_step), peak
end

-- Least squares of bin index against frequency. If the wide spectrum is linear in frequency - which
-- is what an unweighted FFT hands back - this fits with a residual of about one bin, and 1/slope is
-- the width of a bin in Hz. A large residual is the interesting answer, not a failed measurement.
local function bin_fit()
    local n, sx, sy, sxx, sxy = 0, 0.0, 0.0, 0.0, 0.0
    for step, row in pairs(rows) do
        if row.bin ~= nil then
            local x, y = step_hz(step), row.bin
            n = n + 1
            sx, sy = sx + x, sy + y
            sxx, sxy = sxx + x * x, sxy + x * y
        end
    end
    if n < 3 then return nil end

    local denom = n * sxx - sx * sx
    if denom == 0.0 then return nil end

    local slope = (n * sxy - sx * sy) / denom
    local intercept = (sy - slope * sx) / n

    local worst = 0.0
    for step, row in pairs(rows) do
        if row.bin ~= nil then
            local predicted = slope * step_hz(step) + intercept
            local err = math.abs(predicted - row.bin)
            if err > worst then worst = err end
        end
    end
    return slope, intercept, worst, n
end

----------------------------------------------------------------------------- drawing

local DIM = 0xFF888888
local TEXT = 0xFFDDDDDD
local HOT = 0xFFFFDD66
local WARN = 0xFF6699FF

tw.on_frame(function()
    if not tw.engine_ready() then return end

    local t = ch.time:get()
    local length = ch.length:get()

    local matches = length ~= nil and math.abs(length - EXPECTED_LENGTH) <= LENGTH_TOLERANCE

    local step = nil
    if matches and t ~= nil then
        local into = t - CAL_LEAD_IN
        if into >= 0.0 then
            local index = math.floor(into / CAL_STEP)
            local local_t = into - index * CAL_STEP
            if index >= 0 and index < CAL_STEPS and local_t >= SAMPLE_FROM and local_t <= SAMPLE_TO then
                step = index
            end
        end
    end

    if step ~= nil then
        record(step)
    end

    ------------------------------------------------------------------ header
    local w, h = tw.hud.size()
    local fs = tw.hud.font_size()
    local x0 = math.floor(fs * 0.8)
    local y = math.floor(h * 0.18)

    tw.hud.text(x0, y, "-- Spectrum Calibration --", HOT, fs)
    y = y + fs + 4

    if not matches then
        tw.hud.text(x0, y, string.format(
            "waiting for the calibration track (want %.1f s, this song is %s)",
            EXPECTED_LENGTH, length and string.format("%.1f s", length) or "--"), WARN, fs)
        y = y + fs + 2
    end

    tw.hud.text(x0, y, string.format("t = %s   step %s (%s)   recorded %d / %d",
        t and string.format("%.2f", t) or "--",
        step and tostring(step) or "-",
        step and fmt_hz(step_hz(step)) or "-",
        recorded, CAL_STEPS), DIM, fs)
    y = y + fs + 8

    ------------------------------------------------------------------ heatmap
    -- 12 rows of bands against 61 columns of frequency. Each row is scaled by its own maximum: the
    -- shape of one band's response is the question, and a shared scale would hide every band that
    -- the game happens to drive quietly.
    local cell = math.max(4, math.floor((w * 0.62) / CAL_STEPS))
    local row_h = math.max(6, math.floor(fs * 0.85))
    local map_y = y

    for band = 0, BANDS - 1 do
        local peak = 0.0
        for _, row in pairs(rows) do
            if row.bands[band] > peak then peak = row.bands[band] end
        end

        local by = map_y + band * row_h
        tw.hud.text(x0, by, string.format("%2d", band), DIM, fs * 0.8)

        for s = 0, CAL_STEPS - 1 do
            local bx = x0 + math.floor(fs * 1.8) + s * cell
            local row = rows[s]
            local k = 0.0
            if row ~= nil and peak > 0.0 then
                k = math.min(1.0, row.bands[band] / peak)
            end
            local shade = math.floor(k * 255.0)
            local colour
            if row == nil then
                colour = 0x18FFFFFF
            else
                -- Warm ramp: dark blue floor to the same yellow the rest of the overlay uses.
                colour = 0xFF000000 + shade * 0x10000 + math.floor(shade * 0.86) * 0x100 + math.floor(60 + shade * 0.4)
            end
            tw.hud.rect(bx, by, bx + cell - 1, by + row_h - 1, colour, 0, 0)
        end
    end

    y = map_y + BANDS * row_h + 2

    -- One tick per octave along the bottom.
    for s = 0, CAL_STEPS - 1, CAL_PER_OCTAVE do
        local bx = x0 + math.floor(fs * 1.8) + s * cell
        tw.hud.text(bx, y, fmt_hz(step_hz(s)), DIM, fs * 0.75)
    end
    y = y + fs + 10

    ------------------------------------------------------------------ conclusions
    tw.hud.text(x0, y, "band passbands (half-power, from the measurement above)", TEXT, fs)
    y = y + fs + 2

    for band = 0, BANDS - 1 do
        local lo, hi, at, peak = band_range(band)
        local line
        if lo == nil then
            line = string.format("  %2d   no response recorded", band)
        else
            line = string.format("  %2d   %-10s .. %-10s   peak at %-10s (%.3f)",
                band, fmt_hz(lo), fmt_hz(hi), fmt_hz(at), peak)
        end
        tw.hud.text(x0, y, line, TEXT, fs)
        y = y + fs + 1
    end

    y = y + 6
    if not wide_seen then
        tw.hud.text(x0, y, "SpectrumHit256: nothing readable - the game is on a reduced FFT path", WARN, fs)
    else
        local slope, intercept, worst, n = bin_fit()
        if slope == nil or slope == 0.0 then
            tw.hud.text(x0, y, "SpectrumHit256: not enough steps recorded yet", DIM, fs)
        else
            tw.hud.text(x0, y, string.format(
                "SpectrumHit256: bin = %.5f * Hz + %.2f  ->  %.2f Hz per bin,"
                .. " worst residual %.2f bins over %d steps",
                slope, intercept, 1.0 / slope, worst, n), TEXT, fs)
        end
    end
end)
