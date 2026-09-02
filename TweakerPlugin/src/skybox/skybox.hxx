#pragma once

// Skybox Replacer: draws a cube map skybox where Audiosurf would have drawn its sky sphere.
//
// The sphere is a 2007-era stand-in for a cube map - a single equirectangular-ish PNG smeared over
// a coarse globe, with all the pole pinching and seam stretching that implies. Everything needed to
// replace it is already reachable from the plugin, and none of it needs a game file to be patched:
//
//  1. framework/texture_hook catches every Quest3D texture load and names the channel it landed in.
//     The three sky channels are "Tex_WhiteSkysphere" / "Tex_GreySkysphere" / "Tex_BlackSkysphere",
//     and this module remembers the IDirect3DTexture9 each of them currently holds. A skin swap or
//     a device reset changes that texture, and both paths funnel back through the same hook.
//  2. framework/d3d9 mirrors stage 0's texture binding and offers every draw call to an
//     interceptor. When stage 0 holds a remembered sky texture, the game is mid-sky-sphere.
//  3. skybox/sky_renderer then draws a unit cube around the camera with the packed cube map on it,
//     and the game's own draw is skipped.
//
// Prototype scope, deliberately: settings come from a file next to the DLL (skybox/skybox_config),
// the cube maps are the ones baked into the DLL's resources, and nothing is wired to TW_OVL or the
// host UI yet.
namespace tw::skybox
{
struct sky_program;

// Loads settings, subscribes to the texture hook, and attaches the device/draw listeners. Must run
// before framework::d3d9::install_d3d9_hooks() publishes those hooks to the render thread, for the
// same reason ui::initialize() does - see the note in install_d3d9_hooks().
void initialize() noexcept;

void shutdown() noexcept;

// Retry hook for framework::texture::install_texture_hook(). The Texture channel DLL is normally
// mapped long before the plugin is injected, but a very early injection can beat it, and there is
// nothing to lose by asking again once the game has a device.
void retry_texture_hook() noexcept;

[[nodiscard]] bool is_enabled() noexcept;
void set_enabled(bool value) noexcept;

// What the module is currently doing, for the overlay's Skybox tab to display. Cheap: plain reads
// of already-known values, no device calls.
struct status {
    bool enabled {};
    bool sky_detected {}; // the game's sky sphere texture has been seen at least once
    bool cubemap_ready {};
    int face_size {};     // 0 until a cube map exists
    int requested_face_size {};
    bool load_failed {};

    // The sky program in use, or null when the cube map path is. When it is set, none of the cube
    // map fields above mean anything - no image is loaded at all in that mode. See sky_program and
    // Docs/Internal/skybox-procedural.md.
    const sky_program* program {};
    std::string_view program_name;

    // The compiler's last word on the current program: empty when it compiled cleanly. Points into
    // the program itself, so it is good until the next poll_reload() - which, like this call, only
    // ever happens on the render thread.
    std::string_view program_diagnostics;

    bool program_compiling {}; // a worker thread is building it; the game keeps its own sky meanwhile

    bool probe_markers {};
    bool program_has_markers {}; // whether the current program has anything to do with the flag above

    // Percentage of the viewport the shader path renders at, upscaled back on the way out. 100 is
    // native. Only meaningful for a program - a cube map is a texture lookup and gains nothing.
    int shader_quality {};

    // Measured on the GPU around the sky draw, smoothed; 0 when the device cannot time it or
    // nothing has been drawn yet. See sky_timer for why this exists instead of watching the frame
    // rate.
    float draw_microseconds {};
};

[[nodiscard]] status current_status() noexcept;

enum class reload_outcome {
    none,
    started, // the file changed; a worker thread is compiling it
    reloaded,
    failed,  // it did not compile; the previous sky is still up
};

// Per-frame housekeeping for the running program: picks up a finished background compile, and -
// when `watch` is set - notices that its .hlsl changed and starts a new one.
//
// `watch` should be "the overlay is open". Stat-ing a file costs nothing until it does (an
// antivirus, a cold cache), and it is only worth paying while somebody is actually editing the
// shader and watching the result. Collecting a finished compile is not gated, because one started
// just before the overlay closed still has to land.
//
// The outcome is returned rather than logged because a failed edit is precisely when its author
// needs to hear about it, and the log does not exist in release builds.
//
// Rate-limited internally. Call once per frame from the render thread; a no-op for the built-in
// programs and for the cube map path.
reload_outcome poll_reload(bool watch) noexcept;

// Switches to a sky program by id (see sky_program), or back to the cube map path when `id` is
// empty. Persists to the config file, which is also where the image keys get cleared.
void select_program(std::string_view id);

// The probe program's axis markers. Persists too.
void set_probe_markers(bool value) noexcept;

// Resolution the shader path renders at, as a percentage of the viewport. Persists.
void set_shader_quality(int percent) noexcept;

// Every layer of the sky currently selected, in draw order, or empty for a cube map. A built-in or
// a lone .hlsl reports as a single layer, so callers have one shape to work with.
//
// The pointers stay valid for the life of the process; the span does not survive a change of sky.
[[nodiscard]] std::span<sky_program* const> active_layers() noexcept;

// Moves one knob of one layer. The value reaches the sky on the next draw with no recompile.
//
// What the move does depends on what backs the knob, and that is the only branch: a shader constant
// goes into that layer's constant block; a layer property is handed to whoever draws that kind of
// layer; a *shared* value goes into the sky's shared block, after which every layer re-evaluates its
// bindings - so moving one light turns the clouds as well as the sky. See sky_shared.
//
// `persist` writes it to the settings file. The overlay passes false while a slider is being
// dragged and true when it is released: a drag is hundreds of values, and a settings file rewritten
// hundreds of times is a settings file being used as a scratchpad.
void set_layer_param(int layer_index, int index, std::array<float, 3> value, bool persist);

// Puts every knob of every layer, and every shared value, back to what the manifest asked for.
//
// Its own entry point rather than a loop over set_layer_param(..., true) at the call site, because
// that loop rewrites the whole settings file once per parameter. This writes it once.
void reset_sky_params();

// Drops the current cube map so the next sky draw rebuilds it from whatever the config now names.
// The rebuild is deliberately deferred rather than done here: it decodes an image and can project a
// panorama, and the caller is the UI, drawing inside EndScene.
void request_reload() noexcept;

// Applies a catalog entry: records it in the config and requests the reload above.
void select_packed(std::string_view resource_key);
void select_file(std::string_view path);

// True once per downscale, filling `from`/`to` with the face sizes involved. The UI polls this so a
// silently softer sky becomes a visible notification - there is otherwise nothing to tell the user
// that the memory ceiling, rather than their art, decided how sharp the sky is.
[[nodiscard]] bool consume_downscale_notice(int& from, int& to) noexcept;
} // namespace tw::skybox
