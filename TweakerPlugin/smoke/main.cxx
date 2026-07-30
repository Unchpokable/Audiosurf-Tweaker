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
#include "ui/pending_actions.hxx"
#include "ui/plugins/interactive/menu.hxx"
#include "ui/plugins/static/notefeed.hxx"
#include "ui/plugins/static/pins.hxx"
#include "ui/plugins/static/watermark.hxx"
#include "ui/texture_cache.hxx"
#include "ui/theme.hxx"
#include "ui/widgets/button.hxx"
#include "ui/widgets/color_picker.hxx"
#include "ui/widgets/item_group.hxx"
#include "ui/widgets/list_item.hxx"
#include "ui/widgets/popup_menu.hxx"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
struct wgl_window_data {
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

// Toggled from the smoke controls window - see draw_smoke_controls(). Lets both pending_actions
// branches (Docs/Internal/overlay-protocol.md, "Reverse-sync") be eyeballed without a real host:
// on, requests "round-trip" instantly; off, they're swallowed and pending_actions' own timeout +
// notefeed failure toast fires ~5s later.
bool g_smoke_auto_confirm = true;

// Mirrors overlay_ipc.cxx's percent_decode - smoke has no ipc/ dependency of its own, so this is a
// small local copy rather than exposing that file-local helper across TUs for one call site.
std::string smoke_percent_decode(std::string_view s)
{
    const auto from_hex = [](char c) -> int {
        if(c >= '0' && c <= '9') {
            return c - '0';
        }
        if(c >= 'a' && c <= 'f') {
            return 10 + c - 'a';
        }
        if(c >= 'A' && c <= 'F') {
            return 10 + c - 'A';
        }
        return -1;
    };

    std::string out;
    out.reserve(s.size());
    for(std::size_t i = 0; i < s.size();) {
        if(s[i] == '%' && i + 2 < s.size()) {
            const int hi = from_hex(s[i + 1]);
            const int lo = from_hex(s[i + 2]);
            if(hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 3;
                continue;
            }
        }
        out.push_back(s[i]);
        ++i;
    }
    return out;
}

// pending_actions' send backend (see ui/pending_actions.hxx) - stands in for tw::ipc::send_overlay_command,
// which doesn't exist in this build (no real TW_OVL IPC here). When g_smoke_auto_confirm is on, applies
// the request straight into overlay_state, as if the host had echoed TWEAK_SET/CURRENT_SKIN back
// immediately; when off, it's a no-op "sent into the void" so pending_actions' own deadline fires.
bool smoke_send_overlay_command(std::string_view op_line)
{
    if(!g_smoke_auto_confirm) {
        return true;
    }

    const auto space = op_line.find(' ');
    const std::string_view op = space == std::string_view::npos ? op_line : op_line.substr(0, space);
    const std::string_view rest = space == std::string_view::npos ? std::string_view {} : op_line.substr(space + 1);

    if(op == "NOTIFY_TWEAK") {
        const auto sp2 = rest.find(' ');
        if(sp2 == std::string_view::npos) {
            return true;
        }
        const auto id = tw::ui::overlay_state::resolve_tweak_id(rest.substr(0, sp2));
        const bool enabled = rest.substr(sp2 + 1) == "true";
        tw::ui::overlay_state::set_tweak(id, enabled);
    }
    else if(op == "NOTIFY_SKIN") {
        tw::ui::overlay_state::set_current_skin(smoke_percent_decode(rest));
    }

    return true;
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
    tw::ui::overlay_state::set_tweak(tweak_id::sidewinder_camera, true, true);
}

void draw_smoke_controls()
{
    using namespace tw::ui::widgets;

    static button push_toast_btn { "smoke_push_toast", { 200.f, 32.f } };
    static button toggle_skin_btn { "smoke_toggle_skin", { 200.f, 32.f } };
    static button toggle_qp_btn { "smoke_toggle_qp", { 200.f, 32.f } };
    static button open_popup_btn { "smoke_open_popup", { 200.f, 32.f } };
    static item_group actions { "smoke_actions", "Actions" };
    static item_group color_group { "smoke_color", "Color" };
    static color_picker accent_picker { "smoke_accent", { 280.f, 0.f } };
    static ImVec4 bound_color { 0.2f, 0.83f, 0.75f, 1.f };
    static popup_menu ctx_menu { "smoke_ctx_menu", "Context" };
    static list_item ctx_copy { "smoke_ctx_copy", { 160.f, 28.f } };
    static list_item ctx_paste { "smoke_ctx_paste", { 160.f, 28.f } };
    static int toast_count = 0;
    static bool alt_skin = false;
    static bool qp_override = true;
    static int ctx_action = 0;

    ImGui::SetNextWindowSize(ImVec2 { 360.f, 520.f }, ImGuiCond_FirstUseEver);
    ImGui::Begin("Smoke controls");

    ImGui::TextUnformatted("Real TweakerPlugin overlay modules, fed with fake overlay_state data.");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::TextUnformatted("Press Insert to toggle the menu (drag title bar to move, corner to resize).");
    ImGui::TextUnformatted("RMB in this window opens popup_menu; LMB on Open popup too.");
    ImGui::Separator();

    actions.set_inner_padding({ 10.f, 8.f });
    actions.begin();
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

    toggle_qp_btn.update(qp_override ? "Clear QP marker" : "Set QP marker");
    if(toggle_qp_btn.clicked()) {
        qp_override = !qp_override;
        tw::ui::overlay_state::set_tweak(tw::ui::overlay_state::tweak_id::sidewinder_camera, true, qp_override);
    }

    open_popup_btn.update("Open popup (LMB)");
    if(open_popup_btn.clicked()) {
        ctx_menu.open();
    }
    ImGui::Checkbox("Auto-confirm NOTIFY_TWEAK/NOTIFY_SKIN (simulate host online)", &g_smoke_auto_confirm);
    ImGui::TextUnformatted("Off = requests time out after ~5s and show a notefeed failure toast.");
    actions.end();

    color_group.set_inner_padding({ 10.f, 8.f });
    color_group.begin();
    accent_picker.update(bound_color);
    ImGui::ColorButton("##smoke_swatch", bound_color, ImGuiColorEditFlags_AlphaPreviewHalf, ImVec2 { 48.f, 24.f });
    ImGui::SameLine();
    ImGui::Text(
        "RGBA %.2f %.2f %.2f %.2f%s",
        bound_color.x,
        bound_color.y,
        bound_color.z,
        bound_color.w,
        accent_picker.changed() ? " *" : "");
    color_group.end();

    if(ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(ImGuiMouseButton_Right)
        && !ctx_menu.opened()) {
        ctx_menu.open();
    }

    if(ctx_action != 0) {
        ImGui::Text("Last popup action: %s", ctx_action == 1 ? "Copy" : "Paste");
    }

    ImGui::End();

    if(ctx_menu.opened()) {
        ctx_menu.begin();
        ctx_copy.set_content({ .text = "Copy" });
        ctx_copy.update();
        if(ctx_copy.clicked()) {
            ctx_action = 1;
            ctx_menu.close();
        }
        ctx_paste.set_content({ .text = "Paste" });
        ctx_paste.update();
        if(ctx_paste.clicked()) {
            ctx_action = 2;
            ctx_menu.close();
        }
        ctx_menu.end();
    }
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
        // Mirrors the DLL's own toggle-key subscriber (ui_main.cxx, handle_toggle_key): the menu
        // no longer polls the keyboard, the harness that owns the message pump tells it when the
        // hotkey was pressed. Bit 30 of lparam is the previous key state - set means auto-repeat.
        case WM_KEYDOWN:
            if(wparam == VK_INSERT && (lparam & (LPARAM { 1 } << 30)) == 0) {
                tw::ui::plugins::interactive::menu::toggle_visible();
            }
            return 0;
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
    tw::ui::pending_actions::set_send_backend(&smoke_send_overlay_command);
    tw::ui::overlay_config::load("smoke_overlay.cfg");
    seed_fake_overlay_state();

    tw::ui::theme::apply_dark();
    tw::ui::theme::from_config(tw::ui::overlay_config::theme_overrides());

    ImGui_ImplWin32_EnableDpiAwareness();
    const float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT { 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

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

    HWND hwnd = ::CreateWindowW(wc.lpszClassName,
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
        ImFont* loaded = io.Fonts->AddFontFromMemoryTTF(const_cast<void*>(static_cast<const void*>(font->bytes.data())),
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

    // Smoke-harness-only backdrop - a real overlay has no window background to paint, so this
    // isn't part of tw::ui::theme (matches former theme::app_background's dark value, #FF1B1D21).
    constexpr ImVec4 clear_color { 0x1B / 255.f, 0x1D / 255.f, 0x21 / 255.f, 1.f };

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
        tw::ui::pending_actions::update(cache);

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
