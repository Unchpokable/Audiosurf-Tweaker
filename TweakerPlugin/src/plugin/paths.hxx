#pragma once

// Where everything the plugin reads or writes lives. See Docs/Internal/plugin-offline-mode.md, §4.1/§4.4.
//
//   engine\QuestViewer.exe          <- the anchor
//   engine\channels\TweakerPlugin.dll
//   engine\TweakerStuff\
//       Config\                     overlay.cfg, scripts.cfg
//       SkyboxReplacer\             module.json, Skyboxes\, SkyConfigs\
//       Scripts\                    *.lua
//       Logs\
//       DISABLE                     optional: the plugin stays inert when this file exists
//
// Anchored to the game's executable rather than to the working directory or to the DLL. The game
// moves its working directory at runtime (Unicode_CurrentDirectory, GetOpenFileName without
// OFN_NOCHANGEDIR), and the DLL may sit in engine\channels\ or, when injected, in the Tweaker's own
// folder - while QuestViewer.exe is in engine\ either way.
//
// Computed once by resolve(), in DllMain; everything after that is a read of immutable state, safe from any
// thread without a lock.
namespace tw::plugin::paths
{
// Resolves every path. Touches nothing on disk and logs nothing, so it can run in DllMain before the
// DISABLE check. False when the executable's own path cannot be read, in which case every getter below
// returns an empty path.
bool resolve() noexcept;

// Creates the directories that do not exist yet. Startup thread, after resolve().
void create_directories() noexcept;

// The game's executable - what DllMain compares against QuestViewer.exe.
[[nodiscard]] const std::filesystem::path& executable_file() noexcept;
[[nodiscard]] const std::filesystem::path& engine_root() noexcept;
[[nodiscard]] const std::filesystem::path& stuff_root() noexcept;
[[nodiscard]] const std::filesystem::path& config_dir() noexcept;
[[nodiscard]] const std::filesystem::path& skybox_replacer_root() noexcept;
[[nodiscard]] const std::filesystem::path& skyboxes_dir() noexcept;
[[nodiscard]] const std::filesystem::path& sky_configs_dir() noexcept;
[[nodiscard]] const std::filesystem::path& scripts_dir() noexcept;
[[nodiscard]] const std::filesystem::path& logs_dir() noexcept;

// config_dir() / name.
[[nodiscard]] std::filesystem::path config_file(std::wstring_view name);

// skybox_replacer_root() / "module.json" - the Skybox Replacer's own settings, as opposed to the
// per-sky files under sky_configs_dir().
[[nodiscard]] std::filesystem::path skybox_module_config();

// Whether TweakerStuff\DISABLE exists.
[[nodiscard]] bool is_disabled() noexcept;

// UTF-8 in and out, without the exceptions std::filesystem::path::string() throws for anything the
// ANSI code page cannot represent. Every name that leaves the file system - an id in a settings file, a
// label in the overlay - goes through these.
[[nodiscard]] std::string to_utf8(const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path from_utf8(std::string_view text);
} // namespace tw::plugin::paths
