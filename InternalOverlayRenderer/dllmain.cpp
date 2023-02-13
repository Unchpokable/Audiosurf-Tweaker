// dllmain.cpp : Определяет точку входа для приложения DLL.
#include "dllmain.h"

endScene pEndScene;
reset pReset;

LPDIRECT3DDEVICE9 ActualD3DDevice = nullptr;
LPD3DXFONT font = nullptr;
HINSTANCE DllHandle = nullptr;
PPVOID D3DVTable = nullptr;
D3DDEVICE_CREATION_PARAMETERS ViewportParams;

bool OverlayVisible = true;

#pragma region Overlay Rectange Parameters

int ovlRectLX = 50;
int ovlRectLY = 50;

int ovlRectWidth = 450;
int ovlRectHeight = 50;

int padding = 2;

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

HRESULT __stdcall HookedReset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* presentParameters)
{
    pDevice->GetCreationParameters(&ViewportParams);

    RECT rect;
    GetWindowRect(ViewportParams.hFocusWindow, &rect);

    ovlRectLY = rect.bottom - 100;

    ActualD3DDevice = pDevice;
    D3DX_CREATE_DEFAULT_OVERLAY_FONT(pDevice, font);
    return pReset(pDevice, presentParameters);
}

HRESULT __stdcall  HookedEndScene(LPDIRECT3DDEVICE9 pDevice)
{
    if (ActualD3DDevice == nullptr)
        ActualD3DDevice = pDevice;

    if (!OverlayVisible)
        return pEndScene(pDevice);

    auto rectx1 = ovlRectLX, rectx2 = ovlRectLX + ovlRectWidth, recty1 = ovlRectLY, recty2 = ovlRectLY + ovlRectHeight;


    D3DRECT rectangle = { rectx1, recty1, rectx2, recty2 };
    //pDevice->Clear(1, &rectangle, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 0.0f, 0); // this draws a rectangle

    if (!font)
        D3DX_CREATE_DEFAULT_OVERLAY_FONT(pDevice, font);
    
    RECT textRectangle;

    SetRect(&textRectangle, rectx1 + padding, recty1 + padding, rectx2 - padding, recty2 - padding);

    font->DrawTextA(NULL, "Audiosurf Tweaker overlay v0.1\nPress Ctrl+End to Exit overlay\nInsert to change visibility", -1, &textRectangle, DT_NOCLIP | DT_LEFT, D3DCOLOR_ARGB(255,153,255,153));
    return pEndScene(pDevice);
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
        if (GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_END)) 
        {
            DetourDetachHook(&(PVOID&)pEndScene, (PVOID)HookedEndScene);
            DetourDetachHook(&(PVOID&)pReset, (PVOID)HookedReset);
            fclose(fp);
            FreeConsole();
            CreateThread(0, 0, EjectThread, 0, 0, 0);
            break;
        }

        if (GetAsyncKeyState(VK_INSERT))
        {
            OverlayVisible = !OverlayVisible;
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

