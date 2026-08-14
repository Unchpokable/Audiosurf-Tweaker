// No TweakerPlugin PCH here - this TU is shared with smoke_test (see src/ui/CMakeLists.txt) and
// has no D3D9/Quest3D dependency, only STL. Same convention as overlay_state.cxx.
#include "ui/wire_text.hxx"

#include <cstddef>
#include <cstdint>

namespace
{
constexpr char k_hex[] = "0123456789ABCDEF";

std::uint8_t from_hex(char c) noexcept
{
    if(c >= '0' && c <= '9') {
        return static_cast<std::uint8_t>(c - '0');
    }

    if(c >= 'a' && c <= 'f') {
        return static_cast<std::uint8_t>(10 + c - 'a');
    }

    if(c >= 'A' && c <= 'F') {
        return static_cast<std::uint8_t>(10 + c - 'A');
    }

    return 0xFF;
}
} // namespace

namespace tw::ui::wire
{
std::string percent_encode(std::string_view text)
{
    std::string out;
    out.reserve(text.size());

    for(const unsigned char c : text) {
        const bool unreserved =
            (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        if(unreserved) {
            out.push_back(static_cast<char>(c));
            continue;
        }

        out.push_back('%');
        out.push_back(k_hex[c >> 4]);
        out.push_back(k_hex[c & 0x0F]);
    }

    return out;
}

std::string percent_decode(std::string_view text)
{
    std::string out;
    out.reserve(text.size());

    for(std::size_t i = 0; i < text.size();) {
        if(text[i] == '%' && i + 2 < text.size()) {
            const auto hi = from_hex(text[i + 1]);
            const auto lo = from_hex(text[i + 2]);

            if(hi != 0xFF && lo != 0xFF) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 3;
                continue;
            }
        }

        out.push_back(text[i]);
        ++i;
    }

    return out;
}
} // namespace tw::ui::wire
