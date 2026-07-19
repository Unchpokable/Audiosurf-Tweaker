<#
.SYNOPSIS
    Builds a ready-to-run distributable bundle of Audiosurf Tweaker into /distr/.

.DESCRIPTION
    Publishes TweakerUI as a self-contained, single-file win-x64 app (bundled .NET runtime, no
    separate install required on the target machine, one exe instead of a ~250-file folder),
    builds LegacyDataConverter (the .NET Framework 4.8.1 legacy skin/palette converter - frozen
    off net10 on purpose, see Directory.Build.props) through full MSBuild since it's an old-style
    csproj the dotnet CLI can't load, and drops both straight into /distr/TweakerUI/ as a single
    flat folder.

    PublishSingleFile bundles the managed app + .NET runtime into one exe; native assets
    (SkiaSharp/HarfBuzzSharp/Avalonia natives) can't run from inside that bundle, so
    IncludeNativeLibrariesForSelfExtract=true self-extracts just those to
    %TEMP%\.net\TweakerUI\<hash>\ on first launch and reuses that cache on later launches - this
    is expected .NET single-file behavior, not a bug, and needs no cleanup of its own. The two
    *.pdb files those native packages carry alongside their .dll (89MB/20MB - debug symbols for
    SkiaSharp/HarfBuzzSharp's native code, never loaded at runtime) get deleted explicitly below;
    neither DebugType=none nor CopyOutputSymbolsToPublishDirectory=false reaches them since
    they're copied as native package content, not through the managed PDB pipeline.

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

# --- 1. TweakerUI: self-contained publish -----------------------------------------------------
$tweakerOut = Join-Path $distDir "TweakerUI"
Write-Host "==> Publishing TweakerUI ($Configuration, $Runtime, self-contained single-file)"
dotnet publish $tweakerUiProj -c $Configuration -r $Runtime --self-contained true `
    -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:IncludeAllContentForSelfExtract=true `
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

# --- Summary ------------------------------------------------------------------------------------
$stats = Get-ChildItem $distDir -Recurse -File | Measure-Object -Property Length -Sum
Write-Host "`n==> Bundle ready at $distDir"
Write-Host ("    {0} files, {1:N1} MB" -f $stats.Count, ($stats.Sum / 1MB))
