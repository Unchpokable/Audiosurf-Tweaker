#pragma once

class EngineInterface;

// No need to forward declare HWND, we're using Precompiled Headers

namespace tw::plugin::quest3d
{
extern EngineInterface* g_engine;
extern HWND g_game_handle;
} // namespace tw::plugin::quest3d
