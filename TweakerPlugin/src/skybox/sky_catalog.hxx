#pragma once

// The list of skies the user can choose between: the shader programs, the cube maps baked into the
// DLL, and whatever config::skybox_dir() points at.
//
// Kept separate from sky_cubemap (which turns one chosen source into a cube texture) because the
// two answer different questions and change for different reasons - "what exists" is a directory
// scan the UI drives, "how do I load this one" is a decode.
namespace tw::skybox
{
// Declaration order is list order - refresh_catalog sorts by it, so the shader programs come first,
// then the bundled images, then whatever is on disk.
enum class entry_kind {
    program,     // a built-in shader; `id` is a sky_program id
    shader_file, // a .hlsl on disk, compiled when it is picked; `id` is a path
    packed,      // a resource baked into the DLL; `id` is a tw::resource key
    file,        // a cross or panorama image on disk; `id` is a path
    face_dir,    // a folder of six faces; `id` is a path
};

struct catalog_entry {
    std::string display_name;
    std::string id;
    entry_kind kind {};

    // Only for the two disk kinds: what the scan saw, so the tab can say "2048px, 6 faces" without
    // decoding anything. Zero when unknown (a packed entry, or a file whose header would not read).
    int face_size {};
};

// Rescans. Cheap enough to call on demand (a directory listing plus an image header per entry) but
// not per frame - the UI calls it on open and on an explicit refresh.
void refresh_catalog();

[[nodiscard]] std::span<const catalog_entry> catalog() noexcept;

// Bumped by every refresh_catalog() that changed anything, so a list widget can rebuild its rows
// only when there is something new rather than on every frame.
[[nodiscard]] unsigned int catalog_generation() noexcept;

// Index of the entry matching the current config selection, or -1 when the selection names
// something the scan did not find (a hand-edited path, a deleted folder).
[[nodiscard]] int selected_catalog_index() noexcept;
} // namespace tw::skybox
