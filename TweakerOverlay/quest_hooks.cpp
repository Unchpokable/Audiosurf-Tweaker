#include "pch.hpp"

#include "quest_hooks.hpp"

#include "quest_offsets.hpp"

Aco_DX8_Direct3D* tw::game::try_find_directX_channel(EngineInterface* engine_interface)
{
    auto channel_count = engine_interface->GetChannelGroupCount();

    for(int group_id { 0 }; group_id < channel_count; ++group_id) {
        auto group = engine_interface->GetChannelGroup(group_id);

        auto channels_count = group->GetChannelCount();

        for(int channel_id { 0 }; channel_id < channels_count; ++channel_id) {
            auto channel = group->GetChannel(channel_id);

            if(std::strcmp(channel->GetChannelName(), "Aco_DX8_Direct3D") == 0) {
                return dynamic_cast<Aco_DX8_Direct3D*>(channel);
            }
        }
    }

    return nullptr;
}

void* tw::game::get_engine_graphics_channel(Aco_DX8_Direct3D* channel)
{
    void* direct_graphics = *reinterpret_cast<void**>(reinterpret_cast<char*>(channel) + tw::offsets::ACO_DX8_DIRECT3D_GRAPHICS_OFFSET);
    return direct_graphics;
}
