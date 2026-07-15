#!/usr/bin/env bash
# Builds a debuggable, multi-file distributable bundle of Audiosurf Tweaker into /distr/.
# Bash/Git-Bash-under-Windows counterpart of DeployDebug.ps1 - same steps, same output layout,
# kept in sync by hand since there's no cross-shell script-sharing story here worth the
# indirection (same convention as Deploy.ps1/Deploy.sh).
#
# Debug counterpart of Deploy.sh. Where Deploy.sh collapses TweakerUI into one single-file exe
# for distribution, this script publishes it the "normal" way - one DLL per assembly plus the
# self-contained .NET runtime (~200+ files) and every PDB left in place (managed AND the native
# SkiaSharp/HarfBuzzSharp ones that Deploy.sh deletes) - so a debugger can attach to the running
# TweakerUI.exe process and resolve symbols/step through code, which PublishSingleFile's
# self-extracting bundle makes impractical (native assets only unpack to a temp cache on launch,
# and single-file managed assemblies confuse some debuggers' module resolution).
#
# LegacyDataConverter is NOT built Debug - it's not the thing being debugged here, it's built
# Release exactly like Deploy.sh does, so ILRepack's default merge target still IL-merges it into
# one dependency-free exe. Building it Debug instead was tried and reverted: a Debug build leaves
# its net481 System.Text.Json dependency closure as loose DLLs (System.Numerics.Vectors,
# System.Buffers, etc.), and those collide by filename with TweakerUI's own .NET 10 versions of
# the same assemblies once both sit in the same output folder - skipping the clobber breaks
# LegacyDataConverter instead (it needs its own net481-targeted copies, not TweakerUI's net10
# ones). Keeping it merged/Release sidesteps the collision entirely - single exe, nothing loose.
#
# asbridge.exe is located and copied the same way as Deploy.sh - its own CMake pre-build hook
# always builds it Release regardless of the managed Configuration (see TweakerUI.csproj), so
# there's nothing debug-specific to do there.
#
# Usage: Scripts/DeployDebug.sh [Configuration] [Runtime]
#   Configuration - build configuration for TweakerUI, default Debug. LegacyDataConverter always
#                   builds Release regardless of this value (see above).
#   Runtime       - RID for the self-contained TweakerUI publish, default win-x64 (the only
#                   supported target - the live game IPC is Windows-only regardless, see roadmap)

set -euo pipefail

CONFIGURATION="${1:-Debug}"
RUNTIME="${2:-win-x64}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
DIST_DIR="$REPO_ROOT/distr"
TWEAKER_UI_PROJ="$REPO_ROOT/TweakerUI/TweakerUI.csproj"
LEGACY_CONVERTER_PROJ="$REPO_ROOT/LegacyDataConverter/LegacyDataConverter.csproj"

echo "==> Cleaning $DIST_DIR"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

# --- 1. TweakerUI: multi-file publish, symbols kept ----------------------------------------------
TWEAKER_OUT="$DIST_DIR/TweakerUI"
echo "==> Publishing TweakerUI ($CONFIGURATION, $RUNTIME, self-contained, multi-file with PDBs)"
dotnet publish "$TWEAKER_UI_PROJ" -c "$CONFIGURATION" -r "$RUNTIME" --self-contained true \
    -o "$TWEAKER_OUT"

echo "==> Locating asbridge.exe from the CMake pre-build hook"
ASBRIDGE_EXE="$(find "$REPO_ROOT/TweakerUI/bin" -iname "asbridge.exe" -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-)"
if [ -z "$ASBRIDGE_EXE" ]; then
    echo "ERROR: asbridge.exe not found under TweakerUI/bin - did the CMake pre-build hook run/fail silently?" >&2
    exit 1
fi
cp -f "$ASBRIDGE_EXE" "$TWEAKER_OUT/"
echo "    $ASBRIDGE_EXE -> $TWEAKER_OUT"

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

echo "==> Building LegacyDataConverter (Release - stays IL-merged even in a debug bundle)"
# Deliberately always Release (not $CONFIGURATION) - see header. Every switch below uses the
# single-dash long form (-p:/-restore/-nologo), not /p:/-restore - Git Bash rewrites a bare
# leading "/" into a POSIX path lookup (e.g. "/restore" -> some unrelated filesystem path), which
# silently corrupts the MSBuild command line. Single-dash long-form switches sidestep that
# entirely (MSBuild accepts both forms on Windows).
"$MSBUILD" "$LEGACY_CONVERTER_PROJ" -p:Configuration=Release -p:Platform=AnyCPU -restore -verbosity:minimal -nologo

# ILRepack.Lib.MSBuild.Task's own default Release-only target already merged every dependency DLL
# into this one exe and deleted the loose copies - just the single file to copy, straight next to
# TweakerUI.exe (see LegacyConverter.cs for why it must live there, not a subfolder).
LEGACY_EXE="$REPO_ROOT/LegacyDataConverter/bin/Release/LegacyDataConverter.exe"
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
echo "==> Debug bundle ready at $DIST_DIR"
echo "    $FILE_COUNT files, ${TOTAL_SIZE_MB} MB"
echo "    Attach a debugger to TweakerUI.exe once running - PDBs are all present."
