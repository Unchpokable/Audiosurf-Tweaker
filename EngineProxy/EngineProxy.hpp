#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <string.h>

#include <A3d_List.h>
#include "d3dx9math.h"
#include "A3d_Channels.h"
#include "A3d_ChannelGroup.h"
#include "A3d_EngineInterface.h"

#ifdef ENGINEPROXY_EXPORTS
#define ENGINEPROXY_API __declspec(dllexport)
#else
#define ENGINEPROXY_API __declspec(dllimport)
#endif

#define ENGINEPROXY_CHANNEL_NAME "EngineProxy"

static const GUID ENGINEPROXY_CHANNEL_GUID = { 0x3a141435, 0x6f12, 0x4886, { 0x93, 0x2d, 0x71, 0xce, 0x5f, 0x21, 0x1d, 0xe0 } };

extern "C"
{
ENGINEPROXY_API EngineInterface* get_engine();
}

class ENGINEPROXY_API Aco_EngineProxy : public A3d_Channel {
public:
    Aco_EngineProxy();
    virtual ~Aco_EngineProxy() override = default;

    virtual void CallChannel() override;
};

#define ENGINEPROXYDLL_EXPORTS extern "C" {                 \
    __declspec(dllexport) DllInterface* __cdecl InitDll()   \
    {                                                       \
        return new Aco_EngineProxy();                       \
    }                                                       \
}                                                           