#include "pch.hxx"

#include "framework/texture_hook.hxx"

#include "framework/detour_transaction.hxx"
#include "framework/ready.hxx"

#include "plugin/boot_log.hxx"
#include "plugin/diagnostics.hxx"

namespace
{
// Quest3D's "Texture" channel type, Aco_DX8_Texture. The DLL is named after the channel GUID (see
// DX8_TEXTURE_CHANNEL_GUID in the SDK's Aco_DX8_Texture.h, and engine/channels.lst which maps the
// name "Texture" onto this file).
constexpr const char* k_texture_channel_module = "BC052C38-2D5D-4F0C-A0CA-654D0AFC584A.dll";
constexpr const char* k_highpoly_module = "HighPoly.dll";

// MSVC mangling of, respectively:
//   public: virtual bool __thiscall Aco_DX8_Texture::LoadTextureFromMemory(char*, int)
//   public: virtual struct IDirect3DTexture9* __thiscall Aco_DX8_Texture::GetTexture(void)
//   public: char const* __thiscall A3d_Channel::GetChannelName(void)
constexpr const char* k_load_from_memory_symbol = "?LoadTextureFromMemory@Aco_DX8_Texture@@UAE_NPADH@Z";
constexpr const char* k_get_texture_symbol = "?GetTexture@Aco_DX8_Texture@@UAEPAUIDirect3DTexture9@@XZ";
constexpr const char* k_get_channel_name_symbol = "?GetChannelName@A3d_Channel@@QAEPBDXZ";

// All three below are __thiscall in the game, declared __fastcall here: on x86 the two agree on
// `this` in ECX and on the remaining arguments being pushed right-to-left with the callee cleaning
// up, so an ignored second parameter standing in for EDX makes the signatures interchangeable.
// Same trick, same caveat, as engine/engine_control.cxx.
using load_from_memory_fn = bool(__fastcall*)(Aco_DX8_Texture* self, void* edx, char* buffer, int buffer_size);
using get_texture_fn = IDirect3DTexture9*(__fastcall*)(Aco_DX8_Texture * self, void* edx);
using get_channel_name_fn = const char*(__fastcall*)(A3d_Channel * self, void* edx);

load_from_memory_fn true_load_texture_from_memory = nullptr;
get_texture_fn g_get_texture = nullptr;
get_channel_name_fn g_get_channel_name = nullptr;

std::atomic<bool> g_installed { false };

std::vector<std::pair<tw::framework::texture::texture_about_to_load_fn, tw::framework::texture::texture_loaded_fn>> g_subscribers;

// Cold path: a handful of calls per song start, on the engine thread. Readable beats fast here, and
// logging is affordable.
bool __fastcall load_texture_from_memory_hook(Aco_DX8_Texture* self, void* edx, char* buffer, int buffer_size)
{
    const char* name = tw::framework::texture::channel_name(reinterpret_cast<A3d_Channel*>(self));

    // Early load: the hook can be live before the subscribers are. A texture that goes by here is one the
    // sky will not recognise until it loads again, so each is written down - this is how the lifecycle log
    // answers whether startup was fast enough (plugin-offline-mode.md, Ф2).
    if(!tw::framework::ready::published()) [[unlikely]] {
        TW_BOOT_LOG("texture: '{}' loaded before the plugin was ready - passed through", name != nullptr ? name : "<unnamed>");
        return true_load_texture_from_memory(self, edx, buffer, buffer_size);
    }

    for(const auto& [about_to_load, loaded] : g_subscribers) {
        if(about_to_load != nullptr) {
            about_to_load(self, name);
        }
    }

    const bool ok = true_load_texture_from_memory(self, edx, buffer, buffer_size);
    if(!ok) {
        return ok;
    }

    IDirect3DTexture9* texture = tw::framework::texture::channel_texture(self);

    for(const auto& [about_to_load, loaded] : g_subscribers) {
        if(loaded != nullptr) {
            loaded(self, name, texture);
        }
    }

    return ok;
}

void* resolve(HMODULE module_handle, const char* symbol) noexcept
{
    if(module_handle == nullptr) {
        return nullptr;
    }

    return reinterpret_cast<void*>(::GetProcAddress(module_handle, symbol));
}

enum class install_result : std::uint8_t {
    installed,
    already_installed,
    unresolved,
    attach_failed,
};

// Shared by both load modes, and silent: the early caller runs under the loader lock and may only write
// the lifecycle log, so reporting is each caller's own business.
install_result install(HMODULE texture_module, tw::framework::detour::suspend threads) noexcept
{
    if(g_installed.load(std::memory_order_relaxed)) {
        return install_result::already_installed;
    }

    void* p_load_from_memory = resolve(texture_module, k_load_from_memory_symbol);
    void* p_get_texture = resolve(texture_module, k_get_texture_symbol);
    void* p_get_channel_name = resolve(::GetModuleHandleA(k_highpoly_module), k_get_channel_name_symbol);

    if(p_load_from_memory == nullptr || p_get_texture == nullptr || p_get_channel_name == nullptr) {
        return install_result::unresolved;
    }

    // The two plain thunks are usable the moment they are set, and stay usable even if the detour
    // below fails - a consumer that already knows a channel can still read its texture.
    g_get_texture = reinterpret_cast<get_texture_fn>(p_get_texture);
    g_get_channel_name = reinterpret_cast<get_channel_name_fn>(p_get_channel_name);

    true_load_texture_from_memory = reinterpret_cast<load_from_memory_fn>(p_load_from_memory);

    const bool ok = tw::framework::detour::attach(
        {
            { reinterpret_cast<void**>(&true_load_texture_from_memory), reinterpret_cast<void*>(load_texture_from_memory_hook) },
        },
        threads);

    if(!ok) {
        true_load_texture_from_memory = nullptr;
        return install_result::attach_failed;
    }

    g_installed.store(true, std::memory_order_relaxed);
    return install_result::installed;
}
} // namespace

namespace tw::framework::texture
{
void subscribe(texture_about_to_load_fn about_to_load, texture_loaded_fn loaded) noexcept
{
    g_subscribers.emplace_back(about_to_load, loaded);
}

bool is_installed() noexcept
{
    return g_installed.load(std::memory_order_relaxed);
}

const char* channel_name(A3d_Channel* channel) noexcept
{
    if(g_get_channel_name == nullptr || channel == nullptr) {
        return nullptr;
    }

    return g_get_channel_name(channel, nullptr);
}

IDirect3DTexture9* channel_texture(Aco_DX8_Texture* channel) noexcept
{
    if(g_get_texture == nullptr || channel == nullptr) {
        return nullptr;
    }

    return g_get_texture(channel, nullptr);
}

bool install_texture_hook() noexcept
{
    // GetModuleHandleA rather than DetourFindFunction: the latter falls back to LoadLibrary when the
    // module is not mapped, and a bare "<guid>.dll" resolved against the process search path could map
    // a second copy of a channel DLL from somewhere other than engine/channels. Not being mapped is a
    // legitimate answer here ("too early, try again"), not something to force.
    switch(install(::GetModuleHandleA(k_texture_channel_module), tw::framework::detour::suspend::others)) {
        case install_result::already_installed:
            return true;
        case install_result::installed:
            TW_LOG_INFO("texture_hook: installed on Aco_DX8_Texture::LoadTextureFromMemory");
            TW_BOOT_LOG("texture: LoadTextureFromMemory hooked (late)");
            return true;
        case install_result::unresolved:
            TW_LOG_WARNING("texture_hook: entry points not resolvable yet - not installed");
            return false;
        case install_result::attach_failed:
            TW_LOG_ERROR("texture_hook: DetourAttach on Aco_DX8_Texture::LoadTextureFromMemory failed");
            TW_BOOT_LOG("texture: DetourAttach on LoadTextureFromMemory FAILED (late)");
            return false;
    }

    return false;
}

bool install_texture_hook_from_loader(HMODULE texture_module) noexcept
{
    switch(install(texture_module, tw::framework::detour::suspend::none)) {
        case install_result::already_installed:
            TW_BOOT_LOG("texture: hook already installed - a second image of the channel? ignored");
            return true;
        case install_result::installed:
            TW_BOOT_LOG("texture: LoadTextureFromMemory hooked (early, module {})", static_cast<void*>(texture_module));
            return true;
        case install_result::unresolved:
            TW_BOOT_LOG("texture: entry points not found in the channel or HighPoly.dll - sky textures will not be recognised");
            return false;
        case install_result::attach_failed:
            TW_BOOT_LOG("texture: DetourAttach on LoadTextureFromMemory FAILED (early)");
            return false;
    }

    return false;
}
} // namespace tw::framework::texture
