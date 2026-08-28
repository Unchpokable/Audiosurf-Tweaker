#include "pch.hxx"

#include "skybox/sky_timer.hxx"

#include "plugin/diagnostics.hxx"

namespace
{
IDirect3DDevice9* g_device = nullptr;

IDirect3DQuery9* g_disjoint = nullptr;
IDirect3DQuery9* g_frequency = nullptr;
IDirect3DQuery9* g_start = nullptr;
IDirect3DQuery9* g_stop = nullptr;

// Set when the device turns down a timestamp query, so an unsupported device costs one attempt
// rather than one per frame.
bool g_unsupported = false;

// Follows the overlay. Nothing reads the result while it is closed, and a query pair issued into
// that silence is pure overhead - small, but it is also the only thing this module does.
bool g_enabled = false;

// True between end() and the frame that reads the result back. Nothing new is measured while a
// measurement is in flight, which is what keeps GetData from ever having to block.
bool g_pending = false;
bool g_open = false;

float g_average_us = 0.f;

template<typename T>
void release_and_clear(T*& query) noexcept
{
    if(query != nullptr) {
        query->Release();
        query = nullptr;
    }
}

void release_all() noexcept
{
    release_and_clear(g_disjoint);
    release_and_clear(g_frequency);
    release_and_clear(g_start);
    release_and_clear(g_stop);

    g_pending = false;
    g_open = false;
}

bool create_queries(IDirect3DDevice9* device)
{
    const bool ok = SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, &g_disjoint))
                    && SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &g_frequency))
                    && SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &g_start))
                    && SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &g_stop));

    if(!ok || g_disjoint == nullptr || g_frequency == nullptr || g_start == nullptr || g_stop == nullptr) {
        TW_LOG_INFO("sky_timer: this device does not do timestamp queries - the sky draw will not be timed");
        release_all();
        return false;
    }

    return true;
}

// Non-blocking read of the pair issued on some earlier frame. Returns false while the GPU has not
// caught up, which on a fast card is most frames.
bool collect()
{
    BOOL disjoint = FALSE;
    UINT64 frequency = 0;
    UINT64 start = 0;
    UINT64 stop = 0;

    if(g_disjoint->GetData(&disjoint, sizeof(disjoint), 0) != S_OK || g_frequency->GetData(&frequency, sizeof(frequency), 0) != S_OK
        || g_start->GetData(&start, sizeof(start), 0) != S_OK || g_stop->GetData(&stop, sizeof(stop), 0) != S_OK) {
        return false;
    }

    g_pending = false;

    // Disjoint means the GPU clock changed mid-measurement (a power state transition, typically) and
    // the two timestamps are not comparable. Discarding is the only correct response.
    if(disjoint || frequency == 0 || stop <= start) {
        return true;
    }

    const double seconds = static_cast<double>(stop - start) / static_cast<double>(frequency);
    const auto microseconds = static_cast<float>(seconds * 1e6);

    // Exponential smoothing rather than a ring buffer: the number is read by a human off a UI label,
    // and one float of state beats sixty.
    constexpr float k_alpha = 0.1f;
    g_average_us = g_average_us == 0.f ? microseconds : g_average_us + k_alpha * (microseconds - g_average_us);

    return true;
}
} // namespace

namespace tw::skybox::timer
{
void set_enabled(bool value) noexcept
{
    g_enabled = value;
}

void begin(IDirect3DDevice9* device) noexcept
{
    g_open = false;

    if(device == nullptr || g_unsupported || !g_enabled) {
        return;
    }

    if(device != g_device) {
        // A device we have never seen. Nothing can be released through the old one - if it is gone,
        // its queries went with it - so the pointers are dropped, the same bargain sky_renderer and
        // sky_shader strike.
        g_disjoint = nullptr;
        g_frequency = nullptr;
        g_start = nullptr;
        g_stop = nullptr;
        g_pending = false;
        g_device = device;
    }

    if(g_disjoint == nullptr && !create_queries(device)) {
        g_unsupported = true;
        return;
    }

    if(g_pending && !collect()) {
        // Still in flight. Reissuing now would overwrite the very result we are waiting for.
        return;
    }

    g_disjoint->Issue(D3DISSUE_BEGIN);
    g_frequency->Issue(D3DISSUE_END);
    g_start->Issue(D3DISSUE_END);

    g_open = true;
}

void end() noexcept
{
    if(!g_open) {
        return;
    }

    g_stop->Issue(D3DISSUE_END);
    g_disjoint->Issue(D3DISSUE_END);

    g_open = false;
    g_pending = true;
}

float average_microseconds() noexcept
{
    return g_average_us;
}

void on_device_lost() noexcept
{
    release_all();
}

void release_device_resources() noexcept
{
    release_all();

    g_device = nullptr;
    g_unsupported = false;
    g_average_us = 0.f;
}
} // namespace tw::skybox::timer
