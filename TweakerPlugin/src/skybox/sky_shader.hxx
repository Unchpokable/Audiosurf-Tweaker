#pragma once

// Creation and lifetime of one sky program's device-side resources: its compiled shader pair, and
// the textures its layer declares.
//
// The two live together because their lifetime is one lifetime. Both are created lazily on the first
// draw against a device, both are dropped by the same invalidate() when a hot reload replaces the
// program's contents, and both are released by the same unbind listener. Splitting them into two
// modules would mean two caches keyed the same way, invalidated at the same moments, and eventually
// one of them forgotten at one of those moments.
//
// Unlike buffers and state blocks, neither is pool-backed: shaders survive IDirect3DDevice9::Reset
// untouched and the textures are D3DPOOL_MANAGED, which is the same guarantee bought differently.
// That is why there is no on_device_lost() here to match sky_renderer's.
namespace tw::skybox
{
struct sky_program;

namespace shader
{
struct pair {
    IDirect3DVertexShader9* vertex {};
    IDirect3DPixelShader9* pixel {};

    [[nodiscard]] bool valid() const noexcept
    {
        return vertex != nullptr && pixel != nullptr;
    }
};

// One texture bound to one sampler register, with the state it wants there.
//
// The sampler state is resolved to D3D constants here rather than carried as the manifest's own
// enums, so that the draw path - which should know nothing about package formats - can apply it
// without a translation step in the middle of a frame.
struct sampler {
    int slot {};
    IDirect3DBaseTexture9* texture {};

    DWORD address {}; // D3DTADDRESS_WRAP / _CLAMP / _MIRROR
    DWORD filter {};  // D3DTEXF_LINEAR / _POINT, for both min and mag

    // Whether the file brought a mip chain. Without one, asking for a mip filter is asking the
    // driver to interpolate between levels that do not exist.
    bool mipped {};
};

// Creates `program`'s shaders against `device` if they do not exist yet, and returns them. Returns
// an invalid pair on failure - and remembers that per program, so a program this device cannot run
// costs one attempt rather than one per frame.
//
// `program` must outlive the cache entry, which every entry in sky_program's table does.
[[nodiscard]] pair ensure(IDirect3DDevice9* device, const sky_program& program) noexcept;

// The textures `program`'s layer declares, loaded against `device` on first use, in the order the
// manifest listed them.
//
// Empty for a program that declares none - which is every sky written before this existed, and is
// why a layer with no `textures` block binds nothing and behaves exactly as it did.
//
// A texture that will not load is reported once and left out of the span. The layer still draws:
// the shader samples an unbound stage and gets black, which is a visible, diagnosable result, and a
// better one than a sky that vanishes because one asset was exported wrong.
//
// Must be called after ensure() for the same program - it shares the cache slot ensure() creates.
[[nodiscard]] std::span<const sampler> textures(IDirect3DDevice9* device, const sky_program& program) noexcept;

// Drops the pair cached for one program, so the next ensure() rebuilds it. This is what a hot
// reload needs: the program object stays the same (everything holds pointers to it) and only its
// bytecode changed, which the already-created IDirect3DPixelShader9 knows nothing about.
//
// Releases through the held device, so it must run while that device is alive - which it is, since
// the only caller is the reload poll and that runs inside EndScene.
void invalidate(const sky_program& program) noexcept;

// Releases every cached pair against the currently held device. Must run while that device is
// still alive, i.e. from the unbind listener.
void release_device_resources() noexcept;
} // namespace shader
} // namespace tw::skybox
