#pragma once

// Skybox Replacer settings. Plugin-local and file-backed, deliberately not part of TW_OVL: this is
// a prototype with no host-side UI yet, so the file next to the DLL is the whole control surface.
//
// Flat `key=value`, same shape as ui/overlay_config, but a separate file and a separate module -
// overlay_config is shared with smoke_test and is about overlay cosmetics, while none of this means
// anything outside a real game process.
namespace tw::skybox::config
{
// No-op if the file is missing or unreadable, leaving the defaults in place. `path` is remembered
// for save().
void load(std::string_view path);

// Writes the current values back. Called on shutdown and after a runtime toggle, both cold.
void save();

[[nodiscard]] bool enabled() noexcept;
void set_enabled(bool value) noexcept;

// Packed-resource key of the cross cube map to use, e.g. "skyboxes/cloudy_01.png".
[[nodiscard]] const std::string& skybox_key() noexcept;

// When non-empty, overrides skybox_key(): a path to a cross image, or to a directory holding six
// square face images. Relative paths resolve against the DLL's own directory.
//
// This is how a high-resolution sky gets in. The bundled art is 512px per face, which a modern
// screen magnifies four to eight times over - baking anything sharper into the DLL is not an
// option (a 2048px-per-face cross is an 8192x6144 image), so past that point the art lives on disk.
[[nodiscard]] const std::string& skybox_file() noexcept;

// Faces smaller than this get a Catmull-Rom upscale on load. 0 leaves the art alone. Changes the
// reconstruction filter, not the amount of detail - see cubemap_source::min_face_size.
[[nodiscard]] int min_face_size() noexcept;

// Exposure for Radiance .hdr sources. Ignored for ordinary 8-bit images.
[[nodiscard]] float hdr_exposure() noexcept;

// Folder scanned for user-supplied skyboxes, listed alongside the packed ones in the overlay's
// Skybox tab. Each direct child counts as one entry: an image file (cross or panorama), or a
// subfolder holding six faces. Empty disables the scan. Resolved like skybox_file.
//
// Distinct from skybox_file, which names the *one* skybox in use: this is the browse root.
[[nodiscard]] const std::string& skybox_dir() noexcept;

// Records the skybox the user picked and writes the file. Exactly one of the two keys is
// meaningful at a time, so this sets both together rather than leaving the caller to remember that
// a non-empty skybox_file silently wins - which is the kind of half-updated state that makes a
// settings file lie about what is loaded.
void select_packed(std::string_view resource_key);
void select_file(std::string_view path);

// Degrees. `yaw` turns the sky about the world up axis, `pitch` tips it - both exist because the
// demo art has a sun in it and where that sun sits relative to the track is a matter of taste.
[[nodiscard]] float yaw_degrees() noexcept;
[[nodiscard]] float pitch_degrees() noexcept;

// Id of the sky program to run (see skybox/sky_program), or empty for the cube map path.
//
// A program computes the sky per pixel instead of sampling an image, so it is sharp at any
// resolution and costs no memory - see Docs/Internal/skybox-procedural.md. When this is set it wins
// over both keys above, and select_packed/select_file clear it, for the same reason those two clear
// each other: three keys that can each silently override the others is how a settings file starts
// lying about what is loaded.
[[nodiscard]] const std::string& sky_program() noexcept;
void select_program(std::string_view id);

// Percentage of the viewport the shader path renders at, upscaled back on the way out: 100 (native),
// 67, 50 or 33. Only the shader path uses it - a cube map is one texture lookup and gains nothing.
//
// A setting rather than a decision because both answers are defensible. The sky draw covers the
// whole screen with no depth rejection, so a heavy program pays for every pixel; but a light one on
// a fast card has no reason to render soft.
[[nodiscard]] int shader_quality() noexcept;
void set_shader_quality(int percent) noexcept;

// Saved values for sky program parameters, under keys of the form "param.<program>.<c0x>" (see
// sky_program::param_storage_key). Stored as text because a parameter is one float or three, and a
// generic pair of accessors is a great deal less machinery than a typed setting per knob a shader
// author might invent.
//
// Unrecognised param.* keys are kept and written back untouched: a shader that is not loaded right
// now must not lose its settings just because something else was.
[[nodiscard]] std::string_view param_value(std::string_view key) noexcept;
void set_param_value(std::string_view key, std::string_view value);

// The axis markers the probe program draws (+X/+Y/+Z dots and a ring on the horizon). Only that one
// program reads it; it lives here rather than in the program table because it is the one value
// worth flipping while looking at the screen.
[[nodiscard]] bool probe_markers() noexcept;
void set_probe_markers(bool value) noexcept;

// True when the game's world treats +Z as up rather than +Y, in which case the cube map has to be
// rotated a quarter turn to match. Exposed as a setting rather than hard-coded because it is the
// one thing about Audiosurf's coordinate system that cannot be settled without running it.
[[nodiscard]] bool z_up() noexcept;
} // namespace tw::skybox::config
