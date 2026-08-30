#include "pch.hxx"

#include "lua/lua_channels.hxx"

#include "plugin/diagnostics.hxx"
#include "plugin/quest3d_state.hxx"

namespace
{
constexpr const char* k_highpoly_module = "HighPoly.dll";

// MSVC mangling of, in order:
//   public: virtual class A3d_ChannelGroup* __thiscall EngineInterface::GetChannelGroup(char const*, int)
//   public: virtual class A3d_ChannelGroup* __thiscall EngineInterface::GetChannelGroup(int)
//   public: virtual int __thiscall EngineInterface::GetChannelGroupCount(void)
//   public: virtual char const* __thiscall A3d_ChannelGroup::GetPoolName(void)
//   public: virtual class A3d_Channel* __thiscall A3d_ChannelGroup::GetChannel(char const*)
//   public: char const* __thiscall A3d_Channel::GetChannelName(void)
constexpr const char* k_get_group_by_name = "?GetChannelGroup@EngineInterface@@UAEPAVA3d_ChannelGroup@@PBDH@Z";
constexpr const char* k_get_group_by_index = "?GetChannelGroup@EngineInterface@@UAEPAVA3d_ChannelGroup@@H@Z";
constexpr const char* k_get_group_count = "?GetChannelGroupCount@EngineInterface@@UAEHXZ";
constexpr const char* k_get_pool_name = "?GetPoolName@A3d_ChannelGroup@@UAEPBDXZ";
constexpr const char* k_get_group_file_name = "?GetChannelGroupFileName@A3d_ChannelGroup@@UAEPBDXZ";
constexpr const char* k_get_channel_by_name = "?GetChannel@A3d_ChannelGroup@@UAEPAVA3d_Channel@@PBD@Z";
constexpr const char* k_get_channel_by_index = "?GetChannel@A3d_ChannelGroup@@UAEPAVA3d_Channel@@H@Z";
constexpr const char* k_get_channel_name = "?GetChannelName@A3d_Channel@@QAEPBDXZ";

// All __thiscall in the game, declared __fastcall here: on x86 the two agree on `this` in ECX and on
// the remaining arguments being pushed right-to-left with the callee cleaning up, so an ignored
// second parameter standing in for EDX makes the signatures interchangeable. Same trick, same
// caveat, as framework/texture_hook.cxx and framework/channel_hook.cxx.
using get_group_by_name_fn = A3d_ChannelGroup*(__fastcall*)(EngineInterface* self, void* edx, const char* name, int instance);
using get_group_by_index_fn = A3d_ChannelGroup*(__fastcall*)(EngineInterface* self, void* edx, int index);
using get_group_count_fn = int(__fastcall*)(EngineInterface* self, void* edx);
using get_pool_name_fn = const char*(__fastcall*)(A3d_ChannelGroup* self, void* edx);
using get_group_file_name_fn = const char*(__fastcall*)(A3d_ChannelGroup* self, void* edx);
using get_channel_by_name_fn = A3d_Channel*(__fastcall*)(A3d_ChannelGroup* self, void* edx, const char* name);
using get_channel_by_index_fn = A3d_Channel*(__fastcall*)(A3d_ChannelGroup* self, void* edx, int index);
using get_channel_name_fn = const char*(__fastcall*)(A3d_Channel* self, void* edx);

get_group_by_name_fn g_get_group_by_name = nullptr;
get_group_by_index_fn g_get_group_by_index = nullptr;
get_group_count_fn g_get_group_count = nullptr;
get_pool_name_fn g_get_pool_name = nullptr;
get_group_file_name_fn g_get_group_file_name = nullptr;
get_channel_by_name_fn g_get_channel_by_name = nullptr;
get_channel_by_index_fn g_get_channel_by_index = nullptr;
get_channel_name_fn g_get_channel_name = nullptr;

bool g_ready = false;

// Slot 17 of the channel vtable - Aco_FloatChannel::GetFloat and every override of it. See
// Docs/Internal/reversing-journal-engine.md §2.2. Called, never detoured, so a plain byte offset
// into the object's vtable is all that is needed.
constexpr std::size_t k_vtable_get_float_offset = 0x44;

// Slot 4 of the base vtable - A3d_Channel::GetChannelType, present on every channel type because it
// belongs to the 17-slot base (reversing-journal-engine.md §2.1).
constexpr std::size_t k_vtable_get_channel_type_offset = 0x10;

// A3d_Channel::channelTypeP_ - the cached ChannelType the engine fills in lazily. GetChannelType()
// allocates and populates it on first call and hands back a copy; reading the pointer avoids the
// copy entirely once it exists.
constexpr std::size_t k_channel_type_ptr_offset = 0x0c;

// Verified against the binary: A3d_Channel::GetChannelType does operator_new(0x84) and copies 0x21
// dwords into it.
constexpr std::size_t k_channel_type_size = 0x84;
constexpr std::size_t k_channel_type_guid_offset = 0x50;
constexpr std::size_t k_channel_type_base_guid_offset = 0x60;

// Aco_StringChannel, from its own vtable dump. Slots 17/18 collide numerically with
// Aco_FloatChannel's GetFloat/GetOldFloat and mean something completely different - which is exactly
// why kind_of() exists.
constexpr std::size_t k_vtable_get_string_offset = 0x44;      // GetString  -> const char*
constexpr std::size_t k_vtable_get_if_use_wchar_offset = 0x5c; // GetIfUseWChar -> bool
constexpr std::size_t k_vtable_get_wstring_offset = 0x60;      // GetWString -> const WCHAR*

// Aco_FloatChannel::SetFloat, slot 19.
constexpr std::size_t k_vtable_set_float_offset = 0x4c;

// Aco_VectorChannel, from its own vtable dump (channels/9D045960-...dll, ??_7Aco_VectorChannel@@6B@
// at RVA 0x2130, cross-referenced against that DLL's exported RVAs):
//
//   17 (+0x44) GetVector -> D3DXVECTOR3    18 (+0x48) SetVector    19 (+0x4c) SetFloat(int, float)
//
// The table ends at slot 20, so a vector channel has exactly three slots of its own. Note that 17
// and 19 are occupied on both this family and the numeric one, with unrelated meanings and - for 19
// - an incompatible signature. See get_vector() in the header.
constexpr std::size_t k_vtable_get_vector_offset = 0x44;

// A3d_Channel::ingoreTreeCountState_ (from the CHIC chunk) and channelCalculatedAtCount_, the two
// fields CheckRenderCount consults. See reversing-journal-engine.md §4.4.
constexpr std::size_t k_ignore_tree_count_offset = 0x60;
constexpr std::size_t k_calculated_at_offset = 0x10;

// The group tree count is a counter that wraps at 30000, so any value outside that range can never
// compare equal to it - which is all it takes to make CheckRenderCount decide the channel is stale.
constexpr std::int32_t k_impossible_tree_count = -1;

using channel_get_float_fn = float(__fastcall*)(A3d_Channel* self, void* edx);
using channel_set_float_fn = void(__fastcall*)(A3d_Channel* self, void* edx, float value);
using channel_get_string_fn = const char*(__fastcall*)(A3d_Channel* self, void* edx);
using channel_get_wstring_fn = const wchar_t*(__fastcall*)(A3d_Channel* self, void* edx);
using channel_get_bool_fn = bool(__fastcall*)(A3d_Channel* self, void* edx);

// Returns a large struct by value: on x86 MSVC that means a hidden buffer pointer passed as the
// first stack argument, which is what the decompiler shows as in_stack_00000004. The same shape
// covers Aco_VectorChannel::GetVector, which returns a 12-byte D3DXVECTOR3 the same way.
using channel_get_type_fn = void*(__fastcall*)(A3d_Channel* self, void* edx, void* out);

// Base guids that identify a channel family. All six come from the SDK headers and were
// cross-checked against the base guid column in channels.lst, which partitions all 226 types the
// same way (reversing-journal-engine.md §3.2).
struct kind_guid {
    const GUID* guid;
    tw::lua::channels::kind value;
};

const kind_guid k_kind_guids[] = {
    { &FLOAT_CHANNEL_GUID, tw::lua::channels::kind::number },
    { &STRING_GUID, tw::lua::channels::kind::text },
    { &VECTOR_GUID, tw::lua::channels::kind::vector },
    { &MATRIX_CHANNEL_GUID, tw::lua::channels::kind::matrix },
    { &DX8_TEXTURE_CHANNEL_GUID, tw::lua::channels::kind::texture },
    { &OBJECTDATA_CHANNEL_GUID, tw::lua::channels::kind::object },
};

tw::lua::channels::kind kind_from_type_record(const std::byte* record) noexcept
{
    for(const kind_guid& entry : k_kind_guids) {
        // Base guid first: it is what says "derives from", and it is what makes Expression Value and
        // Lua Script read as numbers. The own guid is checked too so the root types themselves
        // (Value, Text, ...) classify as their own family.
        if(std::memcmp(record + k_channel_type_base_guid_offset, entry.guid, sizeof(GUID)) == 0
            || std::memcmp(record + k_channel_type_guid_offset, entry.guid, sizeof(GUID)) == 0) {
            return entry.value;
        }
    }

    return tw::lua::channels::kind::other;
}

// Which vtable slot a given kind's accessor will call through. Used only to prove the slot is real
// code before anything calls it.
std::size_t accessor_slot_offset(tw::lua::channels::kind as) noexcept
{
    switch(as) {
    case tw::lua::channels::kind::number:
        return k_vtable_get_float_offset;
    case tw::lua::channels::kind::text:
        return k_vtable_get_string_offset;
    case tw::lua::channels::kind::vector:
        return k_vtable_get_vector_offset;
    default:
        return 0; // no accessor - slot 0 (the destructor) always exists, so this never rejects
    }
}

std::string g_text_buffer;
std::string g_group_buffer;

// True when the address looks like real code in a mapped module. A last line of defence under the
// type check: even a correctly typed channel is only worth calling through if its vtable entry
// points somewhere sane, and getting this wrong once corrupts the process in ways that only show up
// later (a hang inside ntdll at shutdown, for instance, long after the damage was done).
bool is_executable_address(const void* address) noexcept
{
    if(address == nullptr) {
        return false;
    }

    MEMORY_BASIC_INFORMATION info {};
    if(::VirtualQuery(address, &info, sizeof(info)) != sizeof(info)) {
        return false;
    }

    if(info.State != MEM_COMMIT) {
        return false;
    }

    constexpr DWORD k_executable = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

    return (info.Protect & k_executable) != 0;
}

// GetModuleHandleA rather than DetourFindFunction, for the same reason texture_hook.cxx gives: the
// latter falls back to LoadLibrary, and "not mapped yet" is a legitimate answer here, not something
// to force.
void* resolve(const char* module_name, const char* symbol) noexcept
{
    HMODULE module_handle = ::GetModuleHandleA(module_name);
    if(module_handle == nullptr) {
        return nullptr;
    }

    return reinterpret_cast<void*>(::GetProcAddress(module_handle, symbol));
}

// "Environment\Puzzle.cgr" -> "Puzzle". The engine's own GetChannelGroup(const char*) compares
// against the full stored file name, which is whatever path the group was loaded through - so a
// script asking for "Puzzle" would never match it.
std::string_view bare_group_name(const char* file_name) noexcept
{
    if(file_name == nullptr) {
        return {};
    }

    std::string_view view { file_name };

    if(const std::size_t slash = view.find_last_of("\\/"); slash != std::string_view::npos) {
        view.remove_prefix(slash + 1);
    }

    if(const std::size_t dot = view.find_last_of('.'); dot != std::string_view::npos) {
        view = view.substr(0, dot);
    }

    return view;
}

bool equals_ignore_case(std::string_view a, const char* b) noexcept
{
    if(b == nullptr) {
        return false;
    }

    const std::string_view other { b };

    return a.size() == other.size() && ::_strnicmp(a.data(), other.data(), a.size()) == 0;
}

// Fallback scan for find_group(). Groups get addressed two ways in practice and neither is the full
// path the engine stores: by pool name ("StatCollector"), which is what the .cgr cross-group records
// use and what the gameplay journal names throughout, and by the bare file name ("Puzzle"), which is
// what anyone reading that journal will type. Both are accepted.
//
// Cold path - only ever runs on a resolve miss.
A3d_ChannelGroup* find_group_by_scan(EngineInterface* engine, const char* name) noexcept
{
    const int count = g_get_group_count(engine, nullptr);
    for(int i = 0; i < count; ++i) {
        A3d_ChannelGroup* group = g_get_group_by_index(engine, nullptr, i);
        if(group == nullptr) {
            continue;
        }

        const char* pool = g_get_pool_name(group, nullptr);
        if(pool != nullptr && ::_stricmp(pool, name) == 0) {
            return group;
        }

        if(equals_ignore_case(bare_group_name(g_get_group_file_name(group, nullptr)), name)) {
            return group;
        }
    }

    return nullptr;
}
} // namespace

namespace tw::lua::channels
{
bool initialize() noexcept
{
    if(g_ready) {
        return true;
    }

    void* by_name = resolve(k_highpoly_module, k_get_group_by_name);
    void* by_index = resolve(k_highpoly_module, k_get_group_by_index);
    void* group_count = resolve(k_highpoly_module, k_get_group_count);
    void* pool_name = resolve(k_highpoly_module, k_get_pool_name);
    void* group_file_name = resolve(k_highpoly_module, k_get_group_file_name);
    void* channel_by_name = resolve(k_highpoly_module, k_get_channel_by_name);
    void* channel_by_index = resolve(k_highpoly_module, k_get_channel_by_index);
    void* channel_name_fn = resolve(k_highpoly_module, k_get_channel_name);

    if(by_name == nullptr || by_index == nullptr || group_count == nullptr || pool_name == nullptr || group_file_name == nullptr
        || channel_by_name == nullptr || channel_by_index == nullptr || channel_name_fn == nullptr) {
        TW_LOG_WARNING("lua_channels: HighPoly.dll entry points not resolvable yet - graph access unavailable");
        return false;
    }

    g_get_group_by_name = reinterpret_cast<get_group_by_name_fn>(by_name);
    g_get_group_by_index = reinterpret_cast<get_group_by_index_fn>(by_index);
    g_get_group_count = reinterpret_cast<get_group_count_fn>(group_count);
    g_get_pool_name = reinterpret_cast<get_pool_name_fn>(pool_name);
    g_get_group_file_name = reinterpret_cast<get_group_file_name_fn>(group_file_name);
    g_get_channel_by_name = reinterpret_cast<get_channel_by_name_fn>(channel_by_name);
    g_get_channel_by_index = reinterpret_cast<get_channel_by_index_fn>(channel_by_index);
    g_get_channel_name = reinterpret_cast<get_channel_name_fn>(channel_name_fn);

    g_ready = true;
    TW_LOG_INFO("lua_channels: HighPoly.dll graph entry points resolved");

    return true;
}

bool is_ready() noexcept
{
    return g_ready;
}

bool has_engine() noexcept
{
    return tw::plugin::quest3d::g_engine != nullptr;
}

A3d_ChannelGroup* find_group(const char* name) noexcept
{
    if(!g_ready || name == nullptr) {
        return nullptr;
    }

    EngineInterface* engine = tw::plugin::quest3d::g_engine;
    if(engine == nullptr) {
        return nullptr;
    }

    A3d_ChannelGroup* group = g_get_group_by_name(engine, nullptr, name, 0);
    if(group != nullptr) {
        return group;
    }

    return find_group_by_scan(engine, name);
}

int group_count() noexcept
{
    if(!g_ready || tw::plugin::quest3d::g_engine == nullptr) {
        return 0;
    }

    return g_get_group_count(tw::plugin::quest3d::g_engine, nullptr);
}

const char* group_describe(int index) noexcept
{
    g_group_buffer.clear();

    if(!g_ready || tw::plugin::quest3d::g_engine == nullptr) {
        return g_group_buffer.c_str();
    }

    A3d_ChannelGroup* group = g_get_group_by_index(tw::plugin::quest3d::g_engine, nullptr, index);
    if(group == nullptr) {
        return g_group_buffer.c_str();
    }

    const char* pool = g_get_pool_name(group, nullptr);
    const char* file = g_get_group_file_name(group, nullptr);

    g_group_buffer.assign(pool != nullptr ? pool : "?");
    g_group_buffer.append(" | ");
    g_group_buffer.append(file != nullptr ? file : "?");

    return g_group_buffer.c_str();
}

A3d_Channel* find_channel(A3d_ChannelGroup* group, const char* name) noexcept
{
    if(!g_ready || group == nullptr || name == nullptr) {
        return nullptr;
    }

    return g_get_channel_by_name(group, nullptr, name);
}

A3d_Channel* find_channel_at(A3d_ChannelGroup* group, int index) noexcept
{
    if(!g_ready || group == nullptr || index < 0) {
        return nullptr;
    }

    return g_get_channel_by_index(group, nullptr, index);
}

const char* kind_name(kind value) noexcept
{
    switch(value) {
    case kind::number:
        return "float";
    case kind::text:
        return "text";
    case kind::vector:
        return "vector";
    case kind::matrix:
        return "matrix";
    case kind::texture:
        return "texture";
    case kind::object:
        return "object";
    case kind::other:
        return "other";
    default:
        return "unknown";
    }
}

kind kind_of(A3d_Channel* channel) noexcept
{
    if(channel == nullptr) {
        return kind::unknown;
    }

    const std::byte* const* vtable = *reinterpret_cast<const std::byte* const* const*>(channel);
    if(vtable == nullptr) {
        return kind::unknown;
    }

    // Prefer the cached ChannelType the engine already built. Falling back to the virtual call costs
    // a 132-byte stack buffer and a struct-return ABI that is only worth exercising when it has to
    // be; either way this runs once per resolve, never per frame.
    const auto* cached = *reinterpret_cast<const std::byte* const*>(reinterpret_cast<const std::byte*>(channel) + k_channel_type_ptr_offset);
    if(cached != nullptr) {
        return kind_from_type_record(cached);
    }

    void* get_type_slot = const_cast<std::byte*>(vtable[k_vtable_get_channel_type_offset / sizeof(void*)]);
    if(!is_executable_address(get_type_slot)) {
        return kind::unknown;
    }

    alignas(4) std::byte buffer[k_channel_type_size] {};
    reinterpret_cast<channel_get_type_fn>(get_type_slot)(channel, nullptr, buffer);

    return kind_from_type_record(buffer);
}

bool is_callable_as(A3d_Channel* channel, kind as) noexcept
{
    if(channel == nullptr) {
        return false;
    }

    const std::byte* const* vtable = *reinterpret_cast<const std::byte* const* const*>(channel);
    if(vtable == nullptr) {
        return false;
    }

    return is_executable_address(vtable[accessor_slot_offset(as) / sizeof(void*)]);
}

const char* get_text(A3d_Channel* channel) noexcept
{
    g_text_buffer.clear();

    if(channel == nullptr) [[unlikely]] {
        return g_text_buffer.c_str();
    }

    const std::byte* const* vtable = *reinterpret_cast<const std::byte* const* const*>(channel);

    // Text channels keep either a narrow or a wide string and will happily hand back a stale/empty
    // one of the wrong flavour, so ask which mode this channel is in rather than guessing.
    const auto use_wchar = reinterpret_cast<channel_get_bool_fn>(
        const_cast<std::byte*>(vtable[k_vtable_get_if_use_wchar_offset / sizeof(void*)]));

    if(use_wchar(channel, nullptr)) {
        const auto get_wide =
            reinterpret_cast<channel_get_wstring_fn>(const_cast<std::byte*>(vtable[k_vtable_get_wstring_offset / sizeof(void*)]));
        const wchar_t* wide = get_wide(channel, nullptr);
        if(wide == nullptr || *wide == L'\0') {
            return g_text_buffer.c_str();
        }

        // UTF-8 on the way out: this ends up in Lua strings and in ImGui, both of which expect it.
        const int needed = ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
        if(needed > 1) {
            g_text_buffer.resize(static_cast<std::size_t>(needed) - 1);
            ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, g_text_buffer.data(), needed, nullptr, nullptr);
        }

        return g_text_buffer.c_str();
    }

    const auto get_narrow =
        reinterpret_cast<channel_get_string_fn>(const_cast<std::byte*>(vtable[k_vtable_get_string_offset / sizeof(void*)]));
    const char* narrow = get_narrow(channel, nullptr);
    if(narrow != nullptr) {
        g_text_buffer.assign(narrow);
    }

    return g_text_buffer.c_str();
}

float get_float(A3d_Channel* channel) noexcept
{
    if(channel == nullptr) [[unlikely]] {
        return 0.f;
    }

    // Read the channel's own vtable and call slot 17 through it, rather than calling the exported
    // Aco_FloatChannel::GetFloat with an explicit `this`. The distinction matters: the export would
    // run the *base* implementation on every channel, so an Expression Value would return its child
    // instead of its formula. See Docs/Internal/reversing-journal-engine.md §2.5.
    const std::byte* const* vtable = *reinterpret_cast<const std::byte* const* const*>(channel);
    auto fn = reinterpret_cast<channel_get_float_fn>(const_cast<std::byte*>(vtable[k_vtable_get_float_offset / sizeof(void*)]));

    return fn(channel, nullptr);
}

bool get_vector(A3d_Channel* channel, float out[3]) noexcept
{
    if(channel == nullptr || out == nullptr) [[unlikely]] {
        return false;
    }

    const std::byte* const* vtable = *reinterpret_cast<const std::byte* const* const*>(channel);
    if(vtable == nullptr) [[unlikely]] {
        return false;
    }

    // Hidden-buffer struct return: the destination is passed as the first *stack* argument, after
    // ecx/edx. The buffer is three floats because D3DXVECTOR3 is three floats - and it is written
    // by the callee, so it must be real storage, not `out` reinterpreted (out is already exactly
    // that shape, but keeping the call's ABI buffer separate means a future caller passing something
    // narrower cannot be corrupted by it).
    alignas(4) float buffer[3] { 0.f, 0.f, 0.f };
    auto fn = reinterpret_cast<channel_get_type_fn>(const_cast<std::byte*>(vtable[k_vtable_get_vector_offset / sizeof(void*)]));
    fn(channel, nullptr, buffer);

    out[0] = buffer[0];
    out[1] = buffer[1];
    out[2] = buffer[2];

    return true;
}

void set_float(A3d_Channel* channel, float value) noexcept
{
    if(channel == nullptr) [[unlikely]] {
        return;
    }

    const std::byte* const* vtable = *reinterpret_cast<const std::byte* const* const*>(channel);
    auto fn = reinterpret_cast<channel_set_float_fn>(const_cast<std::byte*>(vtable[k_vtable_set_float_offset / sizeof(void*)]));

    fn(channel, nullptr, value);
}

bool read_array_vector(A3d_Channel* array_vector, A3d_Channel* indexer, float index, float out[3]) noexcept
{
    if(array_vector == nullptr || indexer == nullptr || out == nullptr) [[unlikely]] {
        return false;
    }

    // `Stats: TrafficPattern` ships without a CHIC chunk, so it *is* memoised - walking it without
    // this would hand back row 0 for every index in the frame. See read_array below.
    auto* raw = reinterpret_cast<std::byte*>(array_vector);
    if(*reinterpret_cast<const std::uint8_t*>(raw + k_ignore_tree_count_offset) == 0) {
        *reinterpret_cast<std::int32_t*>(raw + k_calculated_at_offset) = k_impossible_tree_count;
    }

    const float saved = get_float(indexer);
    set_float(indexer, index);

    const bool ok = get_vector(array_vector, out);

    set_float(indexer, saved);

    return ok;
}

float read_array(A3d_Channel* array_value, A3d_Channel* indexer, float index) noexcept
{
    if(array_value == nullptr || indexer == nullptr) [[unlikely]] {
        return 0.f;
    }

    // Most Array Value channels ship with CHIC = 1 (400 of 431), meaning "ignore tree count" - they
    // are not memoised at all, which is exactly why the game's own ForLoop can walk a column in a
    // single frame. For the handful that are memoised, the second read of a frame would otherwise
    // return the first index's value, so the memo is invalidated by hand.
    auto* raw = reinterpret_cast<std::byte*>(array_value);
    if(*reinterpret_cast<const std::uint8_t*>(raw + k_ignore_tree_count_offset) == 0) {
        *reinterpret_cast<std::int32_t*>(raw + k_calculated_at_offset) = k_impossible_tree_count;
    }

    const float saved = get_float(indexer);
    set_float(indexer, index);

    const float value = get_float(array_value);

    // Unconditional: the cursor belongs to the game, and a moved one breaks its logic, not ours.
    set_float(indexer, saved);

    return value;
}

const char* channel_name(A3d_Channel* channel) noexcept
{
    if(!g_ready || channel == nullptr) {
        return nullptr;
    }

    return g_get_channel_name(channel, nullptr);
}
} // namespace tw::lua::channels
