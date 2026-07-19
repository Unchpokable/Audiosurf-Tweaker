<#
.SYNOPSIS
    Builds a debuggable, multi-file distributable bundle of Audiosurf Tweaker into /distr/.

.DESCRIPTION
    Debug counterpart of Deploy.ps1. Where Deploy.ps1 collapses TweakerUI into one single-file
    exe for distribution, this script publishes it the "normal" way - one DLL per assembly plus
    the self-contained .NET runtime (~200+ files) and every PDB left in place (managed AND the
    native SkiaSharp/HarfBuzzSharp ones that Deploy.ps1 deletes) - so a debugger can attach to the
    running TweakerUI.exe process and resolve symbols/step through code, which PublishSingleFile's
    self-extracting bundle makes impractical (native assets only unpack to a temp cache on launch,
    and single-file managed assemblies confuse some debuggers' module resolution).

    LegacyDataConverter is NOT built Debug - it's not the thing being debugged here, it's built
    Release exactly like Deploy.ps1 does, so ILRepack's default merge target still IL-merges it
    into one dependency-free exe. Building it Debug instead was tried and reverted: a Debug build
    leaves its net481 System.Text.Json dependency closure as loose DLLs (System.Numerics.Vectors,
    System.Buffers, etc.), and those collide by filename with TweakerUI's own .NET 10 versions of
    the same assemblies once both sit in the same output folder - skipping the clobber breaks
    LegacyDataConverter instead (it needs its own net481-targeted copies, not TweakerUI's net10
    ones). Keeping it merged/Release sidesteps the collision entirely - single exe, nothing loose.

    asbridge.exe is located and copied the same way as Deploy.ps1 - its own CMake pre-build hook
    always builds it Release regardless of the managed Configuration (see TweakerUI.csproj), so
    there's nothing debug-specific to do there.

.PARAMETER Configuration
    Build configuration for TweakerUI. Default: Debug. LegacyDataConverter always builds Release
    regardless of this value (see DESCRIPTION).

.PARAMETER Runtime
    RID for the self-contained TweakerUI publish. Default: win-x64 (the only supported target -
    the live game IPC is Windows-only regardless, see roadmap).

.EXAMPLE
    .\Scripts\DeployDebug.ps1
#>
[CmdletBinding()]
param(
    [string]$Configuration = "Debug",
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

# --- 1. TweakerUI: multi-file publish, symbols kept ---------------------------------------------
$tweakerOut = Join-Path $distDir "TweakerUI"
Write-Host "==> Publishing TweakerUI ($Configuration, $Runtime, self-contained, multi-file with PDBs)"
dotnet publish $tweakerUiProj -c $Configuration -r $Runtime --self-contained true -o $tweakerOut
if ($LASTEXITCODE -ne 0) { throw "TweakerUI publish failed" }

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

Write-Host "==> Building LegacyDataConverter (Release - stays IL-merged even in a debug bundle)"
# Deliberately always Release (not $Configuration) - see file header. Platform=AnyCPU pinned
# explicitly - an ambient $env:Platform (e.g. "x64", set by some VS developer shells) otherwise
# leaks into MSBuild's property resolution and silently redirects the output to
# bin\x64\Release\ instead of the project's own bin\Release\, breaking the fixed path below.
& $msbuild $legacyConverterProj "/p:Configuration=Release" /p:Platform=AnyCPU /restore /verbosity:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw "LegacyDataConverter build failed" }

# ILRepack.Lib.MSBuild.Task's own default Release-only target already merged every dependency DLL
# into this one exe and deleted the loose copies - just the single file to copy, straight next to
# TweakerUI.exe (see LegacyConverter.cs for why it must live there, not a subfolder).
$legacyExe = Join-Path $repoRoot "LegacyDataConverter\bin\Release\LegacyDataConverter.exe"
if (-not (Test-Path $legacyExe)) { throw "LegacyDataConverter.exe not found at $legacyExe" }
Copy-Item $legacyExe -Destination $tweakerOut -Force
Write-Host "    $legacyExe -> $tweakerOut"

# --- Summary ------------------------------------------------------------------------------------
$stats = Get-ChildItem $distDir -Recurse -File | Measure-Object -Property Length -Sum
Write-Host "`n==> Debug bundle ready at $distDir"
Write-Host ("    {0} files, {1:N1} MB" -f $stats.Count, ($stats.Sum / 1MB))
Write-Host "    Attach a debugger to TweakerUI.exe once running - PDBs are all present."
