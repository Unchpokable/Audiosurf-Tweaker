#include "ui/plugins/static/notefeed.hxx"

#include "ui/image/svg.hxx"
#include "ui/kit/anim.hxx"
#include "ui/kit/paint.hxx"
#include "ui/overlay_config.hxx"
#include "ui/theme.hxx"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

// Renderer-agnostic (no D3D9/GL, no TweakerPlugin PCH) - shared with smoke_test, same convention
// as overlay_state.cxx/texture_cache.cxx (see src/ui/CMakeLists.txt).
namespace
{
constexpr float k_lifetime_ms = 3000.f;
constexpr float k_fade_in_ms = 200.f;
constexpr float k_fade_out_ms = 400.f;
// The feed's own motion, not one of the shared roles in theme::motion: a toast settling into a new
// slot is neither a hover nor a page transition.
constexpr tw::ui::theme::timing k_reflow { 220, tw::ui::theme::easing::cubic_out };
constexpr float k_row_gap = 8.f;
constexpr float k_margin = 16.f;
constexpr float k_width = 300.f;
constexpr float k_rounding = 8.f;

// Padding as a fraction of the text line height, one pixel budget spent on both axes. It has to come
// from the vertical metric on both - deriving the left/right margin from the (per-entry,
// text-dependent) box width instead would make the padding look inconsistent from one toast to the
// next.
constexpr float k_pad_ratio = 0.5f;

// Past this a toast is cut off rather than grown. Lua errors arrive with a traceback attached, and a
// toast half the screen tall is no longer a notification; the full text is in the log.
constexpr float k_max_lines = 4.f;

// How many rows reserved_rect() claims, counted in single-line toasts of the current font. See the
// comment there for why this is a fixed number.
constexpr float k_max_reserved_rows = 6.f;

// A layout change smaller than this is snapped, not animated.
constexpr float k_min_reflow_px = 0.5f;

// Reflows one toast can have in flight at once. Several toasts expiring inside k_reflow_ms of each
// other is the realistic worst case; past the cap the oldest is folded into the newest (add_reflow).
constexpr std::size_t k_max_reflows = 4;

// One in-flight layout change: its value travels from (old target - new target) to zero.
//
// Additive rather than a single position tween. A toast is drawn at its layout target plus the sum of
// its reflows, so a second change arriving mid-animation starts a curve of its own on top of the first
// one instead of restarting cubicOut from wherever the toast happened to be - which is what made the
// stack jerk when toasts expired in quick succession.
struct reflow {
    tw::ui::kit::anim::channel offset { k_reflow };
};

struct entry {
    std::string text;
    // The resource key, not a resolved ImTextureID: a toast can easily outlive a device change, and
    // the texture handle would be dangling by then (see ui/image/svg.hxx). Resolved per frame in
    // update() instead - a hash lookup.
    std::string icon_key;
    double spawn_time_ms = 0.0;

    // Wrapped (and line-capped) text size, valid for the font and size it was measured with. Measured
    // in update() rather than push(): push() can run outside an ImGui frame, where there is no font.
    const ImFont* measured_font = nullptr;
    float measured_size = 0.f;
    ImVec2 text_size;

    // Layout position relative to the top of the stack. Only meaningful once `placed`: a toast gets its
    // first target on the first update() after push().
    bool placed = false;
    float target_y = 0.f;

    std::array<reflow, k_max_reflows> reflows { };
    std::size_t reflow_count = 0;
};

std::vector<entry> g_entries;
double g_clock_ms = 0.0;

// The box around every toast the last update() drew, for last_rect(). Recorded while drawing, because a
// toast's height depends on wrapped text and its position on the stack's motion - recomputing either
// would mean running the layout twice.
float g_rect[4] { 0.f, 0.f, 0.f, 0.f };
bool g_rect_valid = false;

void measure(entry& e, ImFont* font, float font_size, float wrap_width)
{
    if(e.measured_font == font && e.measured_size == font_size) {
        return;
    }

    e.measured_font = font;
    e.measured_size = font_size;

    const ImVec2 full = ImGui::CalcTextSize(e.text.c_str(), nullptr, false, wrap_width);
    e.text_size = ImVec2 { std::min(full.x, wrap_width), std::min(full.y, k_max_lines * font_size) };
}

void advance_reflows(entry& e)
{
    std::size_t kept = 0;
    for(std::size_t i = 0; i < e.reflow_count; ++i) {
        reflow& r = e.reflows[i];
        r.offset.step();
        if(!r.offset.moving()) {
            continue;
        }
        if(kept != i) {
            e.reflows[kept] = std::move(r);
        }
        ++kept;
    }
    e.reflow_count = kept;
}

void add_reflow(entry& e, float delta)
{
    if(e.reflow_count == k_max_reflows) {
        // Full: the oldest one's remaining distance joins the new curve. The sum does not change, so
        // the drawn position does not jump - only that leftover part of the motion restarts easing.
        delta += e.reflows[0].offset.value();
        std::move(e.reflows.begin() + 1, e.reflows.begin() + static_cast<std::ptrdiff_t>(e.reflow_count), e.reflows.begin());
        --e.reflow_count;
    }

    e.reflows[e.reflow_count++].offset.restart(delta, 0.f);
}

float reflow_offset(const entry& e)
{
    float sum = 0.f;
    for(std::size_t i = 0; i < e.reflow_count; ++i) {
        sum += e.reflows[i].offset.value();
    }
    return sum;
}

float alpha_at(float age)
{
    if(age < k_fade_in_ms) {
        return age / k_fade_in_ms;
    }
    if(age > k_lifetime_ms - k_fade_out_ms) {
        return std::clamp((k_lifetime_ms - age) / k_fade_out_ms, 0.f, 1.f);
    }
    return 1.f;
}
} // namespace

namespace tw::ui::plugins::statics::notefeed
{
namespace paint = tw::ui::kit::paint;

void initialize() noexcept
{
    g_entries.clear();
    g_clock_ms = 0.0;
}

void shutdown() noexcept
{
    g_entries.clear();
}

void push(std::string_view text, std::string_view icon_resource_key)
{
    entry e;
    e.text.assign(text);
    e.icon_key.assign(icon_resource_key);
    e.spawn_time_ms = g_clock_ms;
    g_entries.push_back(std::move(e));
}

void update() noexcept
{
    const auto dt = tw::ui::kit::anim::dt_ms();
    g_clock_ms += dt;

    std::erase_if(g_entries, [](const entry& e) { return g_clock_ms - e.spawn_time_ms >= k_lifetime_ms; });

    g_rect_valid = false;
    if(g_entries.empty()) {
        return;
    }

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    const bool right = overlay_config::feed_side() == overlay_config::side::right;

    ImFont* font = ImGui::GetFont();
    const float text_h = ImGui::GetTextLineHeight();
    const float pad = text_h * k_pad_ratio;
    const float half_pad = pad * 0.5f;
    const int icon_px = static_cast<int>(std::lround(text_h));

    // Layout and drawing in one pass: a toast's target depends only on the toasts above it, and those
    // have already been measured by the time it is reached.
    float next_target = 0.f;
    const entry* previous = nullptr;
    for(entry& e : g_entries) {
        // Space for the icon is reserved by key, not by whether its texture resolved - otherwise the
        // text would rewrap, and every toast below would move, on the frame the texture shows up.
        const float icon_dim = e.icon_key.empty() ? 0.f : text_h;
        measure(e, font, text_h, k_width - pad - icon_dim);

        const float target = next_target;
        if(!e.placed) [[unlikely]] {
            // A new toast joins the stack's motion: had it existed earlier, it would have received
            // exactly the reflows the toast above it did. Without this it would sit at its final
            // target while the toast above is still travelling up, and the two would overlap.
            e.placed = true;
            e.target_y = target;
            if(previous != nullptr) {
                e.reflows = previous->reflows;
                e.reflow_count = previous->reflow_count;
            }
        }
        else {
            advance_reflows(e);
            if(target != e.target_y) {
                const float delta = e.target_y - target;
                e.target_y = target;
                if(std::abs(delta) >= k_min_reflow_px) {
                    add_reflow(e, delta);
                }
            }
        }

        const float box_w = e.text_size.x + icon_dim + pad;
        const float box_h = e.text_size.y + pad;
        next_target = target + box_h + k_row_gap;
        previous = &e;

        const float alpha = alpha_at(static_cast<float>(g_clock_ms - e.spawn_time_ms));
        const float top = k_margin + e.target_y + reflow_offset(e);
        const float origin_x = right ? viewport.x - k_margin - box_w : k_margin;

        const ImVec2 p_min { origin_x, top };
        const ImVec2 p_max { origin_x + box_w, top + box_h };

        if(!g_rect_valid) {
            g_rect[0] = p_min.x;
            g_rect[1] = p_min.y;
            g_rect[2] = p_max.x;
            g_rect[3] = p_max.y;
            g_rect_valid = true;
        }
        else {
            g_rect[0] = std::min(g_rect[0], p_min.x);
            g_rect[1] = std::min(g_rect[1], p_min.y);
            g_rect[2] = std::max(g_rect[2], p_max.x);
            g_rect[3] = std::max(g_rect[3], p_max.y);
        }

        const ImVec4 bg { theme::surface.x, theme::surface.y, theme::surface.z, theme::surface.w * 0.92f * alpha };
        const ImVec4 border { theme::border.x, theme::border.y, theme::border.z, theme::border.w * alpha };
        draw->AddRectFilled(p_min, p_max, paint::to_u32(bg), k_rounding);
        draw->AddRect(p_min, p_max, paint::to_u32(border), k_rounding);

        const float text_x = p_min.x + half_pad + icon_dim;
        const float text_y = p_min.y + half_pad;

        if(icon_dim > 0.f) {
            const ImTextureID icon = image::svg::get_resource(e.icon_key).at(icon_px);
            // Aligned to the first line, not centred in the box: next to wrapped text a centred icon
            // floats loose between lines.
            // Drawn at the size it was baked at, which is the line height rounded. The tint carries the fade.
            paint::icon(draw, icon, ImVec2 { p_min.x + half_pad, text_y }, static_cast<float>(icon_px), ImVec4 { 1.f, 1.f, 1.f, alpha });
        }

        const ImVec4 text_col { theme::text_primary.x, theme::text_primary.y, theme::text_primary.z, theme::text_primary.w * alpha };
        // Clipped to the measured height, which is where k_max_lines cuts the text off.
        const ImVec4 text_clip { text_x, text_y, p_max.x, text_y + e.text_size.y };
        draw->AddText(
            font, text_h, ImVec2 { text_x, text_y }, paint::to_u32(text_col), e.text.c_str(), nullptr, k_width - pad - icon_dim, &text_clip);
    }
}

bool last_rect(float& x0, float& y0, float& x1, float& y1) noexcept
{
    if(!g_rect_valid) {
        return false;
    }

    x0 = g_rect[0];
    y0 = g_rect[1];
    x1 = g_rect[2];
    y1 = g_rect[3];

    return true;
}

void reserved_rect(float& x0, float& y0, float& x1, float& y1) noexcept
{
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    const bool right = overlay_config::feed_side() == overlay_config::side::right;

    x0 = right ? viewport.x - k_margin - k_width : k_margin;
    x1 = x0 + k_width;

    // A fixed strip, not the whole column: reserving full height turned every layout that respects it
    // into "everything shifts sideways", and a HUD trying to sit in the bottom corner ended up nowhere
    // near it. Counted in single-line toasts of the current font, so the strip follows the font scale but
    // not what is on screen.
    const float row_h = ImGui::GetTextLineHeight() * (1.f + k_pad_ratio);
    y0 = k_margin;
    y1 = k_margin + k_max_reserved_rows * row_h + (k_max_reserved_rows - 1.f) * k_row_gap;
}
} // namespace tw::ui::plugins::statics::notefeed
