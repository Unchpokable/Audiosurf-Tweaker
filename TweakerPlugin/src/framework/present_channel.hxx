#pragma once

// Interception of Quest3D's Present channel, Aco_DX8_PresentChannel - the one place in the game that
// ends a shown frame.
//
// Its CallChannel closes the frame's last scene (IDirect3DDevice9::EndScene) and presents it straight
// after (IDirect3DDevice9::Present), and nothing the game draws comes later. Sampled on the live game: no
// other caller of Present ever showed up on the render thread's stack
// (Docs/Internal/reversing-journal-render.md §11).
//
// Why the overlay hangs off this and not off d3d9 itself: anything hooked in d3d9 is shared with every
// other program that hooks d3d9, and those programs do not queue up politely. Steam's overlay follows a
// jmp it finds at the start of IDirect3DDevice9::Present to the first real code and patches that - which
// was our hk_present - so its screenshot and recording hook ran before our overlay was drawn, and every
// capture came out without it. Code inside the game's own channel DLL is something no general-purpose
// overlay knows about or touches, so whatever is drawn from here is in the back buffer before any of
// them sees it.
namespace tw::framework::present_channel
{
// Both run on the render thread, once per call of the channel: `before` ahead of the channel's own code,
// `after` once it has returned, i.e. after the frame was presented (or deliberately not - the channel
// skips Present while the window is invisible, and on the frame it resets the device after a resize).
using channel_fn = void (*)();

// Registration is additive and permanent, like texture_hook's. Must happen before framework::ready is
// published: until then the hook forwards untouched, and from then on the list is read without a lock.
void subscribe(channel_fn before, channel_fn after) noexcept;

// Late load (injected), and the retry on the first device bind. Detours CallChannel with the other threads
// suspended. Fails quietly when the channel DLL is not mapped yet; a successful install is idempotent.
bool install() noexcept;

// Early load (engine\channels\), from the channel DLL's own loader notification: under the loader lock,
// before anyone has called into it - no thread suspension, lifecycle log only. The caller pins the module
// first (plugin-offline-mode.md, Р-25).
bool install_from_loader(HMODULE channel_module) noexcept;

[[nodiscard]] bool is_installed() noexcept;
} // namespace tw::framework::present_channel
