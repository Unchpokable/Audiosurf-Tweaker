#!/usr/bin/env bash
# Builds a ready-to-run distributable bundle of Audiosurf Tweaker into /distr/.
# Bash/Git-Bash-under-Windows counterpart of Deploy.ps1 - same steps, same output layout, kept in
# sync by hand since there's no cross-shell script-sharing story here worth the indirection.
#
# Publishes TweakerUI as a self-contained, single-file win-x64 app (bundled .NET runtime, no
# separate install required on the target machine, one exe instead of a ~250-file folder), builds
# LegacyDataConverter (the .NET Framework 4.8.1 legacy skin/palette converter - frozen off net10
# on purpose, see Directory.Build.props) through full MSBuild since it's an old-style csproj the
# dotnet CLI can't load, and drops both straight into /distr/TweakerUI/ as a single flat folder.
#
# PublishSingleFile bundles the managed app + .NET runtime into one exe; native assets
# (SkiaSharp/HarfBuzzSharp/Avalonia natives) can't run from inside that bundle, so
# IncludeNativeLibrariesForSelfExtract=true self-extracts just those to
# %TEMP%\.net\TweakerUI\<hash>\ on first launch and reuses that cache on later launches - this is
# expected .NET single-file behavior, not a bug, and needs no cleanup of its own. The two *.pdb
# files those native packages carry alongside their .dll (89MB/20MB - debug symbols for
# SkiaSharp/HarfBuzzSharp's native code, never loaded at runtime) get deleted explicitly below;
# neither DebugType=none nor CopyOutputSymbolsToPublishDirectory=false reaches them since they're
# copied as native package content, not through the managed PDB pipeline.
#
# asbridge.exe is NOT built separately here - TweakerUI.csproj already has its own CMake pre-build
# hook (BuildAsBridge/CopyAsBridge targets) that runs as part of any build, including the one
# `dotnet publish` performs internally. It only lands in the intermediate build output though, not
# in the publish folder itself (that target copies to $(OutDir), which `dotnet publish`'s own
# file-tracking doesn't pick up since asbridge.exe isn't a project item) - so this script locates
# it after publish and copies it across explicitly as a separate step.
#
# LegacyDataConverter has no real .NET Framework equivalent of "self-contained" (the CLR itself is
# an OS component on net481, not something an app can bundle) - instead its own csproj pulls in
# ILRepack.Lib.MSBuild.Task, which IL-merges every dependency DLL straight into
# LegacyDataConverter.exe as part of a normal Release build. What comes out of bin/Release/ is
# already a single dependency-free exe, so this script just copies that one file - no DLLs to drag
# along, no separate ServiceTools/ subfolder needed. TweakerCore.Engine.LegacyConverter shells out
# to "LegacyDataConverter.exe" expecting it directly next to TweakerUI.exe, which is exactly where
# this places it.
#
# Usage: Scripts/Deploy.sh [Configuration] [Runtime]
#   Configuration - default Release
#   Runtime       - RID for the self-contained TweakerUI publish, default win-x64 (the only
#                   supported target - the live game IPC is Windows-only regardless, see roadmap)

set -euo pipefail

CONFIGURATION="${1:-Release}"
RUNTIME="${2:-win-x64}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
DIST_DIR="$REPO_ROOT/distr"
TWEAKER_UI_PROJ="$REPO_ROOT/TweakerUI/TweakerUI.csproj"
LEGACY_CONVERTER_PROJ="$REPO_ROOT/LegacyDataConverter/LegacyDataConverter.csproj"

echo "==> Cleaning $DIST_DIR"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

# --- 1. TweakerUI: self-contained publish -------------------------------------------------------
TWEAKER_OUT="$DIST_DIR/TweakerUI"
echo "==> Publishing TweakerUI ($CONFIGURATION, $RUNTIME, self-contained single-file)"
dotnet publish "$TWEAKER_UI_PROJ" -c "$CONFIGURATION" -r "$RUNTIME" --self-contained true \
    -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:IncludeAllContentForSelfExtract=true \
    -p:DebugType=none -p:CopyOutputSymbolsToPublishDirectory=false \
    -o "$TWEAKER_OUT"

# See header - native SkiaSharp/HarfBuzzSharp *.pdb files ride along regardless of the
# DebugType/CopyOutputSymbolsToPublishDirectory switches above (those only cover managed PDBs).
rm -f "$TWEAKER_OUT"/*.pdb

echo "==> Locating asbridge.exe from the CMake pre-build hook"
ASBRIDGE_EXE="$(find "$REPO_ROOT/TweakerUI/bin" -iname "asbridge.exe" -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-)"
if [ -z "$ASBRIDGE_EXE" ]; then
    echo "ERROR: asbridge.exe not found under TweakerUI/bin - did the CMake pre-build hook run/fail silently?" >&2
    exit 1
fi
cp -f "$ASBRIDGE_EXE" "$TWEAKER_OUT/"
echo "    $ASBRIDGE_EXE -> $TWEAKER_OUT"

echo "==> Locating InjectHelper.exe from the CMake pre-build hook"
INJECT_HELPER_EXE="$(find "$REPO_ROOT/TweakerUI/bin" -iname "InjectHelper.exe" -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-)"
if [ -z "$INJECT_HELPER_EXE" ]; then
    echo "ERROR: InjectHelper.exe not found under TweakerUI/bin - did the CMake pre-build hook run/fail silently?" >&2
    exit 1
fi
cp -f "$INJECT_HELPER_EXE" "$TWEAKER_OUT/"
echo "    $INJECT_HELPER_EXE -> $TWEAKER_OUT"

# TweakerPlugin.dll is optional - its CMake build needs the DirectX/Quest3D SDKs and a vendored
# ImGui tree (none guaranteed present, see TweakerUI.csproj's BuildTweakerPlugin target), and the
# overlay itself is an opt-in [Experimental] Settings toggle - a bundle without it is still a
# complete, working Tweaker.
echo "==> Locating TweakerPlugin.dll from the CMake pre-build hook (optional - in-game overlay)"
TWEAKER_PLUGIN_DLL="$(find "$REPO_ROOT/TweakerUI/bin" -iname "TweakerPlugin.dll" -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-)"
if [ -n "$TWEAKER_PLUGIN_DLL" ]; then
    cp -f "$TWEAKER_PLUGIN_DLL" "$TWEAKER_OUT/"
    echo "    $TWEAKER_PLUGIN_DLL -> $TWEAKER_OUT"
else
    echo "    TweakerPlugin.dll not found - in-game overlay will be unavailable in this bundle (see TweakerPlugin/cmake/*.cmake)."
fi

# --- 2. LegacyDataConverter: old-style net481 csproj, needs full MSBuild, not dotnet CLI ---------
echo "==> Locating MSBuild.exe (LegacyDataConverter is net481, dotnet CLI can't build it)"
VSWHERE="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
if [ ! -f "$VSWHERE" ]; then
    echo "ERROR: vswhere.exe not found at $VSWHERE - is Visual Studio installed?" >&2
    exit 1
fi
MSBUILD="$("$VSWHERE" -latest -requires Microsoft.Component.MSBuild -find "MSBuild\\**\\Bin\\MSBuild.exe" | head -1)"
if [ -z "$MSBUILD" ]; then
    echo "ERROR: MSBuild.exe not found via vswhere" >&2
    exit 1
fi

echo "==> Building LegacyDataConverter ($CONFIGURATION)"
# Platform=AnyCPU pinned explicitly, and every switch below uses the single-dash long form
# (-p:/-restore/-nologo), not /p:/-restore - Git Bash rewrites a bare leading "/" into a POSIX
# path lookup (e.g. "/restore" -> some unrelated filesystem path), which silently corrupts the
# MSBuild command line. Single-dash long-form switches sidestep that entirely (MSBuild accepts
# both forms on Windows).
"$MSBUILD" "$LEGACY_CONVERTER_PROJ" -p:Configuration="$CONFIGURATION" -p:Platform=AnyCPU -restore -verbosity:minimal -nologo

# ILRepack.Lib.MSBuild.Task's own default Release-only target already merged every dependency DLL
# into this one exe and deleted the loose copies - just the single file to copy, straight next to
# TweakerUI.exe (see the header / LegacyConverter.cs for why it must live there, not a subfolder).
LEGACY_EXE="$REPO_ROOT/LegacyDataConverter/bin/$CONFIGURATION/LegacyDataConverter.exe"
if [ ! -f "$LEGACY_EXE" ]; then
    echo "ERROR: LegacyDataConverter.exe not found at $LEGACY_EXE" >&2
    exit 1
fi
cp -f "$LEGACY_EXE" "$TWEAKER_OUT/"
echo "    $LEGACY_EXE -> $TWEAKER_OUT"

# --- Summary ---------------------------------------------------------------------------------
FILE_COUNT="$(find "$DIST_DIR" -type f | wc -l)"
TOTAL_SIZE_MB="$(find "$DIST_DIR" -type f -printf '%s\n' | awk '{sum+=$1} END {printf "%.1f", sum/1048576}')"
echo ""
echo "==> Bundle ready at $DIST_DIR"
echo "    $FILE_COUNT files, ${TOTAL_SIZE_MB} MB"
