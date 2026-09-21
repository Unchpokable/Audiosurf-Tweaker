#include "pch.hxx"

#include "plugin/paths.hxx"

#include "plugin/boot_log.hxx"
#include "plugin/diagnostics.hxx"

namespace
{
std::filesystem::path g_executable_file;
std::filesystem::path g_engine_root;
std::filesystem::path g_stuff_root;
std::filesystem::path g_config_dir;
std::filesystem::path g_skybox_replacer_root;
std::filesystem::path g_skyboxes_dir;
std::filesystem::path g_sky_configs_dir;
std::filesystem::path g_scripts_dir;
std::filesystem::path g_logs_dir;

std::filesystem::path executable_path() noexcept
{
    // MAX_PATH is not a limit Steam libraries respect, so the buffer grows until the name fits.
    std::wstring buffer(MAX_PATH, L'\0');

    for(;;) {
        const DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if(length == 0) {
            return {};
        }

        if(length < buffer.size()) {
            buffer.resize(length);
            break;
        }

        buffer.resize(buffer.size() * 2);
    }

    return std::filesystem::path { std::move(buffer) };
}

void ensure_directory(const std::filesystem::path& directory) noexcept
{
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);

    if(ec) {
        TW_LOG_WARNING("paths: cannot create '{}': {}", tw::plugin::paths::to_utf8(directory), ec.message());
        TW_BOOT_LOG("paths: cannot create '{}': error {}", tw::plugin::paths::to_utf8(directory), ec.value());
    }
}
} // namespace

namespace tw::plugin::paths
{
bool resolve() noexcept
{
    g_executable_file = executable_path();
    if(g_executable_file.empty()) {
        return false;
    }

    g_engine_root = g_executable_file.parent_path();
    g_stuff_root = g_engine_root / L"TweakerStuff";
    g_config_dir = g_stuff_root / L"Config";
    g_skybox_replacer_root = g_stuff_root / L"SkyboxReplacer";
    g_skyboxes_dir = g_skybox_replacer_root / L"Skyboxes";
    g_sky_configs_dir = g_skybox_replacer_root / L"SkyConfigs";
    g_scripts_dir = g_stuff_root / L"Scripts";
    g_logs_dir = g_stuff_root / L"Logs";

    return true;
}

void create_directories() noexcept
{
    if(g_engine_root.empty()) {
        return;
    }

    // All of them, up front: a folder that exists is a folder the user can find and drop things into,
    // which is half of what a fixed layout is for.
    for(const std::filesystem::path* directory :
        { &g_config_dir, &g_skyboxes_dir, &g_sky_configs_dir, &g_scripts_dir, &g_logs_dir }) {
        ensure_directory(*directory);
    }

    TW_LOG_INFO("paths: engine root '{}'", to_utf8(g_engine_root));
}

const std::filesystem::path& executable_file() noexcept
{
    return g_executable_file;
}

const std::filesystem::path& engine_root() noexcept
{
    return g_engine_root;
}

const std::filesystem::path& stuff_root() noexcept
{
    return g_stuff_root;
}

const std::filesystem::path& config_dir() noexcept
{
    return g_config_dir;
}

const std::filesystem::path& skybox_replacer_root() noexcept
{
    return g_skybox_replacer_root;
}

const std::filesystem::path& skyboxes_dir() noexcept
{
    return g_skyboxes_dir;
}

const std::filesystem::path& sky_configs_dir() noexcept
{
    return g_sky_configs_dir;
}

const std::filesystem::path& scripts_dir() noexcept
{
    return g_scripts_dir;
}

const std::filesystem::path& logs_dir() noexcept
{
    return g_logs_dir;
}

std::filesystem::path config_file(std::wstring_view name)
{
    if(g_config_dir.empty()) {
        return {};
    }

    return g_config_dir / name;
}

std::filesystem::path skybox_module_config()
{
    if(g_skybox_replacer_root.empty()) {
        return {};
    }

    return g_skybox_replacer_root / L"module.json";
}

bool is_disabled() noexcept
{
    if(g_stuff_root.empty()) {
        return false;
    }

    std::error_code ec;
    return std::filesystem::is_regular_file(g_stuff_root / L"DISABLE", ec);
}

std::string to_utf8(const std::filesystem::path& path)
{
    const std::wstring& wide = path.native();
    if(wide.empty()) {
        return {};
    }

    const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if(bytes <= 0) {
        return {};
    }

    std::string out(static_cast<std::size_t>(bytes), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), bytes, nullptr, nullptr);
    return out;
}

std::filesystem::path from_utf8(std::string_view text)
{
    if(text.empty()) {
        return {};
    }

    const int chars = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if(chars <= 0) {
        return {};
    }

    std::wstring wide(static_cast<std::size_t>(chars), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), chars);
    return std::filesystem::path { std::move(wide) };
}
} // namespace tw::plugin::paths
