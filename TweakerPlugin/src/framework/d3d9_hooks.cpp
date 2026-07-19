#include "pch.h"

#include "hooks/d3d9_hooks.h"

#include "hooks/detour_transaction.h"

#include "quest3d/state.h"

#include "ui/tamperer_window.h"

// imgui_impl_win32.h intentionally omits this declaration (to avoid forcing
// <windows.h> on every consumer) and asks callers to copy it in manually.
// NOTE: must keep this exact name - it's the real symbol imgui exports, not
// ours to rename to match our naming convention.
// NOLINTNEXTLINE(readability-identifier-naming)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND h_wnd, UINT msg, WPARAM w_param, LPARAM l_param);

namespace
{
// All D3D9 interface methods are COM: __stdcall with the instance pointer as
// an explicit first stack argument (not __thiscall/ECX), so plain function
// pointers with this signature can call straight into a resolved vtable slot.
using create_device_fn = long(__stdcall*)(LPDIRECT3D9, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, LPDIRECT3DDEVICE9*);
using reset_fn = long(__stdcall*)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);
using end_scene_fn = long(__stdcall*)(LPDIRECT3DDEVICE9);
using release_fn = ULONG(__stdcall*)(LPDIRECT3DDEVICE9);

create_device_fn o_create_device = nullptr;
reset_fn o_reset = nullptr;
end_scene_fn o_end_scene = nullptr;
release_fn o_release = nullptr;
WNDPROC g_wnd_proc_original = nullptr;

// The device/window ImGui is currently bound to. Null until the first
// hk_create_device() call succeeds. Quest3D can tear its D3D9 device down and
// build a brand new one - not just Reset() the existing one - e.g. across
// certain fullscreen transitions, which can also mean a different focus
// window. bind_device()/unbind_imgui() retarget or release both in response
// to the actual CreateDevice/Reset/Release calls the game makes, rather than
// inferring a change from stale state on the next EndScene().
LPDIRECT3DDEVICE9 g_bound_device = nullptr;
HWND g_bound_window = nullptr;

LRESULT __stdcall CALLBACK hk_wnd_proc(HWND h_wnd, UINT u_msg, WPARAM w_param, LPARAM l_param)
{
    if(u_msg == WM_ACTIVATEAPP && w_param == FALSE && g_bound_device != nullptr) {
        D3DDEVICE_CREATION_PARAMETERS params;
        g_bound_device->GetCreationParameters(&params);
        if(params.hFocusWindow == h_wnd) {
            ShowWindow(h_wnd, SW_MINIMIZE);
        }
    }

    if(ImGui_ImplWin32_WndProcHandler(h_wnd, u_msg, w_param, l_param) && hooks::g_show_menu)
        return true;

    return CallWindowProc(g_wnd_proc_original, h_wnd, u_msg, w_param, l_param);
}

void hook_window(HWND window)
{
    g_wnd_proc_original = (WNDPROC)SetWindowLong(window, GWL_WNDPROC, (LRESULT)hk_wnd_proc);
    quest3d::g_game_handle = window;
}

// Tears down whatever ImGui is currently bound to, if anything. Does not
// touch g_bound_device itself beyond comparing/clearing the pointer - by the
// time this runs from hk_release() the device has already been freed, so it
// must never be dereferenced here.
void unbind_imgui()
{
    if(g_bound_device == nullptr) {
        return;
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();

    if(g_bound_window != nullptr) {
        SetWindowLong(g_bound_window, GWL_WNDPROC, (LRESULT)g_wnd_proc_original);
    }

    g_bound_device = nullptr;
    g_bound_window = nullptr;
}

// (Re)targets ImGui at `device`. Cheap no-op if it's already bound to this
// exact device/window pair. Called from hk_create_device() (a genuinely new
// device), and from hk_reset() after a successful Reset() (which can change
// the focus window - e.g. a windowed<->exclusive-fullscreen toggle - without
// tearing the device down).
void bind_device(LPDIRECT3DDEVICE9 device)
{
    D3DDEVICE_CREATION_PARAMETERS params;
    device->GetCreationParameters(&params);

    if(device == g_bound_device && params.hFocusWindow == g_bound_window) {
        return;
    }

    const bool first_bind = g_bound_device == nullptr;
    unbind_imgui();

    if(first_bind) {
        ImGui::CreateContext();
    }

    ImGui_ImplWin32_Init(params.hFocusWindow);
    ImGui_ImplDX9_Init(device);
    ui::init_file_dialogs();

    hook_window(params.hFocusWindow);

    g_bound_device = device;
    g_bound_window = params.hFocusWindow;
}

// Quest3D can call EndScene() more than once per frame - once per
// intermediate render target (shadow maps, reflections, other channel-driven
// render-to-texture passes) before the pass that actually targets the
// swapchain's back buffer. Drawing the overlay unconditionally on every
// EndScene() is exactly how it ends up rendered "somewhere else": onto
// whichever of those intermediate surfaces happened to be bound at the time.
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

long __stdcall hk_create_device(LPDIRECT3D9 p_d3d9, UINT adapter, D3DDEVTYPE device_type, HWND focus_window, DWORD behavior_flags,
    D3DPRESENT_PARAMETERS* p_presentation_parameters, LPDIRECT3DDEVICE9* pp_returned_device_interface)
{
    const long result = o_create_device(p_d3d9, adapter, device_type, focus_window, behavior_flags, p_presentation_parameters, pp_returned_device_interface);

    // This fires for every brand new device the game creates, not just a
    // Reset() of an existing one - Quest3D isn't guaranteed to release an old
    // device before creating its replacement, so bind_device() (not an
    // unbind first) is correct here regardless of ordering.
    if(SUCCEEDED(result) && pp_returned_device_interface != nullptr && *pp_returned_device_interface != nullptr) {
        bind_device(*pp_returned_device_interface);
    }

    return result;
}

long __stdcall hk_reset(LPDIRECT3DDEVICE9 p_device, D3DPRESENT_PARAMETERS* p_presentation_parameters)
{
    // Reset() is a method call on an *existing* device - it's never how the
    // game hands us a brand new one (that's hk_create_device's job). Only
    // touch device objects if ImGui is actually bound to this particular
    // device right now.
    const bool imgui_bound_here = p_device == g_bound_device;
    if(imgui_bound_here) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }

    const long result = o_reset(p_device, p_presentation_parameters);

    // Device objects can only be (re)created once the device has actually
    // come back. If Reset() itself failed - still lost, wrong parameters,
    // whatever - leave them invalidated; hk_end_scene() checks
    // TestCooperativeLevel() every frame and simply won't render ImGui until
    // a later Reset() succeeds and gets us back here.
    if(imgui_bound_here && SUCCEEDED(result)) {
        ImGui_ImplDX9_CreateDeviceObjects();

        // Reset() can also swap the device's focus window (e.g. a
        // windowed<->exclusive-fullscreen toggle) without recreating the
        // device itself - bind_device() re-hooks the window if it changed
        // and is a no-op otherwise.
        bind_device(p_device);
    }

    return result;
}

long __stdcall hk_end_scene(LPDIRECT3DDEVICE9 p_device)
{
    // late load
    if(g_bound_device == nullptr) {
        bind_device(p_device);
    }

    // Polled here rather than on a separate thread: EndScene already runs
    // once per frame on the game's render thread, which naturally throttles
    // GetAsyncKeyState() and avoids racing hooks::g_show_menu against
    // hk_wnd_proc()/the drawing below.
    if(GetAsyncKeyState(VK_END) & 1) {
        hooks::g_show_menu = !hooks::g_show_menu;
    }

    // Only ever draw onto the device ImGui is actually bound to - hk_create_device()
    // guarantees that's set before any EndScene() call can reach us for it.
    if(p_device == g_bound_device && p_device->TestCooperativeLevel() == D3D_OK) {
        if(hooks::g_show_menu && is_rendering_to_back_buffer(p_device)) {
            ImGui_ImplDX9_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            // Audiosurf has its own cursor which renders below the ImGui window,
            // and it ignores all other windows above it and registers clicks
            // through them. No workaround for this (yet) - this is
            // Audiosurf/Quest3D's fault, not ours.
            ImGui::GetIO().MouseDrawCursor = true;

            ui::draw_tamper_window();

            ImGui::EndFrame();
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        }
    }

    return o_end_scene(p_device);
}

ULONG __stdcall hk_release(LPDIRECT3DDEVICE9 p_device)
{
    if(p_device == g_bound_device) {
        const ULONG live_refs_before_this_call = p_device->AddRef() - 1;
        o_release(p_device);

        if(live_refs_before_this_call == 1) {
            unbind_imgui();
        }
    }

    return o_release(p_device);
}

// Creates a throwaway D3D9 object + device purely to read out the live
// vtable slots for IDirect3D9::CreateDevice and IDirect3DDevice9::Release/
// Reset/EndScene, then tears both down again. Mirrors the bootstrap kiero
// used to do internally (same device creation flags), just done directly
// with Detours instead of going through a third-party wrapper.
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

    WNDCLASSEX window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = DefWindowProc;
    window_class.hInstance = GetModuleHandle(nullptr);
    window_class.lpszClassName = L"Quest3DTampererBootstrap";
    RegisterClassEx(&window_class);

    HWND window = CreateWindow(window_class.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, window_class.hInstance, nullptr);

    bool resolved = false;

    IDirect3D9* d3d9 = direct3d_create9(D3D_SDK_VERSION);
    if(d3d9 != nullptr) {
        D3DPRESENT_PARAMETERS params{};
        params.Windowed = TRUE;
        params.SwapEffect = D3DSWAPEFFECT_DISCARD;
        params.hDeviceWindow = window;
        params.BackBufferFormat = D3DFMT_UNKNOWN;

        LPDIRECT3DDEVICE9 device = nullptr;
        // SOFTWARE_VERTEXPROCESSING + DISABLE_DRIVER_MANAGEMENT: this device
        // only ever exists long enough to read its vtable - it's never
        // actually rendered to.
        if(SUCCEEDED(d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT, &params, &device)) &&
            device != nullptr) {
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

namespace hooks
{
bool install_d3d9_hooks()
{
    void* p_create_device = nullptr;
    void* p_reset = nullptr;
    void* p_end_scene = nullptr;
    void* p_release = nullptr;

    if(!resolve_d3d9_functions(p_create_device, p_reset, p_end_scene, p_release)) {
        return false;
    }

    o_create_device = reinterpret_cast<create_device_fn>(p_create_device);
    o_reset = reinterpret_cast<reset_fn>(p_reset);
    o_end_scene = reinterpret_cast<end_scene_fn>(p_end_scene);
    o_release = reinterpret_cast<release_fn>(p_release);

    const bool ok = detour::attach({
        {reinterpret_cast<void**>(&o_create_device), reinterpret_cast<void*>(hk_create_device)},
        {reinterpret_cast<void**>(&o_reset),         reinterpret_cast<void*>(hk_reset)        },
        {reinterpret_cast<void**>(&o_end_scene),     reinterpret_cast<void*>(hk_end_scene)    },
        {reinterpret_cast<void**>(&o_release),       reinterpret_cast<void*>(hk_release)      },
    });

    if(!ok) {
        o_create_device = nullptr;
        o_reset = nullptr;
        o_end_scene = nullptr;
        o_release = nullptr;
    }

    return ok;
}

} // namespace hooks
