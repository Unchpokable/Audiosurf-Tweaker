#pragma once

#include "global.h"

struct alignas(std::uint32_t) Context32 
{
    std::uint32_t init_addr; // 0x0 
    std::uint32_t user_data; // 0x4
    std::uint32_t ret_to;    // 0x8
};

