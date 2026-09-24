#pragma once

// Which scripts the user has turned off, persisted between sessions.
//
// A separate flat `key=value` file (engine\TweakerStuff\Config\scripts.cfg), same shape as
// ui/overlay_config, for the same reason those are separate from each other: this means nothing
// outside a process with a Scripts\ folder, and folding it into the overlay's cosmetic settings would
// put it in a file shared with smoke_test.
//
// Only the exceptions are stored. A script that has never been touched is enabled, so dropping a new
// .lua into Scripts\ makes it run - which is what someone who just downloaded one expects - and
// removing a script leaves at worst a stale line naming a file that no longer exists.
namespace tw::lua::config
{
// No-op when the file is missing, leaving every script enabled. `path` is remembered for save().
void load(const std::filesystem::path& path);

// Writes the current set out. Called after each toggle, which is cold - a toggle is a click.
void save();

[[nodiscard]] bool enabled(std::string_view file) noexcept;
void set_enabled(std::string_view file, bool value);

// The settle window engine::state uses to decide the game has finished loading: the set of channel
// groups must have stood still for this many engine frames **and** this many milliseconds.
//
// Both, because neither is trustworthy on its own - the engine has been measured at 625 Hz during a
// load and at the screen's refresh rate in the menu, so a window counted only in frames expires
// inside the very period it exists to wait out (lua-engine-fix-roadmap.md §4.1).
//
// Here rather than hardcoded because this is the one number that could plausibly need tuning on a
// machine nobody has: a slow disk stretches loading, and a wrong value shows up as scripts starting
// slightly too early or a second of extra wait. Written back out by save(), so editing the file by
// hand survives a toggle in the Scripts tab.
//
// Zero or negative means "use the default". Read once, at startup.
[[nodiscard]] int settle_frames() noexcept;
[[nodiscard]] int settle_ms() noexcept;
} // namespace tw::lua::config
