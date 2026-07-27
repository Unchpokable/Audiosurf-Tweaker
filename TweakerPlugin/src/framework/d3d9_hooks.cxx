#include "pch.hxx"

#include "framework/d3d9_hooks.hxx"
#include "framework/detour_transaction.hxx"
#include "framework/wndproc_hub.hxx"

#include "plugin/diagnostics.hxx"
#include "plugin/quest3d_state.hxx"

namespace
{
using create_device_fn = long(__stdcall*)(LPDIRECT3D9, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, LPDIRECT3DDEVICE9*);
using reset_fn = long(__stdcall*)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);
using end_scene_fn = long(__stdcall*)(LPDIRECT3DDEVICE9);
using release_fn = ULONG(__stdcall*)(LPDIRECT3DDEVICE9);

create_device_fn o_create_device = nullptr;
reset_fn o_reset = nullptr;
end_scene_fn o_end_scene = nullptr;
release_fn o_release = nullptr;

tw::framework::d3d9::ui_plugin_draw_fn g_ui_draw = nullptr;
tw::framework::d3d9::device_reset_listener_fn g_ui_reset_pre = nullptr;
tw::framework::d3d9::device_reset_listener_fn g_ui_reset_post = nullptr;
tw::framework::d3d9::device_bind_fn g_ui_bind = nullptr;
tw::framework::d3d9::device_unbind_fn g_ui_unbind = nullptr;

LPDIRECT3DDEVICE9 g_bound_device = nullptr;

// The two facts about the bound device that a *different* thread needs to read.
//
// handle_activate_app runs on whichever thread owns the game window's message pump, while
// bind_device/unbind_device run on the render thread. Reaching for g_bound_device from the message
// side to re-query GetCreationParameters was a use-after-free waiting to happen: the pointer can be
// released between the null check and the call, and no amount of atomics on the pointer itself
// would fix that. Caching the two values at bind time removes the need to touch the device object
// from the window thread at all - what's left is a pair of plain scalars, and those an atomic
// genuinely does make safe.
//
// x86-only target, so both are pointer-or-smaller and always lock-free.
std::atomic<HWND> g_bound_window { nullptr };
std::atomic<bool> g_bound_windowed { true };

static_assert(std::atomic<HWND>::is_always_lock_free, "d3d9 hook state must be lock-free: it is read from a WndProc");
static_assert(std::atomic<bool>::is_always_lock_free, "d3d9 hook state must be lock-free: it is read from a WndProc");

// Reads Windowed off the device's implicit swap chain. GetCreationParameters, which is what the
// rest of bind_device uses, does not carry it - and the present parameters passed to
// CreateDevice/Reset are not available on the hk_end_scene late-load path at all, so querying the
// swap chain is the one way to answer this identically for all three bind sites. Cold path: runs
// once per bind, never per frame.
bool query_windowed(LPDIRECT3DDEVICE9 device) noexcept
{
    IDirect3DSwapChain9* swap_chain = nullptr;
    if(FAILED(device->GetSwapChain(0, &swap_chain)) || swap_chain == nullptr) {
        // Unknown - assume windowed, the conservative answer: it only suppresses the minimize
        // below, and a game that fails to minimize is a far smaller problem than a windowed game
        // that minimizes itself every time the user clicks another app.
        return true;
    }

    D3DPRESENT_PARAMETERS params {};
    const bool ok = SUCCEEDED(swap_chain->GetPresentParameters(&params));
    swap_chain->Release();

    return ok ? params.Windowed != FALSE : true;
}

// Observer only - never swallows WM_ACTIVATEAPP, it just mirrors the minimize-on-focus-loss
// behavior Quest3DTamperer's hk_wnd_proc had (see ../Quest3DTamperer/Quest3DTamperer/src/hooks/d3d9_hooks.cpp),
// which got lost when this hook was ported into TweakerPlugin.
//
// Only ever minimizes an exclusive-fullscreen device. Fullscreen is the case that actually needs
// it (a fullscreen D3D9 device that keeps the display mode while another app is in front leaves
// the desktop in a broken state); a windowed game minimizing itself the moment the user clicks the
// Tweaker desktop window is just hostile - and, since a minimized device fails
// TestCooperativeLevel, it also silently stops the overlay from drawing.
bool handle_activate_app(HWND hwnd, UINT /*msg*/, WPARAM wparam, LPARAM /*lparam*/, LRESULT& /*out_result*/)
{
    if(wparam != FALSE) {
        return false;
    }

    if(g_bound_windowed.load(std::memory_order_relaxed)) {
        return false;
    }

    if(hwnd != g_bound_window.load(std::memory_order_relaxed)) {
        return false;
    }

    TW_LOG_INFO("d3d9: WM_ACTIVATEAPP(FALSE) on fullscreen device, minimizing hwnd={}", static_cast<const void*>(hwnd));
    ::ShowWindow(hwnd, SW_MINIMIZE);

    return false;
}

void bind_device(LPDIRECT3DDEVICE9 device)
{
    D3DDEVICE_CREATION_PARAMETERS params {};
    device->GetCreationParameters(&params);

    const bool windowed = query_windowed(device);

    if(device == g_bound_device && params.hFocusWindow == g_bound_window.load(std::memory_order_relaxed)) {
        // Same device and window, but a Reset() may still have flipped windowed<->fullscreen.
        g_bound_windowed.store(windowed, std::memory_order_relaxed);
        return;
    }

    g_bound_device = device;
    g_bound_window.store(params.hFocusWindow, std::memory_order_relaxed);
    g_bound_windowed.store(windowed, std::memory_order_relaxed);
    tw::plugin::quest3d::g_game_handle = params.hFocusWindow;

    TW_LOG_INFO("d3d9: bind_device device={} hwnd={} windowed={}",
        static_cast<const void*>(device),
        static_cast<const void*>(params.hFocusWindow),
        windowed);

    tw::framework::wndproc::install(params.hFocusWindow);

    if(g_ui_bind != nullptr) {
        g_ui_bind(device, params.hFocusWindow);
    }
}

void unbind_device()
{
    TW_LOG_INFO("d3d9: unbind_device device={}", static_cast<const void*>(g_bound_device));

    // Clear our own tracking state BEFORE notifying the listener. The on_unbind listener drives
    // ImGui_ImplDX9_Shutdown(), which calls Release() on this same device - and since Release is
    // detoured, that lands straight back in hk_release(). With g_bound_device already nulled here,
    // that re-entrant call fails the `== g_bound_device` guard and simply forwards to o_release()
    // instead of recursing into a second unbind_device() - which would re-run
    // ImGui_ImplDX9_Shutdown() on an already-deleted backend and trip its IM_ASSERT(bd != nullptr).
    g_bound_device = nullptr;
    g_bound_window.store(nullptr, std::memory_order_relaxed);
    g_bound_windowed.store(true, std::memory_order_relaxed);

    tw::framework::wndproc::uninstall();

    if(g_ui_unbind != nullptr) {
        g_ui_unbind();
    }
}

bool is_rendering_to_back_buffer(LPDIRECT3DDEVICE9 device)
{
    IDirect3DSurface9* render_target = nullptr;
    if(FAILED(device->GetRenderTarget(0, &render_target)) || render_target == nullptr) {
        return false;
    }

    IDirect3DSurface9* back_buffer = nullptr;
    const bool matches = SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer)) && back_buffer == render_target;

    render_target->Release();
    if(back_buffer != nullptr) {
        back_buffer->Release();
    }

    return matches;
}

long __stdcall hk_create_device(LPDIRECT3D9 p_d3d9,
    UINT adapter,
    D3DDEVTYPE device_type,
    HWND focus_window,
    DWORD behavior_flags,
    D3DPRESENT_PARAMETERS* p_presentation_parameters,
    LPDIRECT3DDEVICE9* pp_returned_device_interface)
{
    const long result = o_create_device(
        p_d3d9, adapter, device_type, focus_window, behavior_flags, p_presentation_parameters, pp_returned_device_interface);

    if(SUCCEEDED(result) && pp_returned_device_interface != nullptr && *pp_returned_device_interface != nullptr) {
        TW_LOG_INFO("d3d9: hk_create_device succeeded, focus_window={}", static_cast<const void*>(focus_window));
        bind_device(*pp_returned_device_interface);
    }
    else if(FAILED(result)) {
        TW_LOG_WARNING("d3d9: hk_create_device failed, hr=0x{:08X}", static_cast<unsigned long>(result));
    }

    return result;
}

long __stdcall hk_reset(LPDIRECT3DDEVICE9 p_device, D3DPRESENT_PARAMETERS* p_presentation_parameters)
{
    // Reset() is a method call on an *existing* device, never how the game hands us a brand new
    // one (that's hk_create_device's job) - only touch listener/rebind state if this is actually
    // the device we're bound to right now.
    const bool bound_here = p_device == g_bound_device;

    if(bound_here && g_ui_reset_pre != nullptr) {
        g_ui_reset_pre();
    }

    if(bound_here) {
        TW_LOG_INFO("d3d9: hk_reset on bound device (windowed={})",
            p_presentation_parameters != nullptr ? (p_presentation_parameters->Windowed != FALSE) : true);
    }

    const long result = o_reset(p_device, p_presentation_parameters);

    if(bound_here && FAILED(result)) {
        TW_LOG_WARNING("d3d9: Reset failed, hr=0x{:08X} - device objects stay invalidated", static_cast<unsigned long>(result));
    }

    if(bound_here && SUCCEEDED(result)) {
        if(g_ui_reset_post != nullptr) {
            g_ui_reset_post();
        }

        // Reset() can swap the focus window (e.g. a windowed<->exclusive-fullscreen toggle)
        // without recreating the device itself - re-run bind_device() to catch that; it no-ops if
        // neither the device nor the window actually changed.
        bind_device(p_device);
    }

    return result;
}

long __stdcall hk_end_scene(LPDIRECT3DDEVICE9 p_device)
{
    if(g_bound_device == nullptr) {
        bind_device(p_device);
    }

    if(p_device == g_bound_device && p_device->TestCooperativeLevel() == D3D_OK) {
        if(g_ui_draw != nullptr && is_rendering_to_back_buffer(p_device)) {
            g_ui_draw(p_device);
        }
    }

    return o_end_scene(p_device);
}

ULONG __stdcall hk_release(LPDIRECT3DDEVICE9 p_device)
{
    // Detours patches IDirect3DDevice9::Release in place, so this fires for the Release() of *any*
    // object whose vtable slot 2 resolves to that same implementation - which, with optimized
    // builds/COM aggregation, can be objects other than our device. The `== g_bound_device` guard
    // is what keeps us from acting on such a foreign object, and it also scopes the AddRef/o_release
    // refcount probe to the one device we actually track.
    if(p_device == g_bound_device) {
        const ULONG live_refs_before_this_call = p_device->AddRef() - 1;
        o_release(p_device);

        // Instrumented on purpose. The claim this branch rests on - that we can spot the game's
        // final Release by probing the refcount - is suspect: ImGui_ImplDX9_Init takes its own
        // reference on the device (imgui_impl_dx9.cpp), so while the overlay is bound the count
        // cannot be 1 at the moment the game lets go, and unbind_device() would never run. If this
        // log never reports refs==1 across an alt-tab / fullscreen toggle / device recreation,
        // then the crash-free behavior everyone is relying on is "we never unbind" rather than
        // "we unbind at the right time", and this should be rewritten to say so explicitly.
        TW_LOG_DEBUG("d3d9: hk_release on bound device, refs_before={}", static_cast<unsigned long>(live_refs_before_this_call));

        if(live_refs_before_this_call == 1) {
            TW_LOG_INFO("d3d9: bound device reached its final Release, unbinding");
            unbind_device();
        }
    }

    return o_release(p_device);
}

bool resolve_d3d9_functions(void*& out_create_device, void*& out_reset, void*& out_end_scene, void*& out_release)
{
    HMODULE d3d9_module = GetModuleHandle(L"d3d9.dll");
    if(d3d9_module == nullptr) {
        return false;
    }

    auto direct3d_create9 = reinterpret_cast<IDirect3D9*(__stdcall*)(UINT)>(GetProcAddress(d3d9_module, "Direct3DCreate9"));
    if(direct3d_create9 == nullptr) {
        return false;
    }

    WNDCLASSEX window_class {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = DefWindowProc;
    window_class.hInstance = GetModuleHandle(nullptr);
    window_class.lpszClassName = L"TweakerPluginBootstrap";
    RegisterClassEx(&window_class);

    HWND window = CreateWindow(
        window_class.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, window_class.hInstance, nullptr);

    bool resolved = false;

    IDirect3D9* d3d9 = direct3d_create9(D3D_SDK_VERSION);
    if(d3d9 != nullptr) {
        D3DPRESENT_PARAMETERS params {};
        params.Windowed = TRUE;
        params.SwapEffect = D3DSWAPEFFECT_DISCARD;
        params.hDeviceWindow = window;
        params.BackBufferFormat = D3DFMT_UNKNOWN;

        LPDIRECT3DDEVICE9 device = nullptr;
        if(SUCCEEDED(d3d9->CreateDevice(D3DADAPTER_DEFAULT,
               D3DDEVTYPE_HAL,
               window,
               D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT,
               &params,
               &device))
            && device != nullptr) {
            void** d3d9_vtable = *reinterpret_cast<void***>(d3d9);
            void** device_vtable = *reinterpret_cast<void***>(device);

            out_create_device = d3d9_vtable[16];
            out_release = device_vtable[2];
            out_reset = device_vtable[16];
            out_end_scene = device_vtable[42];

            resolved = true;

            device->Release();
        }

        d3d9->Release();
    }

    DestroyWindow(window);
    UnregisterClass(window_class.lpszClassName, window_class.hInstance);

    return resolved;
}

} // namespace

namespace tw::framework::d3d9
{

void attach_ui_plugin(ui_plugin_draw_fn fn)
{
    g_ui_draw = fn;
}

void detach_ui_plugin()
{
    g_ui_draw = nullptr;
}

void attach_device_reset_listener(device_reset_listener_fn pre, device_reset_listener_fn post)
{
    g_ui_reset_pre = pre;
    g_ui_reset_post = post;
}

void detach_device_reset_listener()
{
    g_ui_reset_pre = nullptr;
    g_ui_reset_post = nullptr;
}

void attach_device_bind_listener(device_bind_fn on_bind, device_unbind_fn on_unbind)
{
    g_ui_bind = on_bind;
    g_ui_unbind = on_unbind;
}

void detach_device_bind_listener()
{
    g_ui_bind = nullptr;
    g_ui_unbind = nullptr;
}

bool install_d3d9_hooks()
{
    void* p_create_device = nullptr;
    void* p_reset = nullptr;
    void* p_end_scene = nullptr;
    void* p_release = nullptr;

    if(!resolve_d3d9_functions(p_create_device, p_reset, p_end_scene, p_release)) {
        TW_LOG_ERROR("d3d9: could not resolve vtable entries off a bootstrap device - no overlay this session");
        return false;
    }

    o_create_device = reinterpret_cast<create_device_fn>(p_create_device);
    o_reset = reinterpret_cast<reset_fn>(p_reset);
    o_end_scene = reinterpret_cast<end_scene_fn>(p_end_scene);
    o_release = reinterpret_cast<release_fn>(p_release);

    // Subscribe BEFORE the detours go live, not after. attach() publishes hk_end_scene to the
    // render thread the instant it commits, and that thread's very first frame runs bind_device ->
    // wndproc::install, at which point hub_wndproc starts iterating the subscriber vectors. A
    // subscribe() landing in that window pushes into a std::vector while another thread walks it -
    // reallocation under an active iteration, i.e. undefined behavior on a code path that only
    // misbehaves during the first few frames after injection.
    //
    // Subscribing early is free: subscriptions are inert until wndproc::install() runs, and if the
    // attach below fails the handler simply never fires (its own guards return false when nothing
    // is bound).
    tw::framework::wndproc::subscribe(WM_ACTIVATEAPP, &handle_activate_app);

    const bool ok = tw::framework::detour::attach({
        { reinterpret_cast<void**>(&o_create_device), reinterpret_cast<void*>(hk_create_device) },
        { reinterpret_cast<void**>(&o_reset), reinterpret_cast<void*>(hk_reset) },
        { reinterpret_cast<void**>(&o_end_scene), reinterpret_cast<void*>(hk_end_scene) },
        { reinterpret_cast<void**>(&o_release), reinterpret_cast<void*>(hk_release) },
    });

    if(!ok) {
        TW_LOG_ERROR("d3d9: DetourAttach failed - no overlay this session");
        o_create_device = nullptr;
        o_reset = nullptr;
        o_end_scene = nullptr;
        o_release = nullptr;
    }
    else {
        TW_LOG_INFO("d3d9: hooks installed (CreateDevice/Reset/EndScene/Release)");
    }

    return ok;
}

} // namespace tw::framework::d3d9
