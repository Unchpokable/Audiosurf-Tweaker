#include "pch.hxx"

#include "skybox/sky_paths.hxx"

#include "plugin/diagnostics.hxx"
#include "plugin/paths.hxx"

namespace
{
bool is_plain_name(std::string_view id) noexcept
{
    if(id.empty() || id == "." || id == "..") {
        return false;
    }

    for(const char c : id) {
        if(c == '/' || c == '\\' || c == ':' || static_cast<unsigned char>(c) < 0x20) {
            return false;
        }
    }

    return true;
}
} // namespace

namespace tw::skybox
{
std::string skybox_id(const std::filesystem::path& entry)
{
    return tw::plugin::paths::to_utf8(entry.filename());
}

std::filesystem::path skybox_path(std::string_view id)
{
    const std::filesystem::path& root = tw::plugin::paths::skyboxes_dir();
    if(root.empty()) {
        return {};
    }

    if(!is_plain_name(id)) {
        TW_LOG_WARNING("sky_paths: '{}' is not a name of an entry in the Skyboxes folder", id);
        return {};
    }

    return root / tw::plugin::paths::from_utf8(id);
}
} // namespace tw::skybox
