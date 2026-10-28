#include "pch.hxx"

#include "framework/present_channel.hxx"

#include "framework/detour_transaction.hxx"
#include "framework/ready.hxx"

#include "plugin/boot_log.hxx"
#include "plugin/diagnostics.hxx"

namespace
{
// Quest3D's "Present" channel type. Like every channel DLL it is named after its GUID.
constexpr const char* k_present_channel_module = "5A4A4C8D-81A4-4A1E-828D-53C15D3B8E3C.dll";

// public: virtual void __thiscall Aco_DX8_PresentChannel::CallChannel(void) - exported by name.
constexpr const char* k_call_channel_symbol = "?CallChannel@Aco_DX8_PresentChannel@@UAEXXZ";

// __thiscall declared as __fastcall, with an ignored EDX parameter: same trick, same caveat, as
// texture_hook.cxx and engine/engine_control.cxx. The SDK has no header for this class, so `this` is
// typed as its base - nothing here looks inside it.
using call_channel_fn = void(__fastcall*)(A3d_Channel* self, void* edx);

call_channel_fn true_call_channel = nullptr;

std::atomic<bool> g_installed { false };

std::vector<std::pair<tw::framework::present_channel::channel_fn, tw::framework::present_channel::channel_fn>> g_subscribers;

// Hot path, once per shown frame: one relaxed load and a walk over a list of one.
void __fastcall call_channel_hook(A3d_Channel* self, void* edx)
{
    if(!tw::framework::ready::published()) [[unlikely]] {
        true_call_channel(self, edx);
        return;
    }

    for(const auto& [before, after] : g_subscribers) {
        if(before != nullptr) {
            before();
        }
    }

    true_call_channel(self, edx);

    for(auto it = g_subscribers.rbegin(); it != g_subscribers.rend(); ++it) {
        if(it->second != nullptr) {
            it->second();
        }
    }
}

enum class install_result : std::uint8_t {
    installed,
    already_installed,
    unresolved,
    attach_failed,
};

// Shared by both load modes, and silent: the early caller runs under the loader lock.
install_result attach_hook(HMODULE channel_module, tw::framework::detour::suspend threads) noexcept
{
    if(g_installed.load(std::memory_order_relaxed)) {
        return install_result::already_installed;
    }

    if(channel_module == nullptr) {
        return install_result::unresolved;
    }

    void* p_call_channel = reinterpret_cast<void*>(::GetProcAddress(channel_module, k_call_channel_symbol));
    if(p_call_channel == nullptr) {
        return install_result::unresolved;
    }

    true_call_channel = reinterpret_cast<call_channel_fn>(p_call_channel);

    const bool ok = tw::framework::detour::attach(
        {
            { reinterpret_cast<void**>(&true_call_channel), reinterpret_cast<void*>(call_channel_hook) },
        },
        threads);

    if(!ok) {
        true_call_channel = nullptr;
        return install_result::attach_failed;
    }

    g_installed.store(true, std::memory_order_relaxed);
    return install_result::installed;
}
} // namespace

namespace tw::framework::present_channel
{
void subscribe(channel_fn before, channel_fn after) noexcept
{
    g_subscribers.emplace_back(before, after);
}

bool is_installed() noexcept
{
    return g_installed.load(std::memory_order_relaxed);
}

bool install() noexcept
{
    // GetModuleHandleA, not DetourFindFunction: see install_texture_hook - not being mapped yet is an
    // answer, and loading a second copy of a channel DLL from the search path is not.
    switch(attach_hook(::GetModuleHandleA(k_present_channel_module), tw::framework::detour::suspend::others)) {
        case install_result::already_installed:
            return true;
        case install_result::installed:
            TW_LOG_INFO("present_channel: installed on Aco_DX8_PresentChannel::CallChannel");
            TW_BOOT_LOG("present: Present channel hooked (late) - the overlay draws before the game presents");
            return true;
        case install_result::unresolved:
            return false;
        case install_result::attach_failed:
            TW_LOG_ERROR("present_channel: DetourAttach on Aco_DX8_PresentChannel::CallChannel failed");
            TW_BOOT_LOG("present: DetourAttach on the Present channel FAILED (late) - the overlay draws from d3d9 Present, "
                        "screen capture will miss it");
            return false;
    }

    return false;
}

bool install_from_loader(HMODULE channel_module) noexcept
{
    switch(attach_hook(channel_module, tw::framework::detour::suspend::none)) {
        case install_result::already_installed:
            TW_BOOT_LOG("present: hook already installed - a second image of the channel? ignored");
            return true;
        case install_result::installed:
            TW_BOOT_LOG("present: Present channel hooked (early, module {})", static_cast<void*>(channel_module));
            return true;
        case install_result::unresolved:
            TW_BOOT_LOG("present: CallChannel not exported by the Present channel - the overlay draws from d3d9 Present, "
                        "screen capture will miss it");
            return false;
        case install_result::attach_failed:
            TW_BOOT_LOG("present: DetourAttach on the Present channel FAILED (early) - the overlay draws from d3d9 Present, "
                        "screen capture will miss it");
            return false;
    }

    return false;
}
} // namespace tw::framework::present_channel
