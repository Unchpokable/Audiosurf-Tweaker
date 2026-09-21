#pragma once

#include "skybox/sky_catalog.hxx"

// Skybox Replacer's own settings: engine\TweakerStuff\SkyboxReplacer\module.json.
//
//   {
//     "enabled": true,
//     "sky": { "kind": "package", "id": "ODC_VolTex.sky" },
//     "cubemap": { "hdr_exposure": 1.0, "min_face_size": 0 },
//     "orientation": { "yaw_degrees": 0.0, "pitch_degrees": 0.0, "z_up": false },
//     "shader": { "quality": 100 }
//   }
//
// Module-wide values only. What a user moved on one particular sky lives in that sky's own file under
// SkyConfigs\ (see sky_settings) - a different file with a different meaning, which is why the two sit
// in different folders and a sky may be called anything, "module" included.
//
// Plugin-local and deliberately not part of TW_OVL: none of this is host state.
namespace tw::skybox::config
{
// The sky a fresh module.json selects, and the cube map the draw path falls back to when the selected
// sky is a shader or package that cannot be loaded.
constexpr std::string_view k_default_packed = "skyboxes/cloudy_01.png";

// Which sky is selected. `kind` is the catalog's own kind, so matching the selection against the
// catalog is a comparison rather than an inference.
//
// `id` depends on the kind:
//   program     - a built-in shader's id ("gradient", "night")
//   packed      - a resource key baked into the DLL ("skyboxes/cloudy_01.png")
//   package, shader_file, file, face_dir
//               - the name of one entry directly under SkyboxReplacer\Skyboxes: "ODC_VolTex.sky",
//                 "Chroma.hlsl". A single name, never a path - see sky_paths.
struct selection {
    entry_kind kind { entry_kind::packed };
    std::string id;
};

// Reads the file. Missing: the defaults are written out at once, so there is a file to find and edit.
// Unreadable or not an object: the defaults are used, and the file is left alone until something is
// changed from the overlay - rewriting it on load would silently destroy a hand edit with one typo in it.
// Unknown keys are ignored, and not written back.
void load(const std::filesystem::path& path);

[[nodiscard]] bool enabled() noexcept;
void set_enabled(bool value);

[[nodiscard]] const selection& sky() noexcept;

// Records the choice and writes the file.
void select(entry_kind kind, std::string_view id);

// Exposure for Radiance .hdr sources. Ignored for ordinary 8-bit images.
[[nodiscard]] float hdr_exposure() noexcept;

// Faces smaller than this get a Catmull-Rom upscale on load. 0 leaves the art alone. Changes the
// reconstruction filter, not the amount of detail - see cubemap_source::min_face_size.
[[nodiscard]] int min_face_size() noexcept;

// Degrees. `yaw` turns the sky about the world up axis, `pitch` tips it - both exist because the
// demo art has a sun in it and where that sun sits relative to the track is a matter of taste. Applied
// to the cube map and to shader skies alike.
[[nodiscard]] float yaw_degrees() noexcept;
[[nodiscard]] float pitch_degrees() noexcept;

// True when the game's world treats +Z as up rather than +Y, in which case the sky has to be rotated a
// quarter turn to match.
[[nodiscard]] bool z_up() noexcept;

// Percentage of the viewport a shader sky renders at, upscaled back on the way out: 100 (native), 67,
// 50 or 33. A cube map is one texture lookup and gains nothing from it.
[[nodiscard]] int shader_quality() noexcept;
void set_shader_quality(int percent);

// The spelling of a kind in module.json, and back. Unknown text is nullopt.
[[nodiscard]] std::string_view kind_key(entry_kind kind) noexcept;
[[nodiscard]] std::optional<entry_kind> parse_kind(std::string_view key) noexcept;
} // namespace tw::skybox::config
