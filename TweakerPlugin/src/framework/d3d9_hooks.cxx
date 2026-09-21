#include "pch.hxx"

#include "framework/d3d9_hooks.hxx"
#include "framework/d3d9_state.hxx"
#include "framework/detour_transaction.hxx"
#include "framework/ready.hxx"
#include "framework/wndproc_hub.hxx"

#include "plugin/boot_log.hxx"
#include "plugin/diagnostics.hxx"
#include "plugin/quest3d_state.hxx"

namespace
{
using direct3d_create9_fn = IDirect3D9*(__stdcall*)(UINT);
using create_device_fn = long(__stdcall*)(LPDIRECT3D9, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, LPDIRECT3DDEVICE9*);
using reset_fn = long(__stdcall*)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);
using end_scene_fn = long(__stdcall*)(LPDIRECT3DDEVICE9);
using present_fn = long(__stdcall*)(LPDIRECT3DDEVICE9, const RECT*, const RECT*, HWND, const RGNDATA*);
using swap_chain_present_fn = long(__stdcall*)(IDirect3DSwapChain9*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);
using set_texture_fn = long(__stdcall*)(LPDIRECT3DDEVICE9, DWORD, IDirect3DBaseTexture9*);
using draw_primitive_fn = long(__stdcall*)(LPDIRECT3DDEVICE9, D3DPRIMITIVETYPE, UINT, UINT);
using draw_indexed_primitive_fn = long(__stdcall*)(LPDIRECT3DDEVICE9, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);

direct3d_create9_fn o_direct3d_create9 = nullptr;
create_device_fn o_create_device = nullptr;
reset_fn o_reset = nullptr;
end_scene_fn o_end_scene = nullptr;
present_fn o_present = nullptr;
swap_chain_present_fn o_swap_chain_present = nullptr;
set_texture_fn o_set_texture = nullptr;
draw_primitive_fn o_draw_primitive = nullptr;
draw_indexed_primitive_fn o_draw_indexed_primitive = nullptr;

tw::framework::d3d9::ui_plugin_draw_fn g_ui_draw = nullptr;
tw::framework::d3d9::draw_intercept_fn g_draw_intercept = nullptr;

// Registration happens once, at plugin load, from the single startup thread; the vectors are
// read-only from then on. Same shape and same reasoning as wndproc_hub's subscriber vectors: a
// handful of entries, walked linearly, and never mutated while the render thread is walking them
// (see framework/ready.hxx - subscribing has to be done before the hooks may act).
std::vector<std::pair<tw::framework::d3d9::device_reset_listener_fn, tw::framework::d3d9::device_reset_listener_fn>> g_reset_listeners;
std::vector<std::pair<tw::framework::d3d9::device_bind_fn, tw::framework::d3d9::device_unbind_fn>> g_bind_listeners;

LPDIRECT3DDEVICE9 g_bound_device = nullptr;

// Early load (engine\channels\): the chain Direct3DCreate9 -> IDirect3D9::CreateDevice -> device methods
// installs itself off the game's own objects, one link per first object. See hook_direct3d_create9 below.
std::atomic<bool> g_create_device_hooked { false };
std::atomic<bool> g_device_hooks_installed { false };

// Counts the game's successful CreateDevice calls, for the lifecycle log only. Written on whichever thread
// creates a device - the game's main thread in practice.
std::atomic<int> g_devices_created { 0 };

// Stage 0..7 texture bindings, mirrored off hk_set_texture so an interceptor can ask "what is bound
// right now" without a GetTexture()/Release() pair on every single draw call. Not owning
// references - the game owns the lifetime, and the mirror is cleared wherever that lifetime could
// have ended underneath us (bind/unbind, and either side of a Reset).
constexpr DWORD k_tracked_stages = 8;
std::array<IDirect3DBaseTexture9*, k_tracked_stages> g_stage_texture {};

// Set for the duration of a draw_intercept_fn call. Anything the interceptor draws re-enters the
// draw hooks below, and without this the very first replacement draw would recurse forever.
bool g_in_draw_intercept = false;

// Set for the duration of the overlay's own pass (see draw_overlay_frame). Everything the overlay
// submits comes back through the hooks in this file - the EndScene that closes its scene, and the
// SetTexture/DrawIndexedPrimitive behind every ImGui command - and none of it is the game's. Without
// this the interceptor would be offered the font atlas as a candidate sky texture, and the stage-0
// mirror would be left describing a binding the game never made.
bool g_in_overlay_pass = false;

// True from the first Present observed on the bound device onwards.
//
// The overlay is drawn from Present, and everything below assumes Present is reached. If it is not -
// a swap-chain path neither hook covers, a present routine the runtime resolves somewhere else - the
// overlay would silently vanish instead of merely flickering, which is the worse failure by a wide
// margin. So hk_end_scene keeps the old draw as a fallback and this latch disarms it the moment a
// real Present proves the primary path works. Costs one bool test per EndScene, and at most one
// duplicated overlay pass on the very first frame after a device binds.
bool g_present_seen = false;

// Set around the original Present. The D3D9 runtime is free to implement
// IDirect3DDevice9::Present in terms of the swap chain's, and both are detoured here - without this
// a nested call would draw the overlay a second time, which is precisely the defect being fixed.
bool g_in_present = false;

// The two facts about the bound device that a *different* thread needs to read.
//
// handle_activate_app runs on whichever thread owns the game window's message pump, while
// bind_device/unbind_device run on the render thread. Reaching for g_bound_device from the message
// side to re-query GetCreationParameters was a use-after-free waiting to happen: the pointer can be
// released between the null check and the call, and no amount of atomics on the pointer itself
// would fix that. Caching the two values at bind time removes the need to touch the device object
// from the window thread at all - what's left is a pair of plain scalars, and those an atomic
// genuinely does make safe.
//
// x86-only target, so both are pointer-or-smaller and always lock-free.
std::atomic<HWND> g_bound_window { nullptr };
std::atomic<bool> g_bound_windowed { true };

static_assert(std::atomic<HWND>::is_always_lock_free, "d3d9 hook state must be lock-free: it is read from a WndProc");
static_assert(std::atomic<bool>::is_always_lock_free, "d3d9 hook state must be lock-free: it is read from a WndProc");

// Reads Windowed off the device's implicit swap chain. GetCreationParameters, which is what the
// rest of bind_device uses, does not carry it - and the present parameters passed to
// CreateDevice/Reset are not available on the hk_end_scene late-load path at all, so querying the
// swap chain is the one way to answer this identically for all three bind sites. Cold path: runs
// once per bind, never per frame.
bool query_windowed(LPDIRECT3DDEVICE9 device) noexcept
{
    IDirect3DSwapChain9* swap_chain = nullptr;
    if(FAILED(device->GetSwapChain(0, &swap_chain)) || swap_chain == nullptr) {
        // Unknown - assume windowed, the conservative answer: it only suppresses the minimize
        // below, and a game that fails to minimize is a far smaller problem than a windowed game
        // that minimizes itself every time the user clicks another app.
        return true;
    }

    D3DPRESENT_PARAMETERS params {};
    const bool ok = SUCCEEDED(swap_chain->GetPresentParameters(&params));
    swap_chain->Release();

    return ok ? params.Windowed != FALSE : true;
}

// Only ever minimizes an exclusive-fullscreen device. Fullscreen is the case that actually needs
// it (a fullscreen D3D9 device that keeps the display mode while another app is in front leaves
// the desktop in a broken state); a windowed game minimizing itself the moment the user clicks the
// Tweaker desktop window is just hostile - and, since a minimized device fails
// TestCooperativeLevel, it also silently stops the overlay from drawing.
bool handle_activate_app(HWND hwnd, UINT /*msg*/, WPARAM wparam, LPARAM /*lparam*/, LRESULT& /*out_result*/)
{
    if(wparam != FALSE) {
        return false;
    }

    if(g_bound_windowed.load(std::memory_order_relaxed)) {
        return false;
    }

    if(hwnd != g_bound_window.load(std::memory_order_relaxed)) {
        return false;
    }

    TW_LOG_INFO("d3d9: WM_ACTIVATEAPP(FALSE) on fullscreen device, minimizing hwnd={}", static_cast<const void*>(hwnd));
    ::ShowWindow(hwnd, SW_MINIMIZE);

    return false;
}

// Defined below; bind_device() calls it to retire the previous device before adopting a new one.
void unbind_device();

void bind_device(LPDIRECT3DDEVICE9 device)
{
    D3DDEVICE_CREATION_PARAMETERS params {};
    device->GetCreationParameters(&params);

    const bool windowed = query_windowed(device);

    if(device == g_bound_device && params.hFocusWindow == g_bound_window.load(std::memory_order_relaxed)) {
        // Same device and window, but a Reset() may still have flipped windowed<->fullscreen.
        g_bound_windowed.store(windowed, std::memory_order_relaxed);
        return;
    }

    // Only a genuinely different device gets the old binding torn down. With no Release hook this is
    // the only place a device change is ever noticed, so the unbind listener has to be driven from
    // here rather than left to the listener's own internal bookkeeping.
    //
    // The `device != g_bound_device` half is hardening rather than a bug fix, and it is worth being
    // precise about which: hFocusWindow is fixed when the device is created and does not change
    // across Reset, so `device == g_bound_device` already implies the windows match and the early
    // return above catches every same-device case. This branch was therefore only ever reachable
    // with a genuinely different device.
    //
    // It is still worth stating, for two reasons. It matches what d3d9_hooks.hxx documents -
    // on_unbind fires when a *different* device replaces the current one - so the code and the
    // contract now agree instead of agreeing by accident. And it stops the log line below from
    // claiming "device changed 0x1234 -> 0x1234", which is what it printed if the branch were ever
    // entered with the same pointer, and which would send anyone reading it in the wrong direction.
    if(g_bound_device != nullptr && device != g_bound_device) {
        TW_LOG_INFO("d3d9: device changed {} -> {}, unbinding the old one",
            static_cast<const void*>(g_bound_device),
            static_cast<const void*>(device));
        unbind_device();
    }
    else if(g_bound_device != nullptr) {
        TW_LOG_INFO("d3d9: same device {}, focus window {} -> {} - re-targeting without unbinding",
            static_cast<const void*>(device),
            static_cast<const void*>(g_bound_window.load(std::memory_order_relaxed)),
            static_cast<const void*>(params.hFocusWindow));
    }

    g_bound_device = device;
    g_bound_window.store(params.hFocusWindow, std::memory_order_relaxed);
    g_bound_windowed.store(windowed, std::memory_order_relaxed);
    g_stage_texture.fill(nullptr);
    // A different device is a fresh question about how this one presents: re-arm the EndScene
    // fallback until a Present on *this* device proves the primary path.
    g_present_seen = false;
    tw::plugin::quest3d::g_game_handle = params.hFocusWindow;

    TW_LOG_INFO("d3d9: bind_device device={} hwnd={} windowed={}",
        static_cast<const void*>(device),
        static_cast<const void*>(params.hFocusWindow),
        windowed);

    tw::framework::wndproc::install(params.hFocusWindow);

    for(const auto& [on_bind, on_unbind] : g_bind_listeners) {
        if(on_bind != nullptr) {
            on_bind(device, params.hFocusWindow);
        }
    }
}

void unbind_device()
{
    TW_LOG_INFO("d3d9: unbind_device device={}", static_cast<const void*>(g_bound_device));

    // Tracking state is cleared before the listener runs, so anything the listener does while
    // tearing itself down (ImGui_ImplDX9_Shutdown releases its own reference to this device) sees a
    // plugin that already considers nothing bound. With the Release hook gone this is no longer
    // load-bearing against re-entrancy - that release now goes straight to d3d9.dll - but the
    // ordering stays: a listener is entitled to call back into this module, and finding it in a
    // half-torn-down state is how the old crash cascade started.
    g_bound_device = nullptr;
    g_bound_window.store(nullptr, std::memory_order_relaxed);
    g_bound_windowed.store(true, std::memory_order_relaxed);
    g_stage_texture.fill(nullptr);
    g_present_seen = false;

    tw::framework::wndproc::uninstall();

    // Reverse registration order: a consumer that registered later may have been built on top of an
    // earlier one, so it gets to tear down while that earlier one is still standing.
    for(auto it = g_bind_listeners.rbegin(); it != g_bind_listeners.rend(); ++it) {
        if(it->second != nullptr) {
            it->second();
        }
    }
}

bool is_rendering_to_back_buffer(LPDIRECT3DDEVICE9 device)
{
    IDirect3DSurface9* render_target = nullptr;
    if(FAILED(device->GetRenderTarget(0, &render_target)) || render_target == nullptr) {
        return false;
    }

    IDirect3DSurface9* back_buffer = nullptr;
    const bool matches = SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer)) && back_buffer == render_target;

    render_target->Release();
    if(back_buffer != nullptr) {
        back_buffer->Release();
    }

    return matches;
}

// Defined below, with the rest of the early chain.
void install_device_hooks_from_game(LPDIRECT3DDEVICE9 device);

// Initialisation, not per-frame: a handful of calls per session (startup, each windowed<->fullscreen
// switch), so the lifecycle log is allowed here (§4.5).
long __stdcall hk_create_device(LPDIRECT3D9 p_d3d9,
    UINT adapter,
    D3DDEVTYPE device_type,
    HWND focus_window,
    DWORD behavior_flags,
    D3DPRESENT_PARAMETERS* p_presentation_parameters,
    LPDIRECT3DDEVICE9* pp_returned_device_interface)
{
    const long result = o_create_device(
        p_d3d9, adapter, device_type, focus_window, behavior_flags, p_presentation_parameters, pp_returned_device_interface);

    if(FAILED(result) || pp_returned_device_interface == nullptr || *pp_returned_device_interface == nullptr) {
        TW_LOG_WARNING("d3d9: hk_create_device failed, hr=0x{:08X}", static_cast<unsigned long>(result));
        TW_BOOT_LOG("d3d9: game CreateDevice failed, hr=0x{:08X}", static_cast<unsigned long>(result));
        return result;
    }

    const LPDIRECT3DDEVICE9 device = *pp_returned_device_interface;
    const bool ready = tw::framework::ready::published();

    TW_LOG_INFO("d3d9: hk_create_device succeeded, focus_window={}", static_cast<const void*>(focus_window));
    TW_BOOT_LOG("d3d9: game CreateDevice #{} -> device {}, windowed={}, plugin {}",
        g_devices_created.fetch_add(1, std::memory_order_relaxed) + 1,
        static_cast<void*>(device),
        p_presentation_parameters != nullptr ? p_presentation_parameters->Windowed != FALSE : true,
        ready ? "ready" : "not ready yet - binding waits for the first EndScene after it is");

    // Early load only - in the late one this flag was set when the throwaway device supplied the entries.
    if(!g_device_hooks_installed.exchange(true)) {
        install_device_hooks_from_game(device);
    }

    if(ready) {
        bind_device(device);
    }

    return result;
}

long __stdcall hk_reset(LPDIRECT3DDEVICE9 p_device, D3DPRESENT_PARAMETERS* p_presentation_parameters)
{
    // Reset() is a method call on an *existing* device, never how the game hands us a brand new
    // one (that's hk_create_device's job) - only touch listener/rebind state if this is actually
    // the device we're bound to right now.
    const bool bound_here = p_device == g_bound_device;

    if(bound_here) {
        // Every stage binding is about to become meaningless: whatever the game had bound is either
        // released across the Reset or re-set afterwards, and a mirror entry surviving the gap is
        // exactly how a stale pointer would alias a freshly allocated texture.
        g_stage_texture.fill(nullptr);

        for(const auto& [pre, post] : g_reset_listeners) {
            if(pre != nullptr) {
                pre();
            }
        }
    }

    if(bound_here) {
        TW_LOG_INFO("d3d9: hk_reset on bound device (windowed={})",
            p_presentation_parameters != nullptr ? (p_presentation_parameters->Windowed != FALSE) : true);
    }

    const long result = o_reset(p_device, p_presentation_parameters);

    if(bound_here && FAILED(result)) {
        TW_LOG_WARNING("d3d9: Reset failed, hr=0x{:08X} - device objects stay invalidated", static_cast<unsigned long>(result));
    }

    if(bound_here && SUCCEEDED(result)) {
        for(const auto& [pre, post] : g_reset_listeners) {
            if(post != nullptr) {
                post();
            }
        }

        // Reset() can swap the focus window (e.g. a windowed<->exclusive-fullscreen toggle)
        // without recreating the device itself - re-run bind_device() to catch that; it no-ops if
        // neither the device nor the window actually changed.
        bind_device(p_device);
    }

    return result;
}

// The overlay's entire submission for one presented frame, from Present.
//
// It used to run from hk_end_scene, and that was wrong in a way only semi-transparent pixels could
// show. EndScene is NOT a frame boundary in this game - measured, and written down twice
// (Docs/Internal/skybox-geometry.md and skybox-replacer-roadmap.md, "EndScene fires more than once
// per shown frame"; the sky probe's frame tick was the first thing it broke). More than one of those
// EndScenes renders to the back buffer, so the whole ImGui frame was composited over itself N times,
// and compositing a colour at alpha `a` N times over the same pixels yields an effective alpha of
// 1-(1-a)^N. At a=1 that is idempotent, which is why the menu - which fills its own opaque
// theme::surface background before drawing anything into it - never showed a thing, while every
// background-draw-list element (watermark, pins, notefeed, and everything a script draws through
// tw.hud.*) sits directly on the game's frame at alpha < 1 and drifted with N. N is not constant:
// the game's post-processing passes (RadialBlur, BloomPass, CopyPasteBuffer, FullSceneRadialBlur)
// come and go with the gameplay and read the back buffer back in between our composites, which turns
// "slightly too opaque" into flicker that tracks the music.
//
// Present is the only point that is genuinely once per shown frame, and it is also strictly after
// everything the game draws - including the HUD and the screen-space pass that a "first EndScene of
// the frame" latch would have put the overlay underneath.
void draw_overlay_frame(LPDIRECT3DDEVICE9 device)
{
    if(device != g_bound_device || g_ui_draw == nullptr) {
        return;
    }

    if(device->TestCooperativeLevel() != D3D_OK) {
        return;
    }

    // At Present the game has normally left the back buffer bound, but "normally" is not a contract,
    // and a frame that ends with an offscreen target still set would put the whole overlay somewhere
    // nobody ever sees. The scope is what makes retargeting safe to do here: a render target is one
    // of the three states no state block carries, so the one ImGui takes around its own draw could
    // not put this back.
    tw::framework::d3d9::state_scope scope(device);

    if(!is_rendering_to_back_buffer(device)) {
        IDirect3DSurface9* back_buffer = nullptr;
        if(FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer)) || back_buffer == nullptr) {
            return;
        }

        // The scope does not retain `wanted`; the swap chain owns the surface either way.
        scope.render_target(0, back_buffer);
        back_buffer->Release();
    }

    // D3D9 rejects every draw call outside a scene, and Present must not be called inside one - so
    // the overlay opens and closes a scene of its own rather than borrowing the game's.
    g_in_overlay_pass = true;
    if(SUCCEEDED(device->BeginScene())) {
        g_ui_draw(device);
        device->EndScene();
    }
    g_in_overlay_pass = false;
}

long __stdcall hk_end_scene(LPDIRECT3DDEVICE9 p_device)
{
    // Our own scene, closing. Nothing here is about the game's frame.
    if(g_in_overlay_pass) [[unlikely]] {
        return o_end_scene(p_device);
    }

    // The late bind, and in the early load the first bind of all: a device the game created before the
    // plugin was ready is picked up here once it is. Only reached while nothing is bound, so the flag is
    // read a few times at startup rather than per frame.
    if(g_bound_device == nullptr && tw::framework::ready::published()) [[unlikely]] {
        bind_device(p_device);
    }

    // Fallback only, disarmed by the first Present - see g_present_seen. While it is armed this is
    // the old behaviour verbatim, drawing inside the game's scene because there already is one.
    if(!g_present_seen && p_device == g_bound_device && p_device->TestCooperativeLevel() == D3D_OK) {
        if(g_ui_draw != nullptr && is_rendering_to_back_buffer(p_device)) {
            g_in_overlay_pass = true;
            g_ui_draw(p_device);
            g_in_overlay_pass = false;
        }
    }

    return o_end_scene(p_device);
}

// Shared by both present hooks: the first one to fire on the bound device disarms the EndScene
// fallback, and only then is the overlay drawn - so the pass below can never be the thing that
// re-triggers the fallback through its own EndScene.
void on_present(LPDIRECT3DDEVICE9 p_device)
{
    if(p_device != g_bound_device) {
        return;
    }

    if(!g_present_seen) [[unlikely]] {
        g_present_seen = true;
        TW_LOG_INFO("d3d9: first Present on the bound device - overlay now draws once per frame from Present");
    }

    draw_overlay_frame(p_device);
}

long __stdcall hk_present(LPDIRECT3DDEVICE9 p_device, const RECT* source, const RECT* dest, HWND dest_window, const RGNDATA* dirty)
{
    on_present(p_device);

    g_in_present = true;
    const long result = o_present(p_device, source, dest, dest_window, dirty);
    g_in_present = false;

    return result;
}

long __stdcall hk_swap_chain_present(IDirect3DSwapChain9* p_swap_chain,
    const RECT* source,
    const RECT* dest,
    HWND dest_window,
    const RGNDATA* dirty,
    DWORD flags)
{
    // Skipped when this is the runtime's own implementation of the device Present we already handled
    // - see g_in_present. GetDevice costs an AddRef/Release pair once per frame, which is why the
    // device hook above does not go through here.
    if(!g_in_present) {
        LPDIRECT3DDEVICE9 device = nullptr;
        if(SUCCEEDED(p_swap_chain->GetDevice(&device)) && device != nullptr) {
            on_present(device);
            device->Release();
        }
    }

    return o_swap_chain_present(p_swap_chain, source, dest, dest_window, dirty, flags);
}

// Hot path, three of them. Everything below runs per SetTexture / per draw call, i.e. hundreds to
// thousands of times a frame, so each one is a couple of predictable branches over file-local state
// and no COM traffic of its own.

long __stdcall hk_set_texture(LPDIRECT3DDEVICE9 p_device, DWORD stage, IDirect3DBaseTexture9* p_texture)
{
    // Deliberately blind while an interceptor is running, and equally so while the overlay is
    // drawing. The mirror describes what *the game* has bound, and an interceptor is contractually
    // required to put the device back the way it found it - which it does with a state block, and a
    // state block's Apply() does not come through here. Recording the interceptor's own binds would
    // therefore leave the mirror stuck on a texture that is no longer bound, and the next draw of
    // the same object would go unrecognised. The overlay is the same story with a different owner:
    // ImGui binds a font atlas per command and restores nothing through this path either.
    if(stage < k_tracked_stages && p_device == g_bound_device && !g_in_draw_intercept && !g_in_overlay_pass) [[likely]] {
        g_stage_texture[stage] = p_texture;
    }

    return o_set_texture(p_device, stage, p_texture);
}

// True when the interceptor claimed this draw and the game's own call must not run. Shared by both
// draw hooks so the guard/ordering rules live in exactly one place.
bool intercept_draw(LPDIRECT3DDEVICE9 p_device)
{
    // g_in_overlay_pass alongside the re-entrancy guard: every ImGui command is a
    // DrawIndexedPrimitive, and offering those to an interceptor that matches on a raw stage-0
    // texture pointer is how a released sky texture's recycled address turns one of the overlay's
    // own draws into a suppressed draw plus a stray sky pass.
    //
    // The bound device goes first: it is written on this thread only, and until the plugin is ready it is
    // null, which keeps the interceptor pointer - written by the startup thread - unread until then.
    if(p_device != g_bound_device || g_draw_intercept == nullptr || g_in_draw_intercept || g_in_overlay_pass) [[likely]] {
        return false;
    }

    g_in_draw_intercept = true;
    const bool handled = g_draw_intercept(p_device, g_stage_texture[0]);
    g_in_draw_intercept = false;

    return handled;
}

long __stdcall hk_draw_primitive(LPDIRECT3DDEVICE9 p_device, D3DPRIMITIVETYPE primitive_type, UINT start_vertex, UINT primitive_count)
{
    if(intercept_draw(p_device)) {
        return D3D_OK;
    }

    return o_draw_primitive(p_device, primitive_type, start_vertex, primitive_count);
}

long __stdcall hk_draw_indexed_primitive(LPDIRECT3DDEVICE9 p_device,
    D3DPRIMITIVETYPE primitive_type,
    INT base_vertex_index,
    UINT min_vertex_index,
    UINT num_vertices,
    UINT start_index,
    UINT primitive_count)
{
    if(intercept_draw(p_device)) {
        return D3D_OK;
    }

    return o_draw_indexed_primitive(
        p_device, primitive_type, base_vertex_index, min_vertex_index, num_vertices, start_index, primitive_count);
}

// There is deliberately no IDirect3DDevice9::Release hook.
//
// One used to live here, probing the refcount (AddRef() - 1) to spot the game's final Release and
// unbind on it. Measured against the real game, that premise does not survive contact: Quest3D
// churns the device refcount thousands of times per frame (GetDevice()-style borrow/return per
// object), and the live count sits in the 3000+ range. The count never approaches 1 while the game
// runs, so the unbind branch was dead code - and worse, the count *does* dip briefly during a
// fullscreen transition, so the probe's only realistic chance of firing was a spurious unbind at
// exactly the moment the device is most fragile.
//
// It also cost real work: a detour trampoline on the hottest COM path in the process, plus an extra
// AddRef/Release pair on every one of those thousands of calls per frame, for a signal we could not
// use. And it created the very re-entrancy it then needed guarding against - ImGui_ImplDX9_Shutdown
// calls Release, which landed back inside the hook.
//
// Device changes are detected where they are actually visible instead: hk_create_device and
// hk_reset both run bind_device(), which unbinds the previous device before adopting a new one. The
// only case that leaves uncovered is a game that destroys its device and never makes another, which
// happens at process exit, where there is nothing left to tear down.

// Every vtable entry this module detours - off a throwaway device in the late load, off the game's own
// first device in the early one.
struct resolved_entries {
    void* create_device;
    void* reset;
    void* end_scene;
    void* present;
    // Off the device's implicit swap chain rather than the device itself, and the only entry here
    // allowed to stay null: a device that will not hand out a swap chain still gives us every other
    // hook, and the device-level Present covers the case the game actually uses.
    void* swap_chain_present;
    void* set_texture;
    void* draw_primitive;
    void* draw_indexed_primitive;
};

// Everything but create_device, which lives on IDirect3D9 rather than on the device.
void read_device_entries(LPDIRECT3DDEVICE9 device, resolved_entries& out)
{
    void** device_vtable = *reinterpret_cast<void***>(device);

    out.reset = device_vtable[16];
    out.present = device_vtable[17];
    out.end_scene = device_vtable[42];
    out.set_texture = device_vtable[65];
    out.draw_primitive = device_vtable[81];
    out.draw_indexed_primitive = device_vtable[82];

    // IDirect3DSwapChain9: QueryInterface/AddRef/Release, then Present. Hooked as well as the device
    // entry above because the two are alternatives from the game's side and only one of them fires - and
    // a Present nobody sees means an overlay nobody sees, which is a far worse outcome than the
    // double-draw all of this exists to remove. hk_present guards the case where the runtime implements
    // one in terms of the other.
    IDirect3DSwapChain9* swap_chain = nullptr;
    if(SUCCEEDED(device->GetSwapChain(0, &swap_chain)) && swap_chain != nullptr) {
        void** swap_chain_vtable = *reinterpret_cast<void***>(swap_chain);
        out.swap_chain_present = swap_chain_vtable[3];
        swap_chain->Release();
    }
    else {
        TW_LOG_WARNING("d3d9: device has no swap chain - IDirect3DSwapChain9::Present will not be hooked");
    }
}

// Detours every device entry, plus IDirect3D9::CreateDevice when `entries.create_device` is set (the late
// load, where one throwaway device supplied both). Rolls the pointers back on failure.
bool attach_entries(const resolved_entries& entries, tw::framework::detour::suspend threads)
{
    if(entries.create_device != nullptr) {
        o_create_device = reinterpret_cast<create_device_fn>(entries.create_device);
    }
    o_reset = reinterpret_cast<reset_fn>(entries.reset);
    o_end_scene = reinterpret_cast<end_scene_fn>(entries.end_scene);
    o_present = reinterpret_cast<present_fn>(entries.present);
    o_set_texture = reinterpret_cast<set_texture_fn>(entries.set_texture);
    o_draw_primitive = reinterpret_cast<draw_primitive_fn>(entries.draw_primitive);
    o_draw_indexed_primitive = reinterpret_cast<draw_indexed_primitive_fn>(entries.draw_indexed_primitive);

    // CreateDevice last, so the device entries are a prefix whether or not it is part of this install.
    const std::array<tw::framework::detour::binding, 7> bindings { {
        { reinterpret_cast<void**>(&o_reset), reinterpret_cast<void*>(hk_reset) },
        { reinterpret_cast<void**>(&o_end_scene), reinterpret_cast<void*>(hk_end_scene) },
        { reinterpret_cast<void**>(&o_present), reinterpret_cast<void*>(hk_present) },
        { reinterpret_cast<void**>(&o_set_texture), reinterpret_cast<void*>(hk_set_texture) },
        { reinterpret_cast<void**>(&o_draw_primitive), reinterpret_cast<void*>(hk_draw_primitive) },
        { reinterpret_cast<void**>(&o_draw_indexed_primitive), reinterpret_cast<void*>(hk_draw_indexed_primitive) },
        { reinterpret_cast<void**>(&o_create_device), reinterpret_cast<void*>(hk_create_device) },
    } };

    const std::size_t count = entries.create_device != nullptr ? bindings.size() : bindings.size() - 1;
    const bool ok = tw::framework::detour::attach(std::span { bindings.data(), count }, threads);

    if(!ok) {
        if(entries.create_device != nullptr) {
            o_create_device = nullptr;
        }
        o_reset = nullptr;
        o_end_scene = nullptr;
        o_present = nullptr;
        o_set_texture = nullptr;
        o_draw_primitive = nullptr;
        o_draw_indexed_primitive = nullptr;

        return false;
    }

    // Its own transaction, and its own failure handling: the swap chain's Present is a belt to the
    // device Present's braces (see resolved_entries), so losing it costs nothing as long as the game
    // presents through the device - which is the ordinary case. Failing the whole install over it
    // would trade a working overlay for a hypothetical one.
    if(entries.swap_chain_present != nullptr) {
        o_swap_chain_present = reinterpret_cast<swap_chain_present_fn>(entries.swap_chain_present);

        if(!tw::framework::detour::attach(
               {
                   { reinterpret_cast<void**>(&o_swap_chain_present), reinterpret_cast<void*>(hk_swap_chain_present) },
               },
               threads)) {
            TW_LOG_WARNING("d3d9: DetourAttach on IDirect3DSwapChain9::Present failed - relying on the device Present alone");
            TW_BOOT_LOG("d3d9: IDirect3DSwapChain9::Present not hooked - relying on the device Present alone");
            o_swap_chain_present = nullptr;
        }
    }

    return true;
}

// --- early load: the chain off the game's own objects ------------------------------------------------------
//
// Every transaction below leaves the other threads running, deliberately. Each link patches code nobody
// can have run yet: Direct3DCreate9 is hooked from d3d9.dll's own loader notification, so the first
// IDirect3D9 it returns is the first in the process and its CreateDevice has never been called, and the
// first device's methods have never been called before CreateDevice hands that device back. Suspending
// threads, meanwhile, is a real hazard here - the startup thread and the shader compiles it queues
// allocate constantly, and a thread frozen while holding the heap lock deadlocks the game inside Detours'
// own allocation. Same reasoning in dinput8_hooks.cxx.

// Runs inside the game's first successful CreateDevice, on its thread, before the device is returned.
void install_device_hooks_from_game(LPDIRECT3DDEVICE9 device)
{
    resolved_entries entries {};
    read_device_entries(device, entries);

    const bool ok = attach_entries(entries, tw::framework::detour::suspend::none);
    TW_BOOT_LOG("d3d9: device hooks (Reset/Present/EndScene/SetTexture/DrawPrimitive/DrawIndexedPrimitive{}) {} off the game's device",
        o_swap_chain_present != nullptr ? "/SwapChain::Present" : "",
        ok ? "installed" : "FAILED - no overlay and no sky this session");
}

// Initialisation: once or twice per process, from the game's Direct3DCreate9.
IDirect3D9* __stdcall hk_direct3d_create9(UINT sdk_version)
{
    IDirect3D9* d3d = o_direct3d_create9(sdk_version);

    if(d3d != nullptr && !g_create_device_hooked.exchange(true)) {
        o_create_device = reinterpret_cast<create_device_fn>((*reinterpret_cast<void***>(d3d))[16]);

        const bool ok = tw::framework::detour::attach(
            {
                { reinterpret_cast<void**>(&o_create_device), reinterpret_cast<void*>(hk_create_device) },
            },
            tw::framework::detour::suspend::none);

        if(!ok) {
            o_create_device = nullptr;
        }

        TW_BOOT_LOG("d3d9: game Direct3DCreate9 -> {}; IDirect3D9::CreateDevice {}",
            static_cast<void*>(d3d),
            ok ? "hooked" : "FAILED - no overlay and no sky this session");
    }

    return d3d;
}

bool resolve_d3d9_functions(resolved_entries& out)
{
    HMODULE d3d9_module = GetModuleHandle(L"d3d9.dll");
    if(d3d9_module == nullptr) {
        return false;
    }

    auto direct3d_create9 = reinterpret_cast<IDirect3D9*(__stdcall*)(UINT)>(GetProcAddress(d3d9_module, "Direct3DCreate9"));
    if(direct3d_create9 == nullptr) {
        return false;
    }

    WNDCLASSEX window_class {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = DefWindowProc;
    window_class.hInstance = GetModuleHandle(nullptr);
    window_class.lpszClassName = L"TweakerPluginBootstrap";
    RegisterClassEx(&window_class);

    HWND window = CreateWindow(
        window_class.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, window_class.hInstance, nullptr);

    bool resolved = false;

    IDirect3D9* d3d9 = direct3d_create9(D3D_SDK_VERSION);
    if(d3d9 != nullptr) {
        D3DPRESENT_PARAMETERS params {};
        params.Windowed = TRUE;
        params.SwapEffect = D3DSWAPEFFECT_DISCARD;
        params.hDeviceWindow = window;
        params.BackBufferFormat = D3DFMT_UNKNOWN;

        LPDIRECT3DDEVICE9 device = nullptr;
        if(!SUCCEEDED(d3d9->CreateDevice(D3DADAPTER_DEFAULT,
               D3DDEVTYPE_HAL,
               window,
               D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT,
               &params,
               &device))) {
            // Fallback, trying to create a NULLREF device in case when HAL device failed
            // note: HAL device creation may fail when we're injecting in game when it run in fullscreen already
            // note: for some application builds NULLREF device VTable may be different from HAL. But in case of Audiosurf - NULLREF mostly
            // works
            if(!SUCCEEDED(d3d9->CreateDevice(D3DADAPTER_DEFAULT,
                   D3DDEVTYPE_NULLREF,
                   window,
                   D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT,
                   &params,
                   &device))) {
                TW_LOG_CRITICAL("Unable to create a dummy device to resolve D3D9 virtual table. Exiting...");
                resolved = false;
            }
        }

        if(device) {
            void** d3d9_vtable = *reinterpret_cast<void***>(d3d9);
            out.create_device = d3d9_vtable[16];

            read_device_entries(device, out);
            resolved = true;

            device->Release();
        }

        d3d9->Release();
    }

    DestroyWindow(window);
    UnregisterClass(window_class.lpszClassName, window_class.hInstance);

    return resolved;
}

} // namespace

namespace tw::framework::d3d9
{

void attach_ui_plugin(ui_plugin_draw_fn fn)
{
    g_ui_draw = fn;
}

void detach_ui_plugin()
{
    g_ui_draw = nullptr;
}

void attach_device_reset_listener(device_reset_listener_fn pre, device_reset_listener_fn post)
{
    g_reset_listeners.emplace_back(pre, post);
}

void detach_device_reset_listener(device_reset_listener_fn pre, device_reset_listener_fn post)
{
    std::erase(g_reset_listeners, std::pair { pre, post });
}

void attach_device_bind_listener(device_bind_fn on_bind, device_unbind_fn on_unbind)
{
    g_bind_listeners.emplace_back(on_bind, on_unbind);
}

void detach_device_bind_listener(device_bind_fn on_bind, device_unbind_fn on_unbind)
{
    std::erase(g_bind_listeners, std::pair { on_bind, on_unbind });
}

void attach_draw_interceptor(draw_intercept_fn fn)
{
    g_draw_intercept = fn;
}

void detach_draw_interceptor()
{
    g_draw_intercept = nullptr;
}

void initialize() noexcept
{
    // Before framework::ready is published, like every other subscription: from then on the render
    // thread's first bind runs wndproc::install and hub_wndproc starts iterating the subscriber vectors,
    // and a subscribe() landing after that pushes into a std::vector another thread is walking.
    tw::framework::wndproc::subscribe(WM_ACTIVATEAPP, &handle_activate_app);
}

bool install_d3d9_hooks()
{
    resolved_entries entries {};

    if(!resolve_d3d9_functions(entries)) {
        TW_LOG_ERROR("d3d9: could not resolve vtable entries off a bootstrap device - no overlay this session");
        TW_BOOT_LOG("d3d9: no throwaway device - no overlay and no sky this session");
        return false;
    }

    // The chain the early load would build is not wanted here: these entries are the whole install.
    g_create_device_hooked.store(true, std::memory_order_relaxed);
    g_device_hooks_installed.store(true, std::memory_order_relaxed);

    if(!attach_entries(entries, tw::framework::detour::suspend::others)) {
        TW_LOG_ERROR("d3d9: DetourAttach failed - no overlay this session");
        TW_BOOT_LOG("d3d9: DetourAttach FAILED (late) - no overlay and no sky this session");
        return false;
    }

    TW_LOG_INFO("d3d9: hooks installed (CreateDevice/Reset/EndScene/Present/SetTexture/DrawPrimitive/DrawIndexedPrimitive)");
    TW_BOOT_LOG("d3d9: hooks installed off a throwaway device (late){}", o_swap_chain_present != nullptr ? ", SwapChain::Present too" : "");

    return true;
}

bool hook_direct3d_create9_from_loader(HMODULE d3d9) noexcept
{
    o_direct3d_create9 = reinterpret_cast<direct3d_create9_fn>(::GetProcAddress(d3d9, "Direct3DCreate9"));
    if(o_direct3d_create9 == nullptr) {
        return false;
    }

    const bool ok = tw::framework::detour::attach(
        {
            { reinterpret_cast<void**>(&o_direct3d_create9), reinterpret_cast<void*>(hk_direct3d_create9) },
        },
        tw::framework::detour::suspend::none);

    if(!ok) {
        o_direct3d_create9 = nullptr;
    }

    return ok;
}

} // namespace tw::framework::d3d9
