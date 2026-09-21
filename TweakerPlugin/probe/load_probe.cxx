// Phase 0 load probe - see Docs/Internal/plugin-offline-mode.md, "Ф0".
//
// A throwaway measuring instrument, not part of TweakerPlugin. Dropped into engine\channels\, it answers
// the questions the early-loading design rests on and that nothing short of the real game can answer:
//
//   - who loads a DLL out of channels\, from which thread, and whether it is unloaded again right away
//     (the HighPoly channel scan does LoadLibraryA -> GetProcAddress("GetType") -> FreeLibrary);
//   - the order in which modules map - d3d9.dll, dinput8.dll, the Texture channel - and who maps them;
//   - when the game calls Direct3DCreate9, CreateDevice, Reset and its first Present, relative to all
//     of the above and to its windows (hidden ones included);
//   - whether a throwaway D3D9 device created as soon as d3d9.dll maps (TweakerPlugin's stage 3) runs
//     alongside the game's own D3D initialisation;
//   - whether the working directory moves during a session.
//
// Revision 4 pins d3d9.dll and the Texture channel on first load - see on_dll_notification.
//
// Revision 3. Revision 2 hung the game before its window appeared - see list_process_window and
// on_dll_notification for why and what the notification callback must never do.
//
// The first revision found modules by polling GetModuleHandleW every 5 ms and hooked
// Direct3DCreate9 only after noticing d3d9.dll. The game got its IDirect3D9 and its first device inside
// that gap, so the first run recorded neither - and read the later fullscreen switches as the first
// device. This revision hears every module map through LdrRegisterDllNotification, synchronously on
// the loading thread, and hooks Direct3DCreate9 right there, before the loader returns to whoever
// asked for d3d9.dll. Nothing in the process can call the export before the hook is in.
//
// Everything goes to <engine>\TweakerStuff\Logs\loadprobe.log, appended, one section per game launch.
//
// Modes, chosen without a rebuild - either an empty switch file in <engine>\TweakerStuff\, or a token in
// the probe's own file name (renaming is harder to get wrong than creating an extensionless file in
// Explorer):
//   observe  (loadprobe.observe / TweakerLoadProbe.observe.dll) - no pin, no thread, no notification, no
//            hooks: only attach/detach. Shows the scan's load/unload pair.
//   nodummy  (loadprobe.nodummy / TweakerLoadProbe.nodummy.dll) - full timeline without the throwaway device.
//
// Deliberately self-contained: no TweakerPlugin PCH, no project headers. It must not import d3d9.dll or
// dinput8.dll either - an import would map those modules at probe load and change the order being
// measured - so both are reached through GetProcAddress only.

#include <Windows.h>
#include <TlHelp32.h>
#include <d3d9.h>
#include <detours.h>
#include <intrin.h>
#include <process.h>
#include <winternl.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <format>
#include <string>
#include <string_view>

namespace
{
using direct3d_create9_fn = IDirect3D9*(WINAPI*)(UINT);
using direct3d_create9_ex_fn = HRESULT(WINAPI*)(UINT, void**);
using create_device_fn = HRESULT(WINAPI*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
using reset_fn = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using present_fn = HRESULT(WINAPI*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using direct_input8_create_fn = HRESULT(WINAPI*)(HINSTANCE, DWORD, const GUID&, void**, IUnknown*);

// ntdll's loader notification, declared here because no SDK header carries it. Stable since Vista and
// documented on MSDN (LdrRegisterDllNotification), just not in a header.
struct ldr_dll_notification_data {
    ULONG flags;
    const UNICODE_STRING* full_dll_name;
    const UNICODE_STRING* base_dll_name;
    void* dll_base;
    ULONG size_of_image;
};

using ldr_dll_notification_fn = void(NTAPI*)(ULONG reason, const ldr_dll_notification_data* data, void* context);
using ldr_register_dll_notification_fn = LONG(NTAPI*)(ULONG flags, ldr_dll_notification_fn callback, void* context, void** cookie);

constexpr ULONG k_ldr_reason_loaded = 1;
constexpr ULONG k_ldr_reason_unloaded = 2;

constexpr std::wstring_view k_game_exe = L"QuestViewer.exe";
constexpr std::wstring_view k_texture_channel = L"BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll";

FILETIME g_process_created {};
std::wstring g_engine_dir;
std::wstring g_log_path;
SRWLOCK g_log_lock = SRWLOCK_INIT;

bool g_skip_dummy = false;
DWORD g_attach_thread = 0;

direct3d_create9_fn o_direct3d_create9 = nullptr;
direct3d_create9_ex_fn o_direct3d_create9_ex = nullptr;
create_device_fn o_create_device = nullptr;
reset_fn o_reset = nullptr;
present_fn o_present = nullptr;
direct_input8_create_fn o_direct_input8_create = nullptr;

std::atomic<bool> g_create_device_hooked { false };
std::atomic<bool> g_device_hooks_installed { false };
std::atomic<int> g_game_direct3d_create_calls { 0 };
std::atomic<int> g_game_create_device_calls { 0 };
std::atomic<int> g_game_create_device_in_flight { 0 };
std::atomic<IDirect3DDevice9*> g_last_game_device { nullptr };
std::atomic<IDirect3DDevice9*> g_present_logged_for { nullptr };
std::atomic<bool> g_d3d9_mapped { false };

thread_local bool t_in_dummy = false;

// --- logging ---------------------------------------------------------------------------------------

std::string to_utf8(std::wstring_view wide)
{
    if(wide.empty()) {
        return {};
    }

    const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(bytes), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), bytes, nullptr, nullptr);
    return out;
}

// Seconds since the process was created, not since the probe loaded: the probe's own load time is one
// of the things being measured.
double seconds_since_process_start()
{
    FILETIME now {};
    ::GetSystemTimePreciseAsFileTime(&now);

    const auto to_u64 = [](const FILETIME& ft) {
        return (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    };

    return static_cast<double>(to_u64(now) - to_u64(g_process_created)) / 1.0e7;
}

// Opens, appends and closes per line. Slow, and exactly right for a probe: a line written is a line on
// disk even if the game dies on the next instruction, and nothing is buffered across a FreeLibrary.
// Kernel32 only, so it is safe from DllMain and from a loader notification alike.
void log_line(std::string_view text)
{
    if(g_log_path.empty()) {
        return;
    }

    char prefix[80];
    const DWORD thread = ::GetCurrentThreadId();
    const int prefix_len = std::snprintf(prefix,
        sizeof(prefix),
        "[+%9.3f s] [tid %5lu%s] ",
        seconds_since_process_start(),
        thread,
        thread == g_attach_thread ? " main" : "     ");

    ::AcquireSRWLockExclusive(&g_log_lock);

    HANDLE file = ::CreateFileW(
        g_log_path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        ::WriteFile(file, prefix, static_cast<DWORD>(prefix_len), &written, nullptr);
        ::WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
        ::WriteFile(file, "\r\n", 2, &written, nullptr);
        ::CloseHandle(file);
    }

    ::ReleaseSRWLockExclusive(&g_log_lock);
}

template<typename... TArgs>
void logf(std::format_string<TArgs...> fmt, TArgs&&... args)
{
    log_line(std::format(fmt, std::forward<TArgs>(args)...));
}

std::string module_path_of(HMODULE module)
{
    wchar_t buffer[MAX_PATH] {};
    const DWORD len = ::GetModuleFileNameW(module, buffer, MAX_PATH);
    return to_utf8(std::wstring_view { buffer, len });
}

std::string current_directory()
{
    wchar_t buffer[MAX_PATH] {};
    const DWORD len = ::GetCurrentDirectoryW(MAX_PATH, buffer);
    return to_utf8(std::wstring_view { buffer, len < MAX_PATH ? len : 0 });
}

std::string guid_text(const GUID& g)
{
    return std::format("{:08X}-{:04X}-{:04X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}",
        g.Data1,
        g.Data2,
        g.Data3,
        g.Data4[0],
        g.Data4[1],
        g.Data4[2],
        g.Data4[3],
        g.Data4[4],
        g.Data4[5],
        g.Data4[6],
        g.Data4[7]);
}

bool equals_ignore_case(std::wstring_view a, std::wstring_view b)
{
    return ::CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

bool contains_ignore_case(std::wstring_view haystack, std::wstring_view needle)
{
    if(needle.size() > haystack.size()) {
        return false;
    }

    for(std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        if(equals_ignore_case(haystack.substr(i, needle.size()), needle)) {
            return true;
        }
    }

    return false;
}

// "Module.dll+0x1234" for an address, or the raw address when it belongs to no module.
std::string describe_address(void* address)
{
    HMODULE owner = nullptr;
    if(!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
           static_cast<LPCWSTR>(address),
           &owner)) {
        return std::format("{}", address);
    }

    wchar_t buffer[MAX_PATH] {};
    const DWORD len = ::GetModuleFileNameW(owner, buffer, MAX_PATH);
    std::wstring_view path { buffer, len };
    if(const auto slash = path.find_last_of(L'\\'); slash != std::wstring_view::npos) {
        path.remove_prefix(slash + 1);
    }

    const auto offset = static_cast<std::uintptr_t>(static_cast<char*>(address) - static_cast<char*>(static_cast<void*>(owner)));
    return std::format("{}+0x{:X}", to_utf8(path), offset);
}

// The whole stack of the calling thread, each frame as module+offset. Frames are not filtered: which
// ntdll and kernelbase frames sit between the game and the loader is itself informative.
std::string stack_text(int skip)
{
    std::array<void*, 40> frames {};
    const USHORT count = ::RtlCaptureStackBackTrace(static_cast<DWORD>(skip), static_cast<DWORD>(frames.size()), frames.data(), nullptr);

    std::string line;
    for(USHORT i = 0; i < count; ++i) {
        line += " | ";
        line += describe_address(frames[i]);
    }
    return line;
}

// Every top-level window of the process, hidden ones included - the question is whether the game has a
// window before it has a device, and an invisible window answers that as well as a visible one.
struct window_listing {
    DWORD pid;
    std::string text;
    int count;
};

BOOL CALLBACK list_process_window(HWND hwnd, LPARAM param)
{
    auto* listing = reinterpret_cast<window_listing*>(param);

    DWORD owner_pid = 0;
    const DWORD owner_thread = ::GetWindowThreadProcessId(hwnd, &owner_pid);
    if(owner_pid != listing->pid) {
        return TRUE;
    }

    // InternalGetWindowText, not GetWindowTextW: the latter sends WM_GETTEXT, which for a window of another
    // thread is a SendMessage into that thread. Revision 2 did that from a loader notification running on
    // a non-main thread, under the loader lock, while the main thread - the window's owner - was queued
    // for that same lock on its next channel load. The game hung before showing its window, twice.
    wchar_t class_name[128] {};
    wchar_t title[128] {};
    ::GetClassNameW(hwnd, class_name, 128);
    ::InternalGetWindowText(hwnd, title, 128);

    RECT rect {};
    ::GetWindowRect(hwnd, &rect);

    listing->text += std::format(" | {} class='{}' title='{}' {} {}x{} thread {}",
        static_cast<void*>(hwnd),
        to_utf8(class_name),
        to_utf8(title),
        ::IsWindowVisible(hwnd) ? "visible" : "hidden",
        rect.right - rect.left,
        rect.bottom - rect.top,
        owner_thread);
    ++listing->count;

    return TRUE;
}

std::string process_windows(int& out_count)
{
    window_listing listing { ::GetCurrentProcessId(), {}, 0 };
    ::EnumWindows(&list_process_window, reinterpret_cast<LPARAM>(&listing));
    out_count = listing.count;
    return listing.text;
}

// --- detours ---------------------------------------------------------------------------------------

enum class suspend_threads : std::uint8_t {
    others,
    none,
};

// Same shape as TweakerPlugin's framework/detour_transaction.cxx: every other thread of the process is
// handed to DetourUpdateThread, and the handles are deliberately never closed (see the comment there).
//
// `suspend_threads::none` is for an export of a module that is being loaded right now, patched from its
// own loader notification: nobody can be executing that code yet, so there is no thread whose instruction
// pointer needs moving - and suspending threads while holding the loader lock is how a thread that owns
// the process heap lock ends up frozen while Detours allocates.
bool attach_detour(void** target, void* replacement, suspend_threads suspend = suspend_threads::others)
{
    if(::DetourTransactionBegin() != NO_ERROR) {
        return false;
    }

    HANDLE snapshot = suspend == suspend_threads::others ? ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0) : INVALID_HANDLE_VALUE;
    if(snapshot != INVALID_HANDLE_VALUE) {
        THREADENTRY32 entry {};
        entry.dwSize = sizeof(entry);

        const DWORD this_process = ::GetCurrentProcessId();
        const DWORD this_thread = ::GetCurrentThreadId();

        if(::Thread32First(snapshot, &entry)) {
            do {
                if(entry.th32OwnerProcessID != this_process || entry.th32ThreadID == this_thread) {
                    continue;
                }

                HANDLE thread = ::OpenThread(THREAD_ALL_ACCESS, FALSE, entry.th32ThreadID);
                if(thread != nullptr) {
                    ::DetourUpdateThread(thread);
                }
            } while(::Thread32Next(snapshot, &entry));
        }

        ::CloseHandle(snapshot);
    }

    if(::DetourAttach(target, replacement) != NO_ERROR) {
        ::DetourTransactionAbort();
        return false;
    }

    return ::DetourTransactionCommit() == NO_ERROR;
}

std::string describe_present_parameters(const D3DPRESENT_PARAMETERS* params)
{
    if(params == nullptr) {
        return "params=null";
    }

    return std::format("windowed={} backbuffer={}x{} fmt={} refresh={} swap={} device_window={}",
        params->Windowed,
        params->BackBufferWidth,
        params->BackBufferHeight,
        static_cast<int>(params->BackBufferFormat),
        params->FullScreen_RefreshRateInHz,
        static_cast<int>(params->SwapEffect),
        static_cast<void*>(params->hDeviceWindow));
}

HRESULT WINAPI hk_present(IDirect3DDevice9* device, const RECT* source, const RECT* dest, HWND window, const RGNDATA* dirty)
{
    // First Present per game device: the pointer changes on every device the game makes, and a single
    // compare-exchange per frame is all this costs once it has been logged.
    const IDirect3DDevice9* latest = g_last_game_device.load(std::memory_order_relaxed);
    if(device == latest && g_present_logged_for.exchange(device) != device) {
        logf("d3d9: first Present on game device {}", static_cast<void*>(device));
    }

    return o_present(device, source, dest, window, dirty);
}

HRESULT WINAPI hk_reset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params)
{
    logf("d3d9: Reset begin device={} {}", static_cast<void*>(device), describe_present_parameters(params));
    const HRESULT hr = o_reset(device, params);
    logf("d3d9: Reset end device={} hr=0x{:08X}", static_cast<void*>(device), static_cast<unsigned long>(hr));
    return hr;
}

void install_device_hooks(IDirect3DDevice9* device)
{
    if(g_device_hooks_installed.exchange(true)) {
        return;
    }

    void** vtable = *reinterpret_cast<void***>(device);

    o_reset = reinterpret_cast<reset_fn>(vtable[16]);
    const bool reset_ok = attach_detour(reinterpret_cast<void**>(&o_reset), reinterpret_cast<void*>(&hk_reset));

    o_present = reinterpret_cast<present_fn>(vtable[17]);
    const bool present_ok = attach_detour(reinterpret_cast<void**>(&o_present), reinterpret_cast<void*>(&hk_present));

    logf("d3d9: device detours Reset {} / Present {}", reset_ok ? "installed" : "FAILED", present_ok ? "installed" : "FAILED");
}

HRESULT WINAPI hk_create_device(IDirect3D9* d3d,
    UINT adapter,
    D3DDEVTYPE type,
    HWND focus_window,
    DWORD behavior_flags,
    D3DPRESENT_PARAMETERS* params,
    IDirect3DDevice9** out_device)
{
    const bool dummy = t_in_dummy;
    const int ordinal = dummy ? 0 : g_game_create_device_calls.fetch_add(1) + 1;

    if(!dummy) {
        g_game_create_device_in_flight.fetch_add(1);
    }

    logf("d3d9: CreateDevice #{} begin ({}) d3d={} adapter={} type={} focus_window={} flags=0x{:08X} {}",
        ordinal,
        dummy ? "probe dummy" : "game",
        static_cast<void*>(d3d),
        adapter,
        static_cast<int>(type),
        static_cast<void*>(focus_window),
        behavior_flags,
        describe_present_parameters(params));

    const double started = seconds_since_process_start();
    const HRESULT hr = o_create_device(d3d, adapter, type, focus_window, behavior_flags, params, out_device);
    IDirect3DDevice9* device = (SUCCEEDED(hr) && out_device != nullptr) ? *out_device : nullptr;

    if(!dummy) {
        g_game_create_device_in_flight.fetch_sub(1);
    }

    logf("d3d9: CreateDevice #{} end ({}) hr=0x{:08X} device={} took={:.1f} ms",
        ordinal,
        dummy ? "probe dummy" : "game",
        static_cast<unsigned long>(hr),
        static_cast<void*>(device),
        (seconds_since_process_start() - started) * 1000.0);

    if(!dummy && device != nullptr) {
        g_last_game_device.store(device, std::memory_order_relaxed);
        install_device_hooks(device);
    }

    return hr;
}

void hook_create_device(IDirect3D9* d3d)
{
    if(d3d == nullptr || g_create_device_hooked.exchange(true)) {
        return;
    }

    o_create_device = reinterpret_cast<create_device_fn>((*reinterpret_cast<void***>(d3d))[16]);
    const bool ok = attach_detour(reinterpret_cast<void**>(&o_create_device), reinterpret_cast<void*>(&hk_create_device));
    logf("d3d9: IDirect3D9::CreateDevice detour {}", ok ? "installed" : "FAILED");
}

IDirect3D9* WINAPI hk_direct3d_create9(UINT sdk_version)
{
    const bool dummy = t_in_dummy;
    const int ordinal = dummy ? 0 : g_game_direct3d_create_calls.fetch_add(1) + 1;

    IDirect3D9* d3d = o_direct3d_create9(sdk_version);

    logf("d3d9: Direct3DCreate9 #{} ({}) sdk={} -> {}, caller {}",
        ordinal,
        dummy ? "probe dummy" : "game",
        sdk_version,
        static_cast<void*>(d3d),
        describe_address(_ReturnAddress()));

    if(!dummy && ordinal == 1) {
        logf("d3d9: first game Direct3DCreate9 stack{}", stack_text(1));
    }

    hook_create_device(d3d);
    return d3d;
}

HRESULT WINAPI hk_direct3d_create9_ex(UINT sdk_version, void** out)
{
    const HRESULT hr = o_direct3d_create9_ex(sdk_version, out);
    logf("d3d9: Direct3DCreate9Ex sdk={} -> 0x{:08X}, caller {}",
        sdk_version,
        static_cast<unsigned long>(hr),
        describe_address(_ReturnAddress()));
    return hr;
}

HRESULT WINAPI hk_direct_input8_create(HINSTANCE instance, DWORD version, const GUID& riid, void** out, IUnknown* outer)
{
    const HRESULT hr = o_direct_input8_create(instance, version, riid, out, outer);

    logf("dinput8: DirectInput8Create(version=0x{:X}, riid={}) -> 0x{:08X}, caller {}",
        version,
        guid_text(riid),
        static_cast<unsigned long>(hr),
        describe_address(_ReturnAddress()));

    return hr;
}

// Runs on the thread that is loading d3d9.dll, inside the loader, before that load returns. The export
// exists, the importer has not run a line of it yet, and no other thread can be inside it - hence
// `suspend` is none when called from the notification.
void hook_d3d9_exports(HMODULE d3d9, suspend_threads suspend)
{
    o_direct3d_create9 = reinterpret_cast<direct3d_create9_fn>(::GetProcAddress(d3d9, "Direct3DCreate9"));
    if(o_direct3d_create9 != nullptr) {
        const bool ok =
            attach_detour(reinterpret_cast<void**>(&o_direct3d_create9), reinterpret_cast<void*>(&hk_direct3d_create9), suspend);
        logf("d3d9: Direct3DCreate9 detour {}", ok ? "installed" : "FAILED");
    }

    o_direct3d_create9_ex = reinterpret_cast<direct3d_create9_ex_fn>(::GetProcAddress(d3d9, "Direct3DCreate9Ex"));
    if(o_direct3d_create9_ex != nullptr) {
        const bool ok =
            attach_detour(reinterpret_cast<void**>(&o_direct3d_create9_ex), reinterpret_cast<void*>(&hk_direct3d_create9_ex), suspend);
        logf("d3d9: Direct3DCreate9Ex detour {}", ok ? "installed" : "FAILED");
    }

    g_d3d9_mapped.store(true);
}

void hook_dinput8_exports(HMODULE dinput8, suspend_threads suspend)
{
    o_direct_input8_create = reinterpret_cast<direct_input8_create_fn>(::GetProcAddress(dinput8, "DirectInput8Create"));
    if(o_direct_input8_create == nullptr) {
        return;
    }

    const bool ok = attach_detour(
        reinterpret_cast<void**>(&o_direct_input8_create), reinterpret_cast<void*>(&hk_direct_input8_create), suspend);
    logf("dinput8: DirectInput8Create detour {}", ok ? "installed" : "FAILED");
}

// --- loader notification ---------------------------------------------------------------------------

bool is_interesting_for_stack(std::wstring_view base_name)
{
    return equals_ignore_case(base_name, L"d3d9.dll") || equals_ignore_case(base_name, L"dinput8.dll")
           || equals_ignore_case(base_name, L"D3DX9_38.dll") || equals_ignore_case(base_name, k_texture_channel);
}

void NTAPI on_dll_notification(ULONG reason, const ldr_dll_notification_data* data, void* /*context*/)
{
    if(data == nullptr || data->base_dll_name == nullptr || data->full_dll_name == nullptr) {
        return;
    }

    const std::wstring_view base_name { data->base_dll_name->Buffer, data->base_dll_name->Length / sizeof(wchar_t) };
    const std::wstring_view full_name { data->full_dll_name->Buffer, data->full_dll_name->Length / sizeof(wchar_t) };

    if(reason == k_ldr_reason_unloaded) {
        logf("module: unloaded {} ({})", to_utf8(base_name), data->dll_base);
        return;
    }

    if(reason != k_ldr_reason_loaded) {
        return;
    }

    // Under the loader lock, possibly on a thread that is not the game's: nothing here may wait on another
    // thread. No window APIs at all (the timeline thread tracks windows), no thread suspension.
    logf("module: loaded {} at {} ({}); game device calls so far {}",
        to_utf8(base_name),
        data->dll_base,
        to_utf8(full_name),
        g_game_create_device_calls.load());

    if(is_interesting_for_stack(base_name)) {
        logf("module: {} loader stack{}", to_utf8(base_name), stack_text(1));
    }

    const HMODULE module = static_cast<HMODULE>(data->dll_base);
    const bool is_d3d9 = equals_ignore_case(base_name, L"d3d9.dll");
    const bool is_texture_channel = equals_ignore_case(base_name, k_texture_channel);

    // Revision 4: the modules the plugin will hook are pinned on their first load. HighPoly loads every
    // channel twice - a probe for GetType, then for real - and without a reference of our own the probe's
    // FreeLibrary unmaps them in between (revision 3 logged d3d9.dll unloading 2 ms after it loaded).
    // Pinned, the probe's FreeLibrary drops only the game's reference, the real load gets the image that
    // is already there, and there is one image to hook for the life of the process.
    //
    // What this run is meant to show: no "unloaded" line for either module, one "loaded" each, and a
    // game that still starts and plays - their DllMain now runs once instead of load-unload-load.
    if(is_d3d9 || is_texture_channel) {
        HMODULE pinned = nullptr;
        const BOOL ok = ::GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, static_cast<LPCWSTR>(data->dll_base), &pinned);
        logf("module: {} pinned {} (error {})", to_utf8(base_name), ok ? "ok" : "FAILED", ok ? 0ul : ::GetLastError());
    }

    if(is_d3d9) {
        hook_d3d9_exports(module, suspend_threads::none);
    }
    else if(equals_ignore_case(base_name, L"dinput8.dll")) {
        hook_dinput8_exports(module, suspend_threads::none);
    }
}

// --- timeline thread -------------------------------------------------------------------------------

// TweakerPlugin's resolve_d3d9_functions, reduced to what costs time and touches the runtime: window,
// Direct3DCreate9 (through the hooked export, so it is labelled), HAL device with NULLREF fallback,
// release. No vtable reading - that is free and not the question. What is logged is whether any of
// the game's own D3D calls were running at the same time.
void run_dummy_device()
{
    logf("dummy: begin (game Direct3DCreate9 calls {}, CreateDevice calls {}, CreateDevice in flight {})",
        g_game_direct3d_create_calls.load(),
        g_game_create_device_calls.load(),
        g_game_create_device_in_flight.load());

    const int devices_before = g_game_create_device_calls.load();
    const double started = seconds_since_process_start();
    t_in_dummy = true;

    WNDCLASSEXW window_class {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = ::DefWindowProcW;
    window_class.hInstance = ::GetModuleHandleW(nullptr);
    window_class.lpszClassName = L"TweakerLoadProbeBootstrap";
    ::RegisterClassExW(&window_class);

    HWND window = ::CreateWindowW(
        window_class.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, window_class.hInstance, nullptr);

    auto create = reinterpret_cast<direct3d_create9_fn>(::GetProcAddress(::GetModuleHandleW(L"d3d9.dll"), "Direct3DCreate9"));
    IDirect3D9* d3d = create != nullptr ? create(D3D_SDK_VERSION) : nullptr;

    if(d3d != nullptr) {
        D3DPRESENT_PARAMETERS params {};
        params.Windowed = TRUE;
        params.SwapEffect = D3DSWAPEFFECT_DISCARD;
        params.hDeviceWindow = window;
        params.BackBufferFormat = D3DFMT_UNKNOWN;

        constexpr DWORD k_flags = D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT;

        IDirect3DDevice9* device = nullptr;
        if(FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, k_flags, &params, &device))) {
            logf("dummy: HAL device failed, trying NULLREF");
            d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, window, k_flags, &params, &device);
        }

        if(device != nullptr) {
            device->Release();
        }
        d3d->Release();
    }

    ::DestroyWindow(window);
    ::UnregisterClassW(window_class.lpszClassName, window_class.hInstance);

    t_in_dummy = false;

    logf("dummy: end, took {:.1f} ms; game CreateDevice calls before={} after={}, in flight now {}",
        (seconds_since_process_start() - started) * 1000.0,
        devices_before,
        g_game_create_device_calls.load(),
        g_game_create_device_in_flight.load());
}

unsigned __stdcall timeline_thread(void*)
{
    logf("thread: started");

    bool dummy_done = g_skip_dummy;
    int last_window_count = -1;
    std::string last_cwd = current_directory();

    constexpr double k_fast_phase_seconds = 60.0;

    for(;;) {
        const bool fast = seconds_since_process_start() < k_fast_phase_seconds;

        // Same moment TweakerPlugin's stage 3 would act: as soon as a poll sees d3d9.dll. The loader
        // notification has already hooked the exports by then, so the game's calls are all on record.
        if(!dummy_done && g_d3d9_mapped.load()) {
            dummy_done = true;
            run_dummy_device();
        }

        int window_count = 0;
        std::string windows = process_windows(window_count);
        if(window_count != last_window_count) {
            logf("window: {} top-level window(s){}", window_count, windows);
            last_window_count = window_count;
        }

        std::string cwd = current_directory();
        if(cwd != last_cwd) {
            logf("cwd: changed '{}' -> '{}'", last_cwd, cwd);
            last_cwd = std::move(cwd);
        }

        ::Sleep(fast ? 5 : 1000);
    }
}

// --- bootstrap -------------------------------------------------------------------------------------

bool file_exists(const std::wstring& path)
{
    const DWORD attributes = ::GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

// Resolves the exe folder and prepares the log. `out_is_game` is false for any process that is not the
// game: a DLL in channels\ is loaded by whatever uses the engine, and the probe has nothing to say there.
bool prepare_paths(bool& out_is_game)
{
    wchar_t buffer[MAX_PATH] {};
    const DWORD len = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if(len == 0 || len >= MAX_PATH) {
        return false;
    }

    std::wstring_view exe { buffer, len };
    const auto slash = exe.find_last_of(L'\\');
    if(slash == std::wstring_view::npos) {
        return false;
    }

    out_is_game = equals_ignore_case(exe.substr(slash + 1), k_game_exe);

    g_engine_dir.assign(exe.substr(0, slash));

    const std::wstring stuff = g_engine_dir + L"\\TweakerStuff";
    const std::wstring logs = stuff + L"\\Logs";
    ::CreateDirectoryW(stuff.c_str(), nullptr);
    ::CreateDirectoryW(logs.c_str(), nullptr);

    g_log_path = logs + L"\\loadprobe.log";
    return true;
}

// A mode is on when its switch file exists or its token is in the probe's own file name. Says which, so
// a run that did not pick up a switch is obvious from the first lines of its section.
bool mode_enabled(HMODULE module, std::wstring_view token, std::string& out_reason)
{
    const std::wstring switch_file = g_engine_dir + L"\\TweakerStuff\\loadprobe." + std::wstring { token };
    if(file_exists(switch_file)) {
        out_reason = std::format("switch file '{}'", to_utf8(switch_file));
        return true;
    }

    wchar_t buffer[MAX_PATH] {};
    const DWORD len = ::GetModuleFileNameW(module, buffer, MAX_PATH);
    std::wstring_view name { buffer, len };
    if(const auto slash = name.find_last_of(L'\\'); slash != std::wstring_view::npos) {
        name.remove_prefix(slash + 1);
    }

    if(contains_ignore_case(name, token)) {
        out_reason = std::format("file name '{}'", to_utf8(name));
        return true;
    }

    out_reason = std::format("no '{}' and no '{}' in the file name", to_utf8(switch_file), to_utf8(token));
    return false;
}

void on_process_attach(HMODULE module, LPVOID reserved)
{
    g_attach_thread = ::GetCurrentThreadId();

    FILETIME unused {};
    ::GetProcessTimes(::GetCurrentProcess(), &g_process_created, &unused, &unused, &unused);

    bool is_game = false;
    if(!prepare_paths(is_game)) {
        return;
    }

    if(!is_game) {
        logf("attach: foreign process '{}' - probe stays inert", module_path_of(nullptr));
        return;
    }

    std::string observe_reason;
    std::string nodummy_reason;
    const bool observe = mode_enabled(module, L"observe", observe_reason);
    g_skip_dummy = mode_enabled(module, L"nodummy", nodummy_reason);

    SYSTEMTIME local {};
    ::GetLocalTime(&local);

    logf("attach: ===== {:04}-{:02}-{:02} {:02}:{:02}:{:02} pid {} probe rev 4 mode={}{} =====",
        local.wYear,
        local.wMonth,
        local.wDay,
        local.wHour,
        local.wMinute,
        local.wSecond,
        ::GetCurrentProcessId(),
        observe ? "observe" : "timeline",
        (!observe && g_skip_dummy) ? " (no dummy device)" : "");
    logf("attach: observe: {}", observe_reason);
    logf("attach: nodummy: {}", nodummy_reason);
    logf("attach: module {} at {}, lpReserved={} ({})",
        module_path_of(module),
        static_cast<void*>(module),
        static_cast<void*>(reserved),
        reserved == nullptr ? "dynamic load" : "static load");
    logf("attach: exe '{}', cwd '{}'", module_path_of(nullptr), current_directory());
    logf("attach: HighPoly.dll {}, d3d9.dll {}, dinput8.dll {}",
        ::GetModuleHandleW(L"HighPoly.dll") != nullptr ? "mapped" : "not mapped",
        ::GetModuleHandleW(L"d3d9.dll") != nullptr ? "mapped" : "not mapped",
        ::GetModuleHandleW(L"dinput8.dll") != nullptr ? "mapped" : "not mapped");

    int window_count = 0;
    const std::string windows = process_windows(window_count);
    logf("attach: {} top-level window(s){}", window_count, windows);
    logf("attach: stack{}", stack_text(0));

    if(observe) {
        return;
    }

    HMODULE pinned = nullptr;
    const BOOL pin_ok = ::GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(&on_process_attach), &pinned);
    logf("attach: pin {}", pin_ok ? "ok" : "FAILED");

    // Only once pinned: a registered callback in an image that FreeLibrary then unmaps would be called
    // for the very next module load.
    if(pin_ok) {
        const auto register_notification = reinterpret_cast<ldr_register_dll_notification_fn>(
            ::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "LdrRegisterDllNotification"));

        void* cookie = nullptr;
        const LONG status = register_notification != nullptr ? register_notification(0, &on_dll_notification, nullptr, &cookie) : -1;
        logf("attach: LdrRegisterDllNotification status=0x{:08X}", static_cast<unsigned long>(status));

        // In case d3d9/dinput8 were already mapped - not expected, but the notification would never
        // mention them then.
        if(HMODULE d3d9 = ::GetModuleHandleW(L"d3d9.dll"); d3d9 != nullptr) {
            hook_d3d9_exports(d3d9, suspend_threads::others);
        }
        if(HMODULE dinput8 = ::GetModuleHandleW(L"dinput8.dll"); dinput8 != nullptr) {
            hook_dinput8_exports(dinput8, suspend_threads::others);
        }
    }

    const std::uintptr_t thread = ::_beginthreadex(nullptr, 0, &timeline_thread, nullptr, 0, nullptr);
    logf("attach: timeline thread {}", thread != 0 ? "created" : "FAILED");
    if(thread != 0) {
        ::CloseHandle(reinterpret_cast<HANDLE>(thread));
    }
}
} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    switch(reason) {
        case DLL_PROCESS_ATTACH:
            ::DisableThreadLibraryCalls(module);
            on_process_attach(module, reserved);
            break;
        case DLL_PROCESS_DETACH:
            logf("detach: lpReserved={} ({})", static_cast<void*>(reserved), reserved == nullptr ? "FreeLibrary" : "process exit");
            break;
        default:
            break;
    }

    return TRUE;
}
