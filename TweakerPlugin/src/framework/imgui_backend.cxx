#include "pch.hxx"

#include "framework/imgui_backend.hxx"

#include "plugin/diagnostics.hxx"
#include "plugin/globals.hxx"

#include "resource/resource.hxx"

#include "ui/overlay_config.hxx"
#include "ui/texture_cache.hxx"
#include "ui/theme.hxx"

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

namespace
{
// g_context_created tracks the ImGui context/fonts/theme (process lifetime, torn down only by
// shutdown()). g_initialized tracks whether the DX9/Win32 backends are currently bound to
// g_device - it can go false and true again several times per process, once per device change,
// without the context ever being touched. Kept distinct so a device swap doesn't pay for a font
// atlas reparse + theme reapply every time.
bool g_context_created = false;
bool g_initialized = false;
IDirect3DDevice9* g_device = nullptr;
HWND g_hwnd = nullptr;
std::string g_config_path;

// Input gate (see imgui_backend.hxx). g_input_open tracks the gate's value as of the last message
// so the open->closed / closed->open edges can be detected and cleaned up after.
std::atomic<tw::framework::imgui_backend::input_gate_fn> g_input_gate { nullptr };
bool g_input_open = false;

// Drops every scrap of input state ImGui is holding, and gives back any mouse capture ImGui took.
// Run on both edges of the gate:
//   closing - a button still physically down when the menu closes would otherwise leave ImGui with
//             a stuck ActiveId (next open starts with a widget already "held") and, worse, leave
//             ::SetCapture pointing at us with no button-up ever coming to release it.
//   opening - symmetric: nothing the game did while the gate was shut should look like a click or
//             a keypress aimed at the menu on its very first frame.
void reset_input_state(HWND hwnd)
{
    ImGuiIO& io = ImGui::GetIO();
    io.ClearEventsQueue();
    io.ClearInputKeys();
    io.ClearInputMouse();

    if(hwnd != nullptr && ::GetCapture() == hwnd) {
        ::ReleaseCapture();
    }
}

// Messages ImGui must keep seeing regardless of the gate. None of these touch capture, cursor
// tracking or any other shared Win32 state - they only update ImGui's own bookkeeping, and letting
// it go stale causes real bugs (a menu opened after an alt-tab believing the window is unfocused,
// or an IME codepage from three keyboard layouts ago).
bool is_always_observed(UINT msg) noexcept
{
    return msg == WM_SETFOCUS || msg == WM_KILLFOCUS || msg == WM_INPUTLANGCHANGE || msg == WM_DESTROY;
}

// The message classes ImGui genuinely consumes. Swallowed wholesale while the gate is open; never
// touched while it is closed.
bool is_input_message(UINT msg) noexcept
{
    switch(msg) {
        case WM_MOUSEMOVE:
        case WM_NCMOUSEMOVE:
        case WM_MOUSELEAVE:
        case WM_NCMOUSELEAVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_MBUTTONUP:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
        case WM_XBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_CHAR:
        case WM_SYSCHAR:
            return true;
        default:
            return false;
    }
}

// TweakerPlugin.dll -> .../TweakerPlugin.overlay.cfg, next to the DLL itself. Same
// GetModuleFileNameW + WideCharToMultiByte(CP_UTF8) pattern as resource/self_extract.cxx.
std::string compute_config_path()
{
    wchar_t wide_path[MAX_PATH] {};
    const DWORD len = ::GetModuleFileNameW(tw::plugin::globals::module_handle, wide_path, MAX_PATH);
    if(len == 0 || len >= MAX_PATH) {
        return {};
    }

    const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, wide_path, -1, nullptr, 0, nullptr, nullptr);
    if(bytes <= 1) {
        return {};
    }

    std::string path(static_cast<std::size_t>(bytes - 1), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide_path, -1, path.data(), bytes, nullptr, nullptr);

    const auto dot = path.find_last_of('.');
    if(dot != std::string::npos) {
        path.resize(dot);
    }
    path += ".overlay.cfg";
    return path;
}

// texture_cache's pluggable upload backend (see ui/texture_cache.hxx) - D3DPOOL_MANAGED survives
// Reset() on its own (the D3D9 runtime re-uploads from its system-memory copy), so unlike the
// main ImGui font atlas these small UI icon textures need no InvalidateDeviceObjects handling.
ImTextureID d3d9_upload_texture(const unsigned char* rgba, int width, int height)
{
    if(g_device == nullptr || rgba == nullptr || width <= 0 || height <= 0) {
        return ImTextureID_Invalid;
    }

    IDirect3DTexture9* texture = nullptr;
    if(FAILED(g_device->CreateTexture(
           static_cast<UINT>(width), static_cast<UINT>(height), 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr))
        || texture == nullptr) {
        return ImTextureID_Invalid;
    }

    D3DLOCKED_RECT locked {};
    if(FAILED(texture->LockRect(0, &locked, nullptr, 0))) {
        texture->Release();
        return ImTextureID_Invalid;
    }

    auto* dst_base = static_cast<std::uint8_t*>(locked.pBits);
    for(int y = 0; y < height; ++y) {
        const auto* src_row = rgba + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4u;
        auto* dst_row = dst_base + static_cast<std::size_t>(y) * static_cast<std::size_t>(locked.Pitch);
        for(int x = 0; x < width; ++x) {
            // Source is tightly-packed RGBA8 (stb_image); D3DFMT_A8R8G8B8 stores bytes as B,G,R,A.
            dst_row[x * 4 + 0] = src_row[x * 4 + 2];
            dst_row[x * 4 + 1] = src_row[x * 4 + 1];
            dst_row[x * 4 + 2] = src_row[x * 4 + 0];
            dst_row[x * 4 + 3] = src_row[x * 4 + 3];
        }
    }
    texture->UnlockRect(0);

    return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(texture));
}

bool load_font()
{
    const auto font = tw::resource::get_resource(tw::resource::type::font, "fonts/Roboto-Regular.ttf");
    if(!font || font->bytes.empty()) {
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false; // bytes live in PE LockResource memory - never freed by ImGui
    return io.Fonts->AddFontFromMemoryTTF(
               const_cast<void*>(static_cast<const void*>(font->bytes.data())), static_cast<int>(font->bytes.size()), 18.0f, &font_cfg)
           != nullptr;
}
} // namespace

namespace tw::framework::imgui_backend
{
bool initialize(IDirect3DDevice9* device, HWND hwnd)
{
    if(device == nullptr || hwnd == nullptr) {
        return g_initialized;
    }

    if(g_initialized && device == g_device) {
        return true;
    }

    // Either first bind, or the bound device actually changed (new CreateDevice, or Reset()
    // swapped the focus window) - tear down just the backends, keeping the ImGui context/fonts/
    // theme alive if they already exist, then (re)target everything at the new device/window.
    if(g_initialized) {
        unbind();
    }

    g_device = device;
    tw::ui::texture_cache::set_backend(&d3d9_upload_texture);
    // Icons already uploaded belong to whatever device we were bound to before (or none) - those
    // IDirect3DTexture9 pointers are dangling now, not just stale. Drop them so the next
    // get_or_load() re-decodes and re-uploads through the new device.
    tw::ui::texture_cache::clear();

    if(!g_context_created) {
        g_config_path = compute_config_path();
        if(!g_config_path.empty()) {
            tw::ui::overlay_config::load(g_config_path);
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr; // no imgui.ini next to the game exe - geometry persists via overlay_config

        if(!load_font()) {
            TW_LOG_ERROR("imgui_backend: embedded font fonts/Roboto-Regular.ttf missing or unusable, overlay disabled");
            ImGui::DestroyContext();
            g_device = nullptr;
            return false;
        }

        tw::ui::theme::apply_dark();
        tw::ui::theme::from_config(tw::ui::overlay_config::theme_overrides());
        ImGui::StyleColorsDark();

        g_context_created = true;
        TW_LOG_INFO("imgui_backend: context created (config='{}')", g_config_path);
    }

    // Torn apart rather than short-circuited: ImGui_ImplWin32_Shutdown() asserts on a null backend,
    // so the old `!Init(hwnd) || !Init(device)` + unconditional Win32 shutdown would trip its own
    // IM_ASSERT precisely when the Win32 init was the half that failed - and leak the DX9 backend
    // in the other direction.
    if(!ImGui_ImplWin32_Init(hwnd)) {
        TW_LOG_ERROR("imgui_backend: ImGui_ImplWin32_Init failed (hwnd={})", static_cast<const void*>(hwnd));
        g_device = nullptr;
        return false;
    }

    if(!ImGui_ImplDX9_Init(device)) {
        TW_LOG_ERROR("imgui_backend: ImGui_ImplDX9_Init failed (device={})", static_cast<const void*>(device));
        ImGui_ImplWin32_Shutdown();
        g_device = nullptr;
        return false;
    }

    g_hwnd = hwnd;
    g_initialized = true;
    TW_LOG_INFO("imgui_backend: bound to device={} hwnd={}", static_cast<const void*>(device), static_cast<const void*>(hwnd));
    return true;
}

void unbind()
{
    if(!g_initialized) {
        return;
    }

    TW_LOG_INFO("imgui_backend: unbinding from device={} hwnd={}", static_cast<const void*>(g_device), static_cast<const void*>(g_hwnd));

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();

    g_device = nullptr;
    g_hwnd = nullptr;
    g_initialized = false;
    // The gate's edge tracking is meaningless without a live backend; re-arm it so the next bind
    // re-runs reset_input_state() on its first open instead of assuming it already did.
    g_input_open = false;
}

void shutdown()
{
    if(!g_context_created) {
        return;
    }

    TW_LOG_INFO("imgui_backend: shutting down context");

    unbind();

    tw::ui::overlay_config::save();
    ImGui::DestroyContext();
    g_context_created = false;
}

void on_lost_device()
{
    if(g_initialized) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }
}

void on_reset_device()
{
    if(g_initialized) {
        ImGui_ImplDX9_CreateDeviceObjects();
    }
}

void new_frame()
{
    if(!g_initialized) {
        return;
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void render()
{
    if(!g_initialized) {
        return;
    }

    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

bool is_initialized() noexcept
{
    return g_initialized;
}

void attach_input_gate(input_gate_fn fn) noexcept
{
    g_input_gate.store(fn, std::memory_order_relaxed);
}

void detach_input_gate() noexcept
{
    g_input_gate.store(nullptr, std::memory_order_relaxed);
}

bool wndproc_bridge(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LRESULT& out_result)
{
    if(!g_initialized) {
        return false;
    }

    const input_gate_fn gate = g_input_gate.load(std::memory_order_relaxed);
    const bool input_open = gate != nullptr && gate();

    if(input_open != g_input_open) {
        reset_input_state(hwnd);
        g_input_open = input_open;
    }

    if(!input_open) {
        // Gate shut: hand ImGui only the side-effect-free bookkeeping messages, and never claim
        // anything - the game owns its input entirely. See imgui_backend.hxx for why merely
        // *observing* the rest is not harmless.
        if(is_always_observed(msg)) {
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
        }

        return false;
    }

    ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);

    // Swallow ONLY the message classes ImGui genuinely consumes. Blanket-swallowing every message
    // while the overlay is up was a trap: WM_NCHITTEST got swallowed too, and returning our
    // out_result (0 == HTNOWHERE) for WM_NCHITTEST makes Windows stop generating client mouse
    // messages (WM_LBUTTONDOWN...) for the window entirely. That's exactly the "hover works, clicks
    // don't" symptom: cursor position still arrives (ImGui polls it via GetCursorPos every frame in
    // ImGui_ImplWin32_NewFrame), but no button message ever reaches the handler above. Gate on the
    // message itself so non-input messages (WM_NCHITTEST, WM_SETCURSOR, WM_MOUSEACTIVATE, ...) are
    // always forwarded to the game untouched.
    //
    // No WantCaptureMouse/WantCaptureKeyboard check here on purpose: visibility alone decides, so
    // this stays in lockstep with the dinput8 gate, which zeroes the game's raw device reads over
    // exactly the same interval. Consulting WantCapture* as well would mean input the game can
    // still see through the window (cursor outside the menu rect) while dinput hides it - two
    // gates, two answers, one very confusing bug report.
    if(is_input_message(msg)) {
        out_result = 0;
        return true;
    }

    return false;
}
} // namespace tw::framework::imgui_backend
