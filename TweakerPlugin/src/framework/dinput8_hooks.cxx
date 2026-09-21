#include "pch.hxx"

#include "framework/dinput8_hooks.hxx"

#include "framework/detour_transaction.hxx"

#include "plugin/boot_log.hxx"
#include "plugin/diagnostics.hxx"

namespace
{
constexpr std::ptrdiff_t k_create_device_idx = 3;
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

// Written from the load thread (attach/detach), read from whichever thread the game polls its
// devices on. Atomic so detach_input_gate() can't null it out between the null check and the call.
std::atomic<tw::framework::dinput::input_gate_fn> g_input_gate { nullptr };

bool input_suppressed() noexcept
{
    const auto gate = g_input_gate.load(std::memory_order_relaxed);
    return gate != nullptr && gate();
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
bool setup_hooks_ansi(IDirectInputDevice8A* device, tw::framework::detour::suspend threads)
{
    void** vtable = *reinterpret_cast<void***>(device);

    o_get_device_state_ansi = reinterpret_cast<get_device_state_fn_ansi>(vtable[k_get_device_state_idx]);
    o_get_device_data_ansi = reinterpret_cast<get_device_data_fn_ansi>(vtable[k_get_device_data_idx]);

    const bool ok = tw::framework::detour::attach(
        {
            { reinterpret_cast<void**>(&o_get_device_state_ansi), reinterpret_cast<void*>(&hk_get_device_state_ansi) },
            { reinterpret_cast<void**>(&o_get_device_data_ansi), reinterpret_cast<void*>(&hk_get_device_data_ansi) },
        },
        threads);

    if(!ok) {
        o_get_device_state_ansi = nullptr;
        o_get_device_data_ansi = nullptr;
    }

    return ok;
}

bool setup_hooks_wide(IDirectInputDevice8W* device, tw::framework::detour::suspend threads)
{
    void** vtable = *reinterpret_cast<void***>(device);

    o_get_device_state_wide = reinterpret_cast<get_device_state_fn_wide>(vtable[k_get_device_state_idx]);
    o_get_device_data_wide = reinterpret_cast<get_device_data_fn_wide>(vtable[k_get_device_data_idx]);

    const bool ok = tw::framework::detour::attach(
        {
            { reinterpret_cast<void**>(&o_get_device_state_wide), reinterpret_cast<void*>(&hk_get_device_state_wide) },
            { reinterpret_cast<void**>(&o_get_device_data_wide), reinterpret_cast<void*>(&hk_get_device_data_wide) },
        },
        threads);

    if(!ok) {
        o_get_device_state_wide = nullptr;
        o_get_device_data_wide = nullptr;
    }

    return ok;
}

// --- early load: the chain off the game's own objects ---------------------------------------------------

using direct_input8_create_fn = HRESULT(__stdcall*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
using create_device_fn_ansi = HRESULT(__stdcall*)(IDirectInput8A*, REFGUID, LPDIRECTINPUTDEVICE8A*, LPUNKNOWN);
using create_device_fn_wide = HRESULT(__stdcall*)(IDirectInput8W*, REFGUID, LPDIRECTINPUTDEVICE8W*, LPUNKNOWN);

direct_input8_create_fn o_direct_input8_create { nullptr };
create_device_fn_ansi o_create_device_ansi { nullptr };
create_device_fn_wide o_create_device_wide { nullptr };

// One install per link of the chain, whichever thread gets there first. The game creates its objects on
// its main thread, but nothing guarantees that for every module in the process.
std::atomic<bool> g_create_device_hooked_ansi { false };
std::atomic<bool> g_create_device_hooked_wide { false };
std::atomic<bool> g_device_hooked_ansi { false };
std::atomic<bool> g_device_hooked_wide { false };

// Initialisation, not per-frame: each fires a handful of times per process, on the game's own
// CreateDevice calls. The detour transactions and the lifecycle log are allowed here (§4.5).
//
// Every transaction in this chain leaves the other threads running, and that is the safe choice rather
// than a shortcut. Each link patches a function nobody can have called yet: DirectInput8Create is hooked
// from DllMain before its first caller, so the first object of a kind is the first one in the process and
// its CreateDevice has never run; the same holds one level down for the first device's GetDeviceState.
// Suspending threads, on the other hand, is a real hazard on this thread at this time - the startup
// thread and the shader compiles it queues allocate constantly, and a thread frozen while holding the
// heap lock deadlocks the game inside Detours' own allocation.
HRESULT __stdcall hk_create_device_ansi(IDirectInput8A* self, REFGUID guid, LPDIRECTINPUTDEVICE8A* out, LPUNKNOWN outer)
{
    const HRESULT hr = o_create_device_ansi(self, guid, out, outer);

    if(SUCCEEDED(hr) && out != nullptr && *out != nullptr && !g_device_hooked_ansi.exchange(true)) {
        const bool ok = setup_hooks_ansi(*out, tw::framework::detour::suspend::none);
        TW_BOOT_LOG("dinput: GetDeviceState/GetDeviceData (A) {} off the game's first device", ok ? "hooked" : "FAILED");
    }

    return hr;
}

HRESULT __stdcall hk_create_device_wide(IDirectInput8W* self, REFGUID guid, LPDIRECTINPUTDEVICE8W* out, LPUNKNOWN outer)
{
    const HRESULT hr = o_create_device_wide(self, guid, out, outer);

    if(SUCCEEDED(hr) && out != nullptr && *out != nullptr && !g_device_hooked_wide.exchange(true)) {
        const bool ok = setup_hooks_wide(*out, tw::framework::detour::suspend::none);
        TW_BOOT_LOG("dinput: GetDeviceState/GetDeviceData (W) {} off the game's first device", ok ? "hooked" : "FAILED");
    }

    return hr;
}

// A and W objects have separate vtables with separate CreateDevice entries, so each kind is hooked off the
// first object of that kind.
HRESULT __stdcall hk_direct_input8_create(HINSTANCE instance, DWORD version, REFIID riid, LPVOID* out, LPUNKNOWN outer)
{
    const HRESULT hr = o_direct_input8_create(instance, version, riid, out, outer);
    if(FAILED(hr) || out == nullptr || *out == nullptr) {
        return hr;
    }

    if(IsEqualIID(riid, IID_IDirectInput8A) && !g_create_device_hooked_ansi.exchange(true)) {
        void** vtable = *static_cast<void***>(*out);
        o_create_device_ansi = reinterpret_cast<create_device_fn_ansi>(vtable[k_create_device_idx]);

        const bool ok = tw::framework::detour::attach(
            {
                { reinterpret_cast<void**>(&o_create_device_ansi), reinterpret_cast<void*>(&hk_create_device_ansi) },
            },
            tw::framework::detour::suspend::none);
        if(!ok) {
            o_create_device_ansi = nullptr;
        }

        TW_BOOT_LOG("dinput: IDirectInput8A::CreateDevice {} off the game's object", ok ? "hooked" : "FAILED");
    }
    else if(IsEqualIID(riid, IID_IDirectInput8W) && !g_create_device_hooked_wide.exchange(true)) {
        void** vtable = *static_cast<void***>(*out);
        o_create_device_wide = reinterpret_cast<create_device_fn_wide>(vtable[k_create_device_idx]);

        const bool ok = tw::framework::detour::attach(
            {
                { reinterpret_cast<void**>(&o_create_device_wide), reinterpret_cast<void*>(&hk_create_device_wide) },
            },
            tw::framework::detour::suspend::none);
        if(!ok) {
            o_create_device_wide = nullptr;
        }

        TW_BOOT_LOG("dinput: IDirectInput8W::CreateDevice {} off the game's object", ok ? "hooked" : "FAILED");
    }

    return hr;
}

// --- late load: throwaway objects ------------------------------------------------------------------------

// NOTE on the deliberately leaked COM references below.
//
// These throwaway objects exist only to read two function pointers out of a device vtable. The
// obvious cleanup - Release() both once the vtable has been read - is what this code used to do,
// and it is not safe here. dinput8.dll keeps per-process state whose lifetime is tied to the live
// object count, including the low-level input hooks it installs on behalf of exclusive-mode
// devices; dropping our references drops that count in a pattern the game never produces on its
// own, and the observable result was the game losing keyboard input the plugin had never touched.
//
// We are a guest in someone else's process. Two COM references held for the process lifetime cost
// a few hundred bytes and are reclaimed at exit like everything else; perturbing a third party's
// refcounted global state to save them is not a trade worth making. Deliberate leak, same
// reasoning as the thread handles in detour_transaction.cxx.
bool setup_for_ansi()
{
    IDirectInput8A* dummy { nullptr };
    const HRESULT create_result =
        DirectInput8Create(GetModuleHandleA(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8A, reinterpret_cast<void**>(&dummy), nullptr);

    if(FAILED(create_result) || dummy == nullptr) {
        TW_LOG_ERROR("dinput: DirectInput8Create(A) failed, hr=0x{:08X}", static_cast<unsigned long>(create_result));
        return false;
    }

    IDirectInputDevice8A* mouse { nullptr };
    const HRESULT device_result = dummy->CreateDevice(GUID_SysMouse, &mouse, nullptr);
    if(FAILED(device_result) || mouse == nullptr) {
        TW_LOG_ERROR("dinput: CreateDevice(GUID_SysMouse, A) failed, hr=0x{:08X}", static_cast<unsigned long>(device_result));
        return false;
    }

    return setup_hooks_ansi(mouse, tw::framework::detour::suspend::others);
}

bool setup_for_wide()
{
    IDirectInput8W* dummy { nullptr };
    const HRESULT create_result =
        DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, reinterpret_cast<void**>(&dummy), nullptr);

    if(FAILED(create_result) || dummy == nullptr) {
        TW_LOG_ERROR("dinput: DirectInput8Create(W) failed, hr=0x{:08X}", static_cast<unsigned long>(create_result));
        return false;
    }

    IDirectInputDevice8W* mouse { nullptr };
    const HRESULT device_result = dummy->CreateDevice(GUID_SysMouse, &mouse, nullptr);
    if(FAILED(device_result) || mouse == nullptr) {
        TW_LOG_ERROR("dinput: CreateDevice(GUID_SysMouse, W) failed, hr=0x{:08X}", static_cast<unsigned long>(device_result));
        return false;
    }

    return setup_hooks_wide(mouse, tw::framework::detour::suspend::others);
}
} // namespace

namespace tw::framework::dinput
{
void attach_input_gate(input_gate_fn fn) noexcept
{
    g_input_gate.store(fn, std::memory_order_relaxed);
}

void detach_input_gate() noexcept
{
    g_input_gate.store(nullptr, std::memory_order_relaxed);
}

bool install_hooks(bool use_unicode_interface)
{
    const bool ok = use_unicode_interface ? setup_for_wide() : setup_for_ansi();
    TW_LOG_INFO("dinput: install_hooks({}) -> {}", use_unicode_interface ? "W" : "A", ok ? "ok" : "FAILED");
    TW_BOOT_LOG("dinput: GetDeviceState/GetDeviceData ({}) {} off a throwaway device", use_unicode_interface ? "W" : "A", ok ? "hooked" : "FAILED");
    return ok;
}

bool install_create_hook_from_loader() noexcept
{
    const HMODULE dinput8 = ::GetModuleHandleW(L"dinput8.dll");
    o_direct_input8_create =
        dinput8 != nullptr ? reinterpret_cast<direct_input8_create_fn>(::GetProcAddress(dinput8, "DirectInput8Create")) : nullptr;

    if(o_direct_input8_create == nullptr) {
        return false;
    }

    const bool ok = tw::framework::detour::attach(
        {
            { reinterpret_cast<void**>(&o_direct_input8_create), reinterpret_cast<void*>(&hk_direct_input8_create) },
        },
        tw::framework::detour::suspend::none);

    if(!ok) {
        o_direct_input8_create = nullptr;
    }

    return ok;
}
} // namespace tw::framework::dinput
