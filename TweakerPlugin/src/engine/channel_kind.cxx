#include "pch.hxx"

#include "engine/channel_kind.hxx"

#include "engine/channel_vtable.hxx"
#include "engine/family/fam_matrix.hxx"
#include "engine/family/fam_number.hxx"
#include "engine/family/fam_text.hxx"
#include "engine/family/fam_vector.hxx"

namespace
{
using tw::engine::kind;

// Slot 4 (+0x10) of the 17-slot A3d_Channel base - GetChannelType, present on every channel type
// because it belongs to the base (engine journal §2.1). Not a family slot, which is why it lives
// here and not in a fam_* file.
constexpr std::size_t k_vtable_get_channel_type_offset = 0x10;

// A3d_Channel::channelTypeP_ - the ChannelType the engine fills in lazily on first GetChannelType()
// and keeps. Reading it avoids the virtual call and the 132-byte copy once it exists.
constexpr std::size_t k_channel_type_ptr_offset = 0x0c;

constexpr std::size_t k_type_guid_offset = 0x50;
constexpr std::size_t k_type_base_guid_offset = 0x60;

// GetChannelType returns a ChannelType **by value**: on x86 MSVC that is a hidden pointer to the
// caller's buffer as the first stack argument (engine journal §3.2).
using get_type_fn = void*(__fastcall*)(A3d_Channel*, void*, void*);

struct kind_guid {
    const GUID* guid;
    kind value;
};

// All six from the SDK headers, cross-checked against the base guid column of channels.lst, which
// partitions all 226 types the same way.
const kind_guid k_kind_guids[] = {
    { &FLOAT_CHANNEL_GUID, kind::number },
    { &STRING_GUID, kind::text },
    { &VECTOR_GUID, kind::vector },
    { &MATRIX_CHANNEL_GUID, kind::matrix },
    { &DX8_TEXTURE_CHANNEL_GUID, kind::texture },
    { &OBJECTDATA_CHANNEL_GUID, kind::object },
};

// The engine's cached record when it exists, otherwise the virtual call filling `buffer`. Null when
// neither is available.
const std::byte* type_record(A3d_Channel* channel, std::byte (&buffer)[tw::engine::channel_kind::k_type_record_size]) noexcept
{
    if(channel == nullptr || tw::engine::vtable::of(channel) == nullptr) {
        return nullptr;
    }

    const auto* cached = *reinterpret_cast<const std::byte* const*>(reinterpret_cast<const std::byte*>(channel) + k_channel_type_ptr_offset);
    if(cached != nullptr) {
        return cached;
    }

    if(!tw::engine::vtable::slot_is_code(channel, k_vtable_get_channel_type_offset)) {
        return nullptr;
    }

    tw::engine::vtable::slot<get_type_fn>(channel, k_vtable_get_channel_type_offset)(channel, nullptr, buffer);

    return buffer;
}
} // namespace

namespace tw::engine::channel_kind
{
const char* name(kind value) noexcept
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

kind of(A3d_Channel* channel) noexcept
{
    alignas(4) std::byte buffer[k_type_record_size] {};
    const std::byte* record = type_record(channel, buffer);
    if(record == nullptr) {
        return kind::unknown;
    }

    for(const kind_guid& entry : k_kind_guids) {
        // Base guid first: it is what says "derives from", and it is what makes Expression Value and
        // Lua Script read as numbers. The own guid is checked too so that the root types themselves
        // (Value, Text, ...) classify as their own family.
        if(std::memcmp(record + k_type_base_guid_offset, entry.guid, sizeof(GUID)) == 0
            || std::memcmp(record + k_type_guid_offset, entry.guid, sizeof(GUID)) == 0) {
            return entry.value;
        }
    }

    return kind::other;
}

bool callable_as(A3d_Channel* channel, kind as) noexcept
{
    // Each family checks its own reader slot. This is the only switch in the engine layer over every
    // family, and it is here because this file's job is exactly that: knowing them all.
    switch(as) {
    case kind::number:
        return fam_number::callable(channel);
    case kind::text:
        return fam_text::callable(channel);
    case kind::vector:
        return fam_vector::callable(channel);
    case kind::matrix:
        return fam_matrix::callable(channel);
    default:
        return false;
    }
}

bool is_type(A3d_Channel* channel, const GUID& guid) noexcept
{
    alignas(4) std::byte buffer[k_type_record_size] {};
    const std::byte* record = type_record(channel, buffer);

    return record != nullptr && std::memcmp(record + k_type_guid_offset, &guid, sizeof(GUID)) == 0;
}
} // namespace tw::engine::channel_kind
