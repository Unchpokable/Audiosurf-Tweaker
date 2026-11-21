# Initializes the private Quest3D SDK submodule for a release or nightly build.
#
# The SDK is proprietary and is not in the public tree. The supported layout is a private
# organization repository mounted at TweakerPlugin/Quest3D-SDK with `update = none`, so public
# clones skip it and only a workflow with a token can check it out
# (Docs/Internal/repo-layout-plan.md, Tools/ReleaseChannel/README.md).
#
# Without a submodule or without the token the build continues without the in-game plugin: that
# is the UI-only bundle, and saying so out loud beats failing a release for a missing optional
# credential. A configured submodule with a bad token fails instead - a credential that is
# present but broken must be visible.
param(
    [Parameter(Mandatory)][string] $RepositoryRoot,
    [string] $Token = $env:QUEST3D_TOKEN
)

$ErrorActionPreference = "Stop"

$root = (Resolve-Path $RepositoryRoot).Path
$path = "TweakerPlugin/Quest3D-SDK"
$gitmodules = Join-Path $root ".gitmodules"

if (-not (Test-Path $gitmodules) -or
    -not (Select-String -Path $gitmodules -SimpleMatch "path = $path" -Quiet)) {
    Write-Host "No Quest3D SDK submodule in .gitmodules - building without the in-game plugin"
    exit 0
}

if (-not $Token) {
    Write-Warning "QUEST3D_TOKEN is not set - building without the in-game plugin"
    exit 0
}

$header = "AUTHORIZATION: basic " +
    [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes("x-access-token:$Token"))

try {
    # Global, not local: the submodule clone runs as a separate git process and reads the global
    # config, not the superproject's .git/config. On a hosted runner HOME points at a throwaway
    # directory; on a self-hosted one the finally block removes it again.
    git -C $root config --global "http.https://github.com/.extraheader" $header

    # The submodule is deliberately `update = none` in .gitmodules, and git skips it even when the
    # path is named explicitly. Override the update strategy for this one invocation.
    git -C $root -c "submodule.$path.update=checkout" submodule update --init -- $path
    if ($LASTEXITCODE -ne 0) { throw "git submodule update failed for $path" }

    $headerFile = Join-Path $root "$path/include/A3d_EngineInterface.h"
    if (-not (Test-Path -LiteralPath $headerFile)) {
        throw "Quest3D SDK checkout did not produce $path/include/A3d_EngineInterface.h"
    }
    Write-Host "Quest3D SDK initialized at $path"
}
finally {
    git -C $root config --global --unset-all "http.https://github.com/.extraheader" 2>$null
}
