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

#include <dinput.h>

// DirectX 9. Pulled exclusively from the vendored DirectX SDK (June 2010, see
// cmake/DirectXSDK.cmake) rather than the Windows SDK's d3d9.h, so the whole
// set of D3D9/D3DX9 headers stays internally consistent.
#include <d3d9.h>
#include <d3d9types.h>
#include <d3dx9.h>
#include <d3dx9math.h>

// ID3D10Blob and ID3DInclude, for compiling HLSL in-process (skybox/sky_compile). Deliberately
// D3Dcommon.h and not D3Dcompiler.h: the latter drags in the whole D3D11 reflection surface for a
// 32-bit D3D9 plugin, and the one function we call is resolved through GetProcAddress anyway - so
// its signature is declared where it is used, next to the other hook typedefs in this project.
#include <D3Dcommon.h>

// Microsoft Detours
#include <detours.h>

// LuaJIT (see cmake/LuaJIT.cmake). lua.hpp is upstream's own extern "C" wrapper around
// lua.h/lauxlib.h/lualib.h/luajit.h - the whole C API surface the scripting layer uses, which by
// design is small (Docs/Internal/lua-scripting.md §2.5). Nothing here pulls in the FFI: that side of
// the boundary lives in Lua, not C++.
#include <lua.hpp>

// Vendored logging (src/libuulog, see its LICENCE). Lives in the PCH so the LOG_* macros - and the
// TW_LOG_* release-stripping wrappers in plugin/diagnostics.hxx that sit on top of them - are
// usable from any TU without a per-file include.
#include "libuulog/uulog.hh"

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
#include <Aco_Vector.h>
#include <Aco_Matrix.h>
#include <Aco_DX8_Texture.h>
#include <Aco_DX8_ObjectData.h>
// clang-format on

// Standard library
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cctype>
#include <charconv>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
