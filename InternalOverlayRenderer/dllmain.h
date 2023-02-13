#ifndef D3DTYPES_H
#define D3DTYPES_H

#include <d3d9.h>
#include <d3dx9.h>
#include <iostream>
#include "detours.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "detours.lib")

#define D3DX_CREATE_DEFAULT_OVERLAY_FONT(pDevice, font) D3DXCreateFont(pDevice, 15, 0, FW_BOLD, 1, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma", &font);

typedef void** PPVOID;
typedef HRESULT(__stdcall* endScene)(LPDIRECT3DDEVICE9);
typedef HRESULT(__stdcall* reset)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);

HRESULT __stdcall DetourAttachHook(PVOID*, PVOID);
HRESULT __stdcall DetourDetachHook(PVOID*, PVOID);

HRESULT __stdcall HookedEndScene(LPDIRECT3DDEVICE9);
HRESULT __stdcall HookedReset(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);

DWORD __stdcall EjectThread(LPVOID);
DWORD WINAPI BuildOverlay(HINSTANCE);

void InitD3D9();

#endif // !D3DTYPES_H

