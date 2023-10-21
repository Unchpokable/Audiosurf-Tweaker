#ifndef DLLMAIN_H
#define DLLMAIN_H

#include <d3d9.h>
#include <d3dx9.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <functional>
#include <locale>
#include <vector>
#include <mutex>
#include <string>
#include <sstream>
#include <excpt.h>
#include <map>

#include "detours.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx9.h"
#include "imgui/backends/imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "detours.lib")

#define D3DX_CREATE_DEFAULT_OVERLAY_FONT(pDevice, font) D3DXCreateFont(pDevice, 22, 0, FW_REGULAR, 1, 0, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma", &font)

#define ARRSIZE(arr) (sizeof(arr)/sizeof((arr)[0]))

#define THROW_IF_NULL(ptr, param_name)\
	if ((ptr) == nullptr)\
		throw std::exception("Required parameter was assigned with nullptr");

typedef void** PPVOID;
typedef int* PINT;
typedef float* PFLOAT;
typedef D3DPRESENT_PARAMETERS* PD3DPRESENT_PARAMETERS;
typedef COPYDATASTRUCT* PCOPYDATASTRUCT;

typedef HRESULT(__stdcall* DxEndScene)(LPDIRECT3DDEVICE9);
typedef HRESULT(__stdcall* DxReset)(LPDIRECT3DDEVICE9, PD3DPRESENT_PARAMETERS);

typedef struct Argb {
	int alpha;
	int r;
	int g;
	int b;
} *PArgb_t, Argb_t;

namespace Win32Exceptions {
	int ExcFilter(unsigned int, struct _EXCEPTION_POINTERS*);
}

namespace OverlaySpecs
{
	class DXFunctions
	{
	public:
		DXFunctions();
		DXFunctions(DxEndScene, DxReset);

		DxEndScene GetEndScene() const noexcept;
		DxReset GetReset() const noexcept;

		void SetEndScene(DxEndScene);
		void SetReset(DxReset);
	private:
		DxEndScene m_pEndScene;
		DxReset m_pReset;
	};

	class DXParameters
	{
	public:
		DXParameters()
			: m_pD3DPresentParameters(nullptr), m_pCurrentD3DDevice9(nullptr), m_pFont(nullptr)
		{
		}

		DXParameters(PD3DPRESENT_PARAMETERS pPresentParams, LPDIRECT3DDEVICE9 pDevice, LPD3DXFONT pFont)
			: m_pD3DPresentParameters(pPresentParams), m_pCurrentD3DDevice9(pDevice), m_pFont(pFont)
		{
		}

		~DXParameters()
		{
			delete m_pD3DPresentParameters;
			m_pFont->Release();
			m_pCurrentD3DDevice9->Release();
		}

		PD3DPRESENT_PARAMETERS GetPresentParameters() const noexcept;
		LPDIRECT3DDEVICE9 GetDevice() const noexcept;
		LPD3DXFONT GetFont() const noexcept;

		void SetPresentParameters(PD3DPRESENT_PARAMETERS);
		void SetDevice(LPDIRECT3DDEVICE9);
		void SetFont(LPD3DXFONT);

	private:
		PD3DPRESENT_PARAMETERS m_pD3DPresentParameters;
		LPDIRECT3DDEVICE9 m_pCurrentD3DDevice9;
		LPD3DXFONT m_pFont;
	};

	class OverlayMenuParameters
	{
	public:
		OverlayMenuParameters()
		: m_fontSize(22), m_menuFontSize(16.0f), m_fontColor(nullptr)
		{
		}

		OverlayMenuParameters(const OverlayMenuParameters& other)
		  : m_fontSize(other.m_fontSize),
		    m_menuFontSize(other.m_menuFontSize),
		    m_fontColor(other.m_fontColor)
		{
		}

		~OverlayMenuParameters()
		{
			delete m_fontColor;
			delete m_fontFamily;
		}

		PArgb_t GetFontColor() const noexcept;
		int GetFontSize() const noexcept;
		float GetMenuFontSize() const noexcept;
		LPCSTR GetFontFamily() const noexcept;

		void SetFontSize(int newSize);
		void SetMenuFontSize(float newSize);
		void SetFontColor(PArgb_t newColor);
		void SetFontFamily(LPCSTR family);

	private:
		int m_fontSize;
		float m_menuFontSize;

		//pointers
		PArgb_t m_fontColor;
		LPCSTR m_fontFamily = "Tahoma";
	};

	class OverlayInfopanelParameters
	{
	public:
		OverlayInfopanelParameters()
			: m_leftXPos(50), m_leftYPos(500), m_width(450), m_height(70),
			m_overlayXOffset(new int), m_overlayYOffset(new int)
		{
		}

		OverlayInfopanelParameters(const OverlayInfopanelParameters& other)
			: m_leftXPos(other.m_leftXPos), m_leftYPos(other.m_leftYPos),
			m_width(other.m_width), m_height(other.m_height),
			m_overlayXOffset(new int(*other.m_overlayXOffset)),
			m_overlayYOffset(new int(*other.m_overlayYOffset))
		{
		}

		~OverlayInfopanelParameters()
		{
			delete m_overlayXOffset;
			delete m_overlayYOffset;
		}

		int GetLeftXPos() const noexcept;
		int GetLeftYPos() const noexcept;
		int GetWidth() const noexcept;
		int GetHeight() const noexcept;
		int* GetOverlayXOffset() const noexcept;
		int* GetOverlayYOffset() const noexcept;

		void SetLeftXPos(int);
		void SetLeftYPos(int);
		void SetWidth(int);
		void SetHeight(int);
		void SetOverlayXOffset(int*);
		void SetOverlayYOffset(int*);

	private:
		int m_leftXPos;
		int m_leftYPos;
		int m_width;
		int m_height;
		int* m_overlayXOffset;
		int* m_overlayYOffset;
	};
	class OverlayParameters
	{
	public:
		OverlayParameters()
			: m_menuParameters(nullptr), m_infopanelParameters(nullptr)
		{
		}

		OverlayParameters(OverlayMenuParameters* menuParams, OverlayInfopanelParameters* infopanelParams)
			: m_menuParameters(menuParams), m_infopanelParameters(infopanelParams)
		{
		}

		~OverlayParameters()
		{
			delete m_infopanelParameters;
			delete m_menuParameters;
		}

		OverlayMenuParameters* GetMenuParameters() const noexcept;
		OverlayInfopanelParameters* GetInfopanelParameters() const noexcept;
		void SetMenuParameters(OverlayMenuParameters*);
		void SetInfopanelParameters(OverlayInfopanelParameters*);

		static OverlayParameters* CreateDefault() noexcept;

	private:
		OverlayMenuParameters* m_menuParameters;
		OverlayInfopanelParameters* m_infopanelParameters;
	};

	class OverlayData
	{
	public:
		OverlayData() : m_overlayParameters(OverlayParameters::CreateDefault())
		{
		}

		~OverlayData()
		{
			delete m_overlayParameters;
		}

		const OverlayParameters* GetOverlayParameters() const noexcept;

		OverlayMenuParameters* Menu() const noexcept;
		OverlayInfopanelParameters* Infopanel() const noexcept;
	private:
		OverlayParameters* m_overlayParameters;
	};

	class DXData
	{
	public:
		DXData() : m_dx_parameters(new DXParameters()), m_dx_functions(new DXFunctions())
		{
		}
		~DXData()
		{
			delete m_dx_functions;
			delete m_dx_parameters;
		}

		DXParameters* GetDirectXParameters() const noexcept;
		DXFunctions* GetDirectXFunctions() const noexcept;
	private:
		DXParameters* m_dx_parameters;
		DXFunctions* m_dx_functions;
	};

	class UIData
	{
	public:
		UIData()
		{
			m_skins.clear();
			m_displayInfo.clear();
		}

		UIData(const std::string& display_info, std::initializer_list<std::string> skins)
			: m_skins(skins), m_displayInfo(display_info)
		{
		}

		UIData(const std::string& display_info, const std::vector<std::string>& skins)
			: m_skins(skins), m_displayInfo(display_info)
		{
		}

		const std::vector<std::string>& GetSkins() const noexcept;
		const std::vector<std::string>* GetSkinsPtr() noexcept;
		const std::string& GetInfoPanelMessage() const noexcept;
		LPCSTR GetInfoPanelMessageCStr() const noexcept;

		void AppendSkin(const std::string& skin_name);
		void AppendSkins(const std::vector<std::string>& to_append);
		void UpdateInfoPanelMessage(const std::string& to_append) noexcept;
		void EraseSkin(const std::string& to_delete);
		void SetSkins(const std::vector<std::string>& skins);
		void SetInfoPanelMessage(const std::string& message);

	private:
		std::vector<std::string> m_skins;
		std::string m_displayInfo;
	};

	class ImGUIData
	{
	public:
		ImGUIData()
			: m_toolbox_visible(false), m_initialized(false), m_skins_listbox_selection(0)
		{
		}

		bool IsVisible() const noexcept;
		bool Initialized() const noexcept;
		int* GetSkinsListboxSelectedItemPtr() noexcept;
		int GetSkinsListboxSelectedItem() const noexcept;

		void Show() noexcept;
		void Hide() noexcept;
		void Toggle() noexcept;

		void Initialize(PDIRECT3DDEVICE9 pDevice) noexcept;
		void UnsetInitialized();

		void SetListboxSelection(const int pos);
		void SetupGlobalStyles();

	private:
		static ImVec4 ImGuiColor(const std::string& hexColor);

		bool m_toolbox_visible;
		bool m_initialized;
		int m_skins_listbox_selection;
	};
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
inline bool ImGuiSliderIntWithText(const char* label, int* value, int min, int max, const char* tag);

HRESULT __stdcall DetourAttachHook(PVOID*, PVOID);
HRESULT __stdcall DetourDetachHook(PVOID*, PVOID);

HRESULT __stdcall HookedEndScene(LPDIRECT3DDEVICE9);
HRESULT __stdcall HookedReset(LPDIRECT3DDEVICE9, PD3DPRESENT_PARAMETERS);

LRESULT __stdcall SendCommandToHostApplication(LPSTR);
LRESULT __stdcall SendCopyDataMessage(HWND, LPCSTR);
LRESULT __stdcall SendCopyDataMessage(HWND, LPSTR);
LRESULT __stdcall CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

DWORD __stdcall EjectThread(LPVOID);
DWORD WINAPI BuildOverlay(HINSTANCE);

void EjectOverlayProcess(FILE*);

HRESULT InitDirect3D();
HRESULT Init();
inline void DrawMenu(LPDIRECT3DDEVICE9);
HRESULT ConfigureFont(LPDIRECT3DDEVICE9, LPD3DXFONT*, LPCSTR, int);


inline void HandleCopyDataMessage(PCOPYDATASTRUCT);
inline void HandleWMSize(WPARAM, LPARAM);

inline void UpdateTweaksState(const std::string&);
inline void ProcessConfigurationCommand(std::string&);
inline void LTrim(std::string&);
inline void RTrim(std::string&);

inline void Trim(std::string&);

std::vector<std::string> Split(std::string, std::string);

#endif // !DLLMAIN_H

