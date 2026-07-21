#include "ui/overlay_config.hxx"

// No TweakerPlugin PCH here - shared with smoke_test, only needs STL file I/O.
#include <charconv>
#include <fstream>
#include <sstream>

namespace tw::ui::overlay_config
{
namespace
{
side g_feed_side = side::left;
side g_pins_side = side::right;
std::string g_theme_overrides;
std::string g_loaded_path; // remembered by load(), used by the no-arg save()
ImVec2 g_menu_pos {-1.f, -1.f}; // sentinel: never persisted / first run - caller picks a default
ImVec2 g_menu_size {420.f, 520.f};

std::string_view trim(std::string_view s) noexcept
{
    while(!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
        s.remove_prefix(1);
    }
    while(!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

float parse_float(std::string_view s, float fallback) noexcept
{
    float value = fallback;
    std::from_chars(s.data(), s.data() + s.size(), value);
    return value;
}

side parse_side(std::string_view s, side fallback) noexcept
{
    if(s == "left") {
        return side::left;
    }
    if(s == "right") {
        return side::right;
    }
    return fallback;
}

const char* side_to_string(side s) noexcept
{
    return s == side::left ? "left" : "right";
}
} // namespace

void load(std::string_view path)
{
    g_loaded_path.assign(path);

    std::ifstream file {g_loaded_path};
    if(!file.is_open()) {
        return;
    }

    std::string line;
    bool in_theme_block = false;
    std::ostringstream theme_block;

    while(std::getline(file, line)) {
        if(!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if(in_theme_block) {
            if(line == "theme_overrides_end") {
                in_theme_block = false;
                continue;
            }
            theme_block << line << '\n';
            continue;
        }

        if(line == "theme_overrides_begin") {
            in_theme_block = true;
            continue;
        }

        const auto eq = line.find('=');
        if(eq == std::string::npos) {
            continue;
        }

        const auto key = trim(std::string_view(line).substr(0, eq));
        const auto value = trim(std::string_view(line).substr(eq + 1));

        if(key == "feed_side") {
            g_feed_side = parse_side(value, g_feed_side);
        }
        else if(key == "pins_side") {
            g_pins_side = parse_side(value, g_pins_side);
        }
        else if(key == "menu_pos_x") {
            g_menu_pos.x = parse_float(value, g_menu_pos.x);
        }
        else if(key == "menu_pos_y") {
            g_menu_pos.y = parse_float(value, g_menu_pos.y);
        }
        else if(key == "menu_size_x") {
            g_menu_size.x = parse_float(value, g_menu_size.x);
        }
        else if(key == "menu_size_y") {
            g_menu_size.y = parse_float(value, g_menu_size.y);
        }
    }

    g_theme_overrides = theme_block.str();
}

void save()
{
    if(g_loaded_path.empty()) {
        return;
    }

    std::ofstream file {g_loaded_path, std::ios::trunc};
    if(!file.is_open()) {
        return;
    }

    file << "feed_side=" << side_to_string(g_feed_side) << '\n';
    file << "pins_side=" << side_to_string(g_pins_side) << '\n';
    file << "menu_pos_x=" << g_menu_pos.x << '\n';
    file << "menu_pos_y=" << g_menu_pos.y << '\n';
    file << "menu_size_x=" << g_menu_size.x << '\n';
    file << "menu_size_y=" << g_menu_size.y << '\n';
    file << "theme_overrides_begin\n";
    file << g_theme_overrides;
    if(!g_theme_overrides.empty() && g_theme_overrides.back() != '\n') {
        file << '\n';
    }
    file << "theme_overrides_end\n";
}

side feed_side() noexcept
{
    return g_feed_side;
}

void set_feed_side(side value)
{
    g_feed_side = value;
}

side pins_side() noexcept
{
    return g_pins_side;
}

void set_pins_side(side value)
{
    g_pins_side = value;
}

const std::string& theme_overrides() noexcept
{
    return g_theme_overrides;
}

void set_theme_overrides(std::string text)
{
    g_theme_overrides = std::move(text);
}

ImVec2 menu_pos() noexcept
{
    return g_menu_pos;
}

void set_menu_pos(ImVec2 pos) noexcept
{
    g_menu_pos = pos;
}

ImVec2 menu_size() noexcept
{
    return g_menu_size;
}

void set_menu_size(ImVec2 size) noexcept
{
    g_menu_size = size;
}
} // namespace tw::ui::overlay_config
