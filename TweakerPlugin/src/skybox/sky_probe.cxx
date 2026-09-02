#include "pch.hxx"

#include "skybox/sky_probe.hxx"

#include "plugin/diagnostics.hxx"

namespace
{
tw::skybox::probe::facts g_facts {};

int g_draws_this_frame = 0;

// True at startup and after every device bind, so the first sky draw of a session always describes
// itself without anybody having to ask.
bool g_want_snapshot = true;

// The snapshot is worth reading in the log as well as in the overlay: the overlay is only open when
// somebody is already looking, and the interesting failure mode - a second render target, software
// vertex processing - is the kind of thing a user reports from a log after the fact.
bool g_logged = false;

template<typename T>
void release_if(T* resource) noexcept
{
    if(resource != nullptr) {
        resource->Release();
    }
}

void capture_surfaces(IDirect3DDevice9* device, tw::skybox::probe::facts& out) noexcept
{
    IDirect3DSurface9* target = nullptr;
    if(FAILED(device->GetRenderTarget(0, &target)) || target == nullptr) {
        return;
    }

    D3DSURFACE_DESC desc {};
    if(SUCCEEDED(target->GetDesc(&desc))) {
        out.target_width = static_cast<int>(desc.Width);
        out.target_height = static_cast<int>(desc.Height);
    }

    IDirect3DSurface9* back_buffer = nullptr;
    if(SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer))) {
        out.on_back_buffer = back_buffer == target;
        release_if(back_buffer);
    }

    target->Release();
}

void take_snapshot(IDirect3DDevice9* device) noexcept
{
    tw::skybox::probe::facts& out = g_facts;

    D3DVIEWPORT9 viewport {};
    if(SUCCEEDED(device->GetViewport(&viewport))) {
        out.viewport_width = static_cast<int>(viewport.Width);
        out.viewport_height = static_cast<int>(viewport.Height);
    }

    capture_surfaces(device, out);

    // A device with no depth buffer returns failure rather than a null surface, so both have to
    // count as "none" - the same asymmetry sky_target::begin already deals with.
    IDirect3DSurface9* depth = nullptr;
    out.depth_bound = SUCCEEDED(device->GetDepthStencilSurface(&depth)) && depth != nullptr;
    release_if(depth);

    out.software_vertex_processing = device->GetSoftwareVertexProcessing() != FALSE;

    DWORD value = 0;
    if(SUCCEEDED(device->GetRenderState(D3DRS_ALPHABLENDENABLE, &value))) {
        out.alpha_blend_enabled = value != FALSE;
    }
    if(SUCCEEDED(device->GetRenderState(D3DRS_SRCBLEND, &value))) {
        out.src_blend = value;
    }
    if(SUCCEEDED(device->GetRenderState(D3DRS_DESTBLEND, &value))) {
        out.dest_blend = value;
    }

    out.captured = true;
}
} // namespace

namespace tw::skybox::probe
{
void observe(IDirect3DDevice9* device) noexcept
{
    ++g_draws_this_frame;

    if(!g_want_snapshot) [[likely]] {
        return;
    }
    g_want_snapshot = false;

    take_snapshot(device);

    if(g_logged) {
        return;
    }
    g_logged = true;

    TW_LOG_INFO("sky_probe: intercepted sky draw - render target {}x{} ({}), viewport {}x{}, depth buffer {}",
        g_facts.target_width,
        g_facts.target_height,
        g_facts.on_back_buffer ? "the back buffer" : "NOT the back buffer",
        g_facts.viewport_width,
        g_facts.viewport_height,
        g_facts.depth_bound ? "bound" : "none");
    TW_LOG_INFO("sky_probe: vertex processing {}, game had alpha blend {} (src={}, dest={})",
        g_facts.software_vertex_processing ? "SOFTWARE" : "hardware",
        g_facts.alpha_blend_enabled ? "on" : "off",
        g_facts.src_blend,
        g_facts.dest_blend);
}

void on_frame() noexcept
{
    // A tick that carried no sky draw is not a frame boundary worth reporting, and saying so is the
    // difference between a working counter and a lying one.
    //
    // This runs from EndScene, and EndScene is not once per presented frame: measured in the game,
    // the count read back as "0 this frame, 1 at peak", which only happens if a second EndScene
    // arrives after the sky has already been counted and rolls a zero over the top of it. Skipping
    // the empty ticks makes the number describe the last frame that actually drew sky.
    //
    // The cost is that a genuine stop - a menu screen with no song - freezes the last count instead
    // of falling to zero. Acceptable: the peak is the number this exists to report.
    if(g_draws_this_frame == 0) {
        return;
    }

    g_facts.draws_last_frame = g_draws_this_frame;
    if(g_draws_this_frame > g_facts.draws_peak) {
        g_facts.draws_peak = g_draws_this_frame;
    }

    g_draws_this_frame = 0;
}

void discard_frame() noexcept
{
    g_draws_this_frame = 0;
}

void request_snapshot() noexcept
{
    g_want_snapshot = true;
}

void reset() noexcept
{
    g_facts = {};
    g_draws_this_frame = 0;
    g_want_snapshot = true;
    g_logged = false;
}

facts current() noexcept
{
    return g_facts;
}
} // namespace tw::skybox::probe
