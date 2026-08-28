#include "pch.hxx"

#include "skybox/sky_shader.hxx"

#include "plugin/diagnostics.hxx"

#include "resource/resource.hxx"

#include "skybox/sky_program.hxx"

namespace
{
// One slot per program in sky_program's table, with room to grow before anyone has to think about
// it again. A linear scan over this is cheaper than a map at every size it will ever reach.
constexpr std::size_t k_max_cached = 8;

struct cache_slot {
    const tw::skybox::sky_program* program {};
    tw::skybox::shader::pair shaders {};
    bool failed {};
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

    slot = {};
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
    const DWORD* vertex_code = as_bytecode(tw::skybox::vertex_bytecode(), "sky_cube.vs");
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
