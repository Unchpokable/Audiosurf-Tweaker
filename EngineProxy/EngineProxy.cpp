#include "EngineProxy.hpp"

namespace
{
EngineInterface* engine_interface;
}

namespace
{
ChannelType channelType;
}

extern "C" __declspec(dllexport) ChannelType* __cdecl GetType()
{
    strcpy_s(channelType.name, ENGINEPROXY_CHANNEL_NAME);

    channelType.version = 1;
    channelType.mversion = 1;
    channelType.miversion = 0;

    channelType.guid = ENGINEPROXY_CHANNEL_GUID;

    channelType.baseguid = ACO_CHANNEL_GUID;

    channelType.pluginType = PLUGINTYPE_STANDARD;

    return &channelType;
}

Aco_EngineProxy::Aco_EngineProxy()
{
    AllocConsole();

    SetChannelName(ENGINEPROXY_CHANNEL_NAME);

    engine_interface = engine;
}

void Aco_EngineProxy::CallChannel()
{
    if(CheckRenderCount()) {
        if(engine_interface != nullptr) {
            dMsg("Engine Proxy channel called. Engine is opened for everyone");
        }
    }
}

ENGINEPROXYDLL_EXPORTS;

EngineInterface* get_engine()
{
    return engine_interface;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    switch(reason) {
        case DLL_PROCESS_ATTACH:
            {
                volatile Aco_EngineProxy channel = {};
                return TRUE;
            }
    }
    return TRUE;
}