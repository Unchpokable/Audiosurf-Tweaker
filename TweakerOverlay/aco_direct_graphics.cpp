#include "pch.hpp"

#include "aco_direct_graphics.hpp"

namespace
{
using get_direct3d_func_type = IDirect3D9*(__thiscall*)(void* this_);
get_direct3d_func_type get_direct3d_func;

using get_device_func_type = IDirect3dDevice9*(__thiscall*)(void* this_);
get_device_func_type get_device_func;
} // namespace

void tw::game::initialize()
{
}

IDirect3DDevice9* tw::game::get_device(void* channel)
{
    return nullptr;
}
