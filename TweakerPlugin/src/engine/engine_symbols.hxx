#pragma once

// Entry points resolved out of HighPoly.dll by name.
//
// Everything here is an **export**, so it is taken with GetProcAddress and not by scanning or by
// walking a vtable. Where an exported method is called directly rather than through the object's
// vtable, that is legitimate only because the type is known by construction - the rule in
// Docs/Internal/reversing-journal-engine.md §2.5. For EngineControl the type is not merely assumed:
// engine_control verifies the object's vptr against the exported `??_7EngineControl@@6B@` before it
// calls anything on it, and engine_groups does the same for EngineInterfaceExt.
//
// Addresses and the reasoning behind each one: Docs/Internal/reversing-journal-boot.md §8.
//
// **This is the only GetProcAddress site for HighPoly.** Until Ф3 of
// Docs/Internal/lua-engine-fix-roadmap.md src/lua/lua_channels.cxx kept a resolver of its own; it
// went with the file. A symbol the plugin needs from HighPoly is added here and nowhere else.
namespace tw::engine
{
// HighPoly's EngineControl - the object QuestViewer.exe drives the frame through (boot journal §1.1).
// Opaque on purpose: the only things done with it are passing it back to the original EngineLoop and
// reading its vptr for the identity check. No field of it is ever touched.
class engine_control_handle;

// HighPoly's EngineInterfaceExt - the object EngineInterface keeps at +0x40 and forwards most of its
// own accessors to (boot journal §1.3). Opaque for the same reason: it is only ever passed back to
// one of its own exported methods, and only after its vptr has been matched against the exported
// `??_7EngineInterfaceExt@@6B@`.
class engine_interface_ext_handle;
} // namespace tw::engine

namespace tw::engine::symbols
{
// __fastcall stands in for __thiscall, the same substitution framework/texture_hook.cxx makes. It is
// correct for exactly these signatures because none of them takes a stack argument in a register:
// `this` arrives in ecx either way, the edx slot is written by the caller and ignored by the callee,
// and both conventions return with a bare `ret`.
// The first parameter of each is `this`, the second the dead edx slot. Left unnamed because
// clang-format cannot tell a pointer declarator from a multiplication when the type is only
// forward-declared, and turns `engine_control_handle* self` into `engine_control_handle * self`.
using engine_loop_fn = void(__fastcall*)(engine_control_handle*, void*);
using get_engine_interface_fn = EngineInterface*(__fastcall*)(engine_control_handle*, void*);
using get_tree_count_fn = int(__fastcall*)(EngineInterface*, void*);

using get_group_count_fn = int(__fastcall*)(EngineInterface*, void*);
using get_group_at_fn = A3d_ChannelGroup*(__fastcall*)(EngineInterface*, void*, int);
using delete_group_fn = void(__fastcall*)(EngineInterface*, void*, int);
using get_start_group_fn = int(__fastcall*)(engine_interface_ext_handle*, void*);
using group_release_fn = void(__fastcall*)(A3d_ChannelGroup*, void*);
using group_name_fn = const char*(__fastcall*)(A3d_ChannelGroup*, void*);
using group_index_fn = int(__fastcall*)(A3d_ChannelGroup*, void*);
using group_tree_count_fn = int(__fastcall*)(A3d_ChannelGroup*, void*);
using channel_by_name_fn = A3d_Channel*(__fastcall*)(A3d_ChannelGroup*, void*, const char*);
using channel_by_index_fn = A3d_Channel*(__fastcall*)(A3d_ChannelGroup*, void*, int);
using channel_name_fn = const char*(__fastcall*)(A3d_Channel*, void*);

// The frame spine. Without all four of these there is no per-frame entry to the graph at all, so a
// missing one disables the whole engine layer.
struct table {
    // ?EngineLoop@EngineControl@@UAEXXZ - one call per frame from QuestViewer's message pump, and
    // the only place the whole channel graph is entered from.
    void* engine_loop = nullptr;

    // ??_7EngineControl@@6B@ - the class's vtable, exported by name. This is what makes "is the
    // object the main loop drives really an EngineControl" a check rather than a belief.
    const void* engine_control_vtable = nullptr;

    // ?GetEngineInterface@EngineControl@@UAEPAVEngineInterface@@XZ
    void* get_engine_interface = nullptr;

    // ?GetTreeCalculateCount@EngineInterface@@UAEHXZ - the graph's frame counter. Rings at 30000
    // (boot journal §1.2), so it may only ever be compared for equality.
    void* get_tree_calculate_count = nullptr;
};

// Everything the group registry needs. **Resolved separately and allowed to fail**, which is not
// tidiness: the four symbols above were confirmed on a running game in Ф1, these were read out of a
// disassembler and had not been. Folding them into one all-or-nothing list would mean one absent
// export costs the plugin its frame spine - trading a feature that degrades for one that does not
// come up at all. `groups_ready()` says which world we are in.
struct group_table {
    // ?GetChannelGroupCount@EngineInterface@@UAEHXZ - one call, and the cheapest "did anything load
    // or unload" the engine offers (boot journal §6.3).
    void* get_group_count = nullptr;

    // ?GetChannelGroup@EngineInterface@@UAEPAVA3d_ChannelGroup@@H@Z
    void* get_group_at = nullptr;

    // ?GetChannelGroupFileName@A3d_ChannelGroup@@UAEPBDXZ / ?GetPoolName@...
    void* get_group_file_name = nullptr;
    void* get_pool_name = nullptr;

    // ?GetGroupIndex@A3d_ChannelGroup@@UAEHXZ
    void* get_group_index = nullptr;

    // ??_7EngineInterfaceExt@@6B@ - the identity check for the pointer at EngineInterface+0x40.
    const void* engine_interface_ext_vtable = nullptr;

    // ?GetStartGroup@EngineInterfaceExt@@UAEHXZ - the index of the group EngineLoop calls, i.e. the
    // game's own answer to "how far has it come up" (boot journal §2.4).
    void* get_start_group = nullptr;

    // ?Release@A3d_ChannelGroup@@UAEXXZ - detoured: the wide path every group destruction goes
    // through. ?DeleteChannelGroup@EngineInterface@@UAEXH@Z - detoured: the narrow `Remove Group`
    // path, which is where the pool name and index are still known (boot journal §6.2).
    void* group_release = nullptr;
    void* delete_channel_group = nullptr;

    // ?GetTreeCalculateCount@A3d_ChannelGroup@@QAEHXZ - the engine's frame counter plus the group's
    // own, i.e. the number a channel of this group writes into +0x10 when it is evaluated (boot
    // journal §3.1). Non-virtual. What channel_ref's live() compares against.
    void* group_tree_count = nullptr;

    // Channels inside a group. These three used to be resolved by lua/lua_channels.cxx on its own,
    // which made it the second GetProcAddress site for HighPoly; they moved here in Ф3.
    //
    // ?GetChannel@A3d_ChannelGroup@@UAEPAVA3d_Channel@@PBD@Z - by name, a linear _stricmp scan over
    // the whole group, and names are NOT unique in a group (TrafficCommander has two "TrafficType").
    // ?GetChannel@A3d_ChannelGroup@@UAEPAVA3d_Channel@@H@Z - by index, exact.
    // ?GetChannelName@A3d_Channel@@QAEPBDXZ - one instruction, reads +0x50.
    void* channel_by_name = nullptr;
    void* channel_by_index = nullptr;
    void* channel_name = nullptr;
};

// Safe to call repeatedly; only the first call does work. False means HighPoly.dll is not mapped yet
// or an expected export is missing - `missing()` says which. Resolves both tables; the return value
// only reports the spine, because that is the part nothing can work without.
bool initialize() noexcept;

[[nodiscard]] bool is_ready() noexcept;

// Whether the group tier resolved too. False leaves the spine working and the group registry out.
[[nodiscard]] bool groups_ready() noexcept;

// Zeroed until initialize() has succeeded.
[[nodiscard]] const table& get() noexcept;
[[nodiscard]] const group_table& groups() noexcept;

// The first symbol that failed to resolve, or an empty string. Module-owned, valid for the process.
[[nodiscard]] std::string_view missing() noexcept;
} // namespace tw::engine::symbols
