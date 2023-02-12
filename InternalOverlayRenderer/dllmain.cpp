// dllmain.cpp : Определяет точку входа для приложения DLL.
#include <d3d9.h>
#include <d3dx9.h>
#include <iostream>
#include "detours.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "detours.lib")

#define D3DX_CREATE_DEFAULT_OVERLAY_FONT(pDevice, font) D3DXCreateFont(pDevice, 15, 0, FW_BOLD, 1, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma", &font);

//#pragma region WndProc
//#include <Windows.h>
//
//constexpr LPCSTR WndProcHandlerWindowTitle = "TWD3DOVRLTITLE_TweakerOverlay";
//
//LPSTR overlayMessage;
//
//LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) 
//{
//    LPSTR cdsText;
//
//    switch (msg) 
//    {
//    case WM_COPYDATA:
//        LPSTR cdsText = (LPSTR)((COPYDATASTRUCT*)lParam)->lpData; //Command text sended by host application
//
//    default:
//        return DefWindowProc(hwnd, msg, wParam, lParam);
//    }
//}
//
//#pragma endregion



typedef void** PPVOID;
typedef HRESULT(__stdcall* endScene)(LPDIRECT3DDEVICE9);
typedef HRESULT(__stdcall* reset)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);

endScene pEndScene;
reset pReset;

LPD3DXFONT font;
HINSTANCE DllHandle;
PPVOID D3DVTable;

bool OverlayVisible = true;

HRESULT __forceinline DetourAttachHook(PVOID* ppPointer, PVOID pDetour)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    auto result = DetourAttach(ppPointer, pDetour);
    DetourTransactionCommit();
    return result;
}

HRESULT __forceinline DetourDetachHook(PVOID* ppPointer, PVOID pDetour)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    auto result = DetourDetach(ppPointer, pDetour);
    DetourTransactionCommit();
    return result;
}

HRESULT __stdcall hookedReset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* presentParameters)
{
    std::cout << "hooked Reset called";
    D3DX_CREATE_DEFAULT_OVERLAY_FONT(pDevice, font);
    return pReset(pDevice, presentParameters);
}

HRESULT __stdcall hookedEndScene(LPDIRECT3DDEVICE9 pDevice)
{
    if (!OverlayVisible)
        return pEndScene(pDevice);

    auto padding = 2;
    auto rectx1 = 50, rectx2 = 500, recty1 = 50, recty2 = 100;


    D3DRECT rectangle = { rectx1, recty1, rectx2, recty2 };
    //pDevice->Clear(1, &rectangle, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 0.0f, 0); // this draws a rectangle

    if (!font)
        D3DX_CREATE_DEFAULT_OVERLAY_FONT(pDevice, font);
    
    RECT textRectangle;

    SetRect(&textRectangle, rectx1 + padding, recty1 + padding, rectx2 - padding, recty2 - padding);

    font->DrawTextA(NULL, "Audiosurf Tweaker overlay v0.1\nPress Ctrl+End to Exit overlay\nInsert to change visibility", -1, &textRectangle, DT_NOCLIP | DT_LEFT, D3DCOLOR_ARGB(255,153,255,153));
    return pEndScene(pDevice);
}

void HookEndScene()
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

    IDirect3DDevice9* pDevice = nullptr;

    HRESULT result = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dparams.hDeviceWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dparams, &pDevice);

    if (FAILED(result) || !pDevice)
    {
        std::cout << "Error during creating D3DDevice. Exiting process";
        pD3D->Release();
        return;
    }

    void** vTable = *reinterpret_cast<void***>(pDevice);

    D3DVTable = vTable;

    std::cout << "Hooking EndScene function...\n";

    pEndScene = (endScene)vTable[42];
    DetourAttachHook(&(PVOID&)pEndScene, (PVOID)hookedEndScene);

    std::cout << "Original EndScene at " << &(PVOID&)vTable[42] << " detoured";

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

    std::cout << "We are in " << GetCurrentProcessId() << "\nOverlay initialization...\n";

    HookEndScene();

    while (true)
    {
        Sleep(150);
        if (GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_END)) 
        {
            DetourDetachHook(&(PVOID&)pEndScene, (PVOID)hookedEndScene);
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
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)pEndScene, (PVOID)hookedEndScene);
        DetourTransactionCommit();
        break;
    }
    return TRUE;
}

