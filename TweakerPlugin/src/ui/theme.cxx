#include "ui/theme.hxx"

#include <cctype>
#include <charconv>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace tw::ui::theme
{
namespace
{
ImVec4 from_argb(std::uint32_t argb) noexcept
{
    return ImVec4{
        static_cast<float>((argb >> 16) & 0xFFu) / 255.f,
        static_cast<float>((argb >> 8) & 0xFFu) / 255.f,
        static_cast<float>(argb & 0xFFu) / 255.f,
        static_cast<float>((argb >> 24) & 0xFFu) / 255.f,
    };
}

bool parse_hex_color(std::string_view text, ImVec4& out) noexcept
{
    if (!text.empty() && text.front() == '#')
        text.remove_prefix(1);

    if (text.size() != 6 && text.size() != 8)
        return false;

    std::uint32_t value = 0;
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    if (std::from_chars(begin, end, value, 16).ec != std::errc{})
        return false;

    if (text.size() == 6)
        value |= 0xFF000000u;

    out = from_argb(value);
    return true;
}

std::string_view trim(std::string_view s) noexcept
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

struct theme_init
{
    theme_init() noexcept
    {
        apply_dark();
    }
};

const theme_init g_theme_init {};
} // namespace

void apply_dark() noexcept
{
    // AccentColors.axaml Dark
    accent_primary = from_argb(0xFF2DD4BF);
    accent_secondary = from_argb(0xFF22D3EE);
    accent_text = from_argb(0xFF5EEAD4);
    accent_selection = from_argb(0x5C2DD4BF);
    accent_soft = from_argb(0x262DD4BF);
    accent_hover = from_argb(0x332DD4BF);
    accent_pressed = from_argb(0xFF163338);
    accent_border = from_argb(0x402DD4BF);
    accent_selected = from_argb(0x732DD4BF);
    accent_ghost = from_argb(0x002DD4BF);

    // NeutralColors.axaml Dark
    app_background = from_argb(0xFF1B1D21);
    surface = from_argb(0xFF24262B);
    surface_muted = from_argb(0xFF17181B);
    surface_soft = from_argb(0xFF1E2024);
    surface_hover = from_argb(0xFF2A2D32);
    surface_input = from_argb(0xFF1A1C20);
    surface_row = from_argb(0xFF292B30);
    surface_row_hover = from_argb(0xFF32353B);
    surface_skeleton = from_argb(0xFF2E3036);
    surface_badge = from_argb(0xFF2F3238);
    surface_elevated = from_argb(0xFF33363C);
    control_track_off = from_argb(0xFF40434A);
    control_thumb = from_argb(0xFFECEDEF);

    border = from_argb(0xFF34373D);
    border_subtle = from_argb(0xFF3A3D44);
    border_strong = from_argb(0xFF4A4E56);
    border_divider = from_argb(0xFF3C3F46);
    border_window = from_argb(0xFF55585F);

    text_primary = from_argb(0xFFE8E9EC);
    text_secondary = from_argb(0xFFC4C6CB);
    text_muted = from_argb(0xFF9297A1);
    text_subtle = from_argb(0xFF7D8088);
    text_faint = from_argb(0xFF6C7078);
    text_glyph_muted = from_argb(0xFF5C5F66);
    text_on_accent = from_argb(0xFFF2FDFB);
}

void from_config(std::string_view config) noexcept
{
    apply_dark();
    if (config.empty())
        return;

    static const std::unordered_map<std::string_view, ImVec4*> k_keys = {
        {"accent_primary", &accent_primary},
        {"accent_secondary", &accent_secondary},
        {"accent_text", &accent_text},
        {"accent_selection", &accent_selection},
        {"accent_soft", &accent_soft},
        {"accent_hover", &accent_hover},
        {"accent_pressed", &accent_pressed},
        {"accent_border", &accent_border},
        {"accent_selected", &accent_selected},
        {"accent_ghost", &accent_ghost},
        {"app_background", &app_background},
        {"surface", &surface},
        {"surface_muted", &surface_muted},
        {"surface_soft", &surface_soft},
        {"surface_hover", &surface_hover},
        {"surface_input", &surface_input},
        {"surface_row", &surface_row},
        {"surface_row_hover", &surface_row_hover},
        {"surface_skeleton", &surface_skeleton},
        {"surface_badge", &surface_badge},
        {"surface_elevated", &surface_elevated},
        {"control_track_off", &control_track_off},
        {"control_thumb", &control_thumb},
        {"border", &border},
        {"border_subtle", &border_subtle},
        {"border_strong", &border_strong},
        {"border_divider", &border_divider},
        {"border_window", &border_window},
        {"text_primary", &text_primary},
        {"text_secondary", &text_secondary},
        {"text_muted", &text_muted},
        {"text_subtle", &text_subtle},
        {"text_faint", &text_faint},
        {"text_glyph_muted", &text_glyph_muted},
        {"text_on_accent", &text_on_accent},
    };

    while (!config.empty())
    {
        const auto nl = config.find('\n');
        auto line = trim(nl == std::string_view::npos ? config : config.substr(0, nl));
        config.remove_prefix(nl == std::string_view::npos ? config.size() : nl + 1);

        if (line.empty() || line.front() == '#')
            continue;

        const auto eq = line.find('=');
        if (eq == std::string_view::npos)
            continue;

        const auto key = trim(line.substr(0, eq));
        const auto value = trim(line.substr(eq + 1));
        const auto it = k_keys.find(key);
        if (it == k_keys.end())
            continue;

        ImVec4 parsed {};
        if (parse_hex_color(value, parsed))
            *it->second = parsed;
    }
}
} // namespace tw::ui::theme
