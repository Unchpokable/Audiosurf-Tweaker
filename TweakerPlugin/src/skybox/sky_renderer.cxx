#include "pch.hxx"

#include "skybox/sky_renderer.hxx"

#include "framework/d3d9_state.hxx"

#include "plugin/diagnostics.hxx"

#include "skybox/sky_math.hxx"
#include "skybox/sky_target.hxx"
#include "skybox/sky_timer.hxx"

namespace
{
using state_scope = tw::framework::d3d9::state_scope;

// Position doubles as the cube map lookup vector: for a point on a cube face, the direction and the
// position differ only by a positive scale, which a cube map sampler divides out anyway. That is
// also why eight vertices are enough where a 2D-textured cube would need twenty-four - there are no
// per-face UVs to split them on - and why the interpolation is exact rather than merely close, the
// way it would be on the sphere the game draws.
struct sky_vertex {
    float x;
    float y;
    float z;
    float u;
    float v;
    float w;
};

constexpr DWORD k_sky_fvf = D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE3(0);

constexpr std::array<sky_vertex, 8> k_cube_vertices { {
    { -1.f, -1.f, -1.f, -1.f, -1.f, -1.f },
    { 1.f, -1.f, -1.f, 1.f, -1.f, -1.f },
    { 1.f, 1.f, -1.f, 1.f, 1.f, -1.f },
    { -1.f, 1.f, -1.f, -1.f, 1.f, -1.f },
    { -1.f, -1.f, 1.f, -1.f, -1.f, 1.f },
    { 1.f, -1.f, 1.f, 1.f, -1.f, 1.f },
    { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f },
    { -1.f, 1.f, 1.f, -1.f, 1.f, 1.f },
} };

// Winding is irrelevant - the camera is inside the cube, so culling is off for this draw.
//
// Laid out one face per line: the trailing face labels are the only thing making this table
// readable, and clang-format would otherwise put every index on its own line to fit them.
// clang-format off
constexpr std::array<std::uint16_t, 36> k_cube_indices { {
    0, 1, 2,  0, 2, 3, // -Z
    4, 5, 6,  4, 6, 7, // +Z
    0, 3, 7,  0, 7, 4, // -X
    1, 5, 6,  1, 6, 2, // +X
    0, 4, 5,  0, 5, 1, // -Y
    3, 2, 6,  3, 6, 7, // +Y
} };
// clang-format on

constexpr UINT k_cube_primitive_count = 12;

// Set once at start-up, from the bootstrap thread, before any device exists - so it is read-only by
// the time the render thread walks it. Same shape and same reasoning as d3d9_hooks' listener
// vectors.
tw::skybox::renderer::extra_pass_fn g_extra_pass = nullptr;

IDirect3DDevice9* g_device = nullptr;
IDirect3DVertexBuffer9* g_vertex_buffer = nullptr;
IDirect3DIndexBuffer9* g_index_buffer = nullptr;

// One-shot diagnostics: the first sky draw logs the matrices it derived everything from. Whether
// the game hands the fixed-function pipeline a usable view/projection pair is the single assumption
// this renderer rests on, and it is not something a sandbox can check - so it is written down where
// whoever runs the game can read it.
bool g_logged_first_draw = false;
bool g_logged_first_shaded_draw = false;
bool g_logged_projection = false;

template<typename T>
void release_and_clear(T*& resource) noexcept
{
    if(resource != nullptr) {
        resource->Release();
        resource = nullptr;
    }
}

bool create_buffers(IDirect3DDevice9* device)
{
    constexpr UINT vertex_bytes = sizeof(k_cube_vertices);
    constexpr UINT index_bytes = sizeof(k_cube_indices);

    if(FAILED(device->CreateVertexBuffer(vertex_bytes, D3DUSAGE_WRITEONLY, k_sky_fvf, D3DPOOL_MANAGED, &g_vertex_buffer, nullptr))) {
        TW_LOG_ERROR("sky_renderer: CreateVertexBuffer failed");
        return false;
    }

    if(FAILED(device->CreateIndexBuffer(index_bytes, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &g_index_buffer, nullptr))) {
        TW_LOG_ERROR("sky_renderer: CreateIndexBuffer failed");
        return false;
    }

    void* mapped = nullptr;
    if(FAILED(g_vertex_buffer->Lock(0, vertex_bytes, &mapped, 0)) || mapped == nullptr) {
        TW_LOG_ERROR("sky_renderer: vertex buffer Lock failed");
        return false;
    }
    std::memcpy(mapped, k_cube_vertices.data(), vertex_bytes);
    g_vertex_buffer->Unlock();

    mapped = nullptr;
    if(FAILED(g_index_buffer->Lock(0, index_bytes, &mapped, 0)) || mapped == nullptr) {
        TW_LOG_ERROR("sky_renderer: index buffer Lock failed");
        return false;
    }
    std::memcpy(mapped, k_cube_indices.data(), index_bytes);
    g_index_buffer->Unlock();

    return true;
}

bool ensure_resources(IDirect3DDevice9* device)
{
    if(device != g_device) {
        // Not the device these were built against. Nothing can be released safely here - if the old
        // device is gone its children went with it - so the pointers are dropped, which is the same
        // bargain ui/texture_cache strikes. The unbind listener is what makes this the cold, rare
        // path rather than the normal one.
        g_vertex_buffer = nullptr;
        g_index_buffer = nullptr;
        g_device = device;
    }

    if(g_vertex_buffer == nullptr || g_index_buffer == nullptr) {
        release_and_clear(g_vertex_buffer);
        release_and_clear(g_index_buffer);

        if(!create_buffers(device)) {
            release_and_clear(g_vertex_buffer);
            release_and_clear(g_index_buffer);
            return false;
        }
    }

    return true;
}

// Rebuilds the projection's horizontal scale from the viewport it is actually being drawn into.
//
// The game leaves a **4:3** projection in the fixed-function transform whatever the display is.
// Measured: `fov 56.6 x 43.9 deg, aspect 1.3373` against a 2560x1440 viewport whose aspect is
// 1.7778, and the same 1.3373 at 800x600 - where it happens to be right, which is why the game never
// had to fix it. Everything drawn through it at any widescreen resolution is stretched horizontally
// by the ratio: 1.33 at 16:9.
//
// The game's own sky has been drawn that way since 2008 and nobody could tell, because a cloud
// texture has no circles in it. This sky has a sun, and a sun is where an aspect error stops being
// invisible.
//
// The vertical scale is kept and the horizontal derived from it, not the other way round. At the
// game's native 4:3 the two agree, so 43.9 degrees is the *authored* vertical field of view; keeping
// it and widening horizontally is what every engine does for a wider display, and the alternative -
// keeping the horizontal and cropping vertically - would show a widescreen player less of the sky
// than a 4:3 player sees.
//
// If this is ever wrong, the symptom is unmistakable: the sky would slide against the world as the
// camera turns, rather than merely being the wrong shape.
void correct_projection_aspect(IDirect3DDevice9* device, D3DMATRIX& projection) noexcept
{
    D3DVIEWPORT9 viewport {};
    if(FAILED(device->GetViewport(&viewport)) || viewport.Width == 0 || viewport.Height == 0) {
        return;
    }

    const float y_scale = projection._22;
    if(!std::isfinite(y_scale) || y_scale <= 0.f || !std::isfinite(projection._11) || projection._11 <= 0.f) {
        return;
    }

    const float viewport_aspect = static_cast<float>(viewport.Width) / static_cast<float>(viewport.Height);
    const float given_aspect = y_scale / projection._11;

    projection._11 = y_scale / viewport_aspect;

    // Once, and from here rather than from the draw, because this is the only place that holds both
    // the matrix the game handed over and the one it was replaced with.
    if(!g_logged_projection) {
        g_logged_projection = true;

        constexpr float k_rad_to_deg = 57.29578f;

        TW_LOG_INFO("sky_renderer: the game's projection is {:.1f} x {:.1f} deg, aspect {:.4f}; the viewport is {}x{}, aspect {:.4f}",
            2.f * std::atan(1.f / (y_scale / given_aspect)) * k_rad_to_deg,
            2.f * std::atan(1.f / y_scale) * k_rad_to_deg,
            given_aspect,
            viewport.Width,
            viewport.Height,
            viewport_aspect);

        if(std::fabs(viewport_aspect / given_aspect - 1.f) > 0.01f) {
            TW_LOG_INFO("sky_renderer: it disagrees with the viewport by {:.4f}, so the horizontal field of view was rebuilt from the "
                        "viewport - anything round in the sky would otherwise be drawn that much wider than tall",
                viewport_aspect / given_aspect);
        }
    }
}

// Half-extent for the cube, in view-space units, chosen so the whole thing sits inside the game's
// own frustum. Depth testing and writing are off for this draw, so where exactly it lands between
// the planes does not matter - only that it is not clipped away by them.
//
// For a standard D3D perspective projection, _33 = zf/(zf-zn) and _43 = -zn*zf/(zf-zn), which
// invert to the two lines below. Anything that does not invert to a sane pair (an orthographic
// projection, or a projection the game never set at all) falls back to 1.0 rather than guessing.
float derive_cube_extent(const D3DMATRIX& projection) noexcept
{
    const float m33 = projection._33;
    const float m43 = projection._43;

    if(!std::isfinite(m33) || !std::isfinite(m43) || m33 == 0.f || m33 == 1.f) {
        return 1.f;
    }

    const float near_plane = -m43 / m33;
    const float far_plane = -m43 / (m33 - 1.f);

    if(!std::isfinite(near_plane) || !std::isfinite(far_plane) || near_plane <= 0.f || far_plane <= near_plane) {
        return 1.f;
    }

    // The corners sit at extent*sqrt(3) from the camera, so the upper bound stays under
    // far_plane/sqrt(3); 0.5 is comfortably below 0.577 and leaves room for a sloppy far plane.
    const float lower = near_plane * 1.5f;
    const float upper = far_plane * 0.5f;

    return std::clamp(near_plane * 10.f, lower, std::max(lower, upper));
}

// One-shot, from the first sky draw. Answers the only question that matters about sharpness with a
// number instead of an impression: how many screen pixels one cube face is being stretched across,
// and therefore how big the art actually needs to be.
//
// A cube face spans exactly 90 degrees, and for a standard perspective projection _11 = cot(fovX/2).
// A face at view-space distance d covers x from -d to +d at constant z = d, so screen_x is
// (width/2) * _11 * (x/z) and the face lands on exactly `viewport.Width * _11` pixels. Constant
// across the face, because z does not vary on a plane facing the camera - so this is not a
// worst-case estimate, it is the number.
//
// The viewport matters as much as the texture: if the game is rendering this pass into something
// smaller than the window, no amount of cube map resolution would have helped and the search should
// go elsewhere.
void log_resolution_budget(IDirect3DDevice9* device, IDirect3DCubeTexture9* cube, const D3DMATRIX& projection)
{
    D3DVIEWPORT9 viewport {};
    if(FAILED(device->GetViewport(&viewport)) || viewport.Width == 0) {
        return;
    }

    D3DSURFACE_DESC desc {};
    if(FAILED(cube->GetLevelDesc(0, &desc)) || desc.Width == 0) {
        return;
    }

    if(!std::isfinite(projection._11) || projection._11 <= 0.f) {
        return;
    }

    const float face_screen_pixels = static_cast<float>(viewport.Width) * projection._11;
    const float magnification = face_screen_pixels / static_cast<float>(desc.Width);
    const float fov_x_degrees = 2.f * std::atan(1.f / projection._11) * (180.f / 3.14159265f);

    // Next power of two at or above what the face is being stretched to. Not a hard requirement on
    // modern hardware, but cube maps are conventionally power-of-two and it makes the advice
    // unambiguous.
    unsigned int one_to_one = 1;
    while(one_to_one < static_cast<unsigned int>(face_screen_pixels) && one_to_one < 16384) {
        one_to_one <<= 1;
    }

    // Naming a single "recommended" size was a mistake twice over: it hid what the size costs, and
    // the 1:1 figure it named is not a goal anyone should chase. A cube face spans 90 degrees while
    // the screen shows far less of it at once, so 1:1 sizes a texture whose edges are never
    // simultaneously visible - at a 56 degree FOV on a 1440p screen that is 8192px faces, which is
    // 1.5GB of cube map.
    //
    // So: print the candidates with their magnification and their VRAM, and let whoever reads it
    // pick a point on that curve. Around 2x is where a sky stops reading as soft.
    const auto cube_megabytes = [](unsigned int face) {
        return (static_cast<std::size_t>(face) * face * 4 * 6) / (1024 * 1024);
    };

    TW_LOG_INFO("sky_renderer: viewport {}x{}, horizontal FOV {:.1f} deg -> one cube face covers {:.0f} screen px",
        viewport.Width,
        viewport.Height,
        fov_x_degrees,
        face_screen_pixels);
    TW_LOG_INFO(
        "sky_renderer: cube map is {}px per face -> {:.1f}x magnification (1:1 would need {}px)", desc.Width, magnification, one_to_one);

    for(const unsigned int candidate : { 1024u, 2048u, 4096u }) {
        TW_LOG_INFO("sky_renderer:   {}px faces -> {:.1f}x, {} MB VRAM, wants an equirectangular panorama {}px wide",
            candidate,
            face_screen_pixels / static_cast<float>(candidate),
            cube_megabytes(candidate),
            candidate * 4);
    }
}

// Everything both draw paths need: the geometry bindings and the render states that make a cube
// drawn from the inside read as a sky. Neither path touches the shader bindings here - that is the
// one thing they disagree about, and each sets its own immediately after.
//
// Every write goes through `scope` rather than the device, which is what puts each of these back
// when the draw is over - see framework/d3d9_state.hxx. A device->Set* on this path would be a state
// the game never gets back.
void apply_common_state(state_scope& scope)
{
    scope.fvf(k_sky_fvf);
    scope.stream_source(0, g_vertex_buffer, 0, sizeof(sky_vertex));
    scope.indices(g_index_buffer);

    scope.render_state(D3DRS_ZENABLE, D3DZB_FALSE);
    scope.render_state(D3DRS_ZWRITEENABLE, FALSE);
    scope.render_state(D3DRS_CULLMODE, D3DCULL_NONE);
    scope.render_state(D3DRS_LIGHTING, FALSE);
    scope.render_state(D3DRS_ALPHABLENDENABLE, FALSE);
    scope.render_state(D3DRS_ALPHATESTENABLE, FALSE);
    scope.render_state(D3DRS_FOGENABLE, FALSE);
    scope.render_state(D3DRS_STENCILENABLE, FALSE);
    scope.render_state(D3DRS_SCISSORTESTENABLE, FALSE);
    scope.render_state(D3DRS_CLIPPING, TRUE);
    scope.render_state(D3DRS_CLIPPLANEENABLE, 0);
    scope.render_state(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
    scope.render_state(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);
    scope.render_state(D3DRS_SRGBWRITEENABLE, FALSE);
    scope.render_state(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    scope.render_state(
        D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
}

void apply_sky_state(state_scope& scope, IDirect3DCubeTexture9* cube)
{
    scope.vertex_shader(nullptr);
    scope.pixel_shader(nullptr);

    apply_common_state(scope);

    // Straight texture read, no vertex colour and no lighting term - the game's own sky surface
    // modulates by a faded vertex colour, and inheriting that would tint the replacement in ways
    // the art was not authored for.
    scope.texture(0, cube);
    scope.stage_state(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    scope.stage_state(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    scope.stage_state(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    scope.stage_state(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    scope.stage_state(0, D3DTSS_TEXCOORDINDEX, 0);
    scope.stage_state(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

    scope.texture(1, nullptr);
    scope.stage_state(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    scope.stage_state(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    scope.sampler_state(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    scope.sampler_state(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    scope.sampler_state(0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
    scope.sampler_state(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    scope.sampler_state(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    scope.sampler_state(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    scope.sampler_state(0, D3DSAMP_SRGBTEXTURE, FALSE);
}

void apply_shader_state(state_scope& scope,
    IDirect3DVertexShader9* vertex_shader,
    IDirect3DPixelShader9* pixel_shader,
    const D3DMATRIX& wvp,
    std::span<const float> pixel_constants,
    std::span<const tw::skybox::bytecode::register_run> runs,
    std::span<const tw::skybox::renderer::frame_block> frame_constants)
{
    scope.vertex_shader(vertex_shader);
    scope.pixel_shader(pixel_shader);

    apply_common_state(scope);

    const D3DMATRIX wvp_transposed = tw::skybox::math::transpose(wvp);
    scope.vertex_constants(0, std::span<const float>(&wvp_transposed.m[0][0], 16));

    // One call per run. Batching is not about the call count - there are at most a handful - but
    // about not writing the gaps between them, which belong to the shader's own literals.
    //
    // The runs arrive already merged and in range; the bound below is the span's, and it is here so
    // that a caller passing a short array cannot walk off the end of it.
    const auto registers = static_cast<int>(pixel_constants.size() / 4);

    for(const tw::skybox::bytecode::register_run& run : runs) {
        const int count = (std::min)(run.count, registers - run.first);
        if(run.first < 0 || count <= 0) {
            continue;
        }

        scope.pixel_constants(static_cast<UINT>(run.first),
            pixel_constants.subspan(static_cast<std::size_t>(run.first) * 4, static_cast<std::size_t>(count) * 4));
    }

    // Last, and on their own, because these are the registers whose values this frame decides.
    // Written over whatever the runs just put there, which is the program's stored copy of the same
    // registers - correct either way, and one call each rather than a copy of the whole file.
    for(const tw::skybox::renderer::frame_block& block : frame_constants) {
        const auto count = static_cast<int>(block.values.size() / 4);
        if(block.first < 0 || count <= 0 || block.first + count > registers) {
            continue;
        }

        scope.pixel_constants(static_cast<UINT>(block.first), block.values.first(static_cast<std::size_t>(count) * 4));
    }

    // No texture stage *state* is set: with a pixel shader bound the fixed-function stage cascade is
    // ignored entirely. Textures themselves are bound separately - see apply_samplers - because a
    // program may declare some and most declare none.
}

// Binds a layer's textures to the sampler registers its shader declares.
//
// Sampler state and not stage state: `D3DSAMP_*` is what a programmable shader's `tex2D`/`tex3D`
// obeys, while `D3DTSS_*` configures the fixed-function cascade that a bound pixel shader ignores.
// The two are easy to confuse because the cube map path above legitimately uses the latter.
//
// The W address mode is set alongside U and V unconditionally. It means nothing to a 2D texture and
// everything to a volume: a noise field tiled in x and y but clamped in z would repeat correctly
// across the sky and smear along the depth axis, which reads as directional streaking rather than as
// a wrapping mistake.
void apply_samplers(state_scope& scope, std::span<const tw::skybox::shader::sampler> samplers)
{
    for(const tw::skybox::shader::sampler& one : samplers) {
        const auto stage = static_cast<DWORD>(one.slot);

        scope.texture(stage, one.texture);

        scope.sampler_state(stage, D3DSAMP_ADDRESSU, one.address);
        scope.sampler_state(stage, D3DSAMP_ADDRESSV, one.address);
        scope.sampler_state(stage, D3DSAMP_ADDRESSW, one.address);

        scope.sampler_state(stage, D3DSAMP_MINFILTER, one.filter);
        scope.sampler_state(stage, D3DSAMP_MAGFILTER, one.filter);

        // Only when the file actually brought levels. Asking for a mip filter otherwise tells the
        // driver to interpolate between levels that do not exist.
        scope.sampler_state(stage, D3DSAMP_MIPFILTER, one.mipped ? one.filter : D3DTEXF_NONE);

        scope.sampler_state(stage, D3DSAMP_SRGBTEXTURE, FALSE);
    }
}

struct frame_setup {
    D3DMATRIX view {};
    D3DMATRIX projection {};
    D3DMATRIX world {};

    // Kept alongside `world` rather than read back out of it: world is orientation times scale, so
    // its _11 is only the extent when the orientation happens to be identity.
    float extent {};
};

// The half of a sky draw that is identical whether a cube map or a shader ends up painting the
// cube: read what the game left set, make sure our own resources exist, and build the matrices.
// Returns false having changed nothing, which every caller treats as "let the game draw its own sky
// this frame".
//
// Nothing is saved here any more. Saving is no longer a step at all - it happens one state at a
// time, as each one is set, inside the caller's state_scope.
bool prepare(IDirect3DDevice9* device, const D3DMATRIX& orientation, frame_setup& out)
{
    if(FAILED(device->GetTransform(D3DTS_VIEW, &out.view)) || FAILED(device->GetTransform(D3DTS_PROJECTION, &out.projection))) {
        return false;
    }

    // Before anything derives from it - the cube extent reads the depth row, which this does not
    // touch, but a later reader of the horizontal scale would get the game's wrong one.
    correct_projection_aspect(device, out.projection);

    if(!ensure_resources(device)) {
        return false;
    }

    // A skybox is the camera's rotation without its position: drop the translation row and the cube
    // stays centred on the viewer no matter where the track has carried them.
    out.view._41 = 0.f;
    out.view._42 = 0.f;
    out.view._43 = 0.f;

    out.extent = derive_cube_extent(out.projection);
    out.world = tw::skybox::math::multiply(orientation, tw::skybox::math::uniform_scale(out.extent));

    return true;
}
} // namespace

namespace tw::skybox::renderer
{
bool draw(IDirect3DDevice9* device, IDirect3DCubeTexture9* cube, const D3DMATRIX& orientation) noexcept
{
    if(device == nullptr || cube == nullptr) {
        return false;
    }

    frame_setup setup {};
    if(!prepare(device, orientation, setup)) {
        return false;
    }

    if(!g_logged_first_draw) {
        g_logged_first_draw = true;
        TW_LOG_INFO(
            "sky_renderer: first draw. extent={} proj._33={} proj._43={}", setup.extent, setup.projection._33, setup.projection._43);
        TW_LOG_INFO("sky_renderer: view rows [{} {} {}] [{} {} {}] [{} {} {}]",
            setup.view._11,
            setup.view._12,
            setup.view._13,
            setup.view._21,
            setup.view._22,
            setup.view._23,
            setup.view._31,
            setup.view._32,
            setup.view._33);
        log_resolution_budget(device, cube, setup.projection);
    }

    // From here to the end of the function everything written to the device is written through the
    // scope, and comes back off it when the scope goes out of scope below.
    state_scope scope(device);

    scope.transform(D3DTS_WORLD, setup.world);
    scope.transform(D3DTS_VIEW, setup.view);

    apply_sky_state(scope, cube);

    timer::begin(device);
    device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, static_cast<UINT>(k_cube_vertices.size()), 0, k_cube_primitive_count);

    // The geometry layer belongs to both sky paths - a sprite that vanished on switching to a cube
    // map would read as a bug, not as a design. This path draws through the fixed-function pipeline
    // and so never needed the combined matrix; the pass does, because it runs a vertex shader.
    if(g_extra_pass != nullptr) {
        g_extra_pass(device, math::multiply(math::multiply(setup.world, setup.view), setup.projection));
    }

    timer::end();

    return true;
}

bool draw_program(IDirect3DDevice9* device,
    IDirect3DVertexShader9* vertex_shader,
    IDirect3DPixelShader9* pixel_shader,
    std::span<const float> pixel_constants,
    std::span<const tw::skybox::bytecode::register_run> runs,
    std::span<const frame_block> frame_constants,
    std::span<const tw::skybox::shader::sampler> samplers,
    int scale_percent,
    const D3DMATRIX& orientation) noexcept
{
    if(device == nullptr || vertex_shader == nullptr || pixel_shader == nullptr) {
        return false;
    }

    frame_setup setup {};
    if(!prepare(device, orientation, setup)) {
        return false;
    }

    // The shader transforms the vertices itself, so D3DTS_WORLD/VIEW are not set at all here. The
    // product still has to be built in the order the fixed-function pipeline would have used -
    // world, then view, then projection, row-vector convention throughout - or the sky ends up
    // somewhere other than around the camera.
    const D3DMATRIX wvp = math::multiply(math::multiply(setup.world, setup.view), setup.projection);

    if(!g_logged_first_shaded_draw) {
        g_logged_first_shaded_draw = true;
        TW_LOG_INFO("sky_renderer: first shaded draw. extent={}, pixel constant registers {}", setup.extent, bytecode::describe(runs));
    }

    // The timer spans the redirect and the blit as well as the draw, because what anyone reading it
    // wants to know is what the sky costs - and at anything below 100% the upscale is part of that.
    timer::begin(device);

    const bool scaled = target::begin(device, scale_percent);

    // Opened after target::begin, which saves and puts back the render target, the depth surface and
    // the viewport itself - it has to, because they have to go back before the blit rather than at
    // the end of the draw.
    state_scope scope(device);

    apply_shader_state(scope, vertex_shader, pixel_shader, wvp, pixel_constants, runs, frame_constants);

    // After the shader state, because apply_common_state resets stage 0 for the fixed-function path
    // the cube map uses - and a texture bound before that would be the one it clears.
    apply_samplers(scope, samplers);

    device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, static_cast<UINT>(k_cube_vertices.size()), 0, k_cube_primitive_count);

    if(scaled) {
        target::end(device);
    }

    // Deliberately after the blit and before the timer closes: the pass draws at full resolution
    // onto the restored back buffer, and whatever it costs is part of what the sky costs.
    if(g_extra_pass != nullptr) {
        g_extra_pass(device, wvp);
    }

    timer::end();

    return true;
}

void attach_extra_pass(extra_pass_fn fn) noexcept
{
    g_extra_pass = fn;
}

void on_device_lost() noexcept
{
    // Nothing of this module's own is left to drop: the state block that used to be released here
    // was the one D3DPOOL_DEFAULT-like object it held, and a state_scope is a stack object that
    // exists only for the duration of a draw. The buffers are D3DPOOL_MANAGED and survive a Reset.
    timer::on_device_lost();
    target::on_device_lost();
}

void release_device_resources() noexcept
{
    release_and_clear(g_index_buffer);
    release_and_clear(g_vertex_buffer);
    timer::release_device_resources();
    target::release_device_resources();

    g_device = nullptr;
    g_logged_first_draw = false;
    g_logged_first_shaded_draw = false;
}
} // namespace tw::skybox::renderer
