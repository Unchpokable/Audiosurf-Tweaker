#ifndef DLLMAIN_H
#define DLLMAIN_H

#include <d3d9.h>
#include <d3dx9.h>
#include <iostream>
#include "detours.h"
#include <string>
#include <algorithm>
#include <cctype>
#include <functional>
#include <locale>
#include <vector>
#include <mutex>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx9.h"
#include "imgui/backends/imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "detours.lib")

#define D3DX_CREATE_DEFAULT_OVERLAY_FONT(pDevice, font) D3DXCreateFont(pDevice, 24, 0, FW_BOLD, 1, 0, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma", &font)

#define ARRSIZE(arr) sizeof(arr)/sizeof(arr[0])

constexpr LPCSTR IntallPackageCommandHeader = "tw-Install-package";

typedef void** PPVOID;
typedef D3DPRESENT_PARAMETERS* PD3DPRESENT_PARAMETERS;

typedef HRESULT(__stdcall* endScene)(LPDIRECT3DDEVICE9);
typedef HRESULT(__stdcall* reset)(LPDIRECT3DDEVICE9, PD3DPRESENT_PARAMETERS);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

HRESULT __stdcall DetourAttachHook(PVOID*, PVOID);
HRESULT __stdcall DetourDetachHook(PVOID*, PVOID);

HRESULT __stdcall HookedEndScene(LPDIRECT3DDEVICE9);
HRESULT __stdcall HookedReset(LPDIRECT3DDEVICE9, PD3DPRESENT_PARAMETERS);

LRESULT __stdcall SendCommandToHostApplication(LPSTR);
LRESULT __stdcall CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

DWORD __stdcall EjectThread(LPVOID);
DWORD WINAPI BuildOverlay(HINSTANCE);

void InitD3D9();
void __forceinline DrawMenu();

inline void LTrim(std::string&);
inline void RTrim(std::string&);

inline void Trim(std::string&);

inline std::vector<std::string> Split(std::string, std::string);

#endif // !DLLMAIN_H

