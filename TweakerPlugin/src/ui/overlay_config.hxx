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

// Writes the file immediately. Only for teardown paths that have no next frame (plugin shutdown,
// smoke exit) - interactive call sites want request_save()/flush() below instead.
void save();

// Marks the in-memory settings dirty without touching the disk. Every setter call site in the UI
// uses this: a color picker being dragged reports a change on every single frame, and a synchronous
// open/truncate/write from inside the render loop once per frame is exactly the kind of hot-path
// I/O this project forbids (CLAUDE.md, "Performance (hot-path)").
void request_save();

// Performs the pending write, if any, and clears the dirty flag. Called once per frame from
// menu::update() on frames where no mouse button is held, so a drag collapses into a single write
// when the user lets go. No-op when nothing is dirty.
void flush();

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
