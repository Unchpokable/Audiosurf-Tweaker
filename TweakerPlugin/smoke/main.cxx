// Visual smoke harness for TweakerPlugin ImGui widgets (Win32 + OpenGL3).
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
#include <wincodec.h>

#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

#include "resource/resource.hxx"

#include "ui/theme.hxx"
#include "ui/widgets/button.hxx"
#include "ui/widgets/list_item.hxx"
#include "ui/widgets/list_view.hxx"
#include "ui/widgets/toggle.hxx"

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
GLuint g_scrap_tex = 0;
int g_scrap_w = 0;
int g_scrap_h = 0;

bool create_device_wgl(HWND hwnd, wgl_window_data* data);
void cleanup_device_wgl(HWND hwnd, wgl_window_data* data);
LRESULT WINAPI wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

// Decode PNG bytes → RGBA8 via WIC (smoke-only; no stb_image in tree).
bool decode_png_rgba(std::span<const std::byte> png, std::vector<std::uint8_t>& rgba, int& width, int& height)
{
    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;

    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if(FAILED(hr)) {
        return false;
    }

    hr = factory->CreateStream(&stream);
    if(SUCCEEDED(hr)) {
        hr = stream->InitializeFromMemory(
            reinterpret_cast<BYTE*>(const_cast<std::byte*>(png.data())),
            static_cast<DWORD>(png.size()));
    }
    if(SUCCEEDED(hr)) {
        hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    }
    if(SUCCEEDED(hr)) {
        hr = decoder->GetFrame(0, &frame);
    }
    if(SUCCEEDED(hr)) {
        hr = factory->CreateFormatConverter(&converter);
    }
    if(SUCCEEDED(hr)) {
        hr = converter->Initialize(
            frame,
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);
    }

    UINT w = 0;
    UINT h = 0;
    if(SUCCEEDED(hr)) {
        hr = converter->GetSize(&w, &h);
    }
    if(SUCCEEDED(hr) && w > 0 && h > 0) {
        rgba.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u);
        hr = converter->CopyPixels(
            nullptr,
            w * 4u,
            static_cast<UINT>(rgba.size()),
            rgba.data());
        if(SUCCEEDED(hr)) {
            width = static_cast<int>(w);
            height = static_cast<int>(h);
        }
    }

    if(converter) {
        converter->Release();
    }
    if(frame) {
        frame->Release();
    }
    if(decoder) {
        decoder->Release();
    }
    if(stream) {
        stream->Release();
    }
    if(factory) {
        factory->Release();
    }
    return SUCCEEDED(hr);
}

bool upload_rgba_texture(const std::uint8_t* rgba, int width, int height, GLuint& out_tex)
{
    if(rgba == nullptr || width <= 0 || height <= 0) {
        return false;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glBindTexture(GL_TEXTURE_2D, 0);

    out_tex = tex;
    return tex != 0;
}

void draw_widget_gallery()
{
    using namespace tw::ui::widgets;

    static button primary_btn{"smoke_primary", {160.f, 36.f}};
    static button secondary_btn{"smoke_secondary", {160.f, 36.f}};
    static toggle feature_toggle{"smoke_toggle", {44.f, 24.f}};
    static list_item lone_row{"smoke_lone_row", {0.f, 40.f}};
    static list_view skins{"smoke_list", {0.f, 180.f}};
    static bool items_ready = false;
    static int click_count = 0;
    static bool lone_selected = false;

    if(!items_ready) {
        const std::vector<list_item_content> seed = {
            {.text = "Neon Pulse"},
            {.text = "Mono Track"},
            {.text = "Chrome Wave"},
            {.text = "Void Runner"},
            {.text = "Pixel Drift"},
            {.text = "Solar Flare"},
        };
        skins.set_items(seed);
        lone_row.set_content({.text = "Standalone list_item (click to select)"});
        items_ready = true;
    }

    ImGui::SetNextWindowSize(ImVec2{520.f, 620.f}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Tweaker UI smoke");

    ImGui::TextUnformatted("Theme: dark defaults + embedded Roboto-Regular");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Separator();

    if(g_scrap_tex != 0) {
        ImGui::TextUnformatted("Resource texture: textures/scrap.png");
        const float preview = 96.f;
        ImGui::Image(
            static_cast<ImTextureID>(g_scrap_tex),
            ImVec2{preview, preview * (static_cast<float>(g_scrap_h) / static_cast<float>(g_scrap_w))});
        ImGui::Text("size %dx%d  gl=%u", g_scrap_w, g_scrap_h, g_scrap_tex);
        ImGui::Separator();
    }

    ImGui::TextUnformatted("Buttons");
    primary_btn.update("Primary action");
    if(primary_btn.clicked()) {
        ++click_count;
    }
    ImGui::SameLine();
    ImGui::Text("clicks: %d", click_count);

    secondary_btn.update("Hover / press me");
    ImGui::Separator();

    ImGui::TextUnformatted("Toggle");
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Feature enabled");
    ImGui::SameLine();
    feature_toggle.update();
    ImGui::Text(
        "checked=%s  changed_last_frame=%s",
        feature_toggle.checked() ? "true" : "false",
        feature_toggle.changed() ? "true" : "false");
    ImGui::Separator();

    ImGui::TextUnformatted("list_item (reusable)");
    lone_row.set_selected(lone_selected);
    lone_row.update();
    if(lone_row.clicked()) {
        lone_selected = !lone_selected;
    }
    ImGui::Separator();

    ImGui::TextUnformatted("list_view (single-select + scroll)");
    skins.update();
    ImGui::Text(
        "selected_index=%d  selection_changed=%s",
        skins.selected_index(),
        skins.selection_changed() ? "true" : "false");

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
    if(FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        std::fprintf(stderr, "smoke_test: CoInitializeEx failed\n");
        return 1;
    }

    if(!tw::resource::initialize(::GetModuleHandleW(nullptr))) {
        std::fprintf(stderr, "smoke_test: resource::initialize failed\n");
        CoUninitialize();
        return 1;
    }

    const auto font = tw::resource::get_resource(tw::resource::type::font, "fonts/Roboto-Regular.ttf");
    if(!font || font->bytes.empty()) {
        std::fprintf(stderr, "smoke_test: missing embedded font fonts/Roboto-Regular.ttf\n");
        CoUninitialize();
        return 1;
    }

    const auto scrap = tw::resource::get_resource(tw::resource::type::texture, "textures/scrap.png");
    if(!scrap || scrap->bytes.empty()) {
        std::fprintf(stderr, "smoke_test: missing embedded texture textures/scrap.png\n");
        CoUninitialize();
        return 1;
    }

    std::fprintf(
        stderr,
        "smoke_test: font %zu bytes, texture %zu bytes\n",
        font->bytes.size(),
        scrap->bytes.size());

    tw::ui::theme::apply_dark();

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
        CoUninitialize();
        return 1;
    }
    wglMakeCurrent(g_main_window.hdc, g_hRC);
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    {
        std::vector<std::uint8_t> rgba;
        if(!decode_png_rgba(scrap->bytes, rgba, g_scrap_w, g_scrap_h)
           || !upload_rgba_texture(rgba.data(), g_scrap_w, g_scrap_h, g_scrap_tex)) {
            std::fprintf(stderr, "smoke_test: failed to decode/upload textures/scrap.png\n");
            cleanup_device_wgl(hwnd, &g_main_window);
            wglDeleteContext(g_hRC);
            ::DestroyWindow(hwnd);
            ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
            CoUninitialize();
            return 1;
        }
        std::fprintf(stderr, "smoke_test: scrap texture %dx%d gl=%u\n", g_scrap_w, g_scrap_h, g_scrap_tex);
    }

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
            if(g_scrap_tex) {
                glDeleteTextures(1, &g_scrap_tex);
            }
            cleanup_device_wgl(hwnd, &g_main_window);
            wglDeleteContext(g_hRC);
            ::DestroyWindow(hwnd);
            ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
            CoUninitialize();
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

        draw_widget_gallery();

        ImGui::Render();
        glViewport(0, 0, g_width, g_height);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ::SwapBuffers(g_main_window.hdc);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if(g_scrap_tex) {
        glDeleteTextures(1, &g_scrap_tex);
        g_scrap_tex = 0;
    }

    cleanup_device_wgl(hwnd, &g_main_window);
    wglDeleteContext(g_hRC);
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    CoUninitialize();
    return 0;
}
