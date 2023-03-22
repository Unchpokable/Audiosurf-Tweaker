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

#include "detours.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx9.h"
#include "imgui/backends/imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "detours.lib")

#define D3DX_CREATE_DEFAULT_OVERLAY_FONT(pDevice, font) D3DXCreateFont(pDevice, 22, 0, FW_REGULAR, 1, 0, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma", &font)

#define ARRSIZE(arr) sizeof(arr)/sizeof(arr[0])

namespace win32_excs {
	int ExcFilter(unsigned int, struct _EXCEPTION_POINTERS*);
}

typedef void** PPVOID;
typedef int* PINT;
typedef float* PFLOAT;
typedef D3DPRESENT_PARAMETERS* PD3DPRESENT_PARAMETERS;
typedef COPYDATASTRUCT* PCOPYDATASTRUCT;

typedef HRESULT(__stdcall* endScene)(LPDIRECT3DDEVICE9);
typedef HRESULT(__stdcall* reset)(LPDIRECT3DDEVICE9, PD3DPRESENT_PARAMETERS);

typedef struct Argb {
	int alpha;
	int r;
	int g;
	int b;
} *PArgb_t, Argb_t;

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

inline void ProcessConfigurationCommand(std::string&);
inline void LTrim(std::string&);
inline void RTrim(std::string&);

inline void Trim(std::string&);

std::vector<std::string> Split(std::string, std::string);

#endif // !DLLMAIN_H

