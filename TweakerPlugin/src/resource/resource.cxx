#include "pch.hxx"

#include "resource/resource.hxx"

#include "resource/self_extract.hxx"

namespace
{
bool g_initialized = false;
std::unordered_map<std::string, tw::resource::view> g_index;

std::string normalize_key(std::string_view key)
{
    std::string out;
    out.reserve(key.size());
    for(char ch : key) {
        if(ch == '"') {
            continue;
        }
        if(ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
        if(ch == '\\') {
            ch = '/';
        }
        out.push_back(ch);
    }
    return out;
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
        return false;
    }

    g_index.reserve(entries.size());
    for(auto& e : entries) {
        view v;
        v.kind = e.kind;
        v.bytes = e.bytes;
        g_index.emplace(normalize_key(e.key), v);
    }

    g_initialized = true;
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

    const auto it = g_index.find(normalize_key(key));
    if(it == g_index.end()) {
        return std::unexpected(error::not_found);
    }
    return it->second;
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
