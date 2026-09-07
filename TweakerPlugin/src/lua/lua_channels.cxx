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
constexpr std::size_t k_vtable_set_vector_offset = 0x48;

// Aco_Array_Value and Aco_Array_Vector, from their own vtable dumps (engine journal §2.2.3). Both
// override the setter their family already had - slot 19 on the numeric one, slot 18 on the vector
// one - so writing a table cell needs no new thunk: set_float and set_vector already call exactly
// the right slot with exactly the right signature.
//
// What these types DO need is their own identity check. The offsets below exist only on them, and
// the base guid cannot tell them apart from a plain Value / Value Vector, which every numeric and
// vector channel in the game shares. So the exact type guid is what gates access here, not the kind.
constexpr GUID k_array_value_guid = { 0xDF5BF7F7, 0xC204, 0x4F6E, { 0xBD, 0xB8, 0x66, 0x6A, 0x53, 0xDF, 0xCC, 0x58 } };
constexpr GUID k_array_vector_guid = { 0xDD626E09, 0xF497, 0x4A34, { 0x90, 0x32, 0x47, 0xAD, 0x4D, 0x2B, 0xCB, 0xD7 } };

// Where each keeps its ArrayConnectItem*. Different offsets because the two derive from different
// families and their own fields start after different base layouts.
constexpr std::size_t k_array_value_connect_offset = 0xb0;
constexpr std::size_t k_array_vector_connect_offset = 0xa4;

// ArrayConnectItem's vtable, implemented in the `Array Unique` DLL (2346A6DF-..., RVA 0x4138). Not
// exported, so it was found by shape and confirmed against the call sites in both array types.
//
// Only the read-only slots are named. AddRow / InsertRow / RemoveRow live in this same table at
// +0x38 / +0x3c / +0x4c and are deliberately absent: a row of a game table is an object in the
// game's model, not a slot in an array, and nothing in the scripting layer has business creating or
// destroying one. Naming them here is the warning - a wrong slot offset lands among them.
constexpr std::size_t k_connect_get_if_valid_offset = 0x00; // GetIfValid -> bool
constexpr std::size_t k_connect_connect_offset = 0x04;      // Connect
constexpr std::size_t k_connect_get_row_count_offset = 0x34;
constexpr std::size_t k_connect_get_row_offset = 0x48; // GetRow(int) -> RowItem*, null when absent

using connect_get_bool_fn = bool(__fastcall*)(void* self, void* edx);
using connect_void_fn = void(__fastcall*)(void* self, void* edx);
using connect_get_int_fn = int(__fastcall*)(void* self, void* edx);
using connect_get_row_fn = void*(__fastcall*)(void* self, void* edx, int row);

// A3d_Channel::ingoreTreeCountState_ (from the CHIC chunk) and channelCalculatedAtCount_, the two
// fields CheckRenderCount consults. See reversing-journal-engine.md §4.4.
constexpr std::size_t k_ignore_tree_count_offset = 0x60;
constexpr std::size_t k_calculated_at_offset = 0x10;

// The group tree count is a counter that wraps at 30000, so any value outside that range can never
// compare equal to it - which is all it takes to make CheckRenderCount decide the channel is stale.
constexpr std::int32_t k_impossible_tree_count = -1;

using channel_get_float_fn = float(__fastcall*)(A3d_Channel* self, void* edx);
using channel_set_float_fn = void(__fastcall*)(A3d_Channel* self, void* edx, float value);

// Aco_VectorChannel::SetVector takes D3DXVECTOR3 BY VALUE. On x86 MSVC a trivially-copyable
// 12-byte struct is pushed as three consecutive dwords, so three float parameters have exactly the
// same stack layout and no struct type is needed here. This is read off the shipped binary, not
// assumed: SetVector opens with `mov edx,[esp+8]` and then loads [esp+4] and [esp+0xC], i.e. it
// addresses the three components individually right where three float arguments would land.
//
// __fastcall puts the first integer argument in ecx (this) and the second in edx; floats never go
// in registers, so x/y/z land on the stack in order. That is byte-identical to __thiscall with a
// by-value vector - the same idiom already used for every other accessor in this file.
using channel_set_vector_fn = void(__fastcall*)(A3d_Channel* self, void* edx, float x, float y, float z);
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

// The channel's ChannelType record: the engine's cached copy when it exists, otherwise the virtual
// call filling the caller's buffer. Returns null when neither is available.
//
// Split out because two callers need it for different reasons - kind_of() wants the base guid to
// classify the family, array_connect() wants the exact guid to identify one specific type - and the
// cached-or-call dance is the same either way.
const std::byte* type_record(A3d_Channel* channel, std::byte (&buffer)[k_channel_type_size]) noexcept
{
    const std::byte* const* vtable = *reinterpret_cast<const std::byte* const* const*>(channel);
    if(vtable == nullptr) {
        return nullptr;
    }

    const auto* cached
        = *reinterpret_cast<const std::byte* const*>(reinterpret_cast<const std::byte*>(channel) + k_channel_type_ptr_offset);
    if(cached != nullptr) {
        return cached;
    }

    void* get_type_slot = const_cast<std::byte*>(vtable[k_vtable_get_channel_type_offset / sizeof(void*)]);
    if(!is_executable_address(get_type_slot)) {
        return nullptr;
    }

    reinterpret_cast<channel_get_type_fn>(get_type_slot)(channel, nullptr, buffer);

    return buffer;
}

// Invalidates a channel's per-frame memo, but only when it has one. Most Array Value channels ship
// with CHIC = 1 ("ignore tree count") and are never memoised; for the rest, a stale
// channelCalculatedAtCount_ is what makes the engine hand back the previous row.
void bust_memo(A3d_Channel* channel) noexcept
{
    auto* raw = reinterpret_cast<std::byte*>(channel);
    if(*reinterpret_cast<const std::uint8_t*>(raw + k_ignore_tree_count_offset) == 0) {
        *reinterpret_cast<std::int32_t*>(raw + k_calculated_at_offset) = k_impossible_tree_count;
    }
}

// The ArrayConnectItem behind an Array Value / Array Vector column, or null for anything else.
//
// Three gates, in increasing cost, and each one is load-bearing:
//
//  1. **The exact type guid.** The member offset differs per type and exists on neither of their
//     parents, so reading it off a plain Value would be reading whatever happens to live at +0xb0
//     in an unrelated object. Base guid is not enough here - that is the whole point.
//  2. **The page is committed.** Groups unload; a channel we were handed earlier can be gone, and
//     the pointer inside it stale. Same guard channel_shim::restore uses before writing a vptr back.
//  3. **The slots we will call are code.** Structural proof that the object behind the pointer is
//     still a live ArrayConnectItem rather than reused memory that happens to be non-null.
void* array_connect(A3d_Channel* column) noexcept
{
    if(column == nullptr) {
        return nullptr;
    }

    alignas(4) std::byte buffer[k_channel_type_size] {};
    const std::byte* record = type_record(column, buffer);
    if(record == nullptr) {
        return nullptr;
    }

    std::size_t offset = 0;
    if(std::memcmp(record + k_channel_type_guid_offset, &k_array_value_guid, sizeof(GUID)) == 0) {
        offset = k_array_value_connect_offset;
    }
    else if(std::memcmp(record + k_channel_type_guid_offset, &k_array_vector_guid, sizeof(GUID)) == 0) {
        offset = k_array_vector_connect_offset;
    }
    else {
        return nullptr;
    }

    void* connect = *reinterpret_cast<void* const*>(reinterpret_cast<const std::byte*>(column) + offset);
    if(connect == nullptr) {
        return nullptr;
    }

    MEMORY_BASIC_INFORMATION info {};
    if(::VirtualQuery(connect, &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT) {
        return nullptr;
    }

    const std::byte* const* vtable = *reinterpret_cast<const std::byte* const* const*>(connect);
    if(vtable == nullptr) {
        return nullptr;
    }

    if(!is_executable_address(vtable[k_connect_get_if_valid_offset / sizeof(void*)])
        || !is_executable_address(vtable[k_connect_connect_offset / sizeof(void*)])
        || !is_executable_address(vtable[k_connect_get_row_count_offset / sizeof(void*)])
        || !is_executable_address(vtable[k_connect_get_row_offset / sizeof(void*)])) {
        return nullptr;
    }

    return connect;
}

// Asks the connect item to be usable, exactly the way the engine's own setter path asks: check, and
// if it says no, connect once and check again. Doing less would refuse legitimate writes to a table
// that simply has not been touched yet this session; doing more would be inventing behaviour.
bool connect_ensure_valid(void* connect) noexcept
{
    const std::byte* const* vtable = *reinterpret_cast<const std::byte* const* const*>(connect);

    auto get_if_valid
        = reinterpret_cast<connect_get_bool_fn>(const_cast<std::byte*>(vtable[k_connect_get_if_valid_offset / sizeof(void*)]));
    if(get_if_valid(connect, nullptr)) {
        return true;
    }

    reinterpret_cast<connect_void_fn>(const_cast<std::byte*>(vtable[k_connect_connect_offset / sizeof(void*)]))(connect, nullptr);

    return get_if_valid(connect, nullptr);
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

    // Prefer the cached ChannelType the engine already built. Falling back to the virtual call costs
    // a 132-byte stack buffer and a struct-return ABI that is only worth exercising when it has to
    // be; either way this runs once per resolve, never per frame.
    alignas(4) std::byte buffer[k_channel_type_size] {};
    const std::byte* record = type_record(channel, buffer);
    if(record == nullptr) {
        return kind::unknown;
    }

    return kind_from_type_record(record);
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

void set_vector(A3d_Channel* channel, float x, float y, float z) noexcept
{
    if(channel == nullptr) [[unlikely]] {
        return;
    }

    // No re-check of the family here, exactly as in set_float, and the reason is worth stating
    // because slot 19's signature clash makes it look reckless: the kind was established at resolve
    // from the channel's own ChannelType, so a handle that reaches this function is an
    // Aco_VectorChannel derivative by construction, and every such type has slots 17-19. A vtable
    // sane enough to have passed is_callable_as on slot 17 is the same table these live in.
    const std::byte* const* vtable = *reinterpret_cast<const std::byte* const* const*>(channel);
    auto fn = reinterpret_cast<channel_set_vector_fn>(const_cast<std::byte*>(vtable[k_vtable_set_vector_offset / sizeof(void*)]));

    fn(channel, nullptr, x, y, z);
}

bool read_array_vector(A3d_Channel* array_vector, A3d_Channel* indexer, float index, float out[3]) noexcept
{
    if(array_vector == nullptr || indexer == nullptr || out == nullptr) [[unlikely]] {
        return false;
    }

    // `Stats: TrafficPattern` ships without a CHIC chunk, so it *is* memoised - walking it without
    // this would hand back row 0 for every index in the frame. See read_array below.
    bust_memo(array_vector);

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
    bust_memo(array_value);

    const float saved = get_float(indexer);
    set_float(indexer, index);

    const float value = get_float(array_value);

    // Unconditional: the cursor belongs to the game, and a moved one breaks its logic, not ours.
    set_float(indexer, saved);

    return value;
}

int array_row_count(A3d_Channel* column) noexcept
{
    void* connect = array_connect(column);
    if(connect == nullptr || !connect_ensure_valid(connect)) {
        return -1;
    }

    const std::byte* const* vtable = *reinterpret_cast<const std::byte* const* const*>(connect);
    auto fn = reinterpret_cast<connect_get_int_fn>(const_cast<std::byte*>(vtable[k_connect_get_row_count_offset / sizeof(void*)]));

    return fn(connect, nullptr);
}

bool array_has_row(A3d_Channel* column, int row) noexcept
{
    if(row < 0) {
        return false;
    }

    void* connect = array_connect(column);
    if(connect == nullptr || !connect_ensure_valid(connect)) {
        return false;
    }

    // GetRow, not GetRowOrCreate - they sit next to each other in the vtable (+0x48 and +0x44) and
    // differ in exactly the way that matters here. This one answers the question; that one would
    // silently make the answer yes.
    const std::byte* const* vtable = *reinterpret_cast<const std::byte* const* const*>(connect);
    auto fn = reinterpret_cast<connect_get_row_fn>(const_cast<std::byte*>(vtable[k_connect_get_row_offset / sizeof(void*)]));

    return fn(connect, nullptr, row) != nullptr;
}

bool write_array(A3d_Channel* array_value, A3d_Channel* indexer, float index, float value) noexcept
{
    if(array_value == nullptr || indexer == nullptr) [[unlikely]] {
        return false;
    }

    // Truncation to int is what the engine itself does with the cursor value (ftol right after
    // reading child 0), so the row we check is the row it will use, not one we rounded differently.
    const int row = static_cast<int>(index);
    if(!array_has_row(array_value, row)) {
        return false;
    }

    const float saved = get_float(indexer);
    set_float(indexer, index);

    // Slot 19 on this type is Aco_Array_Value::SetFloat, which reads the cursor itself and writes the
    // row. The plain numeric store it also performs into +0x7c is the side effect the bust below is
    // for, not the effect we are after.
    set_float(array_value, value);

    set_float(indexer, saved);

    bust_memo(array_value);

    return true;
}

bool write_array_vector(A3d_Channel* array_vector, A3d_Channel* indexer, float index, float x, float y, float z) noexcept
{
    if(array_vector == nullptr || indexer == nullptr) [[unlikely]] {
        return false;
    }

    const int row = static_cast<int>(index);
    if(!array_has_row(array_vector, row)) {
        return false;
    }

    const float saved = get_float(indexer);
    set_float(indexer, index);

    set_vector(array_vector, x, y, z);

    set_float(indexer, saved);

    bust_memo(array_vector);

    return true;
}

const char* channel_name(A3d_Channel* channel) noexcept
{
    if(!g_ready || channel == nullptr) {
        return nullptr;
    }

    return g_get_channel_name(channel, nullptr);
}
} // namespace tw::lua::channels
