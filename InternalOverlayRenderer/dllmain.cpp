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
WNDPROC WndProcOriginal;


#pragma region Overlay Rectange Parameters

int ovlRectLX = 50;
int ovlRectLY = 500; // Default Audiosurf window geometry - 800x600. So we place info overlay at 50px offset by OX and and window height - 100px by OY axis

int ovlRectWidth = 450; //450x50 should be enough to display all info that we need
int ovlRectHeight = 50;

int padding = 2;

#pragma endregion

#pragma region UI globals

bool OverlayVisible = true;
bool ImguiToolboxVisible = false;
bool ImguiInitialized = false;
int ListboxSelect(0);

std::vector<std::string> ActualSkinsList{};
std::string DisplayInfo{};

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
    
    if (DisplayInfo.empty())
        DisplayInfo.append("Tweaker overlay v0.1...\nWaiting for connection with host application...");

    font->DrawTextA(NULL, DisplayInfo.c_str(),
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

    std::vector<const char*> localSkinList{};

    for (auto& item : ActualSkinsList)
    {
        localSkinList.push_back(item.c_str());
    }

    ImGui::ListBox("", &ListboxSelect, localSkinList.data(), localSkinList.size(), -1);

    if (ImGui::Button("Install Now"))
    {
        if (ActualSkinsList.size() == 0)
        {
            std::cout << "Debug## Skins list empty, can not send command\n";
            return;
        }

        SendCommandToHostApplication(const_cast<char*>(
            ( std::string(IntallPackageCommandHeader) 
            + std::string(" ") 
            + std::string(ActualSkinsList[ListboxSelect]))
            .c_str()));// tw-Install-package <skin_name>
    }

    ImGui::Spacing();
    ImGui::Text("Press insert to toggle this menu");

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

    auto msgSendResult = SendMessage(HostApplicationHandle, WM_COPYDATA, NULL, (LPARAM)(LPVOID)&cds);
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
    WndProcOriginal = (WNDPROC)SetWindowLongA(GameHandle, GWL_WNDPROC, (LRESULT)WndProc);
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto imgui_wndproc_result = ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);

    if (msg == WM_COPYDATA) {
        auto cds = (COPYDATASTRUCT*)lParam;
        if (cds->cbData > 0)
        {
            auto msg = std::string((LPCSTR)cds->lpData);

            if (msg.find("ascommand") != std::string::npos)
                std::cout << "generic audiosurf command handled\n";

            else if (msg.find("tw-update-listener") != std::string::npos) // catch ovl-update-listener command from host application, that has a signature of "tw-update-listener AsMsgHandler_<unique_symbol_sequence>
            {
                auto HandlerUniqueID = msg.substr(std::string("tw-update-listener").length() + 1);
                std::cout << "tw-update-listener handled with arguments " << HandlerUniqueID << "\n";

                HWND hostWndProcHandler = FindWindowA(NULL, HandlerUniqueID.c_str());
                if (hostWndProcHandler)
                    HostApplicationHandle = hostWndProcHandler;
            }

            else if (msg.find("tw-update-skin-list") != std::string::npos)
            {
                auto list = msg.substr(std::string("tw-update-skin-list").length());

                std::cout << "tw-update-skin-list handled with arguments " << list << "\n";

                LTrim(list);
                ActualSkinsList.clear();

                for (auto& item : Split(list, "; "))
                {
                    std::cout << "appending item " << item << "\n";
                    ActualSkinsList.push_back(item);
                }
            }

            else if (msg.find("tw-update-ovl-info") != std::string::npos)
            {
                auto newInfo = msg.substr(std::string("tw-update-ovl-info").length());
                DisplayInfo.clear();
                DisplayInfo.append(newInfo);
            }

            else if (msg.find("tw-pulse") != std::string::npos)
            {
                if (HostApplicationHandle != nullptr)
                    SendCommandToHostApplication(const_cast<char*>("tw-responce ok"));
            }

            else if (msg.find("tw-append-ovl-info") != std::string::npos)
            {
                auto infoToAppend = msg.substr(std::string("tw-append-ovl-info").length());
                DisplayInfo.append(std::string("\n") + infoToAppend);
            }
        }
    }

    if (imgui_wndproc_result)
        return true;

    return CallWindowProc(WndProcOriginal, hWnd, msg, wParam, lParam);
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
            SetWindowLongA(GameHandle, GWL_WNDPROC, (LONG_PTR)WndProcOriginal);
            fclose(fp);
            FreeConsole();
            CreateThread(0, 0, EjectThread, 0, 0, 0);
            break;
        }

        if (GetAsyncKeyState(VK_INSERT))
        {
            //OverlayVisible = !OverlayVisible;
            ImguiToolboxVisible = !ImguiToolboxVisible;
        }

        /*if (GetAsyncKeyState(VK_SHIFT) && GetAsyncKeyState(VK_END))
        {
        }*/
    }
    return 0;
}

#pragma region string things

inline void LTrim(std::string& str)
{
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
}

inline void RTrim(std::string& str)
{
    str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), str.end());
}

inline void Trim(std::string& str)
{
    RTrim(str);
    LTrim(str);
}

inline std::vector<std::string> Split(std::string src, std::string delimeter)
{
    std::vector<std::string> outputContainer{};

    std::size_t pos = 0;

    while ((pos = src.find(delimeter)) != std::string::npos)
    {
        outputContainer.push_back(src.substr(0, pos));
        src.erase(0, pos + delimeter.length());
    }

    return outputContainer;
}

#pragma endregion

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

