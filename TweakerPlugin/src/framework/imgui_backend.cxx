#include "pch.hxx"

#include "framework/imgui_backend.hxx"

#include "plugin/globals.hxx"

#include "resource/resource.hxx"

#include "ui/overlay_config.hxx"
#include "ui/texture_cache.hxx"
#include "ui/theme.hxx"

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

#include <cstdint>
#include <string>

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
std::string g_config_path;

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
               const_cast<void*>(static_cast<const void*>(font->bytes.data())),
               static_cast<int>(font->bytes.size()),
               18.0f,
               &font_cfg)
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
            ImGui::DestroyContext();
            g_device = nullptr;
            return false;
        }

        tw::ui::theme::apply_dark();
        tw::ui::theme::from_config(tw::ui::overlay_config::theme_overrides());
        ImGui::StyleColorsDark();

        g_context_created = true;
    }

    if(!ImGui_ImplWin32_Init(hwnd) || !ImGui_ImplDX9_Init(device)) {
        ImGui_ImplWin32_Shutdown();
        g_device = nullptr;
        return false;
    }

    g_initialized = true;
    return true;
}

void unbind()
{
    if(!g_initialized) {
        return;
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();

    g_device = nullptr;
    g_initialized = false;
}

void shutdown()
{
    if(!g_context_created) {
        return;
    }

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

bool wndproc_bridge(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LRESULT& out_result)
{
    if(!g_initialized) {
        return false;
    }

    ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);

    const ImGuiIO& io = ImGui::GetIO();
    if(!io.WantCaptureMouse && !io.WantCaptureKeyboard) {
        return false;
    }

    out_result = 0;
    return true;
}
} // namespace tw::framework::imgui_backend
