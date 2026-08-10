#include "pch.hxx"

#include "resource/resource.hxx"

#include "plugin/diagnostics.hxx"

#include "resource/self_extract.hxx"

namespace
{
// Transparent hash/equality, so lookups can be driven by a std::string_view over a stack buffer
// instead of allocating a std::string for the normalized key on every call.
struct transparent_string_hash {
    using is_transparent = void;

    [[nodiscard]] std::size_t operator()(std::string_view key) const noexcept
    {
        return std::hash<std::string_view> {}(key);
    }
};

bool g_initialized = false;
std::unordered_map<std::string, tw::resource::view, transparent_string_hash, std::equal_to<>> g_index;

// Resource keys are asset-relative paths ("textures/TweakerIcon-5.png", "fonts/Roboto-Regular.ttf")
// - short by construction, and bounded by what the .rc generator can emit. This covers every key in
// assets/ several times over; anything longer falls back to the heap rather than truncating, since
// a silently truncated key would resolve to the wrong resource instead of failing.
constexpr std::size_t k_normalized_key_capacity = 256;

// Writes the normalized form of `key` into `buffer` and returns a view of it - or, if it doesn't
// fit, into `spill` and returns a view of that. The returned view is valid only while both
// outparams are alive.
std::string_view normalize_key_into(std::string_view key, std::array<char, k_normalized_key_capacity>& buffer, std::string& spill)
{
    const auto normalize_char = [](char ch) noexcept -> char {
        if(ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch - 'A' + 'a');
        }
        if(ch == '\\') {
            return '/';
        }
        return ch;
    };

    if(key.size() <= buffer.size()) {
        std::size_t out = 0;
        for(const char ch : key) {
            if(ch == '"') {
                continue;
            }
            buffer[out++] = normalize_char(ch);
        }
        return std::string_view(buffer.data(), out);
    }

    spill.clear();
    spill.reserve(key.size());
    for(const char ch : key) {
        if(ch == '"') {
            continue;
        }
        spill.push_back(normalize_char(ch));
    }
    return spill;
}
} // namespace

namespace tw::resource
{
std::string_view view::text() const noexcept
{
    if(bytes.empty()) {
        return {};
    }
    return std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

bool initialize(HMODULE module) noexcept
{
    shutdown();

    std::vector<self_extract::entry> entries;
    if(!self_extract::run(module, entries)) {
        TW_LOG_ERROR("resource: self_extract failed - no embedded assets available");
        return false;
    }

    g_index.reserve(entries.size());
    for(auto& e : entries) {
        view v;
        v.kind = e.kind;
        v.bytes = e.bytes;

        std::array<char, k_normalized_key_capacity> buffer {};
        std::string spill;
        g_index.emplace(std::string(normalize_key_into(e.key, buffer, spill)), v);
    }

    g_initialized = true;
    TW_LOG_INFO("resource: indexed {} embedded entries", g_index.size());
    return true;
}

void shutdown() noexcept
{
    g_index.clear();
    g_initialized = false;
}

std::expected<view, error> get_resource(std::string_view key) noexcept
{
    if(!g_initialized) {
        return std::unexpected(error::not_initialized);
    }

    std::array<char, k_normalized_key_capacity> buffer {};
    std::string spill;
    const auto it = g_index.find(normalize_key_into(key, buffer, spill));
    if(it == g_index.end()) {
        return std::unexpected(error::not_found);
    }
    return it->second;
}

std::vector<std::string_view> list_keys(type kind)
{
    std::vector<std::string_view> keys;
    if(!g_initialized) {
        return keys;
    }

    for(const auto& [key, value] : g_index) {
        if(value.kind == kind) {
            keys.push_back(key);
        }
    }
    return keys;
}

std::expected<view, error> get_resource(type kind, std::string_view key) noexcept
{
    auto result = get_resource(key);
    if(!result) {
        return result;
    }
    if(result->kind != kind) {
        return std::unexpected(error::wrong_type);
    }
    return result;
}
} // namespace tw::resource
