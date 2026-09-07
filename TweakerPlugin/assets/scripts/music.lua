-- @name        Music Probe
-- @author      Audiosurf Tweaker
-- @version     1.0
-- @description What the game already knows about the playing song, measured live
--
-- Diagnostic, and the first step of making the skybox react to the music.
--
-- Audiosurf runs its audio through BASS, and Quest3D's BASS plugin channels hand the result back
-- into the channel graph. sounds/VisMusic.cgr is where that lands, and it is called every frame from
-- XX_StartHere::DoMusic, which sits under Do_Gameplay - so all of this is live during a run, not
-- only in the menu.
--
-- Per frame, VisMusic does (decompiled from the .cgr, see the reversing journal):
--
--     BASS_GetFFT256 -> SpectrumHit[0..SampleCount-1]   (12 coarse bands, normalize mode 1)
--                    -> SpectrumHit256[0..255]          (full spectrum, normalize mode 2)
--     TotalSpectrumHitThisFrame = 0
--     for i in 0..SampleCount:
--         SpectrumHit[i] = max(SpectrumHit[i], 0)
--         TotalSpectrumHitThisFrame += (SpectrumHit[i] > 5 ? 0 : SpectrumHit[i])
--     TotalSpectrumHitThisFrame = min(1, TotalSpectrumHitThisFrame / Highway::maxIntensity)
--
-- So TotalSpectrumHitThisFrame is already normalised against the loudest moment of *this* song,
-- found during the pre-ride analysis. That is a better signal than anything computed from scratch:
-- it means the same 0..1 range on a quiet acoustic track and on a loud one.
--
-- The part that has to be checked in a real game rather than reasoned about is the ranges. What this
-- draws is therefore not just the current values but the lowest and highest each one has been seen
-- at. Leave it running through a whole song, then read the extremes off the screen.

local GROUP = "VisMusic"

-- Names are unique inside VisMusic for everything below - except SampleCount, which exists twice
-- (#10 and #566). That one is taken by index, because a name lookup returns whichever comes first in
-- scan order and that is not necessarily the one BASS_GetFFT256 is wired to.
local SAMPLE_COUNT_INDEX = 10

local ch = {
    level     = tw.float_ch(GROUP, "TotalSpectrumHitThisFrame"),
    time      = tw.float_ch(GROUP, "RunningTime_Seconds"),
    length    = tw.float_ch(GROUP, "SongLength"),
    remaining = tw.float_ch(GROUP, "SongPlayRemaining"),
    beat      = tw.float_ch(GROUP, "BeatHitThisFrame?"),
    beat_val  = tw.float_ch(GROUP, "CurrentBeatVal"),
    state     = tw.float_ch(GROUP, "Bass_State"),
    bands     = tw.float_ch(GROUP, SAMPLE_COUNT_INDEX),
}

-- The normaliser lives in Highway, not VisMusic, and Highway is only loaded during a run. Reading it
-- is how we find out whether a zero ever reaches that division - the shader must never be handed the
-- infinity that would come out of one.
local max_intensity = tw.float_ch("Highway", "maxIntensity")

-- Which of the three FFT branches VisMusic::Do takes this frame. Only the plain BASS one (both flags
-- zero) is wired to fill SpectrumHit256; the other two write the 12-band array alone. Neither flag
-- has a single writer anywhere in the project, so both should read a constant zero - and if they do
-- while the wide spectrum still reads empty, the branch is not the reason and the second output of
-- BASS_GetFFT256 simply is not produced.
--
-- The root group answers to two names and which one the engine is holding is not knowable from the
-- .cgr: "StartGroup" is what the cross-group records call it, "XX_StartHere" is its file. Ask for
-- both and use whichever resolves, rather than printing "--" and leaving the reader unsure whether
-- the flag is zero or the lookup missed.
local minimal_detail = tw.float_ch("Render_IndustrialTunnel", "MinimalDetail?")
local quicktime_fft_a = tw.float_ch("StartGroup", "UseQuickTimeFFT?")
local quicktime_fft_b = tw.float_ch("XX_StartHere", "UseQuickTimeFFT?")

local function quicktime_fft_value()
    local v = quicktime_fft_a:get()
    if v ~= nil then return v end
    return quicktime_fft_b:get()
end

-- Both spectra, each with its own cursor channel. tw.array saves and restores the game's cursor
-- around every read, which matters here: the cursor is shared with the loop above.
local spectrum = tw.array(GROUP, "New Table: SpectrumHit", "SpectrumIndex")
local spectrum256 = tw.array(GROUP, "SpectrumHit256", "Index_SpectrumHit256")

local MAX_BANDS = 32

local seen = {}

local function track(key, value)
    if value == nil then return nil end
    local s = seen[key]
    if s == nil then
        seen[key] = { lo = value, hi = value }
    else
        if value < s.lo then s.lo = value end
        if value > s.hi then s.hi = value end
    end
    return value
end

-- Cost, measured rather than assumed. Each array cell is a cursor save, a write, a read and a
-- restore - four virtual calls - so twelve of them should be nothing and 256 of them should be
-- visible. Knowing which is which decides what the engine is allowed to do per frame later.
local cost_ms = 0.0
local cost_peak = 0.0

local wide_lo, wide_hi, wide_sum = 0.0, 0.0, 0.0
local wide_alive = false
local wide_reads = 0    -- cells that came back as a number at all
local wide_nonzero = 0  -- cells that came back as something other than zero

local function fmt(v, digits)
    if v == nil then return "--" end
    return string.format("%." .. (digits or 3) .. "f", v)
end

tw.on_frame(function()
    if not tw.engine_ready() then
        tw.hud.text(20, 200, "Music Probe: engine not reachable yet", 0xFF6699FF)
        return
    end

    local t0 = os.clock()

    local level     = track("level",     ch.level:get())
    local time      = track("time",      ch.time:get())
    local length    = track("length",    ch.length:get())
    local remaining = track("remaining", ch.remaining:get())
    local beat      = track("beat",      ch.beat:get())
    local beat_val  = track("beat_val",  ch.beat_val:get())
    local state     = track("state",     ch.state:get())
    local mi        = track("maxIntensity", max_intensity:get())

    local band_count = ch.bands:get()
    if band_count == nil or band_count < 1 or band_count > MAX_BANDS then
        band_count = 12
    end
    band_count = math.floor(band_count)

    local bands = {}
    for i = 0, band_count - 1 do
        local v = spectrum:get(i)
        bands[i] = v
        track("band", v)
    end

    -- The 256-bin spectrum every fourth frame: enough to prove it is alive and to bound its range,
    -- without paying for 256 cursor round-trips sixty times a second while the rest is measured.
    if tw.frame % 4 == 0 then
        local lo, hi, sum, n = nil, nil, 0.0, 0
        for i = 0, 255 do
            local v = spectrum256:get(i)
            if v ~= nil then
                if lo == nil or v < lo then lo = v end
                if hi == nil or v > hi then hi = v end
                sum = sum + v
                n = n + 1
                wide_reads = wide_reads + 1
                if v ~= 0.0 then wide_nonzero = wide_nonzero + 1 end
            end
        end
        if n > 0 then
            wide_lo, wide_hi, wide_sum, wide_alive = lo, hi, sum, true
            track("wide_hi", hi)
        else
            wide_alive = false
        end
    end

    local ms = (os.clock() - t0) * 1000.0
    cost_ms = cost_ms * 0.95 + ms * 0.05
    if ms > cost_peak then cost_peak = ms end

    ----------------------------------------------------------------- drawing
    local w, h = tw.hud.size()
    local fs = tw.hud.font_size()

    local x0 = math.floor(fs * 0.8)
    local y = math.floor(h * 0.32)

    local text = 0xFFDDDDDD
    local dim = 0xFF888888
    local hot = 0xFFFFDD66
    local warn = 0xFF6699FF

    local function line(label, value, extra)
        local s = seen[label]
        local range = s and string.format("   [%s .. %s]", fmt(s.lo), fmt(s.hi)) or ""
        tw.hud.text(x0, y, string.format("%-14s %10s%s%s", label, fmt(value), range, extra or ""), text, fs)
        y = y + fs + 2
    end

    tw.hud.text(x0, y, "-- VisMusic --", hot, fs)
    y = y + fs + 4

    line("level", level, (level ~= nil and level >= 1.0) and "   CLAMPED" or "")
    line("time", time)
    line("length", length)
    line("remaining", remaining)
    line("beat", beat)
    line("beat_val", beat_val)
    line("state", state)
    line("maxIntensity", mi, (mi ~= nil and mi <= 0.0) and "   <-- DIVIDE BY ZERO" or "")

    y = y + 4
    tw.hud.text(x0, y, string.format("bands = %d    read cost %.3f ms (peak %.3f)", band_count, cost_ms, cost_peak), dim, fs)
    y = y + fs + 2

    -- Which FFT branch VisMusic took. Both flags should be zero, which is the branch that fills the
    -- wide spectrum - so a zero here next to an empty wide spectrum rules the branch out as the cause.
    local md = minimal_detail:get()
    local qt = quicktime_fft_value()
    tw.hud.text(x0, y, string.format("FFT branch: MinimalDetail? = %s   UseQuickTimeFFT? = %s   -> %s",
        fmt(md, 0), fmt(qt, 0),
        ((md or 0) == 0 and (qt or 0) == 0)
            and "plain BASS (wired to fill both spectra)" or "reduced (12 bands only)"), dim, fs)
    y = y + fs + 2

    -- Three distinct states, and they mean different things: unreadable (the channel did not
    -- resolve), readable but never anything but zero (the array exists and nothing writes it), and
    -- alive. The middle one used to render identically to the third, which is how a dead array can
    -- pass for a live one.
    if not wide_alive then
        tw.hud.text(x0, y, "SpectrumHit256: not readable - channel did not resolve", warn, fs)
    elseif wide_nonzero == 0 then
        tw.hud.text(x0, y, string.format(
            "SpectrumHit256: ALL ZERO over %d cell reads - readable, but nothing writes it", wide_reads), warn, fs)
    else
        tw.hud.text(x0, y, string.format("SpectrumHit256: lo %s  hi %s  sum %s   (%d of %d cells nonzero)",
            fmt(wide_lo), fmt(wide_hi), fmt(wide_sum, 1), wide_nonzero, wide_reads), dim, fs)
    end
    y = y + fs + 8

    -- The four groups the engine actually publishes, and the body next to the game's total.
    --
    -- Side by side on purpose: the game's total sums all twelve bands and nine of them are above
    -- 5.4 kHz, so it climbs on cymbals and hardly moves under a bass line. Watching the two disagree
    -- is the whole reason this line exists - and `body` is the number `Music gain` is set against.
    if bands[0] ~= nil and mi ~= nil and mi > 0.0 then
        local low = bands[0] / mi
        local mid = (bands[1] or 0.0) / mi
        local high = (bands[2] or 0.0) / mi
        local air = 0.0
        for i = 3, band_count - 1 do
            air = air + (bands[i] or 0.0) / mi
        end
        local body = math.min(1.0, low + mid)

        tw.hud.text(x0, y, string.format(
            "body %s  (low %s  mid %s  high %s  air %s)   vs the game's total %s",
            fmt(body), fmt(low), fmt(mid), fmt(high), fmt(air), fmt(level)), text, fs)
        y = y + fs + 2
        track("body", body)
        local s = seen.body
        if s ~= nil then
            tw.hud.text(x0, y, string.format(
                "body over this session: %s .. %s   -> a Music gain of about %.2f reaches full arc at the peak",
                fmt(s.lo), fmt(s.hi), s.hi > 0.01 and (1.0 / s.hi) or 0.0), dim, fs)
            y = y + fs + 2
        end
    end
    y = y + 6

    -- The bands as a meter. Scaled by the largest single band ever seen rather than by a guess,
    -- because the whole point of drawing this is to find out what that number is.
    local band_peak = seen.band and math.max(seen.band.hi, 1e-3) or 1.0
    local bar_w = math.floor(fs * 1.6)
    local bar_h = math.floor(fs * 6)
    local gap = 3

    for i = 0, band_count - 1 do
        local v = bands[i] or 0.0
        local k = math.min(1.0, math.max(0.0, v / band_peak))
        local bx = x0 + i * (bar_w + gap)
        tw.hud.rect(bx, y, bx + bar_w, y + bar_h, 0x40FFFFFF, 0, 0)
        tw.hud.rect(bx, y + bar_h - math.floor(bar_h * k), bx + bar_w, y + bar_h, hot, 0, 0)
    end

    y = y + bar_h + 4
    tw.hud.text(x0, y, string.format("band peak seen: %s", fmt(band_peak)), dim, fs)

    -- The level, with the beat flag as a flash behind it.
    y = y + fs + 8
    local meter_w = math.floor(w * 0.4)
    if beat ~= nil and beat > 0.5 then
        tw.hud.rect(x0 - 2, y - 2, x0 + meter_w + 2, y + fs + 2, 0x60FFFFFF, 0, 0)
    end
    tw.hud.rect(x0, y, x0 + meter_w, y + fs, 0x40FFFFFF, 0, 0)
    if level ~= nil then
        tw.hud.rect(x0, y, x0 + math.floor(meter_w * math.min(1.0, math.max(0.0, level))), y + fs, hot, 0, 0)
    end
end)
