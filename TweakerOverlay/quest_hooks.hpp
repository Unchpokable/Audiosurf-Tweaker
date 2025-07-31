#pragma once

namespace tw::game
{
Aco_DX8_Direct3D* try_find_directX_channel(EngineInterface* engine_interface);
} // namespace tw::game

namespace tw::game
{
void* get_engine_graphics_channel(Aco_DX8_Direct3D* channel);
} // namespace tw::game