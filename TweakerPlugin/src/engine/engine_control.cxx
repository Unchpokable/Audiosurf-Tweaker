#include "pch.hxx"

#include "engine/engine_control.hxx"

#include "engine/engine_frame.hxx"
#include "engine/engine_symbols.hxx"

#include "framework/ready.hxx"

#include "plugin/boot_log.hxx"
#include "plugin/diagnostics.hxx"
#include "plugin/quest3d_state.hxx"

namespace
{
using tw::engine::engine_control_handle;
using tw::engine::control::frame_fn;

// A handful, and deliberately fixed: subscribers are plugin modules registered during startup, not
// anything a script or a user can add to. A vector here would buy a reallocation on the hot path's
// read side in exchange for nothing.
constexpr int k_max_subscribers = 8;

tw::engine::symbols::engine_loop_fn o_engine_loop = nullptr;

std::array<frame_fn, k_max_subscribers> g_pre {};
std::array<frame_fn, k_max_subscribers> g_post {};
int g_pre_count = 0;
int g_post_count = 0;

bool g_installed = false;

// Read from the startup thread while the engine thread writes them.
std::atomic<bool> g_ticked { false };
std::atomic<bool> g_healthy { false };

// Engine thread only, and only meaningful inside the hook.
bool g_identity_checked = false;
bool g_in_frame = false;

// Cold: the first frame, once. Writing to the lifecycle log from the engine thread is allowed here
// for the same reason it is allowed inside bind_device - this is initialisation that happens to run
// on that thread, not per-frame code (plugin-offline-mode.md §4.5).
void verify_identity(engine_control_handle* self) noexcept
{
    g_identity_checked = true;

    const tw::engine::symbols::table& api = tw::engine::symbols::get();
    const void* const vptr = *reinterpret_cast<void* const*>(self);

    if(vptr != api.engine_control_vtable) {
        TW_BOOT_LOG("engine: EngineLoop fired on an object with vptr {}, but EngineControl's vtable is {} - "
                    "the spine is inert this session and the engine pointer stays null",
            vptr,
            api.engine_control_vtable);
        TW_LOG_ERROR("engine_control: vptr {} is not EngineControl's vtable {} - spine disabled", vptr, api.engine_control_vtable);
        return;
    }

    g_healthy.store(true, std::memory_order_relaxed);

    // Called through the export, not through a vtable slot: the object has just proved what it is,
    // which is exactly the condition engine journal §2.5 puts on calling an export by name.
    const auto get_engine_interface = reinterpret_cast<tw::engine::symbols::get_engine_interface_fn>(api.get_engine_interface);
    EngineInterface* const engine = get_engine_interface(self, nullptr);

    if(engine != nullptr) {
        tw::plugin::quest3d::g_engine = engine;
    }

    TW_BOOT_LOG("engine: first EngineLoop - EngineControl {}, EngineInterface {}", static_cast<void*>(self), static_cast<void*>(engine));
    TW_LOG_INFO("engine_control: spine live, EngineInterface={}", static_cast<void*>(engine));
}

void __fastcall hk_engine_loop(engine_control_handle* self, void* /*edx*/)
{
    // The game guards its own re-entry (EngineLoop returns immediately when it is already inside
    // itself), so a nested call does no graph work and must not be counted as a frame or shown to
    // subscribers as one. Ours is a separate flag because the game's lives at an offset, and an
    // offset is a thing that can be wrong.
    if(g_in_frame) [[unlikely]] {
        o_engine_loop(self, nullptr);
        return;
    }

    if(!g_identity_checked) [[unlikely]] {
        verify_identity(self);
        g_ticked.store(true, std::memory_order_relaxed);
    }

    if(!g_healthy.load(std::memory_order_relaxed)) [[unlikely]] {
        o_engine_loop(self, nullptr);
        return;
    }

    g_in_frame = true;

    // Counted whether or not anyone is listening: this is the engine's frame number, and it is true
    // from the first frame regardless of how far the plugin's own startup has got.
    tw::engine::frame::begin();

    // In the early load the detour goes in from DllMain, before the startup thread has registered a
    // single subscriber - so the lists are still being written while this thread could be reading
    // them. framework::ready is the flag that publishes them, exactly as it does for the d3d9 bind
    // listeners and the texture subscribers (framework/ready.hxx). Until it is up, the spine
    // forwards and counts and nothing else.
    if(tw::framework::ready::published()) [[likely]] {
        for(int i = 0; i < g_pre_count; ++i) {
            g_pre[i]();
        }

        o_engine_loop(self, nullptr);

        for(int i = 0; i < g_post_count; ++i) {
            g_post[i]();
        }
    }
    else {
        o_engine_loop(self, nullptr);
    }

    g_in_frame = false;
}
} // namespace

namespace tw::engine::control
{
bool install(framework::detour::suspend threads) noexcept
{
    if(g_installed) {
        return true;
    }

    // No logging on this path: the early load calls install() from DllMain, where plugin diagnostics
    // do not exist yet. The caller reports the outcome through the lifecycle log, which does.
    if(!symbols::initialize()) {
        return false;
    }

    o_engine_loop = reinterpret_cast<symbols::engine_loop_fn>(symbols::get().engine_loop);

    const bool ok = tw::framework::detour::attach(
        {
            { reinterpret_cast<void**>(&o_engine_loop), reinterpret_cast<void*>(hk_engine_loop) },
        },
        threads);

    if(!ok) {
        o_engine_loop = nullptr;
        return false;
    }

    g_installed = true;
    return true;
}

bool installed() noexcept
{
    return g_installed;
}

bool ticked() noexcept
{
    return g_ticked.load(std::memory_order_relaxed);
}

bool healthy() noexcept
{
    return g_healthy.load(std::memory_order_relaxed);
}

void subscribe_pre(frame_fn fn) noexcept
{
    if(fn == nullptr || g_pre_count >= k_max_subscribers) {
        return;
    }

    g_pre[g_pre_count++] = fn;
}

void subscribe_post(frame_fn fn) noexcept
{
    if(fn == nullptr || g_post_count >= k_max_subscribers) {
        return;
    }

    g_post[g_post_count++] = fn;
}
} // namespace tw::engine::control
