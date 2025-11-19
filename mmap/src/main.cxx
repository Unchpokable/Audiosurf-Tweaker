#include "global.h"

#include <iostream>

#include "mmap32.hxx"

#include "proc_tools.hxx"
#include "win_handle.hxx"

namespace
{
constexpr const char* target_dlls[] = {
    "d3d9.dll",
    "d3dx9.dll",
};

struct LoaderArgs {
    const char* target_proc { nullptr };
    const char* dll_path { nullptr };

    enum SearchDllStrategy {
        LocalToExe,
        Global,
    } deps_lookup_strategy { LocalToExe };

    const char* deps_lookup_path { nullptr };
};

struct ParseResult {
    LoaderArgs args;
    const char* error { nullptr };

    explicit operator bool() const
    {
        return error == nullptr;
    }
};

} // namespace

namespace
{
std::optional<std::string_view> extract_value(std::string_view arg, std::string_view prefix) noexcept
{
    if(!arg.starts_with(prefix))
        return std::nullopt;

    auto value = arg.substr(prefix.size());

    if(value.empty())
        return std::nullopt;

    // Убираем кавычки если есть
    if(value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value.remove_prefix(1);
        value.remove_suffix(1);
    }

    return value;
}

ParseResult parse_args(int argc, char* argv[]) noexcept
{
    ParseResult result;

    for(int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);

        if(auto val = extract_value(arg, "--target=")) {
            result.args.target_proc = val->data();
        }
        else if(auto val = extract_value(arg, "--dll=")) {
            result.args.dll_path = val->data();
        }
        else if(auto val = extract_value(arg, "--deps-lookup=")) {
            if(*val == "Global") {
                result.args.deps_lookup_strategy = LoaderArgs::Global;
            }
            else if(*val == "LocalToExe") {
                result.args.deps_lookup_strategy = LoaderArgs::LocalToExe;
            }
            else {
                result.error = "Invalid deps-lookup strategy (expected: LocalToExe or Global)";
                return result;
            }
        }
        else if(auto val = extract_value(arg, "--lookup=")) {
            result.args.deps_lookup_path = val->data();
        }
        else {
            result.error = "Unknown argument";
            return result;
        }
    }

    if(!result.args.target_proc) {
        result.error = "Missing required argument: --target";
        return result;
    }

    if(!result.args.dll_path) {
        result.error = "Missing required argument: --dll";
        return result;
    }

    return result;
}
} // namespace

namespace
{
bool elevate_access_to_debug() noexcept
{
    mmap::raii::WinHandle token;

    if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, token.ptr())) {
        return false;
    }

    TOKEN_PRIVILEGES token_privileges;
    LUID luid;

    if(!LookupPrivilegeValue(nullptr, "SeDebugPrivilege", &luid)) {
        return false;
    }

    token_privileges.PrivilegeCount = 1;
    token_privileges.Privileges[0].Luid = luid;
    token_privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    TOKEN_PRIVILEGES previous_privileges;
    DWORD returned_length;

    if(!AdjustTokenPrivileges(token, FALSE, &token_privileges, sizeof(previous_privileges), &previous_privileges, &returned_length)) {
        return false;
    }

    return true;
}

bool is_admin() noexcept
{
    mmap::raii::WinHandle token;

    if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, token.ptr())) {
        return false;
    }

    DWORD size { 0 };

    GetTokenInformation(token, TokenElevation, nullptr, 0, &size);

    TOKEN_ELEVATION elevation;

    bool is_elevated = false;

    if(GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
        is_elevated = elevation.TokenIsElevated != 0;
    }

    return is_elevated;
}
} // namespace

int main(int argc, char* argv[])
{
    bool debug_access { false };
    auto args = parse_args(argc, argv);

    if(args.error != nullptr) {
        std::cerr << "Loader error: " << args.error;
        return -1;
    }

    if(is_admin()) {
        debug_access = debug_access || elevate_access_to_debug();
    }

    auto dll_path = args.args.dll_path;
    auto target_proc = mmap::tools::get_proc_id(args.args.target_proc);

    if(target_proc == 0) {
        std::cerr << std::format("No process found with name {}", args.args.target_proc);
        return -1;
    }

    if(debug_access) {
        return mmap::mmap_load_dll(dll_path, target_proc);
    }

    std::cerr << "No debug access granted, unable to manual map payload";
    return -1;
}
