#pragma once

#include <imgui.h>

#include <string>
#include <string_view>

// Local, cosmetic overlay settings - corner placement of notefeed/pins/watermark, menu
// geometry, theme overrides. Deliberately NOT part of TW_OVL: none of this is host state:
// see Docs/Internal/overlay-protocol.md, this stays plugin-local. Persisted to a flat
// `key=value` text file (path chosen by the caller - see load()/save()), same style as
// tw::ui::theme::from_config.
namespace tw::ui::overlay_config
{
enum class side {
    left,
    right,
};

// No-op (keeps prior in-memory values) if `path` doesn't exist or can't be parsed - callers don't
// need to check for a first-run/missing-file case themselves. Remembers `path` for save() below,
// so callers that just mutate settings (e.g. the menu's Settings tab) don't need to plumb the
// path through themselves - only the one site that calls load() needs to know it.
void load(std::string_view path);
void save();

[[nodiscard]] side feed_side() noexcept;
void set_feed_side(side value);

[[nodiscard]] side pins_side() noexcept;
void set_pins_side(side value);

[[nodiscard]] const std::string& theme_overrides() noexcept;
void set_theme_overrides(std::string text);

[[nodiscard]] ImVec2 menu_pos() noexcept;
void set_menu_pos(ImVec2 pos) noexcept;

[[nodiscard]] ImVec2 menu_size() noexcept;
void set_menu_size(ImVec2 size) noexcept;
} // namespace tw::ui::overlay_config
