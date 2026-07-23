#include "pch.hxx"

#include "framework/dinput8_hooks.hxx"

#include "framework/detour_transaction.hxx"

namespace
{
constexpr std::ptrdiff_t k_get_device_state_idx = 9;
constexpr std::ptrdiff_t k_get_device_data_idx = 10;
} // namespace

namespace
{
using get_device_state_fn_ansi = HRESULT(__stdcall*)(IDirectInputDevice8A*, DWORD, LPVOID);
using get_device_data_fn_ansi = HRESULT(__stdcall*)(IDirectInputDevice8A*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);

using get_device_state_fn_wide = HRESULT(__stdcall*)(IDirectInputDevice8W*, DWORD, LPVOID);
using get_device_data_fn_wide = HRESULT(__stdcall*)(IDirectInputDevice8W*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);
} // namespace

namespace
{
get_device_state_fn_ansi o_get_device_state_ansi { nullptr };
get_device_data_fn_ansi o_get_device_data_ansi { nullptr };

get_device_state_fn_wide o_get_device_state_wide { nullptr };
get_device_data_fn_wide o_get_device_data_wide { nullptr };

tw::framework::dinput::input_gate_fn g_input_gate { nullptr };

bool input_suppressed() noexcept
{
    return g_input_gate != nullptr && g_input_gate();
}
} // namespace

namespace
{
HRESULT __stdcall hk_get_device_state_ansi(IDirectInputDevice8A* this_ptr, DWORD cb_data, LPVOID data)
{
    const HRESULT hr = o_get_device_state_ansi(this_ptr, cb_data, data);
    if(SUCCEEDED(hr) && data != nullptr && input_suppressed()) {
        std::memset(data, 0, cb_data);
    }

    return hr;
}

HRESULT __stdcall hk_get_device_data_ansi(
    IDirectInputDevice8A* this_ptr, DWORD cb_object_data, LPDIDEVICEOBJECTDATA device_object_data, LPDWORD in_out, DWORD flags)
{
    const HRESULT hr = o_get_device_data_ansi(this_ptr, cb_object_data, device_object_data, in_out, flags);
    if(SUCCEEDED(hr) && in_out != nullptr && input_suppressed()) {
        *in_out = 0;
    }

    return hr;
}

HRESULT __stdcall hk_get_device_state_wide(IDirectInputDevice8W* this_ptr, DWORD cb_data, LPVOID data)
{
    const HRESULT hr = o_get_device_state_wide(this_ptr, cb_data, data);
    if(SUCCEEDED(hr) && data != nullptr && input_suppressed()) {
        std::memset(data, 0, cb_data);
    }

    return hr;
}

HRESULT __stdcall hk_get_device_data_wide(
    IDirectInputDevice8W* this_ptr, DWORD cb_object_data, LPDIDEVICEOBJECTDATA device_object_data, LPDWORD in_out, DWORD flags)
{
    const HRESULT hr = o_get_device_data_wide(this_ptr, cb_object_data, device_object_data, in_out, flags);
    if(SUCCEEDED(hr) && in_out != nullptr && input_suppressed()) {
        *in_out = 0;
    }

    return hr;
}
} // namespace

namespace
{
bool setup_hooks_ansi(IDirectInputDevice8A* device)
{
    void** vtable = *reinterpret_cast<void***>(device);

    o_get_device_state_ansi = reinterpret_cast<get_device_state_fn_ansi>(vtable[k_get_device_state_idx]);
    o_get_device_data_ansi = reinterpret_cast<get_device_data_fn_ansi>(vtable[k_get_device_data_idx]);

    const bool ok = tw::framework::detour::attach({
        { reinterpret_cast<void**>(&o_get_device_state_ansi), reinterpret_cast<void*>(&hk_get_device_state_ansi) },
        { reinterpret_cast<void**>(&o_get_device_data_ansi), reinterpret_cast<void*>(&hk_get_device_data_ansi) },
    });

    if(!ok) {
        o_get_device_state_ansi = nullptr;
        o_get_device_data_ansi = nullptr;
    }

    return ok;
}

bool setup_hooks_wide(IDirectInputDevice8W* device)
{
    void** vtable = *reinterpret_cast<void***>(device);

    o_get_device_state_wide = reinterpret_cast<get_device_state_fn_wide>(vtable[k_get_device_state_idx]);
    o_get_device_data_wide = reinterpret_cast<get_device_data_fn_wide>(vtable[k_get_device_data_idx]);

    const bool ok = tw::framework::detour::attach({
        { reinterpret_cast<void**>(&o_get_device_state_wide), reinterpret_cast<void*>(&hk_get_device_state_wide) },
        { reinterpret_cast<void**>(&o_get_device_data_wide), reinterpret_cast<void*>(&hk_get_device_data_wide) },
    });

    if(!ok) {
        o_get_device_state_wide = nullptr;
        o_get_device_data_wide = nullptr;
    }

    return ok;
}

bool setup_for_ansi()
{
    IDirectInput8A* dummy { nullptr };
    const HRESULT create_result =
        DirectInput8Create(GetModuleHandleA(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8A, reinterpret_cast<void**>(&dummy), nullptr);

    if(FAILED(create_result) || dummy == nullptr) {
        return false;
    }

    IDirectInputDevice8A* mouse { nullptr };
    const bool created = SUCCEEDED(dummy->CreateDevice(GUID_SysMouse, &mouse, nullptr)) && mouse != nullptr;

    bool ok = false;
    if(created) {
        ok = setup_hooks_ansi(mouse);
        mouse->Release();
    }

    dummy->Release();

    return ok;
}

bool setup_for_wide()
{
    IDirectInput8W* dummy { nullptr };
    const HRESULT create_result =
        DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, reinterpret_cast<void**>(&dummy), nullptr);

    if(FAILED(create_result) || dummy == nullptr) {
        return false;
    }

    IDirectInputDevice8W* mouse { nullptr };
    const bool created = SUCCEEDED(dummy->CreateDevice(GUID_SysMouse, &mouse, nullptr)) && mouse != nullptr;

    bool ok = false;
    if(created) {
        ok = setup_hooks_wide(mouse);
        mouse->Release();
    }

    dummy->Release();

    return ok;
}
} // namespace

namespace tw::framework::dinput
{
void attach_input_gate(input_gate_fn fn) noexcept
{
    g_input_gate = fn;
}

void detach_input_gate() noexcept
{
    g_input_gate = nullptr;
}

bool install_hooks(bool is_window_unicode)
{
    return is_window_unicode ? setup_for_wide() : setup_for_ansi();
}
} // namespace tw::framework::dinput
