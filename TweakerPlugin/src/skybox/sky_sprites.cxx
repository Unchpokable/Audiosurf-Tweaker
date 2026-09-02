#include "pch.hxx"

#include "skybox/sky_sprites.hxx"

#include "plugin/diagnostics.hxx"

#include "skybox/sky_math.hxx"
#include "skybox/sky_program.hxx"
#include "skybox/sky_renderer.hxx"
#include "skybox/sky_shader.hxx"
#include "skybox/sky_sprite_atlas.hxx"

namespace
{
namespace atlas = tw::skybox::sprite_atlas;

// 16-bit indices, four vertices per sprite: 16383 is where the index type runs out. The real cap is
// far lower and is about fill rate rather than vertices, but a hard ceiling belongs where the
// hardware puts it.
constexpr int k_max_sprites = 4096;
constexpr int k_min_sprites = 1;

struct sprite_vertex {
    float x;
    float y;
    float z;

    // Quad coordinate, -1..1. The sphere normal and the rim falloff come from this.
    float u;
    float v;

    // Where this corner reads from the atlas.
    float au;
    float av;

    float cx;
    float cy;
    float cz;

    float tx;
    float ty;
    float tz;
};

// Must match sprite_vertex field for field, and match the vs_in of assets/shaders/sky_sprite.vs.hlsl
// in that order. An FVF rather than a vertex declaration for the same reason sky_renderer uses one:
// texture coordinate sets of fixed width is exactly what an FVF can say.
constexpr DWORD k_sprite_fvf
    = D3DFVF_XYZ | D3DFVF_TEX3 | D3DFVF_TEXCOORDSIZE4(0) | D3DFVF_TEXCOORDSIZE3(1) | D3DFVF_TEXCOORDSIZE3(2);

IDirect3DDevice9* g_device = nullptr;
IDirect3DVertexBuffer9* g_vertex_buffer = nullptr;
IDirect3DIndexBuffer9* g_index_buffer = nullptr;

// What the buffers were actually built for, so a knob that did not move does not rebuild them.
// Everything placement depends on is here, which is why this grows whenever placement gains a knob.
struct build_key {
    int count {};
    float half_size {};
    int clusters {};
    float spread {};

    // 0 leaves the clumps on the even lattice; 1 lets each wander a full cell off it. See
    // clump_direction for why that is the useful range and why pure randomness is not.
    float scatter {};

    // Largest aspect ratio a clump may take. 1 is round; above that each clump picks its own ratio
    // and its own heading, because clouds are not discs.
    float elongation {};

    // The band clouds may occupy, as sines of elevation. The floor is the interesting one: below it
    // is the horizon haze the sky shader paints, and a sprite drawn into that reads as a cut-out
    // pasted over the fog rather than as a cloud behind it.
    float floor_y {};
    float ceiling_y {};

    [[nodiscard]] bool operator==(const build_key&) const noexcept = default;
};

build_key g_built {};

// One failed shader creation is enough; a device that will not take vs_3_0 will not start taking it
// later, and retrying every frame would cost more than the layer does.
bool g_failed = false;

// The layer being drawn, or null when the selected sky declares none.
//
// Everything this pass is configured by hangs off it: the compiled shader pair, the constants its
// parameters and bindings produced, and the properties below. Held rather than copied because the
// program object outlives every frame - sky_program never moves or destroys one - and because a copy
// would be a second answer to "what are the clouds", which is the thing being removed rather than
// duplicated.
const tw::skybox::sky_program* g_program = nullptr;

int g_count = 120;
float g_size_degrees = 4.0f;
int g_clusters = 14;
float g_spread_degrees = 14.f;
float g_scatter = 0.85f;
float g_elongation = 2.0f;
float g_floor_degrees = 6.f;
float g_ceiling_degrees = 78.f;

// Seconds since the layer first drew.
//
// Its own clock rather than the one skybox.cxx keeps for the sky program's runtime register. The two
// start within a frame of each other and are never compared - nothing subtracts one from the other -
// so sharing them would only mean threading a float through the renderer's extra-pass hook for no
// observable difference.
float elapsed_seconds() noexcept
{
    static const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
}

template<typename T>
void release_and_clear(T*& resource) noexcept
{
    if(resource != nullptr) {
        resource->Release();
        resource = nullptr;
    }
}

// Deterministic and cheap; the layer has to look the same every run or "did that change?" stops
// being answerable. Integer hash rather than std::rand for exactly that reason.
float hash01(std::uint32_t seed) noexcept
{
    seed ^= seed >> 16;
    seed *= 0x7feb352dU;
    seed ^= seed >> 15;
    seed *= 0x846ca68bU;
    seed ^= seed >> 16;

    return static_cast<float>(seed & 0xffffffU) / static_cast<float>(0x1000000U);
}

struct vec3 {
    float x;
    float y;
    float z;
};

vec3 cross(const vec3& a, const vec3& b) noexcept
{
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

vec3 normalized(const vec3& v) noexcept
{
    const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if(length <= 1e-6f) {
        return { 0.f, 1.f, 0.f };
    }

    return { v.x / length, v.y / length, v.z / length };
}

// The billboard basis, resolved once per sprite on the CPU.
//
// No smooth non-vanishing tangent field exists on a sphere, so any fixed reference axis degenerates
// somewhere - here, at the poles, where the reference and the direction are parallel and the cross
// product collapses. Switching the reference axis for those few sprites settles it permanently.
// Doing the same thing in a shader would mean paying for the branch on every pixel of every frame
// to fix two sprites.
vec3 tangent_for(const vec3& direction) noexcept
{
    const vec3 reference = std::abs(direction.y) > 0.99f ? vec3 { 1.f, 0.f, 0.f } : vec3 { 0.f, 1.f, 0.f };
    return normalized(cross(reference, direction));
}

// Raises a direction to sit at or above `min_y`, keeping its azimuth and its length.
//
// Not a clamp on the vector: setting y and leaving x and z alone would shorten it and, after
// renormalising, drag the azimuth around. Only the elevation should move.
vec3 lift_to(const vec3& direction, float min_y) noexcept
{
    if(direction.y >= min_y) {
        return direction;
    }

    const float ring = std::sqrt((std::max)(0.f, 1.f - min_y * min_y));
    const float horizontal = std::sqrt(direction.x * direction.x + direction.z * direction.z);

    if(horizontal <= 1e-6f) {
        return { ring, min_y, 0.f };
    }

    return { direction.x / horizontal * ring, min_y, direction.z / horizontal * ring };
}

// Folds a direction back into the band, reflecting at each edge rather than clamping to it.
//
// The distinction matters and is not a refinement: the lattice reaches both edges, so half the jitter
// disc of an edge clump points outside the band. Clamping would put every one of those on the
// boundary itself - a row of clumps in a line along the horizon, which is a worse artefact than the
// spiral the jitter was added to break. Reflection is measure-preserving: the density stays even
// right up to the edge because what leaves comes back.
vec3 fold_into(const vec3& direction, float min_y, float max_y) noexcept
{
    float y = direction.y;

    if(y < min_y) {
        y = min_y + (min_y - y);
    }
    else if(y > max_y) {
        y = max_y - (y - max_y);
    }

    // A jitter wider than the band itself can reflect past the far edge. Clamping that residue is
    // fine: by then it is a handful of clumps in a band nobody meant to be that narrow.
    y = std::clamp(y, min_y, max_y);

    const float ring = std::sqrt((std::max)(0.f, 1.f - y * y));
    const float horizontal = std::sqrt(direction.x * direction.x + direction.z * direction.z);

    if(horizontal <= 1e-6f) {
        return { ring, y, 0.f };
    }

    return { direction.x / horizontal * ring, y, direction.z / horizontal * ring };
}

vec3 press_to(const vec3& direction, float max_y) noexcept
{
    if(direction.y <= max_y) {
        return direction;
    }

    const float ring = std::sqrt((std::max)(0.f, 1.f - max_y * max_y));
    const float horizontal = std::sqrt(direction.x * direction.x + direction.z * direction.z);

    if(horizontal <= 1e-6f) {
        return { ring, max_y, 0.f };
    }

    return { direction.x / horizontal * ring, max_y, direction.z / horizontal * ring };
}

// Where one clump sits: an even lattice, jittered.
//
// The lattice is a Fibonacci (golden angle) spiral, which is uniform and deterministic. On its own
// it is also *visibly* a spiral - the eye finds the arms at any count worth drawing, and a sky whose
// clouds lie along a spiral is worse than one whose clouds are merely uneven.
//
// Hashed random directions are not the fix either, and that is worth being clear about rather than
// rediscovering: independent samples clump and leave bald patches at these counts, which is exactly
// the artefact the lattice was chosen to avoid. What is wanted is neither - it is a lattice whose
// points have been let off their exact positions.
//
// So each clump is displaced within its own cell. `scatter` is that displacement as a fraction of a
// cell radius, and a cell radius is derived rather than tuned: N clumps sharing a band of solid
// angle 2*pi*(ceiling - floor) get pi*r^2 each. At scatter = 1 a clump may reach anywhere in its own
// cell, which reads as random and cannot leave a hole.
vec3 clump_direction(int index, const build_key& key) noexcept
{
    const int total = (std::max)(key.clusters, 1);
    const float t = (static_cast<float>(index) + 0.5f) / static_cast<float>(total);

    const float y = key.floor_y + t * (key.ceiling_y - key.floor_y);
    const float ring = std::sqrt((std::max)(0.f, 1.f - y * y));

    constexpr float k_golden_angle = 2.39996323f;
    const float theta = static_cast<float>(index) * k_golden_angle;

    const vec3 anchor = normalized({ std::cos(theta) * ring, y, std::sin(theta) * ring });

    if(key.scatter <= 0.f) {
        return anchor;
    }

    const float band = (std::max)(key.ceiling_y - key.floor_y, 1e-3f);
    const float cell = std::sqrt(2.f * band / static_cast<float>(total));

    const auto seed = static_cast<std::uint32_t>(index) * 2246822519U;
    const float angle = hash01(seed + 61U) * 6.2831853f;
    const float radius = key.scatter * cell * std::sqrt(hash01(seed + 62U));

    const vec3 tangent = tangent_for(anchor);
    const vec3 bitangent = cross(anchor, tangent);

    const float offset = std::tan((std::min)(radius, 1.4f));
    const vec3 moved = normalized({ anchor.x + (tangent.x * std::cos(angle) + bitangent.x * std::sin(angle)) * offset,
        anchor.y + (tangent.y * std::cos(angle) + bitangent.y * std::sin(angle)) * offset,
        anchor.z + (tangent.z * std::cos(angle) + bitangent.z * std::sin(angle)) * offset });

    return fold_into(moved, key.floor_y, key.ceiling_y);
}

bool create_index_buffer(IDirect3DDevice9* device, int sprites)
{
    const UINT bytes = static_cast<UINT>(sprites) * 6 * sizeof(std::uint16_t);

    if(FAILED(device->CreateIndexBuffer(bytes, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &g_index_buffer, nullptr))
        || g_index_buffer == nullptr) {
        TW_LOG_ERROR("sky_sprites: CreateIndexBuffer({} bytes) failed", static_cast<unsigned long>(bytes));
        return false;
    }

    void* mapped = nullptr;
    if(FAILED(g_index_buffer->Lock(0, 0, &mapped, 0)) || mapped == nullptr) {
        TW_LOG_ERROR("sky_sprites: index buffer Lock failed");
        return false;
    }

    auto* indices = static_cast<std::uint16_t*>(mapped);
    for(int i = 0; i < sprites; ++i) {
        const auto base = static_cast<std::uint16_t>(i * 4);

        indices[i * 6 + 0] = base;
        indices[i * 6 + 1] = static_cast<std::uint16_t>(base + 1);
        indices[i * 6 + 2] = static_cast<std::uint16_t>(base + 2);
        indices[i * 6 + 3] = base;
        indices[i * 6 + 4] = static_cast<std::uint16_t>(base + 2);
        indices[i * 6 + 5] = static_cast<std::uint16_t>(base + 3);
    }

    g_index_buffer->Unlock();

    return true;
}

void fill_sprite(sprite_vertex* out, int index, const build_key& key) noexcept
{
    const auto seed = static_cast<std::uint32_t>(index) * 2654435761U;

    // Round-robin rather than contiguous blocks, so a count that does not divide evenly spreads the
    // remainder across every clump instead of piling it into the last one.
    const int cluster = index % (std::max)(key.clusters, 1);
    const vec3 anchor = clump_direction(cluster, key);

    const vec3 anchor_t = tangent_for(anchor);
    const vec3 anchor_b = cross(anchor, anchor_t);

    // The clump's own shape. Clouds are not discs - a bank of them is drawn out along something,
    // usually the wind - so each clump gets an aspect ratio and a heading of its own, hashed from
    // the clump rather than from the sprite so every sprite in it agrees about the shape it is part
    // of. Area is preserved (one axis times `aspect`, the other divided by it), so stretching a
    // clump does not also make it bigger.
    const auto clump_seed = static_cast<std::uint32_t>(cluster) * 2654435761U;
    const float aspect = 1.f + (std::max)(key.elongation - 1.f, 0.f) * hash01(clump_seed + 71U);
    const float heading = hash01(clump_seed + 72U) * 6.2831853f;

    const float head_c = std::cos(heading);
    const float head_s = std::sin(heading);

    const vec3 long_axis { anchor_t.x * head_c + anchor_b.x * head_s,
        anchor_t.y * head_c + anchor_b.y * head_s,
        anchor_t.z * head_c + anchor_b.z * head_s };
    const vec3 short_axis = cross(anchor, long_axis);

    // Uniform in the disc, not in the radius: sqrt() is what stops every clump from having a dense
    // core and a bare edge. The clump still reads as denser in the middle because the sprites
    // overlap more there, which is how a real one reads too.
    const float angle = hash01(seed + 11U) * 6.2831853f;
    const float radius = key.spread * std::sqrt(hash01(seed + 12U));

    const float along = std::tan(radius * aspect) * std::cos(angle);
    const float across = std::tan(radius / aspect) * std::sin(angle);

    const vec3 placed = normalized({ anchor.x + long_axis.x * along + short_axis.x * across,
        anchor.y + long_axis.y * along + short_axis.y * across,
        anchor.z + long_axis.z * along + short_axis.z * across });

    // Size variation. A uniform size reads as a pattern however well the positions are distributed.
    const float scale = key.half_size * (0.55f + 0.9f * hash01(seed + 3U));

    // The whole quad has to clear the floor, not just its centre - a sprite whose lower half hangs
    // into the horizon haze is exactly the cut-out-pasted-over-fog look the floor exists to prevent.
    //
    // The reach is the quad's *diagonal*, not its half-height: a corner sits at (±scale, ±scale) in
    // the tangent plane, so it is sqrt(2) further out than the edge. Using atan(scale) here let every
    // sprite hang 1.8 degrees below the floor, which the offline placement harness measured and no
    // amount of looking at this line would have.
    constexpr float k_root_two = 1.41421356f;
    const float margin = std::atan(scale * k_root_two);
    const vec3 centre = press_to(lift_to(placed, std::sin((std::min)(std::asin(key.floor_y) + margin, 1.4f))),
        std::sin((std::max)(std::asin(key.ceiling_y) - margin, -1.4f)));

    // The sprite's own rotation, folded into the basis rather than into the atlas coordinates. That
    // is deliberate: the pixel shader lights the atlas normal in this basis, so turning the basis
    // turns the shape and its lighting together. Rotating the texture coordinates instead would
    // spin the cloud while leaving its lumps lit from a fixed direction.
    const float spin = hash01(seed + 4U) * 6.2831853f;
    const float cs = std::cos(spin);
    const float sn = std::sin(spin);

    const vec3 base_t = tangent_for(centre);
    const vec3 base_b = cross(centre, base_t);

    const vec3 tangent { base_t.x * cs + base_b.x * sn, base_t.y * cs + base_b.y * sn, base_t.z * cs + base_b.z * sn };
    const vec3 bitangent = cross(centre, tangent);

    const auto rect = atlas::tile(static_cast<int>(hash01(seed + 5U) * static_cast<float>(atlas::k_tile_count)));

    constexpr float k_corner_u[4] = { -1.f, 1.f, 1.f, -1.f };
    constexpr float k_corner_v[4] = { -1.f, -1.f, 1.f, 1.f };

    for(int corner = 0; corner < 4; ++corner) {
        const float u = k_corner_u[corner];
        const float v = k_corner_v[corner];

        sprite_vertex& vertex = out[corner];

        vertex.x = centre.x + (tangent.x * u + bitangent.x * v) * scale;
        vertex.y = centre.y + (tangent.y * u + bitangent.y * v) * scale;
        vertex.z = centre.z + (tangent.z * u + bitangent.z * v) * scale;

        vertex.u = u;
        vertex.v = v;

        vertex.au = u < 0.f ? rect.u0 : rect.u1;
        vertex.av = v < 0.f ? rect.v0 : rect.v1;

        vertex.cx = centre.x;
        vertex.cy = centre.y;
        vertex.cz = centre.z;

        vertex.tx = tangent.x;
        vertex.ty = tangent.y;
        vertex.tz = tangent.z;
    }
}

bool create_vertex_buffer(IDirect3DDevice9* device, const build_key& key)
{
    const UINT bytes = static_cast<UINT>(key.count) * 4 * sizeof(sprite_vertex);

    if(FAILED(device->CreateVertexBuffer(bytes, D3DUSAGE_WRITEONLY, k_sprite_fvf, D3DPOOL_MANAGED, &g_vertex_buffer, nullptr))
        || g_vertex_buffer == nullptr) {
        TW_LOG_ERROR("sky_sprites: CreateVertexBuffer({} bytes) failed", static_cast<unsigned long>(bytes));
        return false;
    }

    void* mapped = nullptr;
    if(FAILED(g_vertex_buffer->Lock(0, 0, &mapped, 0)) || mapped == nullptr) {
        TW_LOG_ERROR("sky_sprites: vertex buffer Lock failed");
        return false;
    }

    auto* vertices = static_cast<sprite_vertex*>(mapped);
    for(int i = 0; i < key.count; ++i) {
        fill_sprite(vertices + i * 4, i, key);
    }

    g_vertex_buffer->Unlock();

    return true;
}

void destroy_buffers() noexcept
{
    release_and_clear(g_index_buffer);
    release_and_clear(g_vertex_buffer);

    g_built = {};
}

// Cold path: only when the device changes or a slider moved.
bool ensure_resources(IDirect3DDevice9* device)
{
    if(device != g_device) {
        // Nothing here can be released through a device we no longer hold, so the pointers are
        // dropped rather than freed - the same bargain sky_renderer and sky_target strike.
        g_vertex_buffer = nullptr;
        g_index_buffer = nullptr;

        g_built = {};
        g_failed = false;
        g_device = device;
    }

    if(g_failed) {
        return false;
    }

    if(atlas::ensure(device) == nullptr) {
        g_failed = true;
        return false;
    }

    build_key key {};
    key.count = std::clamp(g_count, k_min_sprites, k_max_sprites);
    key.half_size = std::tan(tw::skybox::math::to_radians((std::max)(g_size_degrees, 0.05f)));
    key.clusters = std::clamp(g_clusters, 1, key.count);
    key.spread = tw::skybox::math::to_radians(std::clamp(g_spread_degrees, 0.f, 80.f));
    key.scatter = std::clamp(g_scatter, 0.f, 1.f);
    key.elongation = std::clamp(g_elongation, 1.f, 6.f);

    // Ordered here rather than trusted from the manifest: a floor above the ceiling is a band of
    // negative height, and every derivation below - the cell radius, the lattice step - would then
    // be computed from a negative number rather than simply producing no clouds.
    const float floor_degrees = std::clamp(g_floor_degrees, -20.f, 89.f);
    const float ceiling_degrees = std::clamp(g_ceiling_degrees, floor_degrees + 1.f, 90.f);

    key.floor_y = std::sin(tw::skybox::math::to_radians(floor_degrees));
    key.ceiling_y = std::sin(tw::skybox::math::to_radians(ceiling_degrees));

    if(g_vertex_buffer != nullptr && g_index_buffer != nullptr && g_built == key) {
        return true;
    }

    destroy_buffers();

    if(!create_vertex_buffer(device, key) || !create_index_buffer(device, key.count)) {
        destroy_buffers();
        g_failed = true;
        return false;
    }

    g_built = key;

    TW_LOG_INFO("sky_sprites: built {} sprites in {} clumps at {:.2f} deg",
        key.count,
        key.clusters,
        static_cast<double>(g_size_degrees));

    return true;
}

void apply_state(IDirect3DDevice9* device, const D3DMATRIX& wvp, const tw::skybox::shader::pair& shaders)
{
    device->SetFVF(k_sprite_fvf);
    device->SetStreamSource(0, g_vertex_buffer, 0, sizeof(sprite_vertex));
    device->SetIndices(g_index_buffer);

    device->SetVertexShader(shaders.vertex);
    device->SetPixelShader(shaders.pixel);

    const D3DMATRIX wvp_transposed = tw::skybox::math::transpose(wvp);
    device->SetVertexShaderConstantF(0, &wvp_transposed.m[0][0], 4);

    // The vertex stage's own knobs - how the layer drifts and breathes. Same rule as the pixel side:
    // only the runs the shader declares, because the gaps between them hold its `def` literals.
    //
    // Its c0-c3 are the matrix above and no manifest can name them: a shader that did not take the
    // transform where the engine puts it would not draw at all, so that one is structure rather than
    // configuration. Everything else is named.
    for(const tw::skybox::bytecode::register_run& run : g_program->vertex_constant_runs) {
        // Clipped past the matrix, not skipped when it overlaps it. fxc is free to merge c0 through
        // c6 into one run, and dropping a whole run because it starts at the matrix would silently
        // upload none of the knobs beyond it - a shader that compiles, draws, and ignores every
        // setting. The same shape of mistake as the register-ceiling one, and worth the branch.
        const int first_register = (std::max)(run.first, 4);
        const int count = run.first + run.count - first_register;

        if(count <= 0) {
            continue;
        }

        const auto first = static_cast<std::size_t>(first_register) * 4;
        const auto floats = static_cast<std::size_t>(count) * 4;

        if(first + floats <= g_program->vertex_constants.size()) {
            device->SetVertexShaderConstantF(static_cast<UINT>(first_register), &g_program->vertex_constants[first], static_cast<UINT>(count));
        }
    }

    // Time, into whichever register the vertex shader called `g_time` - found by name, so a sky that
    // animates its clouds does not have to know a register number, and one that does not pays
    // nothing. Written after the block above so a stale value from the constants cannot win.
    if(g_program->vertex_time_register >= 4) {
        std::array<float, 4> runtime {};
        const auto slot = static_cast<std::size_t>(g_program->vertex_time_register) * 4;

        if(slot + 4 <= g_program->vertex_constants.size()) {
            std::copy_n(g_program->vertex_constants.begin() + static_cast<std::ptrdiff_t>(slot), 4, runtime.begin());
        }

        runtime[static_cast<std::size_t>(g_program->vertex_time_component)] = elapsed_seconds();

        device->SetVertexShaderConstantF(static_cast<UINT>(g_program->vertex_time_register), runtime.data(), 1);
    }

    // Everything this shader is configured by, in one place: the layer's own parameters and the
    // sky's lights, already resolved into registers by the program that owns them.
    //
    // Nothing is computed here any more, and that is the point of the change rather than a tidy-up.
    // The layer used to work out where the light was, from its own two sliders or from whatever the
    // sky program had published - which meant two representations of one direction, and clouds that
    // agreed with the sky only when somebody had remembered to wire them together. Now there is one
    // answer per sky and every layer reads it.
    //
    // Uploaded run by run, never as one block: the gaps between the runs hold the shader's own `def`
    // literals, and writing one corrupts the program rather than configuring it. Same rule and the
    // same reason as sky_renderer::draw_program - see sky_bytecode.
    for(const tw::skybox::bytecode::register_run& run : g_program->constant_runs) {
        const auto first = static_cast<std::size_t>(run.first) * 4;
        const auto floats = static_cast<std::size_t>(run.count) * 4;

        if(first + floats <= g_program->constants.size()) {
            device->SetPixelShaderConstantF(static_cast<UINT>(run.first), &g_program->constants[first], static_cast<UINT>(run.count));
        }
    }

    // The game leaves ONE / INVSRCCOLOR set with blending switched off (measured - see sky_probe),
    // so there is nothing here worth inheriting: every factor is set explicitly.
    device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    // Straight alpha: everything the cloud sends towards the eye is scattered by its own material,
    // so it scales with how much material there is - which is what the hardware multiply does. See
    // assets/shaders/sky_sprite.ps.hlsl for why premultiplying here was a mistake.
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    // The shader's own clip() does the rejecting; alpha test would be a second, redundant one.
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    device->SetRenderState(
        D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);

    device->SetTexture(0, atlas::ensure(device));

    // CLAMP rather than WRAP: a quad's coordinates stay inside one tile, and clamping is what makes
    // a rounding error at the edge repeat the border texel instead of jumping to the far side of
    // the atlas - which would be a different cloud.
    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
}

// The pass itself. Runs inside sky_renderer's state capture, so nothing here needs restoring.
void draw_pass(IDirect3DDevice9* device, const D3DMATRIX& wvp)
{
    if(g_program == nullptr || device == nullptr) {
        return;
    }

    // Not drawn until both halves of the pair have compiled. A layer whose vertex stage landed and
    // whose pixel stage has not would draw with whatever the previous sky left bound.
    if(!g_program->usable() || g_program->compiling()) {
        return;
    }

    // The same cache every sky program's shaders live in, keyed on the program. Its own vertex stage
    // when the package compiled one, the built-in pair when it did not - and this module does not
    // have to know which, because the program already says.
    const tw::skybox::shader::pair shaders = tw::skybox::shader::ensure(device, *g_program);
    if(!shaders.valid()) {
        return;
    }

    // Deliberately not built here. The rebuild happens on the frame tick, outside the interception -
    // see prepare(). What is left on this path is a check, so a frame where the layout changed draws
    // the previous buffer rather than nothing.
    if(g_vertex_buffer == nullptr || g_index_buffer == nullptr || g_built.count <= 0) {
        return;
    }

    apply_state(device, wvp, shaders);

    device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, static_cast<UINT>(g_built.count * 4), 0, static_cast<UINT>(g_built.count * 2));
}
} // namespace

namespace tw::skybox::sprites
{
void initialize() noexcept
{
    tw::skybox::renderer::attach_extra_pass(&draw_pass);
}

void release_device_resources() noexcept
{
    destroy_buffers();

    // The shader pair is not released here: it lives in sky_shader's cache, which owns it for every
    // program alike and drops it on the same unbind this is called from.
    atlas::release_device_resources();

    g_device = nullptr;
    g_failed = false;
}

bool enabled() noexcept
{
    return g_program != nullptr;
}

// One knob the layer's manifest declared with "prop" rather than "var". False when the name is not
// one this build understands, which is a mistake in the manifest and is said out loud rather than
// swallowed - a knob that silently does nothing is worse than one that is not there.
bool set_property(std::string_view name, float value) noexcept
{
    // Clamped here rather than trusted from the manifest: a slider's range is the author's
    // suggestion, and a hand-edited Config.json asking for two million sprites should get a large
    // layer rather than a failed allocation inside a draw call.
    if(name == "count") {
        g_count = std::clamp(static_cast<int>(value), 0, k_max_sprites);
        return true;
    }

    if(name == "size") {
        g_size_degrees = std::clamp(value, 0.1f, 40.f);
        return true;
    }

    if(name == "clumps") {
        g_clusters = std::clamp(static_cast<int>(value), 1, k_max_sprites);
        return true;
    }

    if(name == "spread") {
        g_spread_degrees = std::clamp(value, 0.f, 80.f);
        return true;
    }

    if(name == "scatter") {
        g_scatter = std::clamp(value, 0.f, 1.f);
        return true;
    }

    if(name == "elongation") {
        g_elongation = std::clamp(value, 1.f, 6.f);
        return true;
    }

    if(name == "floor") {
        g_floor_degrees = std::clamp(value, -20.f, 89.f);
        return true;
    }

    if(name == "ceiling") {
        g_ceiling_degrees = std::clamp(value, -19.f, 90.f);
        return true;
    }

    return false;
}

void prepare(IDirect3DDevice9* device) noexcept
{
    if(device == nullptr || g_program == nullptr) {
        return;
    }

    if(!g_program->usable() || g_program->compiling()) {
        return;
    }

    static_cast<void>(ensure_resources(device));
}

void set_layer(const sky_program* program) noexcept
{
    g_program = program;

    if(program == nullptr) {
        return;
    }

    // Properties are read here rather than pushed in one at a time, because what a `sprites` layer
    // means - which knobs it has and what they do - is this module's to know. The program carries
    // them; nothing between the two has to be taught the list.
    for(const sky_param& param : program->params) {
        if(param.is_property() && !set_property(param.property, param.value[0])) {
            TW_LOG_WARNING("sky_sprites: layer '{}' declares property '{}', which this build does not understand",
                program->package_layer,
                param.property);
        }
    }
}

int live_count() noexcept
{
    return g_built.count;
}

bool ready() noexcept
{
    return !g_failed && g_program != nullptr && g_program->usable() && !g_program->compiling();
}
} // namespace tw::skybox::sprites
