#include "pch.hxx"

#include "lua/api/api_storage.hxx"

#include "lua/lua_storage.hxx"

namespace
{
std::string g_error;
std::string g_list;

std::string_view text_of(const char* text) noexcept
{
    return text != nullptr ? std::string_view { text } : std::string_view {};
}

int failed(std::string reason) noexcept
{
    g_error = std::move(reason);
    return 0;
}
} // namespace

namespace tw::lua::api
{
extern "C" {
int tw_storage_read(int owner, const char* name, const char** data, int* size) noexcept
{
    std::string_view bytes;
    std::string error;
    if(data == nullptr || size == nullptr || !tw::lua::storage::read(owner, text_of(name), bytes, error)) {
        return failed(std::move(error));
    }

    *data = bytes.data();
    *size = static_cast<int>(bytes.size());
    return 1;
}

int tw_storage_write(int owner, const char* name, const char* data, int size) noexcept
{
    std::string error;
    if(size < 0 || (data == nullptr && size > 0)) {
        return failed("nothing to write");
    }
    if(!tw::lua::storage::write(owner, text_of(name), std::string_view { data != nullptr ? data : "", static_cast<std::size_t>(size) }, error)) {
        return failed(std::move(error));
    }
    return 1;
}

int tw_storage_read_json(int owner, const char* name, const void** tape, int* count) noexcept
{
    std::span<const tw::lua::storage::json_node> nodes;
    std::string error;
    if(tape == nullptr || count == nullptr || !tw::lua::storage::read_json(owner, text_of(name), nodes, error)) {
        return failed(std::move(error));
    }

    *tape = nodes.data();
    *count = static_cast<int>(nodes.size());
    return 1;
}

int tw_storage_write_json(int owner, const char* name, const void* tape, int count) noexcept
{
    if(tape == nullptr || count <= 0) {
        return failed("nothing to write");
    }

    const std::span<const tw::lua::storage::json_node> nodes { static_cast<const tw::lua::storage::json_node*>(tape), static_cast<std::size_t>(count) };
    std::string error;
    if(!tw::lua::storage::write_json(owner, text_of(name), nodes, error)) {
        return failed(std::move(error));
    }
    return 1;
}

int tw_storage_exists(int owner, const char* name) noexcept
{
    return tw::lua::storage::exists(owner, text_of(name)) ? 1 : 0;
}

int tw_storage_remove(int owner, const char* name) noexcept
{
    std::string error;
    if(!tw::lua::storage::remove(owner, text_of(name), error)) {
        return failed(std::move(error));
    }
    return 1;
}

const char* tw_storage_list(int owner) noexcept
{
    g_list.clear();
    for(const std::string& name : tw::lua::storage::list(owner)) {
        g_list += name;
        g_list += '\n';
    }
    return g_list.c_str();
}

const char* tw_storage_error() noexcept
{
    return g_error.c_str();
}
}
} // namespace tw::lua::api
