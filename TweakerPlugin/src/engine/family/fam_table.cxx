#include "pch.hxx"

#include "engine/family/fam_table.hxx"

#include "engine/channel_kind.hxx"
#include "engine/channel_vtable.hxx"
#include "engine/family/fam_number.hxx"
#include "engine/family/fam_vector.hxx"

namespace
{
using tw::engine::channel_ref;
using tw::engine::kind;

// The two table types, by their exact type guid. The base guid cannot tell them from a plain Value /
// Value Vector, which every number and vector channel shares, and the member offsets below exist on
// neither parent - reading +0xb0 of a plain Value is reading an unrelated field as a pointer.
constexpr GUID k_array_value_guid = { 0xDF5BF7F7, 0xC204, 0x4F6E, { 0xBD, 0xB8, 0x66, 0x6A, 0x53, 0xDF, 0xCC, 0x58 } };
constexpr GUID k_array_vector_guid = { 0xDD626E09, 0xF497, 0x4A34, { 0x90, 0x32, 0x47, 0xAD, 0x4D, 0x2B, 0xCB, 0xD7 } };

// Where each keeps its ArrayConnectItem*. Different offsets because the two derive from different
// families and their own fields start after different base layouts.
constexpr std::size_t k_array_value_connect = 0xb0;
constexpr std::size_t k_array_vector_connect = 0xa4;

// ArrayConnectItem's vtable, implemented in the `Array Unique` DLL (2346A6DF-..., RVA 0x4138). Not
// exported: found by shape and confirmed against the call sites in both array types.
//
// Only the read-only slots are named. AddRow / InsertRow / RemoveRow live in this same table at
// +0x38 / +0x3c / +0x4c, and GetRowOrCreate at +0x44, and all of them are deliberately absent: a row
// of a game table is an object in the game's model, and nothing here creates or destroys one.
// Naming them is the warning - a wrong offset below lands among them.
constexpr std::size_t k_connect_get_if_valid = 0x00; // GetIfValid -> bool
constexpr std::size_t k_connect_connect = 0x04;      // Connect
constexpr std::size_t k_connect_row_count = 0x34;    // GetRowCount -> int
constexpr std::size_t k_connect_get_row = 0x48;      // GetRow(int) -> RowItem*, null when absent

using connect_bool_fn = bool(__fastcall*)(void*, void*);
using connect_void_fn = void(__fastcall*)(void*, void*);
using connect_int_fn = int(__fastcall*)(void*, void*);
using connect_row_fn = void*(__fastcall*)(void*, void*, int);

// The ArrayConnectItem behind a column, or null. Three gates, in increasing cost, and each one is
// load-bearing:
//
//  1. **the exact type guid**, because the offset is per type and exists on neither parent;
//  2. **the page is committed**, because a column handed over earlier can outlive its group;
//  3. **the slots we call are code** - structural proof that the object behind the pointer is still
//     a live ArrayConnectItem rather than reused memory that happens to be non-null.
void* connect_of(const channel_ref& column) noexcept
{
    if(column.channel == nullptr) {
        return nullptr;
    }

    std::size_t offset = 0;
    if(tw::engine::channel_kind::is_type(column.channel, k_array_value_guid)) {
        offset = k_array_value_connect;
    }
    else if(tw::engine::channel_kind::is_type(column.channel, k_array_vector_guid)) {
        offset = k_array_vector_connect;
    }
    else {
        return nullptr;
    }

    void* connect = *reinterpret_cast<void* const*>(reinterpret_cast<const std::byte*>(column.channel) + offset);
    if(connect == nullptr) {
        return nullptr;
    }

    MEMORY_BASIC_INFORMATION info {};
    if(::VirtualQuery(connect, &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT) {
        return nullptr;
    }

    namespace vt = tw::engine::vtable;
    if(!vt::slot_is_code(connect, k_connect_get_if_valid) || !vt::slot_is_code(connect, k_connect_connect)
        || !vt::slot_is_code(connect, k_connect_row_count) || !vt::slot_is_code(connect, k_connect_get_row)) {
        return nullptr;
    }

    return connect;
}

// Asks the connect item to be usable exactly the way the engine's own setter path does: check, and
// if it says no, connect once and check again. Less would refuse a table nobody has touched yet this
// session; more would be inventing behaviour.
bool connect_ready(void* connect) noexcept
{
    namespace vt = tw::engine::vtable;

    const auto get_if_valid = vt::slot<connect_bool_fn>(connect, k_connect_get_if_valid);
    if(get_if_valid(connect, nullptr)) {
        return true;
    }

    vt::slot<connect_void_fn>(connect, k_connect_connect)(connect, nullptr);

    return get_if_valid(connect, nullptr);
}
} // namespace

namespace tw::engine::fam_table
{
float read(const channel_ref& column, const channel_ref& cursor, float index) noexcept
{
    if(column.family != kind::number || cursor.family != kind::number) [[unlikely]] {
        return 0.f;
    }

    channels::bust_memo(column);

    const float saved = fam_number::get(cursor);
    fam_number::set(cursor, index);

    const float value = fam_number::get(column);

    // Unconditional: the cursor belongs to the game, and a moved one breaks its logic, not ours.
    fam_number::set(cursor, saved);

    return value;
}

bool read_vector(const channel_ref& column, const channel_ref& cursor, float index, float out[3]) noexcept
{
    if(column.family != kind::vector || cursor.family != kind::number || out == nullptr) [[unlikely]] {
        return false;
    }

    // `Stats: TrafficPattern` ships without a CHIC chunk, so it *is* memoised - walking it without
    // this would hand back row 0 for every index in the frame.
    channels::bust_memo(column);

    const float saved = fam_number::get(cursor);
    fam_number::set(cursor, index);

    const bool ok = fam_vector::get(column, out);

    fam_number::set(cursor, saved);

    return ok;
}

int row_count(const channel_ref& column) noexcept
{
    void* connect = connect_of(column);
    if(connect == nullptr || !connect_ready(connect)) {
        return -1;
    }

    return vtable::slot<connect_int_fn>(connect, k_connect_row_count)(connect, nullptr);
}

bool has_row(const channel_ref& column, int row) noexcept
{
    // Before the engine is asked anything: a negative index reaching it is a negative subscript
    // inside it.
    if(row < 0) {
        return false;
    }

    void* connect = connect_of(column);
    if(connect == nullptr || !connect_ready(connect)) {
        return false;
    }

    // GetRow, not GetRowOrCreate. This one answers the question; that one would make the answer yes.
    return vtable::slot<connect_row_fn>(connect, k_connect_get_row)(connect, nullptr, row) != nullptr;
}

bool write(const channel_ref& column, const channel_ref& cursor, float index, float value) noexcept
{
    if(column.family != kind::number || cursor.family != kind::number) [[unlikely]] {
        return false;
    }

    // Truncation to int is what the engine does with the cursor value (ftol right after reading
    // child 0), so the row checked is the row it will use.
    if(!has_row(column, static_cast<int>(index))) {
        return false;
    }

    const float saved = fam_number::get(cursor);
    fam_number::set(cursor, index);

    // Slot 19 on this type is Aco_Array_Value::SetFloat, which reads the cursor itself and writes the
    // row. Its plain store into +0x7c is the side effect the bust below is for.
    fam_number::set(column, value);

    fam_number::set(cursor, saved);

    channels::bust_memo(column);

    return true;
}

bool write_vector(const channel_ref& column, const channel_ref& cursor, float index, float x, float y, float z) noexcept
{
    if(column.family != kind::vector || cursor.family != kind::number) [[unlikely]] {
        return false;
    }

    if(!has_row(column, static_cast<int>(index))) {
        return false;
    }

    const float saved = fam_number::get(cursor);
    fam_number::set(cursor, index);

    fam_vector::set(column, x, y, z);

    fam_number::set(cursor, saved);

    channels::bust_memo(column);

    return true;
}
} // namespace tw::engine::fam_table
