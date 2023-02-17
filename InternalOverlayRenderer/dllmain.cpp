#include "dllmain.h"

endScene pEndScene;
reset pReset;

LPDIRECT3DDEVICE9 ActualD3DDevice = nullptr;
LPD3DXFONT font = nullptr;
HINSTANCE DllHandle = nullptr;
PPVOID D3DVTable = nullptr;
D3DDEVICE_CREATION_PARAMETERS ViewportParams;
HWND HostApplicationHandle;
HWND GameHandle;
WNDPROC g_WndProc_o;

bool OverlayVisible = true;
bool ImguiToolboxVisible = false;
bool ImguiInitialized = false;


#pragma region Overlay Rectange Parameters

int ovlRectLX = 50;
int ovlRectLY = 500; // Default Audiosurf window geometry - 800x600. So we place info overlay at 50px offset by OX and and window height - 100px by OY axis

int ovlRectWidth = 450; //450x50 should be enough to display all info that we need
int ovlRectHeight = 50;

int padding = 2;

#pragma endregion

#pragma region ImGui UI globals

int ListboxSelect(0);

#pragma endregion

HRESULT __stdcall DetourAttachHook(PVOID* ppPointer, PVOID pDetour)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    auto result = DetourAttach(ppPointer, pDetour);
    DetourTransactionCommit();
    return result;
}

HRESULT __stdcall DetourDetachHook(PVOID* ppPointer, PVOID pDetour)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    auto result = DetourDetach(ppPointer, pDetour);
    DetourTransactionCommit();
    return result;
}

HRESULT __stdcall HookedReset(LPDIRECT3DDEVICE9 pDevice, PD3DPRESENT_PARAMETERS presentParameters)
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT result = pReset(pDevice, presentParameters);
    ImGui_ImplDX9_CreateDeviceObjects();

    pDevice->GetCreationParameters(&ViewportParams);

    RECT rect;
    GetWindowRect(ViewportParams.hFocusWindow, &rect);

    ovlRectLY = rect.bottom - 100; // Correct info overlay positioning after D3D Viewport Reset

    ImGui::GetIO().MouseDrawCursor = true;

    ActualD3DDevice = pDevice;
    D3DX_CREATE_DEFAULT_OVERLAY_FONT(pDevice, font);
    ImguiInitialized = false;

    return result;
}

HRESULT __stdcall HookedEndScene(LPDIRECT3DDEVICE9 pDevice)
{
    if (ActualD3DDevice == nullptr)
        ActualD3DDevice = pDevice;
    
    if (!OverlayVisible)
        return pEndScene(pDevice);

    if (!ImguiInitialized)
    {
        D3DDEVICE_CREATION_PARAMETERS params;
        pDevice->GetCreationParameters(&params);

        ImGui::CreateContext();
        ImGui_ImplWin32_Init(params.hFocusWindow);
        ImGui_ImplDX9_Init(pDevice);

        ImguiInitialized = true;
    }

    auto rectx1 = ovlRectLX, rectx2 = ovlRectLX + ovlRectWidth, recty1 = ovlRectLY, recty2 = ovlRectLY + ovlRectHeight;

    if (!font)
        D3DX_CREATE_DEFAULT_OVERLAY_FONT(pDevice, font);
    
    RECT textRectangle;

    SetRect(&textRectangle, rectx1 + padding, recty1 + padding, rectx2 - padding, recty2 - padding);

    font->DrawTextA(NULL, "Audiosurf Tweaker overlay v0.1\nPress Shift+Esc to Exit overlay\nInsert to change visibility\nShift+End toggle menu", 
        -1, &textRectangle, DT_NOCLIP | DT_LEFT, D3DCOLOR_ARGB(255,153,255,153));
    
    if (ImguiToolboxVisible)
    {
        DrawMenu();
    }
    
    return pEndScene(pDevice);
}

void __forceinline DrawMenu()
{
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Audiosurf Tweaker In-game menu", NULL, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Spacing();
    ImGui::Text("Viable Tweaker' texture packages:\n");
    ImGui::Spacing();

    //debug things u know
    const char* items[] = {"Nano", "Fractal", "Point Disarray"};
    ImGui::ListBox("", &ListboxSelect, items, (int)ARRSIZE(items), -1);

    if (ImGui::Button("Install Now"))
    {
        std::cout << "Call Install-package command\n";
    }

    ImGui::Spacing();
    ImGui::Text("Shift+End to toggle this menu");

    ImGui::End();

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

LRESULT __stdcall SendCommandToHostApplication(LPSTR commandText)
{
    if (HostApplicationHandle == nullptr)
        return 0;

    COPYDATASTRUCT cds{};
    cds.dwData = 0;
    cds.cbData = sizeof(char) * (strlen(commandText) + 1);
    cds.lpData = commandText;

    auto msgSendResult = SendMessage(HostApplicationHandle, WM_COPYDATA, (WPARAM)GetForegroundWindow(), (LPARAM)(LPVOID)&cds);
    return msgSendResult;
}

void InitD3D9()
{
    std::cout << "Hooking D3D EndScene...\n";

    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION); 

    if (!pD3D)
        return;
    std::cout << "Direct3D initialized\n";

    D3DPRESENT_PARAMETERS d3dparams = { 0 };

    d3dparams.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dparams.hDeviceWindow = GetForegroundWindow();
    d3dparams.Windowed = true;

    LPDIRECT3DDEVICE9 pDevice = nullptr;

    HRESULT result = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dparams.hDeviceWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dparams, &pDevice);

    if (FAILED(result) || !pDevice)
    {
        std::cout << "Error during creating D3DDevice. Exiting process\n";
        pD3D->Release();
        return;
    }

    PPVOID vTable = *reinterpret_cast<void***>(pDevice);

    D3DVTable = vTable;

    std::cout << "Hooking Direct3D VTable function...\n";

    pEndScene = (endScene)vTable[42];
    pReset = (reset)vTable[16];
    DetourAttachHook(&(PVOID&)pReset, (PVOID)HookedReset);
    DetourAttachHook(&(PVOID&)pEndScene, (PVOID)HookedEndScene);

    std::cout << "Original EndScene at " << vTable[42] << " detoured\n";
    std::cout << "Original Reset at " << vTable[16] << " detoured\n";
    pDevice->Release();
    pD3D->Release();

    GameHandle = FindWindowA(NULL, "Audiosurf");
    g_WndProc_o = (WNDPROC)SetWindowLongA(GameHandle, GWL_WNDPROC, (LRESULT)WndProc);
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    return CallWindowProc(g_WndProc_o, hWnd, msg, wParam, lParam);
}

DWORD __stdcall EjectThread(LPVOID lpParam) 
{
    Sleep(100);
    FreeLibraryAndExitThread(DllHandle, 0);
    return 0;
}

DWORD WINAPI BuildOverlay(HINSTANCE hModule)
{
    AllocConsole();
    FILE* fp;

    freopen_s(&fp, "CONOUT$", "w", stdout);

    if (fp == nullptr)
    {
        std::cout << "Error allocating console\n";
        CreateThread(0, 0, EjectThread, 0, 0, 0);
    }

    std::cout << "We are in " << GetCurrentProcessId() << "\nOverlay initialization...\n";

    InitD3D9();

    while (true)
    {
        Sleep(150);
        if (GetAsyncKeyState(VK_SHIFT) && GetAsyncKeyState(VK_ESCAPE)) 
        {
            DetourDetachHook(&(PVOID&)pEndScene, (PVOID)HookedEndScene);
            DetourDetachHook(&(PVOID&)pReset, (PVOID)HookedReset);
            SetWindowLongA(GameHandle, GWL_WNDPROC, (LONG_PTR)g_WndProc_o);
            fclose(fp);
            FreeConsole();
            CreateThread(0, 0, EjectThread, 0, 0, 0);
            break;
        }

        if (GetAsyncKeyState(VK_INSERT))
        {
            OverlayVisible = !OverlayVisible;
        }

        if (GetAsyncKeyState(VK_SHIFT) && GetAsyncKeyState(VK_END))
        {
            ImguiToolboxVisible = !ImguiToolboxVisible;
        }


    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DllHandle = hModule;
        DisableThreadLibraryCalls(DllHandle);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)BuildOverlay, NULL, 0, NULL);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

