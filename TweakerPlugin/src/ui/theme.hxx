#pragma once

#include <imgui.h>

#include <string_view>

namespace tw::ui::theme
{
// Accent (Dark defaults; RW — mutate at runtime / via from_config)
inline ImVec4 accent_primary {};
inline ImVec4 accent_secondary {};
inline ImVec4 accent_text {};
inline ImVec4 accent_selection {};
inline ImVec4 accent_soft {};
inline ImVec4 accent_hover {};
inline ImVec4 accent_pressed {};
inline ImVec4 accent_border {};
inline ImVec4 accent_selected {};
inline ImVec4 accent_ghost {};

// Surfaces
inline ImVec4 app_background {};
inline ImVec4 surface {};
inline ImVec4 surface_muted {};
inline ImVec4 surface_soft {};
inline ImVec4 surface_hover {};
inline ImVec4 surface_input {};
inline ImVec4 surface_row {};
inline ImVec4 surface_row_hover {};
inline ImVec4 surface_skeleton {};
inline ImVec4 surface_badge {};
inline ImVec4 surface_elevated {};
inline ImVec4 control_track_off {};
inline ImVec4 control_thumb {};

// Borders
inline ImVec4 border {};
inline ImVec4 border_subtle {};
inline ImVec4 border_strong {};
inline ImVec4 border_divider {};
inline ImVec4 border_window {};

// Text
inline ImVec4 text_primary {};
inline ImVec4 text_secondary {};
inline ImVec4 text_muted {};
inline ImVec4 text_subtle {};
inline ImVec4 text_faint {};
inline ImVec4 text_glyph_muted {};
inline ImVec4 text_on_accent {};

void apply_dark() noexcept;
void from_config(std::string_view config) noexcept;
} // namespace tw::ui::theme
