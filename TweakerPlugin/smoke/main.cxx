// Visual smoke harness for TweakerPlugin ImGui widgets (Win32 + OpenGL3).
// Build: cmake --build --preset x86-release --target smoke_test

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

#include "ui/theme.hxx"
#include "ui/widgets/button.hxx"
#include "ui/widgets/list_item.hxx"
#include "ui/widgets/list_view.hxx"
#include "ui/widgets/toggle.hxx"

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
#include <vector>

struct WGL_WindowData
{
    HDC hDC = nullptr;
};

static HGLRC g_hRC = nullptr;
static WGL_WindowData g_MainWindow {};
static int g_Width = 0;
static int g_Height = 0;

bool CreateDeviceWGL(HWND hWnd, WGL_WindowData* data);
void CleanupDeviceWGL(HWND hWnd, WGL_WindowData* data);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void draw_widget_gallery()
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

    if (!items_ready)
    {
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

    ImGui::SetNextWindowSize(ImVec2{520.f, 560.f}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Tweaker UI smoke");

    ImGui::TextUnformatted("Theme: dark defaults (tw::ui::theme)");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Separator();

    ImGui::TextUnformatted("Buttons");
    primary_btn.update("Primary action");
    if (primary_btn.clicked())
        ++click_count;
    ImGui::SameLine();
    ImGui::Text("clicks: %d", click_count);

    secondary_btn.update("Hover / press me");
    ImGui::Separator();

    ImGui::TextUnformatted("Toggle");
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Feature enabled");
    ImGui::SameLine();
    feature_toggle.update();
    if (feature_toggle.changed())
    {
        // edge visible via status line below
    }
    ImGui::Text("checked=%s  changed_last_frame=%s",
        feature_toggle.checked() ? "true" : "false",
        feature_toggle.changed() ? "true" : "false");
    ImGui::Separator();

    ImGui::TextUnformatted("list_item (reusable)");
    lone_row.set_selected(lone_selected);
    lone_row.update();
    if (lone_row.clicked())
        lone_selected = !lone_selected;
    ImGui::Separator();

    ImGui::TextUnformatted("list_view (single-select + scroll)");
    skins.update();
    ImGui::Text("selected_index=%d  selection_changed=%s",
        skins.selected_index(),
        skins.selection_changed() ? "true" : "false");

    ImGui::End();
}

int main(int, char**)
{
    tw::ui::theme::apply_dark();

    ImGui_ImplWin32_EnableDpiAwareness();
    const float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(
        ::MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));

    WNDCLASSEXW wc = {
        sizeof(wc),
        CS_OWNDC,
        WndProc,
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

    if (!CreateDeviceWGL(hwnd, &g_MainWindow))
    {
        CleanupDeviceWGL(hwnd, &g_MainWindow);
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        std::fprintf(stderr, "smoke_test: failed to create OpenGL context\n");
        return 1;
    }
    wglMakeCurrent(g_MainWindow.hDC, g_hRC);
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    ImGui_ImplWin32_InitForOpenGL(hwnd);
    ImGui_ImplOpenGL3_Init();

    const ImVec4 clear_color = tw::ui::theme::app_background;

    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;
        if (::IsIconic(hwnd))
        {
            ::Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        draw_widget_gallery();

        ImGui::Render();
        glViewport(0, 0, g_Width, g_Height);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ::SwapBuffers(g_MainWindow.hDC);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceWGL(hwnd, &g_MainWindow);
    wglDeleteContext(g_hRC);
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

bool CreateDeviceWGL(HWND hWnd, WGL_WindowData* data)
{
    HDC hDc = ::GetDC(hWnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    const int pf = ::ChoosePixelFormat(hDc, &pfd);
    if (pf == 0)
        return false;
    if (::SetPixelFormat(hDc, pf, &pfd) == FALSE)
        return false;
    ::ReleaseDC(hWnd, hDc);

    data->hDC = ::GetDC(hWnd);
    if (!g_hRC)
        g_hRC = wglCreateContext(data->hDC);
    return wglMakeCurrent(data->hDC, g_hRC) == TRUE;
}

void CleanupDeviceWGL(HWND hWnd, WGL_WindowData* data)
{
    wglMakeCurrent(nullptr, nullptr);
    ::ReleaseDC(hWnd, data->hDC);
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            g_Width = LOWORD(lParam);
            g_Height = HIWORD(lParam);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
