#include "pch.hxx"

#include "skybox/skybox.hxx"

#include "framework/d3d9_hooks.hxx"
#include "framework/texture_hook.hxx"

#include "plugin/diagnostics.hxx"
#include "plugin/globals.hxx"

#include "skybox/sky_caps.hxx"
#include "skybox/sky_cubemap.hxx"
#include "skybox/sky_math.hxx"
#include "skybox/sky_paths.hxx"
#include "skybox/sky_program.hxx"
#include "skybox/sky_renderer.hxx"
#include "skybox/sky_shader.hxx"
#include "skybox/sky_timer.hxx"
#include "skybox/skybox_config.hxx"

namespace
{
// The three sky sphere texture channels in Render/CopyPasteBuffer.cgr are Tex_WhiteSkysphere,
// Tex_GreySkysphere and Tex_BlackSkysphere; matching on the shared "skysphere" substring rather
// than the exact three names costs nothing and survives whatever a modded group calls its own.
constexpr std::string_view k_sky_channel_marker = "skysphere";

// Three channels today. Eight leaves room for a group that adds its own without ever growing this
// into something the hot path has to iterate meaningfully.
constexpr int k_max_sky_textures = 8;

// Written from the engine thread (the texture hook), read from the render thread (the draw
// interceptor). Those are the same thread in Quest3D, but the whole point of holding raw device
// pointers behind atomics is not having to bet the process on that: on x86 these compile to plain
// loads and stores, so the guarantee is free.
std::array<Aco_DX8_Texture*, k_max_sky_textures> g_sky_channels {};
std::array<std::atomic<IDirect3DTexture9*>, k_max_sky_textures> g_sky_textures {};
std::atomic<int> g_sky_texture_count { 0 };

static_assert(std::atomic<IDirect3DTexture9*>::is_always_lock_free, "sky texture mirror is read from a draw hook");

IDirect3DCubeTexture9* g_cubemap = nullptr;

// Mirror of config::enabled(), read once per draw call. The config module's own getter is a
// cross-TU call, and this is the very first thing the interceptor does on every draw the game
// makes - the one place in this module where that distinction is worth anything.
std::atomic<bool> g_enabled { false };

// Same reasoning as g_enabled: mirrors of config values the draw interceptor reads. A non-null
// program means the sky is computed by a shader and no image is involved at all; the entries it
// points into are static, so the pointer stays valid for the life of the process.
std::atomic<tw::skybox::sky_program*> g_program { nullptr };
std::atomic<bool> g_probe_markers { true };
std::atomic<int> g_shader_quality { 100 };

// Set once a cube map load has failed for the current device, so a bad or missing asset costs one
// decode attempt rather than one per frame forever.
bool g_cubemap_failed = false;

// What the last successful build produced, for current_status(). Written on the render thread from
// ensure_cubemap, read from the same thread by the overlay - the Skybox tab draws inside EndScene.
int g_face_size = 0;
int g_requested_face_size = 0;

// The one piece of state the UI polls that is genuinely edge-triggered. Atomic because it is set
// during a draw-call interception and cleared from the overlay pass, and those are only the same
// thread by construction rather than by contract.
std::atomic<bool> g_downscale_pending { false };
int g_downscale_from = 0;
int g_downscale_to = 0;

D3DMATRIX g_orientation = tw::skybox::math::identity();

// Diagnostics only, and only interesting the first time each happens.
bool g_logged_no_channels = false;

// Rate limit for the hot-reload stat. Render thread only.
std::chrono::steady_clock::time_point g_last_reload_poll {};

void rebuild_orientation()
{
    namespace math = tw::skybox::math;

    const D3DMATRIX pitch = math::rotation_x(math::to_radians(tw::skybox::config::pitch_degrees()));
    const D3DMATRIX yaw = math::rotation_y(math::to_radians(tw::skybox::config::yaw_degrees()));

    g_orientation = math::multiply(pitch, yaw);

    if(tw::skybox::config::z_up()) {
        // Quarter turn about X, mapping the cube map's +Y-up authoring onto a world whose up axis
        // is +Z. See skybox_config::z_up for why this is a setting and not a constant.
        g_orientation = math::multiply(g_orientation, math::rotation_x(-math::k_pi * 0.5f));
    }
}

// Resolves config::sky_program() into the pointer the draw path reads. An id nothing answers to -
// a typo, or a program that existed in an older build - falls back to the cube map path rather than
// to no sky at all, and says so once.
void apply_program_from_config()
{
    const std::string& id = tw::skybox::config::sky_program();
    tw::skybox::sky_program* program = tw::skybox::find_program(id);

    // Not a built-in and not something already compiled: the id may be a path to a .hlsl the user
    // dropped in skybox_dir. Resolved through the same three roots as skybox_file, so a relative
    // path in the config works the way it does everywhere else here.
    if(program == nullptr && !id.empty()) {
        std::error_code ec;
        const std::filesystem::path path = tw::skybox::resolve_source_path(id);

        if(!path.empty() && std::filesystem::is_regular_file(path, ec)) {
            program = tw::skybox::load_file_program(path);
        }
    }

    if(program == nullptr && !id.empty()) {
        TW_LOG_WARNING("skybox: no sky program called '{}', and no such file - falling back to the cube map path", id);
    }

    g_program.store(program, std::memory_order_relaxed);
}

bool is_sky_channel_name(const char* name) noexcept
{
    if(name == nullptr) {
        return false;
    }

    // Deliberately hand-rolled rather than a std::search with a case-insensitive predicate: the
    // names are ASCII, short, and this runs a few dozen times per song start.
    for(const char* start = name; *start != '\0'; ++start) {
        std::size_t i = 0;
        while(i < k_sky_channel_marker.size() && start[i] != '\0'
              && std::tolower(static_cast<unsigned char>(start[i])) == k_sky_channel_marker[i]) {
            ++i;
        }
        if(i == k_sky_channel_marker.size()) {
            return true;
        }
    }

    return false;
}

// Returns the slot this channel already owns, or -1.
int find_slot(Aco_DX8_Texture* channel) noexcept
{
    const int count = g_sky_texture_count.load(std::memory_order_acquire);
    for(int i = 0; i < count; ++i) {
        if(g_sky_channels[i] == channel) {
            return i;
        }
    }

    return -1;
}

void clear_all_textures() noexcept
{
    for(auto& slot : g_sky_textures) {
        slot.store(nullptr, std::memory_order_relaxed);
    }
}

// Fires while the channel still holds the texture it is about to release. Dropping the mirror here
// is what keeps a freed pointer from outliving its texture: the gap until the matching `loaded`
// callback spans real rendered frames (a reload happens behind a loading screen), and a stale entry
// across that gap can match a completely unrelated texture that reused the address.
void on_texture_about_to_load(Aco_DX8_Texture* channel, const char* /*channel_name*/)
{
    const int slot = find_slot(channel);
    if(slot >= 0) {
        g_sky_textures[slot].store(nullptr, std::memory_order_relaxed);
    }
}

void on_texture_loaded(Aco_DX8_Texture* channel, const char* channel_name, IDirect3DTexture9* texture)
{
    if(texture == nullptr || !is_sky_channel_name(channel_name)) {
        return;
    }

    int slot = find_slot(channel);

    if(slot < 0) {
        const int count = g_sky_texture_count.load(std::memory_order_relaxed);
        if(count >= k_max_sky_textures) {
            TW_LOG_WARNING("skybox: more than {} sky channels seen, ignoring '{}'", k_max_sky_textures, channel_name);
            return;
        }

        slot = count;
        g_sky_channels[slot] = channel;
        g_sky_textures[slot].store(texture, std::memory_order_relaxed);

        // Publishes the slot's contents along with the new count.
        g_sky_texture_count.store(count + 1, std::memory_order_release);

        TW_LOG_INFO("skybox: tracking sky channel '{}' (slot {}), texture={}", channel_name, slot, static_cast<const void*>(texture));
        return;
    }

    g_sky_textures[slot].store(texture, std::memory_order_relaxed);
    TW_LOG_DEBUG("skybox: sky channel '{}' reloaded, texture={}", channel_name, static_cast<const void*>(texture));
}

IDirect3DCubeTexture9* ensure_cubemap(IDirect3DDevice9* device)
{
    if(g_cubemap != nullptr) {
        return g_cubemap;
    }

    if(g_cubemap_failed) {
        return nullptr;
    }

    const tw::skybox::cubemap_source source {
        .resource_key = tw::skybox::config::skybox_key(),
        .file_path = tw::skybox::config::skybox_file(),
        .hdr_exposure = tw::skybox::config::hdr_exposure(),
        .min_face_size = tw::skybox::config::min_face_size(),
    };

    const tw::skybox::cubemap_result result = tw::skybox::create_cubemap(device, source);

    g_cubemap = result.texture;
    g_face_size = result.face_size;
    g_requested_face_size = result.requested_face_size;

    if(g_cubemap == nullptr) {
        g_cubemap_failed = true;
        TW_LOG_ERROR("skybox: '{}' could not be turned into a cube map - the game keeps its own sky this session",
            source.file_path.empty() ? source.resource_key : source.file_path);
        return nullptr;
    }

    if(result.downscaled()) {
        g_downscale_from = result.requested_face_size;
        g_downscale_to = result.face_size;
        g_downscale_pending.store(true, std::memory_order_release);
    }

    return g_cubemap;
}

// Seconds since the first shaded draw. Uploaded to the probe as a constant it currently ignores -
// the plumbing is what is being proven, not the animation.
float elapsed_seconds() noexcept
{
    static const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
}

// Shader draw path: no cube map, no image decode, nothing on disk. Kept out of intercept_draw so
// the ordinary path stays a straight line.
bool draw_sky_program(IDirect3DDevice9* device, const tw::skybox::sky_program& program)
{
    const tw::skybox::shader::pair shaders = tw::skybox::shader::ensure(device, program);
    if(!shaders.valid()) {
        return false;
    }

    // The palette is fixed; only the runtime register carries anything this frame decides, so that
    // is the only thing built here. The program's own array goes to the device untouched.
    //
    // This used to copy the whole block every frame in order to patch one register into it, and the
    // size of that copy was a second, independent constant - which is exactly how half a shader once
    // went missing. Four floats and a separate upload replace both problems.
    std::array<float, 4> runtime {};

    if(program.has_runtime()) {
        runtime[0] = elapsed_seconds();
        if(program.markers_in_runtime_y) {
            runtime[1] = g_probe_markers.load(std::memory_order_relaxed) ? 1.f : 0.f;
        }
    }

    return tw::skybox::renderer::draw_program(device,
        shaders.vertex,
        shaders.pixel,
        program.constants,
        program.constant_runs,
        program.has_runtime() ? std::span<const float> { runtime } : std::span<const float> {},
        tw::skybox::k_runtime_register,
        g_shader_quality.load(std::memory_order_relaxed),
        g_orientation);
}

// Hot path: runs for every single draw call the game makes. Everything before the cube map lookup
// is a load, a compare, and a loop over at most three pointers.
bool intercept_draw(IDirect3DDevice9* device, IDirect3DBaseTexture9* stage0_texture)
{
    if(stage0_texture == nullptr || !g_enabled.load(std::memory_order_relaxed)) [[likely]] {
        return false;
    }

    const int count = g_sky_texture_count.load(std::memory_order_acquire);
    bool is_sky = false;
    for(int i = 0; i < count; ++i) {
        if(g_sky_textures[i].load(std::memory_order_relaxed) == stage0_texture) {
            is_sky = true;
            break;
        }
    }

    if(!is_sky) [[likely]] {
        return false;
    }

    if(const tw::skybox::sky_program* program = g_program.load(std::memory_order_relaxed); program != nullptr) {
        return draw_sky_program(device, *program);
    }

    IDirect3DCubeTexture9* cube = ensure_cubemap(device);
    if(cube == nullptr) {
        return false;
    }

    return tw::skybox::renderer::draw(device, cube, g_orientation);
}

void on_device_bound(IDirect3DDevice9* device, HWND /*hwnd*/)
{
    g_cubemap_failed = false;

    // What this device can actually do - shader models, render target cube maps, the behaviour
    // flags CreateD3D was reversed to produce. Log-only and one-shot; see skybox/sky_caps.
    tw::skybox::caps::report(device);

    // Whatever the previous device handed out is gone; the sky channels will re-publish through
    // Aco_DX8_Texture::RestoreTexture, which funnels back through LoadTextureFromMemory and so
    // through this module's own hook.
    clear_all_textures();

    // A late injection can beat the Texture channel DLL into the process; by the time there is a
    // device, it is certainly mapped.
    tw::skybox::retry_texture_hook();
}

void on_device_unbound()
{
    clear_all_textures();

    if(g_cubemap != nullptr) {
        g_cubemap->Release();
        g_cubemap = nullptr;
    }

    tw::skybox::renderer::release_device_resources();
    tw::skybox::shader::release_device_resources();

    // A replacement device can be a different adapter, or a software one; its answers are worth
    // printing again.
    tw::skybox::caps::reset();
}

// Pre-Reset. The cube map is D3DPOOL_MANAGED and rides this out; the game's sky textures do not,
// and the renderer's state block does not.
void on_device_lost()
{
    clear_all_textures();
    tw::skybox::renderer::on_device_lost();
}

// TweakerPlugin.dll -> .../TweakerPlugin.skybox.cfg, next to the DLL itself. Same shape as
// imgui_backend's compute_config_path (and resource/self_extract's path handling) - duplicated
// rather than shared because the two modules have no other reason to know about each other.
std::string compute_config_path()
{
    wchar_t wide_path[MAX_PATH] {};
    const DWORD len = ::GetModuleFileNameW(tw::plugin::globals::module_handle, wide_path, MAX_PATH);
    if(len == 0 || len >= MAX_PATH) {
        return {};
    }

    const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, wide_path, -1, nullptr, 0, nullptr, nullptr);
    if(bytes <= 1) {
        return {};
    }

    std::string path(static_cast<std::size_t>(bytes - 1), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide_path, -1, path.data(), bytes, nullptr, nullptr);

    const auto dot = path.find_last_of('.');
    if(dot != std::string::npos) {
        path.resize(dot);
    }
    path += ".skybox.cfg";

    return path;
}
} // namespace

namespace tw::skybox
{
void initialize() noexcept
{
    const std::string config_path = compute_config_path();
    if(!config_path.empty()) {
        config::load(config_path);
    }

    rebuild_orientation();

    // Before apply_program_from_config, which resolves the configured id against this list.
    initialize_programs();

    g_enabled.store(config::enabled(), std::memory_order_relaxed);
    g_probe_markers.store(config::probe_markers(), std::memory_order_relaxed);
    g_shader_quality.store(config::shader_quality(), std::memory_order_relaxed);
    apply_program_from_config();

    tw::framework::texture::subscribe(&on_texture_about_to_load, &on_texture_loaded);
    tw::framework::d3d9::attach_device_bind_listener(&on_device_bound, &on_device_unbound);
    tw::framework::d3d9::attach_device_reset_listener(&on_device_lost, nullptr);
    tw::framework::d3d9::attach_draw_interceptor(&intercept_draw);

    TW_LOG_INFO("skybox: initialized (enabled={}, skybox='{}')", config::enabled(), config::skybox_key());
}

void shutdown() noexcept
{
    tw::framework::d3d9::detach_draw_interceptor();
    tw::framework::d3d9::detach_device_reset_listener(&on_device_lost, nullptr);
    tw::framework::d3d9::detach_device_bind_listener(&on_device_bound, &on_device_unbound);

    on_device_unbound();
    config::save();
}

void retry_texture_hook() noexcept
{
    if(tw::framework::texture::is_installed()) {
        return;
    }

    if(!tw::framework::texture::install_texture_hook() && !g_logged_no_channels) {
        g_logged_no_channels = true;
        TW_LOG_WARNING("skybox: the Quest3D Texture channel is not hooked - the sky sphere cannot be identified this session");
    }
}

bool is_enabled() noexcept
{
    return config::enabled();
}

void set_enabled(bool value) noexcept
{
    config::set_enabled(value);
    g_enabled.store(value, std::memory_order_relaxed);
    config::save();
}

reload_outcome poll_reload(bool watch) noexcept
{
    sky_program* program = g_program.load(std::memory_order_relaxed);
    if(program == nullptr || !program->from_file()) {
        return reload_outcome::none;
    }

    // Half a second between stats. Often enough that saving the file and alt-tabbing back feels
    // immediate, rare enough that a filesystem call is not part of every frame - and, with `watch`,
    // not part of any frame at all while nobody is looking.
    constexpr auto k_interval = std::chrono::milliseconds { 500 };

    bool stat_now = false;
    if(watch) {
        const auto now = std::chrono::steady_clock::now();
        if(now - g_last_reload_poll >= k_interval) {
            g_last_reload_poll = now;
            stat_now = true;
        }
    }

    switch(poll_program(*program, stat_now)) {
        case program_event::compiled:
            // The program object is the same one the draw path holds; only its bytecode changed, and
            // the shader already built from the old bytecode knows nothing about that.
            shader::invalidate(*program);
            return reload_outcome::reloaded;

        case program_event::compile_failed:
            return reload_outcome::failed;

        case program_event::compile_started:
            return reload_outcome::started;

        default:
            return reload_outcome::none;
    }
}

status current_status() noexcept
{
    const sky_program* program = g_program.load(std::memory_order_relaxed);

    return status {
        .enabled = g_enabled.load(std::memory_order_relaxed),
        .sky_detected = g_sky_texture_count.load(std::memory_order_acquire) > 0,
        .cubemap_ready = g_cubemap != nullptr,
        .face_size = g_face_size,
        .requested_face_size = g_requested_face_size,
        .load_failed = g_cubemap_failed,
        .program = program,
        .program_name = program != nullptr ? std::string_view { program->display_name } : std::string_view {},
        .program_diagnostics = program != nullptr ? std::string_view { program->diagnostics } : std::string_view {},
        .program_compiling = program != nullptr && program->compiling(),
        .probe_markers = g_probe_markers.load(std::memory_order_relaxed),
        .program_has_markers = program != nullptr && program->markers_in_runtime_y,
        .shader_quality = g_shader_quality.load(std::memory_order_relaxed),
        .draw_microseconds = timer::average_microseconds(),
    };
}

void set_shader_quality(int percent) noexcept
{
    config::set_shader_quality(percent);
    g_shader_quality.store(percent, std::memory_order_relaxed);
}

void set_program_param(int index, std::array<float, 3> value, bool persist)
{
    sky_program* program = g_program.load(std::memory_order_relaxed);
    if(program == nullptr || index < 0 || static_cast<std::size_t>(index) >= program->params.size()) {
        return;
    }

    sky_param& param = program->params[static_cast<std::size_t>(index)];
    param.value = value;

    refresh_constants(*program);

    if(!persist) {
        return;
    }

    std::string text;
    for(int i = 0; i < param.count; ++i) {
        if(i > 0) {
            text += ',';
        }
        text += std::format("{}", param.value[static_cast<std::size_t>(i)]);
    }

    config::set_param_value(param_storage_key(*program, param), text);
    config::save();
}

void reset_program_params()
{
    sky_program* program = g_program.load(std::memory_order_relaxed);
    if(program == nullptr) {
        return;
    }

    for(sky_param& param : program->params) {
        param.value = param.default_value;

        std::string text;
        for(int i = 0; i < param.count; ++i) {
            if(i > 0) {
                text += ',';
            }
            text += std::format("{}", param.value[static_cast<std::size_t>(i)]);
        }

        config::set_param_value(param_storage_key(*program, param), text);
    }

    refresh_constants(*program);

    // One write for the whole reset. Looping over set_program_param(..., true) instead would rewrite
    // the entire settings file once per knob - eleven times over for Starry Night.
    config::save();
}

void select_program(std::string_view id)
{
    config::select_program(id);
    apply_program_from_config();

    // The cube map is dropped rather than kept warm: a program selection can outlive many songs,
    // and holding on to 96 MB of managed texture that nothing samples is the exact memory problem
    // the procedural path exists to avoid.
    request_reload();

    TW_LOG_INFO("skybox: switched to program '{}'", id);
}

void set_probe_markers(bool value) noexcept
{
    config::set_probe_markers(value);
    g_probe_markers.store(value, std::memory_order_relaxed);
}

void request_reload() noexcept
{
    // Released here rather than deferred: this runs on the render thread with the device still
    // bound (the caller is the overlay, drawing inside EndScene), which is the only place letting go
    // of a device resource is safe. Clearing the failure latch is what lets a previously broken
    // selection be retried after the user picks something else.
    if(g_cubemap != nullptr) {
        g_cubemap->Release();
        g_cubemap = nullptr;
    }

    g_cubemap_failed = false;
    g_face_size = 0;
    g_requested_face_size = 0;
}

void select_packed(std::string_view resource_key)
{
    config::select_packed(resource_key);

    // config cleared sky_program as part of that, so the mirror the draw path reads has to follow -
    // otherwise picking an image would leave the shader still painting over it.
    apply_program_from_config();

    request_reload();
    TW_LOG_INFO("skybox: switched to packed '{}'", resource_key);
}

void select_file(std::string_view path)
{
    config::select_file(path);
    apply_program_from_config();
    request_reload();
    TW_LOG_INFO("skybox: switched to file '{}'", path);
}

bool consume_downscale_notice(int& from, int& to) noexcept
{
    if(!g_downscale_pending.exchange(false, std::memory_order_acquire)) {
        return false;
    }

    from = g_downscale_from;
    to = g_downscale_to;

    return true;
}
} // namespace tw::skybox
