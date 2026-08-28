#pragma once

// Creation and lifetime of the skybox's D3D9 shaders, one pair per sky program.
//
// Unlike buffers and state blocks, shaders are not pool-backed: they survive
// IDirect3DDevice9::Reset untouched and only need rebuilding when the device itself is replaced.
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

// Creates `program`'s shaders against `device` if they do not exist yet, and returns them. Returns
// an invalid pair on failure - and remembers that per program, so a program this device cannot run
// costs one attempt rather than one per frame.
//
// `program` must outlive the cache entry, which every entry in sky_program's table does.
[[nodiscard]] pair ensure(IDirect3DDevice9* device, const sky_program& program) noexcept;

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
