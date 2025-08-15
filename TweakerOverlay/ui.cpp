#include "pch.hpp"

#include "ui.hpp"

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

void ui::render_ui(IDirect3DDevice9* device)
{
}

void ui::drop_imgui()
{
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

bool ui::setup_imgui(HWND hwnd, IDirect3DDevice9 *device)
{
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

    if(!ImGui_ImplWin32_Init(hwnd)) {
        return false;
    }

    if(!ImGui_ImplDX9_Init(device)) {
        ImGui_ImplWin32_Shutdown();
        return false;
    }

    return true;
}

bool ui::reinit_imgui(HWND hwnd, IDirect3DDevice9* device)
{
    drop_imgui();
    return setup_imgui(hwnd, device);
}

void ui::backend_invalidate_objects()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
}

bool ui::backend_create_objects()
{
    return ImGui_ImplDX9_CreateDeviceObjects();
}
