<#
.SYNOPSIS
    Builds a ready-to-run distributable bundle of Audiosurf Tweaker into /distr/.

.DESCRIPTION
    Publishes TweakerUI as a win-x64 Native AOT executable. No .NET runtime install is needed.
    Native Avalonia/Skia/HarfBuzz libraries and TweakerNative.dll remain beside the executable;
    there is no CoreCLR bundle or first-launch self-extraction.
    LegacyDataConverter stays on .NET Framework 4.8.1 and is merged by ILRepack.
    Native package debug symbols are removed from the public bundle.

    asbridge.exe is NOT built separately here - TweakerUI.csproj already has its own CMake
    pre-build hook (BuildAsBridge/CopyAsBridge targets) that runs as part of any build, including
    the one `dotnet publish` performs internally. It only lands in the intermediate build output
    though, not in the publish folder itself (that target copies to $(OutDir), which `dotnet
    publish`'s own file-tracking doesn't pick up since asbridge.exe isn't a project item) - so
    this script locates it after publish and copies it across explicitly as a separate step.

    LegacyDataConverter has no real .NET Framework equivalent of "self-contained" (the CLR itself
    is an OS component on net481, not something an app can bundle) - instead its own csproj pulls
    in ILRepack.Lib.MSBuild.Task, which IL-merges every dependency DLL straight into
    LegacyDataConverter.exe as part of a normal Release build. What comes out of `bin\Release\` is
    already a single dependency-free exe, so this script just copies that one file - no DLLs to
    drag along, no separate ServiceTools\ subfolder needed. TweakerCore.Engine.LegacyConverter
    shells out to "LegacyDataConverter.exe" expecting it directly next to TweakerUI.exe, which is
    exactly where this places it.

.PARAMETER Configuration
    Build configuration for both projects. Default: Release.

.PARAMETER Runtime
    RID for the self-contained TweakerUI publish. Default: win-x64 (the only supported target -
    the live game IPC is Windows-only regardless, see roadmap).

.EXAMPLE
    .\Scripts\Deploy.ps1
#>
[CmdletBinding()]
param(
    [string]$Configuration = "Release",
    [string]$Runtime = "win-x64"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$distDir = Join-Path $repoRoot "distr"
$tweakerUiProj = Join-Path $repoRoot "TweakerUI\TweakerUI.csproj"
$legacyConverterProj = Join-Path $repoRoot "LegacyDataConverter\LegacyDataConverter.csproj"

# The sky package and the script project are submodules: fail before distr\ is touched.
. (Join-Path $PSScriptRoot "DeployPreflight.ps1")
Assert-DeploySources -RepositoryRoot $repoRoot

Write-Host "==> Cleaning $distDir"
if (Test-Path $distDir) {
    # Retried - Explorer/Defender/the indexer transiently hold a directory handle open right after
    # a large exe is deleted from inside it often enough in practice that a single attempt flakes.
    for ($i = 0; $i -lt 5; $i++) {
        try { Remove-Item $distDir -Recurse -Force -ErrorAction Stop; break }
        catch { if ($i -eq 4) { throw }; Start-Sleep -Milliseconds 500 }
    }
}
New-Item -ItemType Directory -Path $distDir | Out-Null

# --- 1. TweakerUI: Native AOT publish -----------------------------------------------------
$tweakerOut = Join-Path $distDir "TweakerUI"
Write-Host "==> Publishing TweakerUI ($Configuration, $Runtime, Native AOT)"
dotnet publish $tweakerUiProj -c $Configuration -r $Runtime --self-contained true `
    -p:PublishAot=true `
    -p:DebugType=none -p:CopyOutputSymbolsToPublishDirectory=false `
    -o $tweakerOut
if ($LASTEXITCODE -ne 0) { throw "TweakerUI publish failed" }

# See file header - native SkiaSharp/HarfBuzzSharp *.pdb files ride along regardless of the
# DebugType/CopyOutputSymbolsToPublishDirectory switches above (those only cover managed PDBs).
Get-ChildItem -Path $tweakerOut -Filter "*.pdb" | Remove-Item -Force

Write-Host "==> Locating asbridge.exe from the CMake pre-build hook"
$asbridgeExe = Get-ChildItem -Path (Join-Path $repoRoot "TweakerUI\bin") -Recurse -Filter "asbridge.exe" -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $asbridgeExe) { throw "asbridge.exe not found under TweakerUI\bin - did the CMake pre-build hook run/fail silently?" }
Copy-Item $asbridgeExe.FullName -Destination $tweakerOut -Force
Write-Host "    $($asbridgeExe.FullName) -> $tweakerOut"

Write-Host "==> Locating InjectHelper.exe from the CMake pre-build hook"
$injectHelperExe = Get-ChildItem -Path (Join-Path $repoRoot "TweakerUI\bin") -Recurse -Filter "InjectHelper.exe" -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $injectHelperExe) { throw "InjectHelper.exe not found under TweakerUI\bin - did the CMake pre-build hook run/fail silently?" }
Copy-Item $injectHelperExe.FullName -Destination $tweakerOut -Force
Write-Host "    $($injectHelperExe.FullName) -> $tweakerOut"

# The plugin ships as PluginPayload\ - a mirror of the layout its files take inside the game, so
# installing it is a plain copy of the tree (Docs/Internal/plugin-offline-mode.md §6.1, §6.3):
#
#     PluginPayload\channels\TweakerPlugin.dll      the game loads this by itself at startup
#     PluginPayload\TweakerStuff\Scripts\*.lua      loose Lua files, edited without a rebuild
#     PluginPayload\TweakerStuff\SkyboxReplacer\Skyboxes\*.sky    shipped sky packages
#
# What belongs in there is decided by CopyTweakerPlugin, not here - including which scripts and sky
# packages ship at all (a dev\ subfolder in either source folder never does).
#
# The whole folder is copied rather than reassembled here on purpose: those paths are not
# cosmetic - PluginInstallation keys every file by its path relative to the payload root, and a
# DLL placed anywhere but channels\ installs to a folder the game never scans for channels. One
# place defines that layout (TweakerUI.csproj's CopyTweakerPlugin), and this copies what it made.
#
# Optional: the plugin's CMake build needs the DirectX/Quest3D SDKs and a vendored ImGui tree
# (none guaranteed present, see TweakerUI.csproj's BuildTweakerPlugin target), and installing it
# is an opt-in [Experimental] Settings toggle - a bundle without it is still a complete Tweaker.
Write-Host "==> Locating PluginPayload\ from the CMake pre-build hook (optional - in-game plugin)"
$pluginPayload = Get-ChildItem -Path (Join-Path $repoRoot "TweakerUI\bin") -Recurse -Directory -Filter "PluginPayload" -ErrorAction SilentlyContinue |
    Where-Object { Test-Path (Join-Path $_.FullName "channels\TweakerPlugin.dll") } |
    Sort-Object { (Get-Item (Join-Path $_.FullName "channels\TweakerPlugin.dll")).LastWriteTime } -Descending |
    Select-Object -First 1
if ($pluginPayload) {
    # By the dll's own timestamp, not the folder's: several build trees live under bin\ (x64\Debug,
    # the publish one, the msbuild hook's), and a directory's timestamp says when an entry was last
    # added to it, which is not the same as "this is the freshest plugin".
    Copy-Item $pluginPayload.FullName -Destination $tweakerOut -Recurse -Force
    Write-Host "    $($pluginPayload.FullName) -> $(Join-Path $tweakerOut 'PluginPayload')"
    Assert-PluginPayload -PayloadRoot $pluginPayload.FullName
} else {
    Write-Host "    PluginPayload\channels\TweakerPlugin.dll not found - the in-game plugin will be unavailable in this bundle (see TweakerPlugin/cmake/*.cmake)." -ForegroundColor Yellow
}

# --- 2. LegacyDataConverter: old-style net481 csproj, needs full MSBuild, not dotnet CLI -------
Write-Host "==> Locating MSBuild.exe (LegacyDataConverter is net481, dotnet CLI can't build it)"
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - is Visual Studio installed?" }
$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) { throw "MSBuild.exe not found via vswhere" }

Write-Host "==> Building LegacyDataConverter ($Configuration)"
# Platform=AnyCPU pinned explicitly - an ambient $env:Platform (e.g. "x64", set by some VS
# developer shells) otherwise leaks into MSBuild's property resolution and silently redirects
# the output to bin\x64\$Configuration\ instead of the project's own bin\$Configuration\,
# breaking the fixed path assumed below.
& $msbuild $legacyConverterProj "/p:Configuration=$Configuration" /p:Platform=AnyCPU /restore /verbosity:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw "LegacyDataConverter build failed" }

# ILRepack.Lib.MSBuild.Task's own default Release-only target already merged every dependency DLL
# into this one exe and deleted the loose copies - just the single file to copy, straight next to
# TweakerUI.exe (see file header / LegacyConverter.cs for why it must live there, not a subfolder).
$legacyExe = Join-Path $repoRoot "LegacyDataConverter\bin\$Configuration\LegacyDataConverter.exe"
if (-not (Test-Path $legacyExe)) { throw "LegacyDataConverter.exe not found at $legacyExe" }
Copy-Item $legacyExe -Destination $tweakerOut -Force
Write-Host "    $legacyExe -> $tweakerOut"

# --- 3. twllink: the Lua project linker, Native AOT --------------------------------------------
# One self-contained exe with no runtime to install (Docs/Internal/lua-engine-fix-roadmap.md §9.2,
# §9.9). It goes to DevTools\Binaries\ beside TweakerUI.exe - the folder the Script Dev tab runs it from
# and puts on an editor's PATH (Docs/Internal/script-manager.md §8.1). Not part of
# PluginPayload\ - it is a tool of the host, and the game never sees it. Published into its own
# folder and only the exe is taken: Native AOT leaves ~16 MB of symbols next to it.
$linkerProj = Join-Path $repoRoot "TweakerLuaLinker\TweakerLuaLinker.csproj"
$linkerOut = Join-Path $repoRoot "TweakerLuaLinker\bin\publish"
$devToolsOut = Join-Path $tweakerOut "DevTools\Binaries"
Write-Host "==> Publishing twllink ($Configuration, Native AOT)"
dotnet publish $linkerProj -c $Configuration -p:PublishAot=true -o $linkerOut
if ($LASTEXITCODE -ne 0) { throw "twllink publish failed - Native AOT links with MSVC (Visual Studio's C++ workload)" }
New-Item -ItemType Directory -Force -Path $devToolsOut | Out-Null
Copy-Item (Join-Path $linkerOut "twllink.exe") -Destination $devToolsOut -Force
Write-Host "    $(Join-Path $linkerOut 'twllink.exe') -> $devToolsOut"

# --- 4. The rest of DevTools\: CgrPy -----------------------------------------------------------
# What goes in is decided by TweakerUI.csproj's CopyDevTools target, which stages it as DevToolsPayload\
# (Docs/Internal/dev-bundle-roadmap.md §3.1); this only carries it over, as with PluginPayload\ above.
# The freshest of the bin\ trees is the one with the newest DevToolsPayload.stamp - the publish above
# has just written it. The files inside cannot tell: Copy keeps their source timestamps.
Write-Host "==> Locating DevToolsPayload\ (CgrPy) from the TweakerUI build"
$devToolsStamp = Get-ChildItem -Path (Join-Path $repoRoot "TweakerUI\bin") -Recurse -File -Filter "DevToolsPayload.stamp" -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $devToolsStamp) { throw "DevToolsPayload.stamp not found under TweakerUI\bin - did TweakerUI.csproj's CopyDevTools target run?" }
$devToolsPayload = Join-Path $devToolsStamp.DirectoryName "DevToolsPayload"
$devToolsRoot = Join-Path $tweakerOut "DevTools"
New-Item -ItemType Directory -Force -Path $devToolsRoot | Out-Null
Copy-Item (Join-Path $devToolsPayload "*") -Destination $devToolsRoot -Recurse -Force
Write-Host "    $devToolsPayload -> $devToolsRoot"

# --- Summary ------------------------------------------------------------------------------------
$stats = Get-ChildItem $distDir -Recurse -File | Measure-Object -Property Length -Sum
Write-Host "`n==> Bundle ready at $distDir"
Write-Host ("    {0} files, {1:N1} MB" -f $stats.Count, ($stats.Sum / 1MB))
