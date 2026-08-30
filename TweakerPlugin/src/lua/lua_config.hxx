#pragma once

// Which scripts the user has turned off, persisted between sessions.
//
// A separate flat `key=value` file, same shape as ui/overlay_config and skybox/skybox_config, for
// the same reason those are separate from each other: this means nothing outside a process with a
// scripts/ folder, and folding it into the overlay's cosmetic settings would put it in a file
// shared with smoke_test.
//
// Only the exceptions are stored. A script that has never been touched is enabled, so dropping a new
// .lua into scripts/ makes it run - which is what someone who just downloaded one expects - and
// removing a script leaves at worst a stale line naming a file that no longer exists.
namespace tw::lua::config
{
// No-op when the file is missing, leaving every script enabled. `path` is remembered for save().
void load(std::string_view path);

// Writes the current set out. Called after each toggle, which is cold - a toggle is a click.
void save();

[[nodiscard]] bool enabled(std::string_view file) noexcept;
void set_enabled(std::string_view file, bool value);
} // namespace tw::lua::config
