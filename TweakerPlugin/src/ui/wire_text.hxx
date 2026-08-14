#pragma once

#include <string>
#include <string_view>

// The percent-codec L3 string tokens travel in (Docs/Internal/overlay-protocol.md, L3): tokens are
// space-separated with no quoting, so anything carrying spaces or punctuation - skin names, song
// titles, artists, playlist names - is percent-encoded UTF-8.
//
// Lives here rather than privately in each consumer because both halves have to agree byte for byte
// with C#'s Uri.EscapeDataString/UnescapeDataString on the host, and there are now three callers
// (ipc/overlay_ipc, ui/pending_actions, ui/qp/qp_wire). Three private copies of a codec that must
// stay identical is exactly how they drift apart.
namespace tw::ui::wire
{
// Escapes everything outside RFC 3986's unreserved set (A-Z a-z 0-9 - _ . ~), which is the same set
// Uri.EscapeDataString leaves alone. Uppercase hex, matching it too.
[[nodiscard]] std::string percent_encode(std::string_view text);

// Tolerant by design: a stray '%' that is not followed by two hex digits is passed through as a
// literal rather than dropped, so a malformed token degrades to slightly wrong text instead of
// truncating the message.
[[nodiscard]] std::string percent_decode(std::string_view text);
} // namespace tw::ui::wire
