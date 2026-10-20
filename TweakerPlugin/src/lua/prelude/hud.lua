-- ---------------------------------------------------------------------------------------------------
-- tw.hud - drawing, the overlay's palette and colour helpers.
-- ---------------------------------------------------------------------------------------------------

local ffi = require("ffi")
local S = ...
local C = S.C
local tw = S.tw

local C_hud_text, C_hud_text_sized, C_hud_text_font = C.tw_hud_text, C.tw_hud_text_sized, C.tw_hud_text_font
local C_hud_text_glow, C_hud_measure, C_hud_measure_fnt = C.tw_hud_text_glow, C.tw_hud_measure, C.tw_hud_measure_font
local C_hud_rect, C_hud_rect_corner, C_hud_rect_glow = C.tw_hud_rect, C.tw_hud_rect_corners, C.tw_hud_rect_glow
local C_hud_rect_grad, C_hud_line, C_hud_icon = C.tw_hud_rect_gradient, C.tw_hud_line, C.tw_hud_icon
local C_hud_metric, C_hud_widget_rect = C.tw_hud_metric, C.tw_hud_widget_rect
local C_hud_occupied, C_hud_claim, C_hud_place, C_script_file = C.tw_hud_occupied, C.tw_hud_claim, C.tw_hud_place, C.tw_script_file
local C_font_count, C_font_name = C.tw_font_count, C.tw_font_name
local C_theme_count, C_theme_name, C_theme_color = C.tw_theme_count, C.tw_theme_name, C.tw_theme_color

local rect_out = ffi.new("float[4]")
local size_out = ffi.new("float[2]")

tw.hud = {}

-- The baked font faces, name -> index, built once here so the per-frame path is an array index
-- rather than a string compare. Same shape as the theme table below, for the same reason.
--
-- Faces are weights, not sizes: ImGui rasterizes at whatever size it is drawn, so `size` and `font`
-- are independent. nil means the default face, which is what every existing script gets.
local FONT = {}
for i = 0, C_font_count() - 1 do
    FONT[ffi.string(C_font_name(i))] = i
end

local function font_index(name)
    if name == nil then return 0 end
    local i = FONT[name]
    if i == nil then error("unknown font: " .. tostring(name), 3) end
    return i
end

-- Every face name tw.hud.text accepts.
function tw.hud.fonts()
    local out = {}
    for name in pairs(FONT) do out[#out + 1] = name end
    table.sort(out)
    return out
end

-- Colour is ImGui's packed IM_COL32 (0xAABBGGRR). Default is opaque white.
--
-- `size` is optional and in pixels; omitted means the overlay's own text size. `font` is optional
-- and names a face from tw.hud.fonts(); omitted means the default one.
function tw.hud.text(x, y, text, color, size, font)
    if font then
        C_hud_text_font(x, y, color or 0xFFFFFFFF, tostring(text), size or 0, font_index(font))
    elseif size then
        C_hud_text_sized(x, y, color or 0xFFFFFFFF, tostring(text), size)
    else
        C_hud_text(x, y, color or 0xFFFFFFFF, tostring(text))
    end
end

-- Width and height the same text would occupy. The measurement comes from ImGui, so it matches what
-- tw.hud.text actually draws - which is what makes centering and right-alignment exact rather than
-- approximate.
--
-- Pass the same `font` you will draw with. Measuring one face and drawing another is off by enough
-- to be visible in anything right-aligned, and nothing can catch that for you.
function tw.hud.measure(text, size, font)
    if font then
        C_hud_measure_fnt(tostring(text), size or 0, font_index(font), size_out)
    else
        C_hud_measure(tostring(text), size or 0, size_out)
    end
    return size_out[0], size_out[1]
end

-- Text with a soft glow behind it, in the overlay's own glow style. Draws the text as well - the
-- glow is offset copies of the same glyphs, so splitting it in two would rasterize them twice.
--
-- `glow` defaults to the text colour, which is the common case: a colour that glows in its own hue.
function tw.hud.glow_text(x, y, text, color, glow, size, font, strength)
    color = color or 0xFFFFFFFF
    C_hud_text_glow(x, y, color, glow or color, tostring(text), size or 0, font_index(font), strength or 1)
end

-- The overlay's default text height. Layouts should scale off this instead of assuming a pixel size.
function tw.hud.font_size()
    return C_hud_metric(6)
end

-- Which corners tw.hud.rect rounds. A plain bitmask, so `0` honestly means "none" - unlike ImGui's
-- own flags, where zero means "all" and "none" is a set bit.
tw.hud.corners = {
    none = 0,
    top_left = 1, top_right = 2, bottom_left = 4, bottom_right = 8,
    top = 3, bottom = 12, left = 5, right = 10,
    all = 15,
}

-- A rectangle. `rounding` is the corner radius; `thickness` <= 0 (the default) fills it, anything
-- else strokes an outline. `corners` selects which corners the radius applies to and defaults to
-- all of them - it is what a bar built out of several abutting rectangles needs, so the outer ends
-- round and the internal joins stay square.
function tw.hud.rect(x0, y0, x1, y1, color, rounding, thickness, corners)
    if corners then
        C_hud_rect_corner(x0, y0, x1, y1, color or 0xFFFFFFFF, rounding or 0, thickness or 0, corners)
    else
        C_hud_rect(x0, y0, x1, y1, color or 0xFFFFFFFF, rounding or 0, thickness or 0)
    end
end

-- A soft glow around a rounded rect, in the overlay's own style. Draws only the glow - fill first,
-- then glow, which is also what lets a shape glow in a different colour than it is filled with.
function tw.hud.glow_rect(x0, y0, x1, y1, color, rounding, strength)
    C_hud_rect_glow(x0, y0, x1, y1, color or 0xFFFFFFFF, rounding or 0, strength or 1)
end

-- A rectangle filled with a two-stop linear gradient: `from` at the left edge and `to` at the right,
-- or top and bottom when `vertical` is true. Both colours carry their own alpha, so a backdrop that
-- fades out to nothing is the same call as one colour fading into another.
--
-- No rounding: the primitive underneath is one quad with per-corner colours and has no rounded form.
-- A gradient that wants a soft end gets it from the gradient.
function tw.hud.gradient_rect(x0, y0, x1, y1, from, to, vertical)
    C_hud_rect_grad(x0, y0, x1, y1, from or 0xFFFFFFFF, to or 0, vertical and 1 or 0)
end

function tw.hud.line(x0, y0, x1, y1, color, thickness)
    C_hud_line(x0, y0, x1, y1, color or 0xFFFFFFFF, thickness or 1)
end

-- One of the plugin's packed SVG icons, in a square box of `size` pixels at (x, y).
--
-- `name` is a bare stem ("feat_stealth"); the icons/ prefix and .svg extension are attached on the
-- other side of the boundary, so this reaches the icon set and nothing else. Icons are monochrome
-- and take their colour from `colour`. An unknown name draws nothing rather than raising - an icon
-- is decoration, and a script should not die because a build dropped one.
function tw.hud.icon(name, x, y, size, colour)
    C_hud_icon(tostring(name), x, y, size, colour or 0xFFFFFFFF)
end

-- The overlay's own palette, by name: tw.theme("surface"), tw.theme("text_muted"), and so on.
--
-- Read live, not cached, because the Settings colour pickers edit the theme in place - a script that
-- sampled it once would drift out of the overlay's look the moment a theme changed. The name->index
-- table is built here, once, so the per-frame path is an array index rather than a string compare.
--
-- A script's own borders, backgrounds and muted text should come from here rather than from
-- invented constants: that is the difference between a HUD that belongs to Tweaker and one that
-- merely sits on top of it.
local THEME = {}
for i = 0, C_theme_count() - 1 do
    THEME[ffi.string(C_theme_name(i))] = i
end

function tw.theme(name)
    local i = THEME[name]
    if i == nil then error("unknown theme colour: " .. tostring(name), 2) end
    return C_theme_color(i)
end

-- Every name tw.theme accepts, for a script that wants to enumerate rather than guess.
function tw.theme_names()
    local out = {}
    for name in pairs(THEME) do out[#out + 1] = name end
    table.sort(out)
    return out
end

-- Same colour with a different alpha, since the palette entry usually carries the alpha the overlay
-- wants and a script often wants the same hue at a different weight.
function tw.alpha(colour, a)
    local v = math.floor(math.min(math.max(a, 0), 1) * 255 + 0.5)
    return (colour % 16777216) + v * 16777216
end

-- Scales the alpha a colour already has, rather than replacing it. This is the one a fade wants:
-- every colour in a widget keeps its relative weight while the whole thing dissolves, which is not
-- what happens if each is forced to the same absolute alpha.
function tw.fade(colour, k)
    local a = math.floor(colour / 16777216) * math.min(math.max(k, 0), 1)
    return (colour % 16777216) + math.floor(a + 0.5) * 16777216
end

-- Packs 0..1 floats into the colour format above. Written to take exactly what a vector channel
-- hands back, so `tw.rgb(colour_ch:get())` is the whole path from the game's live palette to a
-- drawing call.
function tw.rgb(r, g, b, a)
    local function q(v)
        v = math.floor((v or 0) * 255 + 0.5)
        if v < 0 then return 0 elseif v > 255 then return 255 else return v end
    end
    return q(r) + q(g) * 256 + q(b) * 65536 + q(a == nil and 1 or a) * 16777216
end

-- Viewport size in pixels. Scripts should position against this rather than hardcoding, since the
-- game runs at whatever resolution the player picked.
function tw.hud.size()
    return C_hud_metric(0), C_hud_metric(1)
end

-- x0, y0, x1, y1 of the area free of the overlay's own always-on chrome. The coarse answer, kept for
-- scripts that only want somewhere uncluttered to sit.
function tw.hud.safe()
    return C_hud_metric(2), C_hud_metric(3), C_hud_metric(4), C_hud_metric(5)
end

local WIDGETS = { notefeed = 0, pins = 1, watermark = 2, menu = 3 }

-- Exactly where one overlay widget is, as x0, y0, x1, y1 - or nil when it is not on screen (no pins
-- are showing, the menu is closed).
--
-- These are this frame's rectangles: scripts draw after every widget has laid itself out, so a HUD
-- can sit flush against the notefeed or beside an open menu and follow it while it is dragged. And
-- because scripts draw last, the geometry is for lining up against the overlay, not for staying out
-- of its way - script output goes on top of overlay chrome, not under it.
function tw.hud.widget(name)
    local id = WIDGETS[name]
    if id == nil then error("unknown overlay widget: " .. tostring(name), 2) end
    if C_hud_widget_rect(id, rect_out) == 0 then return nil end
    return rect_out[0], rect_out[1], rect_out[2], rect_out[3]
end

-- **Sharing the screen with other scripts** (Docs/Internal/script-coexistence.md §4). Every tw.hud.*
-- drawing call is measured in C and noted against the script that made it, so a script asking where things
-- are gets an answer nobody had to declare. The rule that makes it work is the run order: a script sees the
-- scripts above it, which have drawn this frame, and the ones below make room for it.
local OCCUPIED_MAX = 256
local occupied_boxes = ffi.new("float[?]", OCCUPIED_MAX * 4)
local occupied_by = ffi.new("int[?]", OCCUPIED_MAX)

-- Chrome as tw.hud.widget names it; C hands it out as -1 - its widget number.
local CHROME = { [-1] = "notefeed", [-2] = "pins", [-3] = "watermark", [-4] = "menu" }

-- File names by script id, read once each: an id names the same file for the whole session.
local FILES = {}
local function owner_name(by)
    local name = CHROME[by] or FILES[by]
    if name == nil then
        name = ffi.string(C_script_file(by))
        FILES[by] = name
    end
    return name
end

-- What is already on screen this frame, as a list of { x0, y0, x1, y1, by }: the overlay's own chrome
-- (`by` "notefeed", "pins", "watermark", "menu"), then everything the scripts above this one drew or
-- claimed, `by` their file name - nearby pieces of one script merged into one box. Not this script's own.
-- A fresh list every call; call it once per frame and keep what you need.
function tw.hud.occupied()
    local n = C_hud_occupied(occupied_boxes, occupied_by, OCCUPIED_MAX)
    local list = {}
    for i = 0, n - 1 do
        local b = i * 4
        list[i + 1] = {
            x0 = occupied_boxes[b], y0 = occupied_boxes[b + 1], x1 = occupied_boxes[b + 2], y1 = occupied_boxes[b + 3],
            by = owner_name(occupied_by[i]),
        }
    end
    return list
end

-- Takes a box for this frame without drawing in it: a widget that fades out, blinks, or is not drawn
-- every frame keeps its place, and the scripts below do not move in and back out. Call it every frame
-- the box should stay taken; only from on_frame.
function tw.hud.claim(x0, y0, x1, y1)
    C_hud_claim(x0, y0, x1, y1)
end

local CORNERS = { top_left = 0, top_right = 1, bottom_left = 2, bottom_right = 3 }

-- Finds the free spot nearest to a corner for a w x h box and takes it, as if drawn: returns its x0, y0,
-- or nil when nothing fits. `margin` (default 8) is kept from the screen edges and from everything taken -
-- the chrome except the menu, which comes and goes, the scripts above, and this script's own earlier
-- boxes this frame, so two widgets of one script placed at one corner stack instead of overlapping.
function tw.hud.place(w, h, corner, margin)
    local where = CORNERS[corner or "top_left"]
    if where == nil then error("unknown corner: " .. tostring(corner), 2) end
    if C_hud_place(w, h, where, margin or 8, size_out) == 0 then return nil end
    return size_out[0], size_out[1]
end
