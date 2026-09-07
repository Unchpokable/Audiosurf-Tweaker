#include "pch.hxx"

#include "skybox/sky_shader.hxx"

#include "plugin/diagnostics.hxx"

#include "resource/resource.hxx"

#include "skybox/sky_program.hxx"
#include "skybox/sky_texture.hxx"

namespace
{
// One slot per program in sky_program's table, with room to grow before anyone has to think about
// it again. A linear scan over this is cheaper than a map at every size it will ever reach.
constexpr std::size_t k_max_cached = 8;

struct cache_slot {
    const tw::skybox::sky_program* program {};
    tw::skybox::shader::pair shaders {};
    bool failed {};

    // Resolved on the first draw, separately from the shaders: a layer may have no textures at all,
    // and loading a volume is a far heavier thing to do than creating a shader from bytecode already
    // in memory.
    bool textures_resolved {};
    std::vector<tw::skybox::texture::loaded> owned;
    std::vector<tw::skybox::shader::sampler> samplers;
};

IDirect3DDevice9* g_device = nullptr;
std::array<cache_slot, k_max_cached> g_cache {};
std::size_t g_cached = 0;

// Bytecode arrives as bytes and D3D9 wants DWORDs. Both origins satisfy the alignment - PE
// resources are DWORD-aligned by the resource compiler, and a std::vector's storage is
// max_align_t-aligned - but this is exactly the kind of assumption that is worth a branch rather
// than a comment, because the failure mode is a misread token stream rather than a crash.
const DWORD* as_bytecode(std::span<const std::byte> bytes, std::string_view what) noexcept
{
    if(bytes.size() < 4 || (bytes.size() % 4) != 0) {
        TW_LOG_ERROR("sky_shader: '{}' is {} bytes - not a shader blob", what, bytes.size());
        return nullptr;
    }

    if((reinterpret_cast<std::uintptr_t>(bytes.data()) % alignof(DWORD)) != 0) {
        TW_LOG_ERROR("sky_shader: '{}' is not DWORD-aligned", what);
        return nullptr;
    }

    return reinterpret_cast<const DWORD*>(bytes.data());
}

void release_slot(cache_slot& slot) noexcept
{
    if(slot.shaders.pixel != nullptr) {
        slot.shaders.pixel->Release();
    }
    if(slot.shaders.vertex != nullptr) {
        slot.shaders.vertex->Release();
    }

    for(tw::skybox::texture::loaded& one : slot.owned) {
        tw::skybox::texture::release(one);
    }

    slot = {};
}

DWORD address_of(tw::skybox::package::texture_address mode) noexcept
{
    switch(mode) {
        case tw::skybox::package::texture_address::clamp:
            return D3DTADDRESS_CLAMP;
        case tw::skybox::package::texture_address::mirror:
            return D3DTADDRESS_MIRROR;
        default:
            return D3DTADDRESS_WRAP;
    }
}

// Loads every texture the program's layer declares. Failures are logged and dropped rather than
// fatal - see the note on shader::textures().
void resolve_textures(IDirect3DDevice9* device, const tw::skybox::sky_program& program, cache_slot& slot)
{
    slot.textures_resolved = true;

    const tw::skybox::package::layer* layer = program.package_layer_ref();
    if(layer == nullptr || layer->textures.empty()) {
        return;
    }

    const tw::skybox::package::manifest* manifest = program.sky != nullptr ? program.sky->sky.get() : nullptr;
    if(manifest == nullptr) {
        return;
    }

    slot.owned.reserve(layer->textures.size());
    slot.samplers.reserve(layer->textures.size());

    for(const tw::skybox::package::texture_ref& declared : layer->textures) {
        std::vector<std::byte> bytes;
        if(!manifest->files.read(declared.path, bytes)) {
            TW_LOG_ERROR("sky_shader: '{}': could not read texture '{}' from the package", program.id, declared.path);
            continue;
        }

        tw::skybox::texture::loaded one;
        std::string error;

        if(!tw::skybox::texture::create_from_dds(device, bytes, one, error)) {
            TW_LOG_ERROR("sky_shader: '{}': texture '{}': {}", program.id, declared.path, error);
            continue;
        }

        TW_LOG_INFO("sky_shader: '{}': '{}' loaded as a {} texture, {}x{}x{}, {} level(s), into s{}",
            program.id,
            declared.path,
            tw::skybox::texture::shape_name(one.form),
            one.width,
            one.height,
            one.depth,
            one.levels,
            declared.slot);

        tw::skybox::shader::sampler bound;
        bound.slot = declared.slot;
        bound.texture = one.object;
        bound.address = address_of(declared.address);
        bound.filter = declared.filter == tw::skybox::package::texture_filter::point ? D3DTEXF_POINT : D3DTEXF_LINEAR;
        bound.mipped = one.levels > 1;

        slot.owned.push_back(one);
        slot.samplers.push_back(bound);
    }
}

// Drops every cached pointer without releasing anything. The only correct response to being handed
// a device we have never seen: if the old device is gone, its children went with it, and releasing
// through a dangling pointer would be worse than leaking. sky_renderer::ensure_resources strikes
// the same bargain, and the unbind listener is what keeps this the rare path in practice.
void forget_all() noexcept
{
    g_cache = {};
    g_cached = 0;
}

cache_slot* find_slot(const tw::skybox::sky_program& program) noexcept
{
    for(std::size_t i = 0; i < g_cached; ++i) {
        if(g_cache[i].program == &program) {
            return &g_cache[i];
        }
    }

    return nullptr;
}

bool create_shaders(IDirect3DDevice9* device, const tw::skybox::sky_program& program, tw::skybox::shader::pair& out)
{
    // Its own vertex stage when it has one, the shared cube's otherwise. Every `fullsky` sky paints
    // the same cube and all its variety is in the pixel stage; a geometry layer's vertices are quads
    // with a billboard basis baked in, and nothing else can produce those.
    const std::span<const std::byte> vertex_bytes
        = program.vertex_code.empty() ? tw::skybox::vertex_bytecode() : program.vertex_code;

    const DWORD* vertex_code = as_bytecode(vertex_bytes, program.vertex_code.empty() ? "sky_cube.vs" : program.id);
    const DWORD* pixel_code = as_bytecode(program.pixel_bytecode, program.id);
    if(vertex_code == nullptr || pixel_code == nullptr) {
        return false;
    }

    tw::skybox::shader::pair created {};

    HRESULT hr = device->CreateVertexShader(vertex_code, &created.vertex);
    if(FAILED(hr) || created.vertex == nullptr) {
        TW_LOG_ERROR("sky_shader: '{}': CreateVertexShader(vs_3_0) failed, hr=0x{:08X} - the device cannot run this shader model",
            program.id,
            static_cast<unsigned long>(hr));
        return false;
    }

    hr = device->CreatePixelShader(pixel_code, &created.pixel);
    if(FAILED(hr) || created.pixel == nullptr) {
        TW_LOG_ERROR("sky_shader: '{}': CreatePixelShader(ps_3_0) failed, hr=0x{:08X}", program.id, static_cast<unsigned long>(hr));
        created.vertex->Release();
        return false;
    }

    out = created;
    return true;
}
} // namespace

namespace tw::skybox::shader
{
pair ensure(IDirect3DDevice9* device, const sky_program& program) noexcept
{
    if(device == nullptr) {
        return {};
    }

    if(device != g_device) {
        forget_all();
        g_device = device;
    }

    cache_slot* found = find_slot(program);

    if(found != nullptr) {
        // An entry with neither shaders nor a failure is one invalidate() emptied: the program's
        // bytecode was replaced under it, and it wants rebuilding rather than reporting.
        if(found->shaders.valid()) {
            return found->shaders;
        }
        if(found->failed) {
            return {};
        }
    }
    else {
        if(g_cached >= k_max_cached) {
            TW_LOG_WARNING("sky_shader: shader cache is full at {} programs, '{}' cannot be loaded", k_max_cached, program.id);
            return {};
        }

        found = &g_cache[g_cached];
        ++g_cached;
        found->program = &program;
    }

    cache_slot& slot = *found;

    if(!create_shaders(device, program, slot.shaders)) {
        slot.failed = true;
        return {};
    }

    TW_LOG_INFO("sky_shader: '{}' created (vs_3_0 + ps_3_0, constant registers {})",
        program.id,
        tw::skybox::bytecode::describe(program.constant_runs));

    return slot.shaders;
}

std::span<const sampler> textures(IDirect3DDevice9* device, const sky_program& program) noexcept
{
    if(device == nullptr || device != g_device) {
        return {};
    }

    cache_slot* slot = find_slot(program);
    if(slot == nullptr) {
        return {};
    }

    if(!slot->textures_resolved) {
        resolve_textures(device, program, *slot);
    }

    return slot->samplers;
}

void invalidate(const sky_program& program) noexcept
{
    cache_slot* slot = find_slot(program);
    if(slot == nullptr) {
        return;
    }

    // Releasing clears the slot's program pointer too, so the entry has to be re-armed rather than
    // just emptied - otherwise the next ensure() would take a fresh slot and this one would sit
    // there matching nothing.
    release_slot(*slot);
    slot->program = &program;
}

void release_device_resources() noexcept
{
    for(std::size_t i = 0; i < g_cached; ++i) {
        release_slot(g_cache[i]);
    }

    g_cached = 0;
    g_device = nullptr;
}
} // namespace tw::skybox::shader
