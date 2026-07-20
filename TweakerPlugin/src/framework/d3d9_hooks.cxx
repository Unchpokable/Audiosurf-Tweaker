#include "pch.hxx"

#include "framework/d3d9_hooks.hxx"
#include "framework/detour_transaction.hxx"
#include "framework/wndproc_hub.hxx"

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

LPDIRECT3DDEVICE9 g_bound_device = nullptr;
HWND g_bound_window = nullptr;

// Observer only - never swallows WM_ACTIVATEAPP, it just mirrors the minimize-on-focus-loss
// behavior Quest3DTamperer's hk_wnd_proc had (see ../Quest3DTamperer/Quest3DTamperer/src/hooks/d3d9_hooks.cpp),
// which got lost when this hook was ported into TweakerPlugin.
bool handle_activate_app(HWND hwnd, UINT /*msg*/, WPARAM wparam, LPARAM /*lparam*/, LRESULT& /*out_result*/)
{
    if(wparam == FALSE && g_bound_device != nullptr) {
        D3DDEVICE_CREATION_PARAMETERS params{};
        g_bound_device->GetCreationParameters(&params);
        if(params.hFocusWindow == hwnd) {
            ::ShowWindow(hwnd, SW_MINIMIZE);
        }
    }

    return false;
}

void bind_device(LPDIRECT3DDEVICE9 device)
{
    D3DDEVICE_CREATION_PARAMETERS params{};
    device->GetCreationParameters(&params);

    if(device == g_bound_device && params.hFocusWindow == g_bound_window) {
        return;
    }

    g_bound_device = device;
    g_bound_window = params.hFocusWindow;
    tw::plugin::quest3d::g_game_handle = params.hFocusWindow;

    tw::framework::wndproc::install(params.hFocusWindow);
}

void unbind_device()
{
    g_bound_device = nullptr;
    g_bound_window = nullptr;

    tw::framework::wndproc::uninstall();
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

long __stdcall hk_create_device(LPDIRECT3D9 p_d3d9, UINT adapter, D3DDEVTYPE device_type, HWND focus_window, DWORD behavior_flags,
    D3DPRESENT_PARAMETERS* p_presentation_parameters, LPDIRECT3DDEVICE9* pp_returned_device_interface)
{
    const long result = o_create_device(p_d3d9, adapter, device_type, focus_window, behavior_flags, p_presentation_parameters, pp_returned_device_interface);

    if(SUCCEEDED(result) && pp_returned_device_interface != nullptr && *pp_returned_device_interface != nullptr) {
        bind_device(*pp_returned_device_interface);
    }

    return result;
}

long __stdcall hk_reset(LPDIRECT3DDEVICE9 p_device, D3DPRESENT_PARAMETERS* p_presentation_parameters)
{
  return o_reset(p_device, p_presentation_parameters);
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
    if(p_device == g_bound_device) {
        const ULONG live_refs_before_this_call = p_device->AddRef() - 1;
        o_release(p_device);

        if(live_refs_before_this_call == 1) {
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

    WNDCLASSEX window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = DefWindowProc;
    window_class.hInstance = GetModuleHandle(nullptr);
    window_class.lpszClassName = L"TweakerPluginBootstrap";
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

    const bool ok = tw::framework::detour::attach({
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
    } else {
        tw::framework::wndproc::subscribe(WM_ACTIVATEAPP, &handle_activate_app);
    }

    return ok;
}

} // namespace tw::framework::d3d9
