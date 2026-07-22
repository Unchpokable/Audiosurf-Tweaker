#include "ui/widgets/popup_menu.hxx"

#include "ui/theme.hxx"
#include "ui/widgets/detail/draw.hxx"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace
{
constexpr int k_fade_ms = 200;
constexpr float k_title_pad_x = 10.f;
constexpr float k_title_pad_y = 6.f;

ImVec2 snap_px(ImVec2 p) noexcept
{
    return ImVec2 { std::floor(p.x), std::floor(p.y) };
}
} // namespace

namespace tw::ui::widgets
{
popup_menu::popup_menu(const char* id, const char* title)
    : m_id(id ? id : "popup_menu"), m_window_name(std::string("##tw_popup_") + m_id), m_title(title ? title : "")
{
}

void popup_menu::set_title(std::string_view title)
{
    m_title.assign(title.begin(), title.end());
}

void popup_menu::set_rounding(float rounding) noexcept
{
    m_rounding = rounding;
}

void popup_menu::set_inner_padding(ImVec2 padding) noexcept
{
    m_inner_padding = padding;
}

void popup_menu::retarget_opacity(float target)
{
    m_opacity_tween = tweeny::from(m_opacity_t).to(target).during(k_fade_ms).via(tweeny::easing::cubicOut);
}

void popup_menu::step_opacity()
{
    const auto dt = detail::dt_ms();
    if(!m_opacity_tween.isFinished()) {
        m_opacity_t = m_opacity_tween.step(dt);
    }
    else {
        // Snap to endpoint — finished tweens can leave a tiny epsilon that keeps opened() true forever.
        m_opacity_t = m_want_open ? 1.f : 0.f;
    }

    if(!m_want_open && m_opacity_t <= 0.001f) {
        m_opacity_t = 0.f;
        m_interactive = false;
        m_had_focus = false;
        // Keep m_frame_size so the next open() can SetNextWindowSize and avoid ImGui's default 400x400 flash.
    }
}

bool popup_menu::opened() const noexcept
{
    return m_want_open || m_opacity_t > 0.001f;
}

void popup_menu::open()
{
    m_origin = snap_px(ImGui::GetMousePos());
    clamp_origin_to_viewport(m_frame_size.x > 1.f ? m_frame_size : ImVec2 { 1.f, 1.f });

    m_want_open = true;
    m_interactive = true;
    m_had_focus = false;
    // Skip open frame + one more so the opening click (esp. RMB) does not dismiss immediately.
    m_skip_dismiss_frames = 2;
    // Pull the window to the front for the first couple of frames: the click that opens the popup
    // brings its host window forward, and a reappearing window keeps its old z-order otherwise, so
    // the reopened popup would render behind the host (looks like a one-frame flash then nothing).
    m_bring_to_front_frames = 2;

    if(m_opacity_t <= 0.001f) {
        m_opacity_t = 0.f;
    }
    retarget_opacity(1.f);
}

void popup_menu::close()
{
    if(!m_want_open && m_opacity_t <= 0.001f) {
        return;
    }

    m_want_open = false;
    m_interactive = false;
    retarget_opacity(0.f);
}

void popup_menu::clamp_origin_to_viewport(ImVec2 frame_size) noexcept
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;

    frame_size.x = std::max(frame_size.x, 1.f);
    frame_size.y = std::max(frame_size.y, 1.f);

    if(m_origin.x + frame_size.x > display.x) {
        m_origin.x = display.x - frame_size.x;
    }
    if(m_origin.y + frame_size.y > display.y) {
        m_origin.y = display.y - frame_size.y;
    }
    if(m_origin.x < 0.f) {
        m_origin.x = 0.f;
    }
    if(m_origin.y < 0.f) {
        m_origin.y = 0.f;
    }

    m_origin = snap_px(m_origin);
}

void popup_menu::poll_dismiss()
{
    if(!m_interactive || !m_want_open) {
        return;
    }

    if(m_skip_dismiss_frames > 0) {
        --m_skip_dismiss_frames;
        return;
    }

    if(m_frame_size.x <= 1.f || m_frame_size.y <= 1.f) {
        return;
    }

    const bool clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    if(clicked && !ImGui::IsMouseHoveringRect(m_frame_min, m_frame_max, false)) {
        close();
    }
}

void popup_menu::paint_chrome(ImVec2 frame_min, ImVec2 frame_max) const
{
    float rounding = m_rounding;
    rounding = std::clamp(rounding, 0.f, std::min(frame_max.x - frame_min.x, frame_max.y - frame_min.y) * 0.5f);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Body: muted surface (darker than elevated) so button/list_item press/hover accents stay readable.
    detail::add_rect_filled_rounded(draw, frame_min, frame_max, detail::to_u32(theme::surface), rounding);
    detail::add_rect_glow(draw, frame_min, frame_max, rounding, theme::accent_primary, 0.35f);
    draw->AddRect(frame_min, frame_max, detail::to_u32(theme::accent_border), rounding);

    if(m_title_h <= 0.f || m_title.empty()) {
        return;
    }

    const ImVec2 title_max { frame_max.x, frame_min.y + m_title_h };
    draw->AddRectFilled(frame_min, title_max, detail::to_u32(theme::accent_soft), rounding, ImDrawFlags_RoundCornersTop);
    // Flat bottom edge of the title band (cover the rounded bottom of the top-only fill).
    draw->AddRectFilled(
        ImVec2 { frame_min.x, title_max.y - rounding }, ImVec2 { frame_max.x, title_max.y }, detail::to_u32(theme::accent_soft));
    draw->AddLine(
        ImVec2 { frame_min.x + 1.f, title_max.y }, ImVec2 { frame_max.x - 1.f, title_max.y }, detail::to_u32(theme::accent_border));

    const ImVec2 title_size = ImGui::CalcTextSize(m_title.c_str());
    const ImVec2 title_pos = snap_px(ImVec2 {
        frame_min.x + k_title_pad_x,
        frame_min.y + (m_title_h - title_size.y) * 0.5f,
    });
    draw->AddText(title_pos, detail::to_u32(theme::accent_text), m_title.c_str());
}

void popup_menu::begin()
{
    assert(!m_active && "popup_menu::begin() called while already active");

    m_skipped = false;
    m_window_begun = false;
    m_alpha_pushed = false;

    if(!opened()) {
        m_skipped = true;
        return;
    }

    m_active = true;
    step_opacity();
    poll_dismiss();

    // NB: do not bail out here even if step_opacity()/poll_dismiss() just drove opacity to zero or
    // requested a close. The caller has already committed to submitting content between begin()/end()
    // (it only checks opened() once, before begin()); returning without ImGui::Begin() would leave
    // that content to spill into ImGui's implicit fallback "Debug" window - a stray default-styled
    // window that flashes for a frame. Instead we always open the window; a fully-faded frame simply
    // renders at alpha 0 (invisible) and the caller stops calling begin() next frame.

    m_title_h = 0.f;
    if(!m_title.empty()) {
        m_title_h = ImGui::CalcTextSize(m_title.c_str()).y + k_title_pad_y * 2.f;
    }

    if(m_frame_size.x > 1.f && m_frame_size.y > 1.f) {
        clamp_origin_to_viewport(m_frame_size);
    }

    ImGui::SetNextWindowPos(m_origin, ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                             | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground
                             | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if(m_frame_size.x > 1.f && m_frame_size.y > 1.f) {
        ImGui::SetNextWindowSize(m_frame_size, ImGuiCond_Always);
    }
    else {
        // First show (no measured size yet): auto-size to content — never fall through to ImGui's ~400x400 default.
        flags |= ImGuiWindowFlags_AlwaysAutoResize;
    }
    if(!m_interactive) {
        flags |= ImGuiWindowFlags_NoInputs;
    }

    // Bring the popup above its host for the first frames after open() (see m_bring_to_front_frames).
    // SetNextWindowFocus overrides NoFocusOnAppearing's "keep old z-order" behaviour for this frame.
    if(m_bring_to_front_frames > 0) {
        ImGui::SetNextWindowFocus();
        --m_bring_to_front_frames;
    }

    // Belt-and-suspenders with NoBackground: kill stock WindowBg/Border for the Begin() call itself.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, m_rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 { 0.f, 0.f });
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4 { 0.f, 0.f, 0.f, 0.f });
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4 { 0.f, 0.f, 0.f, 0.f });
    ImGui::Begin(m_window_name.c_str(), nullptr, flags);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    m_window_begun = true;

    if(m_interactive && m_want_open && m_skip_dismiss_frames == 0) {
        if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
            m_had_focus = true;
        }
        else if(m_had_focus) {
            close();
        }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * m_opacity_t);
    m_alpha_pushed = true;

    const ImVec2 win_pos = ImGui::GetWindowPos();

    // Paint chrome first (previous-frame size) so content never sits on a stock ImGui surface.
    if(m_frame_size.x > 1.f && m_frame_size.y > 1.f) {
        ImVec2 frame_min = snap_px(win_pos);
        ImVec2 frame_max = snap_px(ImVec2 { win_pos.x + m_frame_size.x, win_pos.y + m_frame_size.y });
        paint_chrome(frame_min, frame_max);
    }

    const float content_top = win_pos.y + m_title_h + m_inner_padding.y;
    ImGui::SetCursorScreenPos(ImVec2 { win_pos.x + m_inner_padding.x, content_top });
    ImGui::BeginGroup();
}

void popup_menu::end()
{
    if(m_skipped) {
        m_skipped = false;
        return;
    }

    assert(m_active && "popup_menu::end() called without matching begin()");

    ImGui::EndGroup();

    ImVec2 content_min = ImGui::GetItemRectMin();
    ImVec2 content_max = ImGui::GetItemRectMax();
    if(content_max.x < content_min.x || content_max.y < content_min.y) {
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        content_min = cursor;
        content_max = cursor;
    }

    const ImVec2 win_pos = ImGui::GetWindowPos();
    ImVec2 frame_min = win_pos;
    ImVec2 frame_max { content_max.x + m_inner_padding.x, content_max.y + m_inner_padding.y };

    // Title can be wider than content — expand frame so the header label isn't clipped.
    if(m_title_h > 0.f && !m_title.empty()) {
        const ImVec2 title_size = ImGui::CalcTextSize(m_title.c_str());
        const float title_right = win_pos.x + k_title_pad_x + title_size.x + k_title_pad_x;
        if(title_right > frame_max.x) {
            frame_max.x = title_right;
        }
    }

    if(frame_max.x < frame_min.x + 1.f) {
        frame_max.x = frame_min.x + 1.f;
    }
    if(frame_max.y < frame_min.y + 1.f) {
        frame_max.y = frame_min.y + 1.f;
    }

    frame_min = snap_px(frame_min);
    frame_max = snap_px(frame_max);

    // First open frame (no prior size): chrome was skipped in begin — draw it now; next frame begin paints first.
    if(m_frame_size.x <= 1.f || m_frame_size.y <= 1.f) {
        paint_chrome(frame_min, frame_max);
    }

    m_frame_min = frame_min;
    m_frame_max = frame_max;
    m_frame_size = ImVec2 { frame_max.x - frame_min.x, frame_max.y - frame_min.y };
    clamp_origin_to_viewport(m_frame_size);

    // Claim exact content size for AlwaysAutoResize (no extra Dummy doubling).
    ImGui::SetCursorScreenPos(win_pos);
    ImGui::Dummy(ImVec2 { std::max(m_frame_size.x, 1.f), std::max(m_frame_size.y, 1.f) });

    if(m_alpha_pushed) {
        ImGui::PopStyleVar();
        m_alpha_pushed = false;
    }

    if(m_window_begun) {
        ImGui::End();
        m_window_begun = false;
    }

    m_active = false;
}
} // namespace tw::ui::widgets
