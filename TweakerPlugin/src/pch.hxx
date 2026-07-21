// Precompiled header: every third-party / SDK include used anywhere in this
// project lives here, and only here. Our own .cxx files include "pch.hxx" plus
// their own matching header and nothing else library-related.
#pragma once

// clang-format off
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <process.h>
#include <tlhelp32.h>
// clang-format on

// DirectX 9. Pulled exclusively from the vendored DirectX SDK (June 2010, see
// cmake/DirectXSDK.cmake) rather than the Windows SDK's d3d9.h, so the whole
// set of D3D9/D3DX9 headers stays internally consistent.
#include <d3d9.h>
#include <d3d9types.h>
#include <d3dx9.h>
#include <d3dx9math.h>

// Microsoft Detours
#include <detours.h>

// Quest3D SDK. Proprietary, obtained separately by whoever builds this DLL
// (see cmake/Quest3DSDK.cmake / Q3D_SDK_DIR) - not part of this repository.
// These headers are 2000s-era MFC-style headers that rely on being included
// in a specific order (they forward-declare, rather than fully define, types
// owned by "later" headers) - clang-format's include sorting would break
// that, hence the off/on guard.
// clang-format off
#include <A3d_List.h>
#include <A3d_Channels.h>
#include <A3d_ChannelGroup.h>
#include <A3d_EngineInterface.h>
#include <Aco_DX8_D3DDeviceUse.h>
#include <Aco_String.h>
#include <Aco_Float.h>
#include <Aco_DX8_Texture.h>
#include <Aco_DX8_ObjectData.h>
// clang-format on

// Standard library
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <initializer_list>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
