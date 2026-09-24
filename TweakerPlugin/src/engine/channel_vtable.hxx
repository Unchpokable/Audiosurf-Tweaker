#pragma once

// Vtable plumbing for Quest3D channels, and nothing else.
//
// **What this file deliberately does not know: what lives in any slot.** Slot 17 is GetFloat on a
// number, GetString on text, GetVector on a vector, GetMatrix on a matrix, InvalidateDeviceObjects
// on a texture and DrawSurfaces on a 3D Object (reversing-journal-boot.md §7.1). A shared header
// that named "slot 17" would be a shared header that is wrong for five families out of six. So the
// meaning of each slot lives in the family file that owns it - src/engine/family/fam_*.cxx - and
// this file only turns an offset into a callable pointer.
//
// The one thing it does know is the calling convention, because that is the same for every family:
// Quest3D's methods are `__thiscall`, declared here as `__fastcall` with a dead second parameter.
// On x86 MSVC the two agree on `this` in ecx and on the callee cleaning the stack, so the edx slot is
// written by us and ignored by the callee. Same substitution as framework/texture_hook.cxx and
// engine/engine_control.cxx. It holds for by-value struct arguments too - SetVector's 12 bytes and
// SetMatrix's 64 land on the stack exactly where the engine's own callers put them, which is checked
// offline in harness/lua/shimtest rather than assumed.
//
// **Not really Quest3D-specific, and meant to move.** of()/slot()/is_code() work on any object with
// one vptr at +0x00 - fam_table already uses them on a connect item that is not a channel - and
// framework/d3d9_hooks.cxx, dinput8_hooks.cxx and channel_shim.cxx all do the same vtable reads by
// hand. Generalising this into framework/ (COM method indices alongside byte offsets, and without the
// __thiscall note above, which is false for COM's __stdcall) is recorded as a follow-up in
// Docs/Internal/lua-engine-fix-roadmap.md §17.1.
namespace tw::engine::vtable
{
// The object's vtable. Every Quest3D channel is single-inheritance with one vptr at +0x00 (engine
// journal §2.1), which is the whole reason this is one load.
[[nodiscard]] inline const void* const* of(const void* object) noexcept
{
    return *static_cast<const void* const* const*>(object);
}

// The entry at a byte offset into the object's vtable, as a function pointer of the caller's choosing.
// Byte offsets rather than slot indices on purpose: every disassembly, every journal and every vtdump
// line names them as `+0x44`, and a slot number is one more conversion to get wrong.
template<typename TFn>
[[nodiscard]] TFn slot(const void* object, std::size_t byte_offset) noexcept
{
    return reinterpret_cast<TFn>(const_cast<void*>(of(object)[byte_offset / sizeof(void*)]));
}

// Whether an address is committed, executable memory. The last line of defence under a type check:
// a correctly typed channel is only worth calling through if the slot points at real code, and
// getting that wrong once corrupts the process in ways that surface much later (a hang in ntdll at
// shutdown, long after the damage). Cold path - resolve time only.
[[nodiscard]] bool is_code(const void* address) noexcept;

// is_code() on the entry at that byte offset of the object's vtable. False for a null object or a
// null vtable.
[[nodiscard]] bool slot_is_code(const void* object, std::size_t byte_offset) noexcept;
} // namespace tw::engine::vtable
