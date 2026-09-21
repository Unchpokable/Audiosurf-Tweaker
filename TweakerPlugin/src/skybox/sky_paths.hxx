#pragma once

// Where a sky on disk is, given its id.
//
// Every disk sky - a package, a lone .hlsl, a cross or panorama image, a folder of faces - is one entry
// directly under engine\TweakerStuff\SkyboxReplacer\Skyboxes, and its id is that entry's name. Not a
// path: the catalog only ever lists direct children, so there is nothing a longer id could name that
// the overlay would show - and an id that cannot contain a separator cannot walk out of the folder.
//
// Its own file because the catalog (turning entries into ids), the config (storing them) and the loader
// (turning them back into files) all need the same answer.
namespace tw::skybox
{
// The UTF-8 id for an entry of the Skyboxes folder: its file or directory name.
[[nodiscard]] std::string skybox_id(const std::filesystem::path& entry);

// The absolute path an id names, or an empty path when the id is not a single plain name (empty, "."
// or "..", or containing a separator, a drive colon or a control character) or when the folder is not
// resolved. Existence is not checked - the caller decides what a missing sky means, and says so.
[[nodiscard]] std::filesystem::path skybox_path(std::string_view id);
} // namespace tw::skybox
