#include "pch.hxx"

#include "framework/loader_watch.hxx"

namespace
{
// Not in any SDK header. Stable since Vista and documented on MSDN under LdrRegisterDllNotification;
// the loaded and unloaded payloads share this layout.
struct ldr_dll_notification_data {
    ULONG flags;
    const UNICODE_STRING* full_dll_name;
    const UNICODE_STRING* base_dll_name;
    void* dll_base;
    ULONG size_of_image;
};

using ldr_dll_notification_fn = void(NTAPI*)(ULONG reason, const ldr_dll_notification_data* data, void* context);
using ldr_register_dll_notification_fn = LONG(NTAPI*)(ULONG flags, ldr_dll_notification_fn callback, void* context, void** cookie);

constexpr ULONG k_reason_loaded = 1;
constexpr ULONG k_reason_unloaded = 2;

// Written once in start(), before the callback is registered; only read afterwards.
std::span<const tw::framework::loader_watch::watch> g_watches;

bool equals_ignore_case(std::wstring_view a, std::wstring_view b) noexcept
{
    return ::CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

void NTAPI on_notification(ULONG reason, const ldr_dll_notification_data* data, void* /*context*/)
{
    if(data == nullptr || data->base_dll_name == nullptr || data->base_dll_name->Buffer == nullptr) {
        return;
    }

    const std::wstring_view base_name { data->base_dll_name->Buffer, data->base_dll_name->Length / sizeof(wchar_t) };
    const HMODULE module = static_cast<HMODULE>(data->dll_base);

    for(const auto& watch : g_watches) {
        if(!equals_ignore_case(base_name, watch.base_name)) {
            continue;
        }

        if(reason == k_reason_loaded && watch.on_loaded != nullptr) {
            watch.on_loaded(module);
        }
        else if(reason == k_reason_unloaded && watch.on_unloaded != nullptr) {
            watch.on_unloaded(module);
        }

        return;
    }
}
} // namespace

namespace tw::framework::loader_watch
{
bool start(std::span<const watch> watches) noexcept
{
    const HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    const auto register_notification = ntdll != nullptr
        ? reinterpret_cast<ldr_register_dll_notification_fn>(::GetProcAddress(ntdll, "LdrRegisterDllNotification"))
        : nullptr;

    if(register_notification == nullptr) {
        return false;
    }

    g_watches = watches;

    // The cookie would only matter to LdrUnregisterDllNotification, and there is no unload path to call it
    // from.
    void* cookie = nullptr;
    if(register_notification(0, &on_notification, nullptr, &cookie) < 0) {
        g_watches = {};
        return false;
    }

    return true;
}

bool pin(HMODULE module) noexcept
{
    HMODULE pinned = nullptr;
    return ::GetModuleHandleExW(
               GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(module), &pinned)
        != FALSE;
}
} // namespace tw::framework::loader_watch
