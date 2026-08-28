#pragma once

// Where a relative skybox path is allowed to mean something.
//
// Its own file because two modules need the same answer - the loader resolving skybox_file, and the
// catalog resolving skybox_dir - and "wherever I put the folder, it did not work" is a bug that only
// stays fixed if both agree on the roots and both say which ones they tried.
namespace tw::skybox
{
// Absolute paths are returned as-is when they exist. Relative ones are tried against, in order:
// the directory holding TweakerPlugin.dll (and its .cfg), the game's working directory (engine/),
// and its parent (the game root).
//
// Returns an empty path when nothing matched, having logged every candidate.
[[nodiscard]] std::filesystem::path resolve_source_path(std::string_view path);

// The directory TweakerPlugin.dll was loaded from; empty if it cannot be determined.
[[nodiscard]] std::filesystem::path dll_directory();
} // namespace tw::skybox
