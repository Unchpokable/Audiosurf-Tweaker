#!/usr/bin/env bash
# Shared deploy preflight: the components the bundle ships are git submodules now
# (Docs/Internal/repo-layout-plan.md §3). Fail before distr/ is rebuilt rather than shipping a
# bundle with no sky package or a script whose project is not there.
#
# Source this file from a deploy script and call both functions.

assert_deploy_sources() {
    local repo_root="$1"
    local required=(
        "$repo_root/Skyboxes/Canvas.sky/Config.json"
        "$repo_root/ScriptProjects/puzzlepro_hud/twproject.json"
        "$repo_root/TweakerPlugin/assets/scripts/puzzlepro_hud.lua"
    )
    local missing=()
    local path

    for path in "${required[@]}"; do
        [ -e "$path" ] || missing+=("$path")
    done

    if [ "${#missing[@]}" -gt 0 ]; then
        printf 'Deploy sources are missing:\n' >&2
        printf '  %s\n' "${missing[@]}" >&2
        printf 'Run: git submodule update --init --recursive\n' >&2
        exit 1
    fi

    local skin_count
    skin_count="$(find "$repo_root/BundledSkins" -maxdepth 1 -type f \( -name '*.askin2' -o -name '*.tasp' \) 2>/dev/null | wc -l | tr -d ' ')"
    if [ "$skin_count" -eq 0 ]; then
        printf 'BundledSkins has no .askin2/.tasp packs:\n  %s\nRun: git submodule update --init --recursive\n' \
            "$repo_root/BundledSkins" >&2
        exit 1
    fi
}

assert_plugin_payload() {
    local payload_root="$1"
    local sky_root="$payload_root/TweakerStuff/SkyboxReplacer/Skyboxes"

    if [ ! -e "$sky_root/Canvas.sky/Config.json" ]; then
        printf 'PluginPayload has no Canvas.sky - the sky copy in TweakerUI.csproj did not run\n' >&2
        exit 1
    fi

    # The Skyboxes and Canvas.sky checkouts are submodules: a .git gitlink file and repository files
    # must never reach the player. TweakerUI.csproj excludes them; this checks the result.
    local junk
    junk="$(find "$sky_root" \( -name '.git*' -o -name '.github' -o -name '.tweaker' \) -print -quit 2>/dev/null || true)"
    if [ -n "$junk" ]; then
        printf 'Repository files reached the plugin payload:\n  %s\n' "$junk" >&2
        exit 1
    fi
}
