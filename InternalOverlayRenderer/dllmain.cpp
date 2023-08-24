#include "dllmain.h"

endScene pEndScene;
reset pReset;

PD3DPRESENT_PARAMETERS D3DPresentParameters = nullptr;
LPDIRECT3DDEVICE9 ActualD3DDevice = nullptr;
LPD3DXFONT font = nullptr;
HINSTANCE DllHandle = nullptr;
D3DDEVICE_CREATION_PARAMETERS ViewportParams;
HWND HostApplicationHandle;
HWND GameHandle;
WNDPROC WndProcOriginal;
FILE* PConsoleOutFile; // Bad to store raw pointers in global memory but i actually dont give a fuck


constexpr LPCSTR IntallPackageCommandHeader = "tw-Install-package";

bool DXCritical_SwapchianNullPtr = false;
bool OverlayInitialized;

int NativeScreenWidth = 0;
int NativeScreenHeight = 0;

#pragma region Overlay Rectange Parameters

int ovlRectLX = 50;
int ovlRectLY = 500; // Default Audiosurf window geometry - 800x600. So we place info overlay at 50px offset by OX and and window height - 100px by OY axis

int ovlRectWidth = 450; //450x70 should be enough to display all info that we need
int ovlRectHeight = 70;

int padding = 2;

PINT OvlXOffset;
PINT OvlYOffset;

#pragma endregion

#pragma region UI globals

bool OverlayVisible = true;
bool GameRunsFullscreen = false;
bool ImguiToolboxVisible = false;
bool ImguiInitialized = false;
int ListboxSelect(0);

std::vector<std::string> ActualSkinsList{};
std::string DisplayInfo{};

PArgb_t FontColor = nullptr;
PINT FontSize = nullptr;
PFLOAT MenuFontSize = nullptr;
LPCSTR FontFamily = "Tahoma";

#pragma endregion

#pragma region tweaks flags

// freeride configs is scrap so no need no turn them inside in-game menu

namespace Assoc
{
    using namespace std;

    auto tweak_InvisibleRoad = new const bool{};
    auto tweak_HiddenSongTitle = new const bool{};
    auto tweak_SidewinderCamera = new const bool{};
    auto tweak_BankingCamera = new const bool{};

    map<string, shared_ptr<bool>> TweaksFlags
    {
        {"InvisibleRoad", make_shared<bool>(tweak_InvisibleRoad) },
        {"HiddenSongTitle", make_shared<bool>(tweak_HiddenSongTitle) },
        {"BankingCamera", make_shared<bool>(tweak_BankingCamera) },
        {"SidewinderCamera", make_shared<bool>(tweak_SidewinderCamera) }
    };
}

#pragma endregion

int win32_excs::ExcFilter(unsigned int code, struct _EXCEPTION_POINTERS* ep)
{
    if (code == EXCEPTION_ACCESS_VIOLATION)
    {
        return EXCEPTION_EXECUTE_HANDLER;
    }
    return 0;
}

HRESULT __stdcall DetourAttachHook(PPVOID ppPointer, PVOID pDetour)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    auto result = DetourAttach(ppPointer, pDetour);
    DetourTransactionCommit();
    return result;
}

HRESULT __stdcall DetourDetachHook(PPVOID ppPointer, PVOID pDetour)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    auto result = DetourDetach(ppPointer, pDetour);
    DetourTransactionCommit();
    return result;
}

HRESULT __stdcall HookedReset(LPDIRECT3DDEVICE9 pDevice, PD3DPRESENT_PARAMETERS presentParameters)
{
    if (pDevice == nullptr || presentParameters == nullptr)
        return pReset(pDevice, presentParameters);

    pDevice->GetCreationParameters(&ViewportParams);
    
    RECT rect;
    GetWindowRect(ViewportParams.hFocusWindow, &rect);

    ovlRectLY = rect.bottom - 100; 

    if (presentParameters->Windowed == FALSE)
        ImGui::GetIO().MouseDrawCursor = true;

    auto font_ok = ConfigureFont(pDevice, &font, FontFamily, *FontSize);
        
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT result = pReset(pDevice, presentParameters);
    ImGui_ImplDX9_CreateDeviceObjects();
    D3DPresentParameters = presentParameters;
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

    auto rectx1 = ovlRectLX + *OvlXOffset, rectx2 = ovlRectLX + ovlRectWidth + *OvlXOffset, recty1 = ovlRectLY + *OvlYOffset, recty2 = ovlRectLY + ovlRectHeight + *OvlYOffset;


    RECT textRectangle;

    SetRect(&textRectangle, rectx1 + padding, recty1 + padding, rectx2 - padding, recty2 - padding);
    
    if (DisplayInfo.empty())
        DisplayInfo.append("Tweaker overlay v0.1...\nWaiting for connection with host application...");

    if (FontColor == nullptr)
        FontColor = new Argb_t{255, 153, 255, 153}; // Default shitty-green color

    if (!font)
        ConfigureFont(pDevice, &font, FontFamily, *FontSize);
    
    font->DrawText(NULL, DisplayInfo.c_str(),
        -1, &textRectangle, DT_NOCLIP | DT_LEFT, D3DCOLOR_ARGB(FontColor->alpha,FontColor->r,FontColor->g,FontColor->b));
    
    if (ImguiToolboxVisible)
    {
        DrawMenu(pDevice);
    }
    
    return pEndScene(pDevice);
}

inline void DrawMenu(LPDIRECT3DDEVICE9 pDevice)
{
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Audiosurf Tweaker In-game menu", NULL, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Skin Changer"))
    {
        ImGui::Text("Available Tweaker texture packages:\n");
        ImGui::Spacing();

        std::vector<const char*> localSkinList{};

        localSkinList.reserve(ActualSkinsList.size());
        for (auto& item : ActualSkinsList)
        {
            localSkinList.push_back(item.c_str());
        }

        ImGui::ListBox("", &ListboxSelect, localSkinList.data(), localSkinList.size(), -1);

        if (ImGui::Button("Install Now"))
        {
            if (ActualSkinsList.empty())
            {
                return;
            }
            std::stringstream cmdText{};
            cmdText << IntallPackageCommandHeader << " " << ActualSkinsList[ListboxSelect];
            SendCommandToHostApplication(const_cast<char*>(cmdText.str().c_str()));// tw-Install-package <skin_name>
        }

        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Tweaker"))
    {
        if (GameHandle == nullptr || !IsWindow(GameHandle))
        {
            ImGui::Text("Game handle is invalid. Tweaks unavailable");
        }

        else
        {
            std::stringstream rawCommand{};
            std::stringstream hostNotifyCommand{};

            hostNotifyCommand << "tw-Notify-Tweak-Changed ";
            bool shouldNotifyHost = false;
            rawCommand << "asconfig ";

            if (ImGui::Checkbox("Invisible Road", Assoc::TweaksFlags["InvisibleRoad"].get()))
            {
                auto flag = *Assoc::TweaksFlags["InvisibleRoad"].get();
                rawCommand << "roadvisible " << std::boolalpha << !flag;
                hostNotifyCommand << "InvisibleRoad " << std::boolalpha << flag;
                SendCopyDataMessage(GameHandle, rawCommand.str().c_str());
                shouldNotifyHost = true;
            }

            if (ImGui::Checkbox("Hidden song title", Assoc::TweaksFlags["HiddenSongTitle"].get()))
            {
                auto flag = *Assoc::TweaksFlags["HiddenSongTitle"].get();
                rawCommand << "showsongname " << std::boolalpha << !flag;
                hostNotifyCommand << "HiddenSong " << std::boolalpha << flag;
                SendCopyDataMessage(GameHandle, rawCommand.str().c_str());
                shouldNotifyHost = true;
            }

            if (ImGui::Checkbox("Sidewinder Camera", Assoc::TweaksFlags["SidewinderCamera"].get()))
            {
                auto flag = *Assoc::TweaksFlags["SidewinderCamera"].get();
                rawCommand << "sidewinder " << std::boolalpha << flag;
                hostNotifyCommand << "SidewinderCamera " << std::boolalpha << flag;
                SendCopyDataMessage(GameHandle, rawCommand.str().c_str());
                shouldNotifyHost = true;
            }

            if (ImGui::Checkbox("Banking camera", Assoc::TweaksFlags["BankingCamera"].get()))
            {
                auto flag = *Assoc::TweaksFlags["BankingCamera"].get();
                rawCommand << "usebankingcamera " << std::boolalpha << flag;
                hostNotifyCommand << "BankingCamera " << std::boolalpha << flag;
                SendCopyDataMessage(GameHandle, rawCommand.str().c_str());
                shouldNotifyHost = true;
            }

            if (shouldNotifyHost)
                SendCopyDataMessage(HostApplicationHandle, hostNotifyCommand.str().c_str());
        }
    }

    if (ImGui::CollapsingHeader("Info panel config"))
    {
        ImGui::Text("Info panel is a little text block at left bottom screen corner\nIts displays info about current installed skin and etc.");

        ImGui::SliderInt("Transp", &FontColor->alpha, 0, 255);
        ImGui::SliderInt("R", &FontColor->r, 0, 255);
        ImGui::SliderInt("G", &FontColor->g, 0, 255);
        ImGui::SliderInt("B", &FontColor->b, 0, 255);

        if (ImGui::SliderInt("Font Size", FontSize, 14, 72))
        {
            ConfigureFont(pDevice, &font, "Tahoma", *FontSize);
        }
        
        ImGui::Spacing();

        ImGui::Text("Info panel position correction");

        ImGui::SliderInt("X axis offset", OvlXOffset, -300, 300);
        ImGui::SliderInt("Y axis offset", OvlYOffset, -300, 300);

        if (ImGui::Button("Apply settings"))
        {
            std::stringstream ss{};
            //tw-Apply-configuration InfopanelFontColor 255 255 255 255; InfopanelFontSize 35; InfopanelXOffset 0; InfopanelYOffset 0
            ss << "tw-Apply-configuration" << " " << "InfopanelFontColor:"<< FontColor->alpha << " " << FontColor->r << " " << FontColor->g << " " << FontColor->b << "; "
                << "InfopanelFontSize:" << *FontSize << "; "
                << "InfopanelXOffset:" << *OvlXOffset << "; "
                << "InfopanelYOffset:" << *OvlYOffset;
            SendCommandToHostApplication(const_cast<char*>(ss.str().c_str()));
        }
    }

    ImGui::Text("Press insert to toggle this menu");

    ImGui::End();

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

LRESULT __stdcall SendCopyDataMessage(HWND hWindow, LPCSTR cdsLpData)
{
    return SendCopyDataMessage(hWindow, const_cast<LPSTR>(cdsLpData));
}

LRESULT __stdcall SendCommandToHostApplication(LPSTR commandText)
{
    return SendCopyDataMessage(HostApplicationHandle, commandText);
}

LRESULT __stdcall SendCopyDataMessage(HWND hWindow, LPSTR cdsLpData)
{
    if (hWindow == nullptr)
        return -1;

    COPYDATASTRUCT cds{};
    cds.dwData = 0;
    cds.cbData = sizeof(char) * (strlen(cdsLpData) + 1);
    cds.lpData = cdsLpData;

    return SendMessage(hWindow, WM_COPYDATA, NULL, (LPARAM)(LPVOID)&cds);
}

HRESULT Init()
{
    HRESULT result = InitDirect3D();

    if (FAILED(result))
    {
        std::cout << "Error during creating D3DDevice. Exiting process\n";
        return E_FAIL;
    }

    GameHandle = FindWindow(NULL, "Audiosurf");
    WndProcOriginal = (WNDPROC)SetWindowLong(GameHandle, GWL_WNDPROC, (LRESULT)WndProc);

    RECT desktop;

    auto hDesktop = GetDesktopWindow();

    GetWindowRect(hDesktop, &desktop);

    NativeScreenHeight = desktop.bottom;
    NativeScreenWidth = desktop.right;

    return 0;
}

HRESULT InitDirect3D()
{
    std::cout << "Hooking Direct3D application...\n";

    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);

    if (!pD3D)
        return E_FAIL;
    std::cout << "Direct3D initialized\n";

    LPDIRECT3DDEVICE9 pDevice = nullptr;

    WNDCLASSEX windowClass;
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = DefWindowProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = GetModuleHandle(NULL);
    windowClass.hIcon = NULL;
    windowClass.hCursor = NULL;
    windowClass.hbrBackground = NULL;
    windowClass.lpszMenuName = NULL;
    windowClass.lpszClassName = "TwOvl";
    windowClass.hIconSm = NULL;

    std::cout << "Registering dummy window class\n";

    ::RegisterClassEx(&windowClass);

    HWND window = ::CreateWindow(windowClass.lpszClassName, "Tweaker Overlay window dummy", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, windowClass.hInstance, NULL);

    std::cout << "Dummy window created\n";

    D3DPRESENT_PARAMETERS params;
    params.BackBufferWidth = 0;
    params.BackBufferHeight = 0;
    params.BackBufferFormat = D3DFMT_UNKNOWN;
    params.BackBufferCount = 0;
    params.MultiSampleType = D3DMULTISAMPLE_NONE;
    params.MultiSampleQuality = NULL;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow = window;
    params.Windowed = 1;
    params.EnableAutoDepthStencil = 0;
    params.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
    params.Flags = NULL;
    params.FullScreen_RefreshRateInHz = 0;
    params.PresentationInterval = 0;

    auto result = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT, &params, &pDevice);

    if (FAILED(result) || !pDevice)
    {
        std::cout << "Device bound to dummy window creation failed\n";

        ::DestroyWindow(window);
        ::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
        return E_FAIL;
    }
    
    std::cout << "Device bound to dummy window created successfully\n";

    PPVOID vTable = *reinterpret_cast<void***>(pDevice);

    pEndScene = (endScene)vTable[42];
    pReset = (reset)vTable[16];
    DetourAttachHook(&(PVOID&)pReset, (PVOID)HookedReset);
    DetourAttachHook(&(PVOID&)pEndScene, (PVOID)HookedEndScene);
    std::cout << "Original EndScene at " << vTable[42] << " detoured\n";
    std::cout << "Original Reset at " << vTable[16] << " detoured\n";
    ::DestroyWindow(window);
    ::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
    
    return 0;
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto imgui_wndproc_result = ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);

    try 
    {
        if (msg == WM_COPYDATA)
        {
            auto cds = (COPYDATASTRUCT*)lParam;
            HandleCopyDataMessage(cds);
        }

        if (msg == WM_SIZE)
        {
            HandleWMSize(wParam, lParam);
        }
    }

    catch (std::exception ex)
    {
        std::cout << "Error while WndProc handling " << ex.what() << "\n";
    }

    if (imgui_wndproc_result)
        return true;

    return CallWindowProc(WndProcOriginal, hWnd, msg, wParam, lParam);
}

inline void HandleCopyDataMessage(PCOPYDATASTRUCT cds) 
{
    if (cds->cbData <= 0)
        return;

    auto msg = std::string((LPCSTR)cds->lpData);

    if (msg.find("tw-update-listener") != std::string::npos) // catch ovl-update-listener command from host application, that has a signature of "tw-update-listener AsMsgHandler_<unique_symbol_sequence>
    {
        auto HandlerUniqueID = msg.substr(std::string("tw-update-listener").length() + 1);
        std::cout << "tw-update-listener handled with arguments " << HandlerUniqueID << "\n";

        HWND hostWndProcHandler = FindWindow(NULL, HandlerUniqueID.c_str());
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

    else if (msg.find("tw-config") != std::string::npos)
    {
        auto configs = Split(msg.substr(std::string("tw-config").length()), std::string("; "));

        for (auto& config : configs)
        {
            ProcessConfigurationCommand(config);
        }
    }
}

inline void HandleWMSize(WPARAM wParam, LPARAM lParam)
{
    UINT width = LOWORD(lParam);
    UINT height = HIWORD(lParam);

    ovlRectLY = height - 100;
}

HRESULT ConfigureFont(LPDIRECT3DDEVICE9 pDevice, LPD3DXFONT* font, LPCSTR fontFamily, int size)
{
    return D3DXCreateFont(pDevice, size, 0, FW_REGULAR, 1, 0, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontFamily, font);
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

    PConsoleOutFile = fp;

    if (fp == nullptr)
    {
        std::cout << "Error allocating console\n";
        CreateThread(0, 0, EjectThread, 0, 0, 0);
    }

    std::cout << "Game PID: " << GetCurrentProcessId() << "\nOverlay initialization...\n";

    FontColor = new Argb{255, 153, 255, 153};
    FontSize = new int;
    *FontSize = 22;

    MenuFontSize = new float;
    *MenuFontSize = 16;

    OvlXOffset = new int;
    OvlYOffset = new int;

    *OvlXOffset = 0;
    *OvlYOffset = 0;

    for (auto const& [key, value] : Assoc::TweaksFlags) 
    {
        *Assoc::TweaksFlags[key] = false;
    }

    auto initResult = Init();

    if (FAILED(initResult))
    {
        std::cout << "Overlay initialization failed with error code " << initResult << "\n Exiting process after 5 second...\n" << "Please, restart audiosurf if you see this message for the first time\n"
            << "If you see this message every time, disable game overlay in Host application settings tab, then restart game and Audiosurf Tweaker. Sorry if this thing doesn't work for you :( ";
        Sleep(5000);
        EjectOverlayProcess(fp);
    }
    
    while (true)
    {
        Sleep(150);
        if (GetAsyncKeyState(VK_SHIFT) && GetAsyncKeyState(VK_ESCAPE)) 
        {
            EjectOverlayProcess(fp);
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

void EjectOverlayProcess(FILE* fp)
{
    DetourDetachHook(&(PVOID&)pEndScene, (PVOID)HookedEndScene);
    DetourDetachHook(&(PVOID&)pReset, (PVOID)HookedReset);
    SetWindowLong(GameHandle, GWL_WNDPROC, (LONG_PTR)WndProcOriginal);

    if (fp != nullptr)
        fclose(fp);
    
    FreeConsole();

    delete FontColor;
    delete FontSize;
    delete FontFamily;
    delete MenuFontSize;
    delete PConsoleOutFile;

    CreateThread(0, 0, EjectThread, 0, 0, 0);
}

#pragma region string things

inline void ProcessConfigurationCommand(std::string& cfgStr)
{
    try
    {
        if (cfgStr.find("font-color") != std::string::npos)
        {
            auto values = Split(cfgStr.substr(std::string("font-color").length()), std::string(" "));
            if (values.size() != 4) // ARGB - 0-255 0-255 0-255 0-255 
                return;
            FontColor->alpha = std::stoi(values[0]);
            FontColor->r = std::stoi(values[1]);
            FontColor->g = std::stoi(values[2]);
            FontColor->b = std::stoi(values[3]);
        }

        else if (cfgStr.find("font-size") != std::string::npos)
        {
            auto value = std::stoi(cfgStr.substr(std::string("font-size").length()));
            *FontSize = value;
            ConfigureFont(ActualD3DDevice, &font, "Tahoma", *FontSize);
        }

        // FIXME: Code duplicate. Do something with it later. Or not?
        else if (cfgStr.find("infopanel-xoffset") != std::string::npos)
        {
            auto value = std::stoi(cfgStr.substr(std::string("infopanel-xoffset").length()));
            *OvlXOffset = value;
        }

        else if (cfgStr.find("infopanel-yoffset") != std::string::npos)
        {
            auto value = std::stoi(cfgStr.substr(std::string("infopanel-yoffset").length()));
            *OvlYOffset = value;
        }

        else if (cfgStr.find("tweak-active") != std::string::npos) // tweak-active invisibleroad true
        {
            auto value = cfgStr.substr(std::string("tweak-active").length());
            UpdateTweaksState(value);
        }
    }
    catch (const std::exception ex)
    {
        std::cout << "Exception during settings apply: " << ex.what() << "\n";
    }
    catch (...)
    {
        std::cout << "Unknown error during settings apply\n";
    }
}

inline void UpdateTweaksState(std::string parameter)
{
    auto values = Split(parameter, " ");
    if (values.size() == 2)// we need 2 values - property and its new value, like "InvisibleRoad true"
    {
        try
        {
            *Assoc::TweaksFlags[values[0]] = (values[1] == std::string("true")); // So, if we want to set any value here to false, we can say that new value of, InvisibleRoad for ex, is "puncake" which is not equals to "true". funny

        }
        catch (...) 
        {
            std::cout << "Unable to turn tweak " << values[1] << "\n" << " cause wrong pointer value at " << Assoc::TweaksFlags[values[0]].get() << "\n";
        }
    }
}

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

std::vector<std::string> Split(std::string src, std::string delimeter)
{
    std::vector<std::string> outputContainer{};

    std::size_t pos = 0;
    Trim(src);

    while ((pos = src.find(delimeter)) != std::string::npos)
    {
        outputContainer.push_back(src.substr(0, pos));
        src.erase(0, pos + delimeter.length());
    }

    outputContainer.push_back(src);
    return outputContainer;
}

#pragma endregion

BOOL APIENTRY DllMain(HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved)
{
    HMODULE existingDll;
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        existingDll = GetModuleHandle("InternalOverlayRenderer.dll");
        if (existingDll != NULL && existingDll != hModule) // No duplicate 
        {
             FreeLibraryAndExitThread(hModule, 0);
             break;
        }
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

