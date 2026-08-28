#include "pch.hxx"

#include "skybox/sky_target.hxx"

#include "plugin/diagnostics.hxx"

namespace
{
// Pre-transformed vertices for the blit back. XYZRHW skips every transform stage, so the quad needs
// no matrices and cannot be disturbed by whatever the game left in them.
struct blit_vertex {
    float x;
    float y;
    float z;
    float rhw;
    float u;
    float v;
};

constexpr DWORD k_blit_fvf = D3DFVF_XYZRHW | D3DFVF_TEX1;

IDirect3DDevice9* g_device = nullptr;
IDirect3DTexture9* g_texture = nullptr;
IDirect3DSurface9* g_surface = nullptr;
int g_width = 0;
int g_height = 0;

// Held between begin() and end(). GetRenderTarget and GetDepthStencilSurface both AddRef, so these
// are owning pointers for exactly that span.
IDirect3DSurface9* g_saved_target = nullptr;
IDirect3DSurface9* g_saved_depth = nullptr;
D3DVIEWPORT9 g_saved_viewport {};
bool g_active = false;

// One failure is enough. A device that will not give us a render target will not start giving us
// one halfway through a song, and retrying every frame would cost more than the feature saves.
bool g_failed = false;

template<typename T>
void release_and_clear(T*& resource) noexcept
{
    if(resource != nullptr) {
        resource->Release();
        resource = nullptr;
    }
}

void destroy_target() noexcept
{
    release_and_clear(g_surface);
    release_and_clear(g_texture);

    g_width = 0;
    g_height = 0;
}

bool ensure_target(IDirect3DDevice9* device, int width, int height)
{
    if(g_texture != nullptr && g_width == width && g_height == height) {
        return true;
    }

    destroy_target();

    // D3DPOOL_DEFAULT is not a choice - render targets have no other pool. It is also the reason
    // this costs no system memory at all, unlike the managed cube maps on the other path.
    const HRESULT hr = device->CreateTexture(static_cast<UINT>(width),
        static_cast<UINT>(height),
        1,
        D3DUSAGE_RENDERTARGET,
        D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT,
        &g_texture,
        nullptr);

    if(FAILED(hr) || g_texture == nullptr) {
        TW_LOG_ERROR("sky_target: CreateTexture({}x{}, RENDERTARGET) failed, hr=0x{:08X}", width, height, static_cast<unsigned long>(hr));
        destroy_target();
        return false;
    }

    if(FAILED(g_texture->GetSurfaceLevel(0, &g_surface)) || g_surface == nullptr) {
        TW_LOG_ERROR("sky_target: GetSurfaceLevel failed");
        destroy_target();
        return false;
    }

    g_width = width;
    g_height = height;

    TW_LOG_INFO("sky_target: rendering the sky at {}x{}", width, height);

    return true;
}

void draw_blit(IDirect3DDevice9* device)
{
    const auto left = static_cast<float>(g_saved_viewport.X);
    const auto top = static_cast<float>(g_saved_viewport.Y);
    const auto right = left + static_cast<float>(g_saved_viewport.Width);
    const auto bottom = top + static_cast<float>(g_saved_viewport.Height);

    // The half-pixel shift is the usual D3D9 correction: pre-transformed coordinates address pixel
    // corners while sampling addresses texel centres, and without it a stretched image lands half a
    // destination pixel to the right and down.
    constexpr float k_half_pixel = 0.5f;

    const std::array<blit_vertex, 4> quad { {
        { left - k_half_pixel, top - k_half_pixel, 0.f, 1.f, 0.f, 0.f },
        { right - k_half_pixel, top - k_half_pixel, 0.f, 1.f, 1.f, 0.f },
        { left - k_half_pixel, bottom - k_half_pixel, 0.f, 1.f, 0.f, 1.f },
        { right - k_half_pixel, bottom - k_half_pixel, 0.f, 1.f, 1.f, 1.f },
    } };

    device->SetVertexShader(nullptr);
    device->SetPixelShader(nullptr);
    device->SetFVF(k_blit_fvf);

    device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

    device->SetTexture(0, g_texture);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    // Linear on the way up is the whole point - a point-sampled upscale would show the reduced
    // resolution as blocks, which is exactly what this is trying not to do.
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);

    // DrawPrimitiveUP rather than a vertex buffer of our own: four vertices, once a frame, and the
    // alternative is another D3DPOOL_MANAGED resource to keep alive across device changes.
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad.data(), sizeof(blit_vertex));

    device->SetTexture(0, nullptr);
}
} // namespace

namespace tw::skybox::target
{
bool begin(IDirect3DDevice9* device, int scale_percent) noexcept
{
    g_active = false;

    if(device == nullptr || scale_percent >= 100 || g_failed) {
        return false;
    }

    if(device != g_device) {
        // A device we have never seen. Nothing can be released through the old one, so the pointers
        // are dropped - the same bargain the renderer and the shader cache strike.
        g_texture = nullptr;
        g_surface = nullptr;
        g_width = 0;
        g_height = 0;
        g_failed = false;
        g_device = device;
    }

    if(FAILED(device->GetViewport(&g_saved_viewport)) || g_saved_viewport.Width == 0 || g_saved_viewport.Height == 0) {
        return false;
    }

    const int width = (std::max)(1, static_cast<int>(g_saved_viewport.Width) * scale_percent / 100);
    const int height = (std::max)(1, static_cast<int>(g_saved_viewport.Height) * scale_percent / 100);

    if(!ensure_target(device, width, height)) {
        g_failed = true;
        return false;
    }

    if(FAILED(device->GetRenderTarget(0, &g_saved_target)) || g_saved_target == nullptr) {
        release_and_clear(g_saved_target);
        return false;
    }

    // A device without a depth buffer is legal and returns failure here rather than null, so this
    // is not an error - it just means there is nothing to put back.
    if(FAILED(device->GetDepthStencilSurface(&g_saved_depth))) {
        g_saved_depth = nullptr;
    }

    if(FAILED(device->SetRenderTarget(0, g_surface))) {
        release_and_clear(g_saved_depth);
        release_and_clear(g_saved_target);
        return false;
    }

    // The sky needs no depth buffer, and the game's is the wrong size for this target anyway.
    device->SetDepthStencilSurface(nullptr);

    // The cube covers the whole viewport, so this clear is not strictly needed. It costs a fraction
    // of a millisecond at reduced resolution and removes a whole class of "what is that at the
    // edge" from ever being a question.
    device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.f, 0);

    g_active = true;

    return true;
}

void end(IDirect3DDevice9* device) noexcept
{
    if(!g_active || device == nullptr) {
        return;
    }
    g_active = false;

    device->SetRenderTarget(0, g_saved_target);
    device->SetDepthStencilSurface(g_saved_depth);

    // SetRenderTarget resets the viewport to the full surface, so the game's has to go back by hand
    // - and it has to go back before the blit, which is measured in its coordinates.
    device->SetViewport(&g_saved_viewport);

    release_and_clear(g_saved_depth);
    release_and_clear(g_saved_target);

    draw_blit(device);
}

void on_device_lost() noexcept
{
    // Whatever begin() was holding cannot survive either, and releasing it here is what keeps a
    // Reset from failing on an outstanding reference.
    release_and_clear(g_saved_depth);
    release_and_clear(g_saved_target);
    g_active = false;

    destroy_target();
}

void release_device_resources() noexcept
{
    on_device_lost();

    g_device = nullptr;
    g_failed = false;
}
} // namespace tw::skybox::target
