# Shared deploy preflight: the components the bundle ships are git submodules now
# (Docs/Internal/repo-layout-plan.md §3). Fail before distr/ is rebuilt rather than shipping a
# bundle with no sky package or a script whose project is not there.
#
# Dot-source this file from a deploy script and call both functions.

function Assert-DeploySources {
    param([Parameter(Mandatory)][string] $RepositoryRoot)

    $required = @(
        (Join-Path $RepositoryRoot "Skyboxes\Canvas.sky\Config.json"),
        (Join-Path $RepositoryRoot "ScriptProjects\puzzlepro_hud\twproject.json"),
        (Join-Path $RepositoryRoot "TweakerPlugin\assets\scripts\puzzlepro_hud.lua")
    )

    $missing = @($required | Where-Object { -not (Test-Path -LiteralPath $_) })
    if ($missing.Count -gt 0) {
        throw ("Deploy sources are missing:`n  " + ($missing -join "`n  ") +
               "`nRun: git submodule update --init --recursive")
    }

    $skins = @(Get-ChildItem (Join-Path $RepositoryRoot "BundledSkins") -File -ErrorAction SilentlyContinue |
               Where-Object { $_.Extension -in ".askin2", ".tasp" })
    if ($skins.Count -eq 0) {
        throw ("BundledSkins has no .askin2/.tasp packs:`n  " + (Join-Path $RepositoryRoot "BundledSkins") +
               "`nRun: git submodule update --init --recursive")
    }
}

function Assert-PluginPayload {
    param([Parameter(Mandatory)][string] $PayloadRoot)

    $skyRoot = Join-Path $PayloadRoot "TweakerStuff\SkyboxReplacer\Skyboxes"
    if (-not (Test-Path -LiteralPath (Join-Path $skyRoot "Canvas.sky\Config.json"))) {
        throw "PluginPayload has no Canvas.sky - the sky copy in TweakerUI.csproj did not run"
    }

    # The Skyboxes and Canvas.sky checkouts are submodules: a .git gitlink file and repository files
    # must never reach the player. TweakerUI.csproj excludes them; this checks the result.
    $junk = @(Get-ChildItem -LiteralPath $skyRoot -Recurse -Force -ErrorAction SilentlyContinue |
              Where-Object { $_.Name -like ".git*" -or $_.Name -eq ".github" -or $_.Name -eq ".tweaker" })
    if ($junk.Count -gt 0) {
        $paths = ($junk | ForEach-Object { $_.FullName }) -join "`n  "
        throw ("Repository files reached the plugin payload:`n  " + $paths)
    }
}
