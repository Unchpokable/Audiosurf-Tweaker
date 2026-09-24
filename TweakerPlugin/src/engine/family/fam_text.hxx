#pragma once

#include "engine/channel_ref.hxx"

// Aco_StringChannel - the text family. Base guid STRING_GUID; Text, Array Text, TextOperator and the
// rest of the string-producing types.
//
//   slot 17  +0x44  GetString() -> const char*
//   slot 18  +0x48  SetString(const char*)            not used here
//   slot 19  +0x4c  SetSingeLine(bool)                not used here - and the engine's spelling
//   slot 23  +0x5c  GetIfUseWChar() -> bool
//   slot 24  +0x60  GetWString() -> const WCHAR*
//
// WHAT A WRONG FAMILY COSTS. Slot 17 here collides with GetFloat on a number: calling GetString on a
// numeric channel returns its float bits as a pointer, and the string read that follows walks
// wherever that points. Slots 23 and 24 exist only on this family - on a number or a vector they are
// past the end of the vtable entirely (the vector table ends at slot 20), so the "past the end" read
// is whatever the linker put next in .rdata.
namespace tw::engine::fam_text
{
[[nodiscard]] bool callable(A3d_Channel* channel) noexcept;

// The channel's text as UTF-8. Both storage modes are handled: the channel is asked which one it is
// in, because it will happily hand back a stale or empty string of the other flavour. Song titles
// and anything localised are wide.
//
// Module-owned buffer, valid until the next call. Never null: an unreadable channel, or a ref of
// another family, yields "".
[[nodiscard]] const char* get(const channel_ref& ref) noexcept;
} // namespace tw::engine::fam_text
