#pragma once

#include "framework/detour_transaction.hxx"

namespace tw::framework
{
// Detours A3d_Channel::CallChannel to capture the engine pointer. `suspend::none` only from DllMain in the
// early load, where the channel scan that is loading the plugin guarantees nobody is running a channel yet
// (plugin-offline-mode.md §4.2, stage 0).
bool install_channel_hook(detour::suspend threads = detour::suspend::others) noexcept;
} // namespace tw::framework
