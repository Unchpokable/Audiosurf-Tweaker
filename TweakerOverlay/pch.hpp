// ReSharper disable CppWrongIncludesOrder
#pragma once

// === C++ STD LIB ===

#include <optional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <stdexcept>
#include <source_location>
#include <mutex>
#include <format>
#include <filesystem>
#include <iostream>
#include <array>

#include <process.h>

#include <assert.h>

// === Quest 3D ===

// clang-format off
//#include <A3d_List.h>
//#include <d3dx9math.h>
//#include <A3d_Channels.h>
//#include <A3d_ChannelGroup.h>
//#include <A3d_EngineInterface.h>
//#include <A3d_List.h>
//#include <A3d_ChannelDialog.h>
//
//#include <Aco_Float.h>
//#include <Aco_Vector.h>
//#include <Aco_String.h>
//#include <Aco_Matrix.h>
//#include <Act_New.h>
//
//#include <Aco_DX8_Direct3D.h>
//#include <Aco_DX8_D3DDeviceUse.h>
//
//#include <A3d_EngineInterface.h>
//#include <Aco_DX8_Texture.h>

//#include <A3d_EngineInterface.h>
//#include <A3d_ChannelGroup.h>

#include <A3d_ChannelGroup.h>

// clang-format on

// === Detours ===

#define NOMINMAX
#include <Windows.h>

#include <d3d9.h>
#include <d3dx9.h>
#include <d3dx9math.h>

#include <detours.h>
