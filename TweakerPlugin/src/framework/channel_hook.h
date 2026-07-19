#pragma once

#include "pch.h"

namespace hooks
{
// Resolves A3d_Channel::CallChannel and detours it so we can capture the
// EngineInterface pointer (quest3d::g_engine) the first time it runs.
void install_channel_hook();
} // namespace hooks
