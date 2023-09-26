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

#define THROW_IF_NULL(ptr, param_name) \
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


// I dont want to separate declaration and implementation of constructors and get-set methods in .h\.cpp files so
namespace OverlayData
{
	class DXFunctions
	{
	public:
		DXFunctions(DxEndScene pEndScene, DxReset pReset)
		{
			if (pEndScene == nullptr || pReset == nullptr)
				throw std::invalid_argument("One of the required DirectX functions was assigned to nullptr");

			m_pEndScene = pEndScene;
			m_pReset = pReset;
		}

		DxEndScene GetEndScene() const noexcept
		{
			return m_pEndScene;
		}

		DxReset GetReset() const noexcept
		{
			return m_pReset;
		}

		void SetEndScene(DxEndScene pEndScene) noexcept(false)
		{
			THROW_IF_NULL(pEndScene, "DxEndScene");
			m_pEndScene = pEndScene;
		}

		void SetReset(DxReset pReset) noexcept(false)
		{
			THROW_IF_NULL(pReset, "DxReset");
			m_pReset = pReset;
		}

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

		PD3DPRESENT_PARAMETERS GetPresentParameters() const { return m_pD3DPresentParameters; }
		LPDIRECT3DDEVICE9 GetDevice() const { return m_pCurrentD3DDevice9; }
		LPD3DXFONT GetFont() const { return m_pFont; }

		void SetPresentParameters(PD3DPRESENT_PARAMETERS pParams)
		{
			m_pD3DPresentParameters = pParams;
		}

		void SetDevice(LPDIRECT3DDEVICE9 pDevice)
		{
			THROW_IF_NULL(pDevice, "Direct3DDevice9 Interface")
			m_pCurrentD3DDevice9 = pDevice;
		}

		void SetFont(LPD3DXFONT pFont)
		{
			THROW_IF_NULL(pFont, "Direct3D Font")
			m_pFont = pFont;
		}

	private:
		PD3DPRESENT_PARAMETERS m_pD3DPresentParameters;
		LPDIRECT3DDEVICE9 m_pCurrentD3DDevice9;
		LPD3DXFONT m_pFont;
	};

	class OverlayMenuParameters
	{

	public:
		OverlayMenuParameters()
			: m_fontColor(nullptr), m_fontSize(new int(22)), m_menuFontSize(new float(16.0f))
		{
		}

		OverlayMenuParameters(const OverlayMenuParameters& other)
			: m_fontColor(other.m_fontColor),
			m_fontSize(new int(*other.m_fontSize)),
			m_menuFontSize(new float(*other.m_menuFontSize))
		{
		}

		OverlayMenuParameters& operator=(const OverlayMenuParameters& other)
		{
			if (this != &other)
			{
				m_fontColor = other.m_fontColor;
				delete m_fontSize;
				m_fontSize = new int(*other.m_fontSize);
				delete m_menuFontSize;
				m_menuFontSize = new float(*other.m_menuFontSize);
			}
			return *this;
		}

		~OverlayMenuParameters()
		{
			delete m_fontSize;
			delete m_menuFontSize;
		}

		PArgb_t GetFontColor() const { return m_fontColor; }
		int GetFontSize() const { return *m_fontSize; }
		float GetMenuFontSize() const { return *m_menuFontSize; }

		void SetFontColor(PArgb_t newFontColor)
		{
			if (newFontColor == nullptr)
			{
				throw std::invalid_argument("FontColor cannot be nullptr");
			}
			m_fontColor = newFontColor;
		}

		void SetFontSize(int* newFontSize)
		{
			THROW_IF_NULL(newFontSize, "FontSize");
			delete m_fontSize;
			m_fontSize = newFontSize;
		}

		void SetFontSize(int size)
		{
			*m_fontSize = size;
		}

		void SetMenuFontSize(float* newMenuFontSize)
		{
			THROW_IF_NULL(newMenuFontSize, "MenuFontSize");
			delete m_menuFontSize;
			m_menuFontSize = newMenuFontSize;
		}

		LPCSTR GetFontFamily()
		{
			return m_fontFamily;
		}

	private:
		PArgb_t m_fontColor;
		int* m_fontSize;
		float* m_menuFontSize;
		constexpr LPCSTR m_fontFamily = "Tahoma";
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

		OverlayInfopanelParameters& operator=(const OverlayInfopanelParameters& other)
		{
			if (this != &other)
			{
				m_leftXPos = other.m_leftXPos;
				m_leftYPos = other.m_leftYPos;
				m_width = other.m_width;
				m_height = other.m_height;

				delete m_overlayXOffset;
				m_overlayXOffset = new int(*other.m_overlayXOffset);

				delete m_overlayYOffset;
				m_overlayYOffset = new int(*other.m_overlayYOffset);
			}
			return *this;
		}

		~OverlayInfopanelParameters()
		{
			delete m_overlayXOffset;
			delete m_overlayYOffset;
		}

		int GetLeftXPos() const { return m_leftXPos; }
		int GetLeftYPos() const { return m_leftYPos; }
		int GetWidth() const { return m_width; }
		int GetHeight() const { return m_height; }
		int* GetOverlayXOffset() const { return m_overlayXOffset; }
		int* GetOverlayYOffset() const { return m_overlayYOffset; }

		void SetLeftXPos(int newLeftXPos)
		{
			m_leftXPos = newLeftXPos;
		}

		void SetLeftYPos(int newLeftYPos)
		{
			m_leftYPos = newLeftYPos;
		}

		void SetWidth(int newWidth)
		{
			m_width = newWidth;
		}

		void SetHeight(int newHeight)
		{
			m_height = newHeight;
		}

		void SetOverlayXOffset(int* newOffset)
		{
			THROW_IF_NULL(newOffset, "OverlayXOffset")
			delete m_overlayXOffset;
			m_overlayXOffset = newOffset;
		}

		void SetOverlayYOffset(int* newOffset)
		{
			THROW_IF_NULL(newOffset, "OverlayYOffset")
			delete m_overlayYOffset;
			m_overlayYOffset = newOffset;
		}

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

		OverlayMenuParameters* GetMenuParameters() const noexcept
		{
			return m_menuParameters;
		}

		OverlayInfopanelParameters* GetInfopanelParameters() const noexcept
		{
			return m_infopanelParameters;
		}

		void SetMenuParameters(OverlayMenuParameters* menuParams)
		{
			m_menuParameters = menuParams;
		}

		void SetInfopanelParameters(OverlayInfopanelParameters* infopanelParams)
		{
			m_infopanelParameters = infopanelParams;
		}

		static OverlayParameters* CreateDefault()
		{
			const auto defaultOverlayMenu = new OverlayMenuParameters();
			const auto defaultOverlayInfopanel = new OverlayInfopanelParameters();
			const auto thiz = new OverlayParameters(defaultOverlayMenu, defaultOverlayInfopanel);
			return thiz;
		}

	private:
		OverlayMenuParameters* m_menuParameters;
		OverlayInfopanelParameters* m_infopanelParameters;
	};

	class OverlayData
	{
	public:
		static void Initialize() noexcept
		{
			m_overlay_parameters = OverlayParameters::CreateDefault();
		}

		static void Free() noexcept
		{
			delete m_overlay_parameters;
		}

		static const OverlayParameters* GetOverlayParameters() noexcept 
		{
			return m_overlay_parameters;
		}

		static OverlayMenuParameters* Menu() { return m_overlay_parameters->GetMenuParameters(); }
		static OverlayInfopanelParameters* Infopanel() { return m_overlay_parameters->GetInfopanelParameters(); }

	private:
		static OverlayParameters* m_overlay_parameters;
	};

	constexpr OverlayMenuParameters* Menu();
	constexpr OverlayInfopanelParameters* Infopanel();
}




extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

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

inline void UpdateTweaksState(std::string);
inline void ProcessConfigurationCommand(std::string&);
inline void LTrim(std::string&);
inline void RTrim(std::string&);

inline void Trim(std::string&);

std::vector<std::string> Split(std::string, std::string);

#endif // !DLLMAIN_H

