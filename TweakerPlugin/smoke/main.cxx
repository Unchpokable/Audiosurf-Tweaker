// Visual smoke harness for the real TweakerPlugin overlay UI (Win32 + OpenGL3) - notefeed, pins,
// watermark, and the interactive menu, driven by fake overlay_state data instead of real TW_OVL
// IPC. See Docs/Internal/tweaker-plugin-widgets.md.
// Build: cmake --build --preset x86-release --target smoke_test
//
// Smoke is a standalone exe (no TweakerPlugin pch.hxx / Quest3D) — library
// includes live here. Plugin sources use pch.hxx instead.

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <GL/gl.h>
#include <tchar.h>

#include <cstdio>
#include <expected>
#include <span>
#include <string>
#include <vector>

// resource.hxx uses std::span/std::expected without including them itself (relies on the
// TweakerPlugin PCH in DLL consumers) - explicit here, same convention as ui/texture_cache.cxx.
#include "resource/resource.hxx"

#include "ui/overlay_config.hxx"
#include "ui/overlay_state.hxx"
#include "ui/plugins/interactive/menu.hxx"
#include "ui/plugins/static/notefeed.hxx"
#include "ui/plugins/static/pins.hxx"
#include "ui/plugins/static/watermark.hxx"
#include "ui/texture_cache.hxx"
#include "ui/theme.hxx"
#include "ui/widgets/button.hxx"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
struct wgl_window_data
{
    HDC hdc = nullptr;
};

HGLRC g_hRC = nullptr;
wgl_window_data g_main_window {};
int g_width = 0;
int g_height = 0;

bool create_device_wgl(HWND hwnd, wgl_window_data* data);
void cleanup_device_wgl(HWND hwnd, wgl_window_data* data);
LRESULT WINAPI wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

// texture_cache's pluggable upload backend (see ui/texture_cache.hxx) - the DLL registers a D3D9
// uploader (framework/imgui_backend.cxx), this registers the GL equivalent so the exact same
// notefeed/pins/watermark/menu code can load real packed icons here too.
ImTextureID gl_upload_texture(const unsigned char* rgba, int width, int height)
{
    if(rgba == nullptr || width <= 0 || height <= 0) {
        return ImTextureID_Invalid;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Explicit clamp (not the GL_REPEAT default) - default wrap mode on non-power-of-two textures
    // is where legacy/compatibility GL contexts are most likely to sample garbage at the edges.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glBindTexture(GL_TEXTURE_2D, 0);

    return tex != 0 ? static_cast<ImTextureID>(tex) : ImTextureID_Invalid;
}

// Bypasses real TW_OVL IPC entirely - overlay_state's setters are public API, so smoke just calls
// them directly to get plausible data flowing into notefeed/pins/menu.
void seed_fake_overlay_state()
{
    using tw::ui::overlay_state::tweak_id;

    std::vector<std::string> skins = {
        "Neon Pulse",
        "Mono Track",
        "Chrome Wave",
        "Void Runner",
        "Pixel Drift",
        "Solar Flare",
        "Aurora Drift",
        "Carbon Night",
    };
    tw::ui::overlay_state::set_skin_list(std::move(skins));
    tw::ui::overlay_state::set_current_skin("Neon Pulse");
    tw::ui::overlay_state::set_tweak(tweak_id::invisible_road, true);
    tw::ui::overlay_state::set_tweak(tweak_id::sidewinder_camera, true);
}

void draw_smoke_controls()
{
    using namespace tw::ui::widgets;

    static button push_toast_btn {"smoke_push_toast", {200.f, 32.f}};
    static button toggle_skin_btn {"smoke_toggle_skin", {200.f, 32.f}};
    static int toast_count = 0;
    static bool alt_skin = false;

    ImGui::SetNextWindowSize(ImVec2 {360.f, 220.f}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Smoke controls");

    ImGui::TextUnformatted("Real TweakerPlugin overlay modules, fed with fake overlay_state data.");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::TextUnformatted("Press Insert to toggle the menu (drag title bar to move, corner to resize).");
    ImGui::Separator();

    push_toast_btn.update("Push notefeed toast");
    if(push_toast_btn.clicked()) {
        ++toast_count;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Test notification #%d", toast_count);
        // A pre-baked, power-of-two 32px variant - notefeed's icon slot is 32px, and the
        // non-power-of-two 48px variant (TweakerIcon-4.png) rendered corrupted/cropped under
        // smoke's legacy-compatibility GL context (no such restriction on the DLL's D3D9 path,
        // but staying POT here avoids the divergence). See assets/textures/ for the full size set.
        tw::ui::plugins::statics::notefeed::push(buf, "textures/TweakerIcon-5.png");
    }

    toggle_skin_btn.update("Toggle current skin");
    if(toggle_skin_btn.clicked()) {
        alt_skin = !alt_skin;
        tw::ui::overlay_state::set_current_skin(alt_skin ? "Void Runner" : "Neon Pulse");
    }

    ImGui::End();
}

bool create_device_wgl(HWND hwnd, wgl_window_data* data)
{
    HDC hdc = ::GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    const int pf = ::ChoosePixelFormat(hdc, &pfd);
    if(pf == 0) {
        return false;
    }
    if(::SetPixelFormat(hdc, pf, &pfd) == FALSE) {
        return false;
    }
    ::ReleaseDC(hwnd, hdc);

    data->hdc = ::GetDC(hwnd);
    if(!g_hRC) {
        g_hRC = wglCreateContext(data->hdc);
    }
    return wglMakeCurrent(data->hdc, g_hRC) == TRUE;
}

void cleanup_device_wgl(HWND hwnd, wgl_window_data* data)
{
    wglMakeCurrent(nullptr, nullptr);
    ::ReleaseDC(hwnd, data->hdc);
}

LRESULT WINAPI wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if(ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }

    switch(msg) {
        case WM_SIZE:
            if(wparam != SIZE_MINIMIZED) {
                g_width = LOWORD(lparam);
                g_height = HIWORD(lparam);
            }
            return 0;
        case WM_SYSCOMMAND:
            if((wparam & 0xfff0) == SC_KEYMENU) {
                return 0;
            }
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}
} // namespace

int main(int, char**)
{
    if(!tw::resource::initialize(::GetModuleHandleW(nullptr))) {
        std::fprintf(stderr, "smoke_test: resource::initialize failed\n");
        return 1;
    }

    const auto font = tw::resource::get_resource(tw::resource::type::font, "fonts/Roboto-Regular.ttf");
    if(!font || font->bytes.empty()) {
        std::fprintf(stderr, "smoke_test: missing embedded font fonts/Roboto-Regular.ttf\n");
        return 1;
    }

    tw::ui::texture_cache::set_backend(&gl_upload_texture);
    tw::ui::overlay_config::load("smoke_overlay.cfg");
    seed_fake_overlay_state();

    tw::ui::theme::apply_dark();
    tw::ui::theme::from_config(tw::ui::overlay_config::theme_overrides());

    ImGui_ImplWin32_EnableDpiAwareness();
    const float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(
        ::MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));

    WNDCLASSEXW wc = {
        sizeof(wc),
        CS_OWNDC,
        wnd_proc,
        0L,
        0L,
        GetModuleHandle(nullptr),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        L"TweakerUiSmoke",
        nullptr,
    };
    ::RegisterClassExW(&wc);

    HWND hwnd = ::CreateWindowW(
        wc.lpszClassName,
        L"TweakerPlugin UI Smoke",
        WS_OVERLAPPEDWINDOW,
        100,
        100,
        static_cast<int>(900 * main_scale),
        static_cast<int>(700 * main_scale),
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr);

    if(!create_device_wgl(hwnd, &g_main_window)) {
        cleanup_device_wgl(hwnd, &g_main_window);
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        std::fprintf(stderr, "smoke_test: failed to create OpenGL context\n");
        return 1;
    }
    wglMakeCurrent(g_main_window.hdc, g_hRC);
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    {
        ImFontConfig font_cfg;
        font_cfg.FontDataOwnedByAtlas = false; // bytes live in PE LockResource memory
        ImFont* loaded = io.Fonts->AddFontFromMemoryTTF(
            const_cast<void*>(static_cast<const void*>(font->bytes.data())),
            static_cast<int>(font->bytes.size()),
            18.0f * main_scale,
            &font_cfg);
        if(loaded == nullptr) {
            std::fprintf(stderr, "smoke_test: AddFontFromMemoryTTF failed\n");
            cleanup_device_wgl(hwnd, &g_main_window);
            wglDeleteContext(g_hRC);
            ::DestroyWindow(hwnd);
            ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return 1;
        }
        std::fprintf(stderr, "smoke_test: Roboto-Regular registered with ImGui\n");
    }

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    ImGui_ImplWin32_InitForOpenGL(hwnd);
    ImGui_ImplOpenGL3_Init();

    tw::ui::plugins::statics::watermark::initialize();
    tw::ui::plugins::statics::pins::initialize();
    tw::ui::plugins::statics::notefeed::initialize();
    tw::ui::plugins::interactive::menu::initialize();

    const ImVec4 clear_color = tw::ui::theme::app_background;

    bool done = false;
    while(!done) {
        MSG msg;
        while(::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if(msg.message == WM_QUIT) {
                done = true;
            }
        }
        if(done) {
            break;
        }
        if(::IsIconic(hwnd)) {
            ::Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        static tw::ui::overlay_state::cache cache;
        tw::ui::overlay_state::refresh(cache);

        draw_smoke_controls();
        tw::ui::plugins::statics::watermark::update();
        tw::ui::plugins::statics::pins::update(cache);
        tw::ui::plugins::statics::notefeed::update();
        tw::ui::plugins::interactive::menu::update(cache);

        ImGui::Render();
        glViewport(0, 0, g_width, g_height);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ::SwapBuffers(g_main_window.hdc);
    }

    tw::ui::overlay_config::save();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    cleanup_device_wgl(hwnd, &g_main_window);
    wglDeleteContext(g_hRC);
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
