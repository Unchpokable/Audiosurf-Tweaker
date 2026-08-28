#include "pch.hxx"

#include "skybox/sky_paths.hxx"

#include "plugin/diagnostics.hxx"
#include "plugin/globals.hxx"

namespace tw::skybox
{
std::filesystem::path dll_directory()
{
    wchar_t wide_path[MAX_PATH] {};
    const DWORD len = ::GetModuleFileNameW(tw::plugin::globals::module_handle, wide_path, MAX_PATH);
    if(len == 0 || len >= MAX_PATH) {
        return {};
    }

    return std::filesystem::path { wide_path }.parent_path();
}

// Resolves a relative skybox path against every root it could sensibly mean, and says out loud
// which ones it tried.
//
// Resolving against the DLL alone was a bad guess: the plugin is injected from wherever the Tweaker
// happens to be installed, which is nowhere near the game, and nowhere anyone would think to drop a
// folder of sky images. The game's own working directory is engine/, and its parent is the game
// root - both are far more natural places to put them.
//
// Returns an empty path when nothing matched, having logged each candidate. That listing is the
// whole point: "it does not work wherever I put it" is unanswerable without it.
std::filesystem::path resolve_source_path(std::string_view path)
{
    const std::filesystem::path candidate { path };

    std::error_code ec;

    if(candidate.is_absolute()) {
        if(std::filesystem::exists(candidate, ec)) {
            return candidate;
        }

        TW_LOG_ERROR("sky_paths: '{}' does not exist", candidate.string());
        return {};
    }

    const std::array<std::filesystem::path, 3> roots {
        dll_directory(),                                 // next to TweakerPlugin.dll (and its .cfg)
        std::filesystem::current_path(ec),               // the game's CWD, i.e. engine/
        std::filesystem::current_path(ec).parent_path(), // the game root, one above engine/
    };

    for(const auto& root : roots) {
        if(root.empty()) {
            continue;
        }

        const std::filesystem::path resolved = root / candidate;
        if(std::filesystem::exists(resolved, ec)) {
            TW_LOG_INFO("sky_paths: resolved '{}' to '{}'", path, resolved.string());
            return resolved;
        }
    }

    TW_LOG_ERROR("sky_paths: could not find '{}'. Tried, in order:", path);
    for(const auto& root : roots) {
        if(!root.empty()) {
            TW_LOG_ERROR("sky_paths:   {}", (root / candidate).string());
        }
    }
    TW_LOG_ERROR("sky_paths: an absolute path sidesteps all of this");

    return {};
}
} // namespace tw::skybox
