#include "pch.hxx"

#include "lua/lua_linemap.hxx"

namespace
{
constexpr std::string_view k_format_line = "# twllink 1";
constexpr std::string_view k_hash_prefix = "# bundle fnv1a64 ";

std::string_view strip_cr(std::string_view line) noexcept
{
    if(!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
    }
    return line;
}

bool is_space(char c) noexcept
{
    return c == ' ' || c == '\t';
}

std::string_view next_field(std::string_view& line) noexcept
{
    while(!line.empty() && is_space(line.front())) {
        line.remove_prefix(1);
    }
    std::size_t end = 0;
    while(end < line.size() && !is_space(line[end])) {
        ++end;
    }
    const std::string_view field = line.substr(0, end);
    line.remove_prefix(end);
    return field;
}

bool parse_uint(std::string_view text, std::uint32_t& value) noexcept
{
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    return ec == std::errc {} && ptr == text.data() + text.size() && !text.empty();
}

bool parse_hex64(std::string_view text, std::uint64_t& value) noexcept
{
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    return ec == std::errc {} && ptr == text.data() + text.size() && text.size() == 16;
}

bool read_bytes(const std::filesystem::path& path, std::string& out) noexcept
{
    std::ifstream file { path, std::ios::binary };
    if(!file.is_open()) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char> { file }, std::istreambuf_iterator<char> {});
    return !file.bad();
}

// Characters that end a file name when walking back from the ':' of a position: what Lua and the
// traceback put around a chunk name ("in function <x.lua:12>", "[string ...]", a message start).
bool ends_name(char c) noexcept
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '<' || c == '(' || c == '[' || c == '\'' || c == '"';
}

// Whether `candidate`, as Lua printed it, names the script: exactly, or truncated to "...<tail>".
bool names_chunk(std::string_view candidate, std::string_view chunk) noexcept
{
    if(candidate == chunk) {
        return true;
    }
    return candidate.size() > 3 && candidate.starts_with("...") && chunk.ends_with(candidate.substr(3));
}
} // namespace

namespace tw::lua
{
std::uint64_t bundle_hash(std::string_view bytes) noexcept
{
    std::uint64_t hash = 0xCBF2'9CE4'8422'2325ull;
    for(const char c : bytes) {
        if(c == '\r') {
            continue;
        }
        hash ^= static_cast<unsigned char>(c);
        hash *= 0x0000'0100'0000'01B3ull;
    }
    return hash;
}

bool parse_line_map(std::string_view text, line_map& out, std::uint64_t& hash, std::string& detail) noexcept
{
    out = {};
    line_map map;
    bool have_hash = false;

    std::size_t number = 0;
    while(!text.empty()) {
        const std::size_t end = text.find('\n');
        const std::string_view line = strip_cr(text.substr(0, end));
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
        ++number;

        if(number == 1) {
            if(line != k_format_line) {
                detail = std::format("line 1 is not \"{}\" - not a map, or one from a newer twllink", k_format_line);
                return false;
            }
            continue;
        }
        if(line.empty()) {
            continue;
        }
        if(line.starts_with('#')) {
            if(line.starts_with(k_hash_prefix)) {
                have_hash = parse_hex64(line.substr(k_hash_prefix.size()), hash);
                if(!have_hash) {
                    detail = std::format("line {}: unreadable bundle hash", number);
                    return false;
                }
            }
            continue;
        }

        std::string_view rest = line;
        line_run run;
        const std::string_view bundle = next_field(rest);
        const std::string_view file = next_field(rest);
        const std::string_view source = next_field(rest);
        if(!parse_uint(bundle, run.bundle_line) || file.empty() || !parse_uint(source, run.source_line) || !next_field(rest).empty()) {
            detail = std::format("line {}: expected <bundle line> <file> <source line>", number);
            return false;
        }
        if(run.bundle_line == 0 || (!map.runs.empty() && run.bundle_line <= map.runs.back().bundle_line)) {
            detail = std::format("line {}: bundle lines must start at 1 and increase", number);
            return false;
        }

        if(file != "-") {
            if(run.source_line == 0) {
                detail = std::format("line {}: source lines start at 1", number);
                return false;
            }
            const auto known = std::ranges::find(map.files, file);
            run.file = static_cast<std::uint32_t>(known - map.files.begin());
            if(known == map.files.end()) {
                map.files.emplace_back(file);
            }
        }
        map.runs.push_back(run);
    }

    if(number == 0) {
        detail = "the map is empty";
        return false;
    }
    if(!have_hash) {
        detail = "no \"# bundle fnv1a64\" line - cannot tell which bundle this map belongs to";
        return false;
    }

    out = std::move(map);
    return true;
}

line_map_status load_line_map(const std::filesystem::path& script, line_map& out, std::string& detail) noexcept
{
    out = {};

    std::filesystem::path map_path = script;
    map_path += L".map";

    std::error_code ec;
    if(!std::filesystem::is_regular_file(map_path, ec)) {
        return line_map_status::absent;
    }

    std::string text;
    if(!read_bytes(map_path, text)) {
        detail = "cannot read the file";
        return line_map_status::malformed;
    }

    line_map map;
    std::uint64_t expected = 0;
    if(!parse_line_map(text, map, expected, detail)) {
        return line_map_status::malformed;
    }

    std::string bundle;
    if(!read_bytes(script, bundle) || bundle_hash(bundle) != expected) {
        detail = "the script was changed after it was built";
        return line_map_status::stale;
    }

    out = std::move(map);
    return line_map_status::loaded;
}

bool map_positions(const line_map& map, std::string_view chunk, std::string_view text, std::string& out)
{
    if(map.empty() || chunk.empty()) {
        return false;
    }

    std::string result;
    std::size_t copied = 0;
    bool changed = false;

    for(std::size_t colon = text.find(':'); colon != std::string_view::npos; colon = text.find(':', colon + 1)) {
        std::size_t digits_end = colon + 1;
        while(digits_end < text.size() && text[digits_end] >= '0' && text[digits_end] <= '9') {
            ++digits_end;
        }
        if(digits_end == colon + 1) {
            continue;
        }

        std::size_t name_start = colon;
        while(name_start > copied && !ends_name(text[name_start - 1])) {
            --name_start;
        }
        if(!names_chunk(text.substr(name_start, colon - name_start), chunk)) {
            continue;
        }

        std::uint32_t line = 0;
        if(!parse_uint(text.substr(colon + 1, digits_end - colon - 1), line)) {
            continue;
        }

        // The last run starting at or before the line.
        const auto after = std::ranges::upper_bound(map.runs, line, {}, &line_run::bundle_line);
        if(after == map.runs.begin() || std::prev(after)->file == k_own_lines) {
            continue;
        }
        const line_run& run = *std::prev(after);

        result.append(text.substr(copied, name_start - copied));
        result.append(map.files[run.file]);
        result.push_back(':');
        result.append(std::to_string(run.source_line + (line - run.bundle_line)));
        copied = digits_end;
        colon = digits_end - 1;
        changed = true;
    }

    if(!changed) {
        return false;
    }

    result.append(text.substr(copied));
    out = std::move(result);
    return true;
}
} // namespace tw::lua
