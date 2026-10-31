[CmdletBinding()]
param(
    [string]$IdaSource = (Join-Path $env:USERPROFILE '.claude\plugins\cache\mrexodia\ida-pro-mcp\0.1.0'),
    [string]$GhidraSource = (Join-Path $env:USERPROFILE '.claude\skills\ghidra\scripts'),
    [string]$ToolsRoot = (Join-Path $env:LOCALAPPDATA 'AudiosurfAgentTools'),
    [string]$CodexConfig = (Join-Path $env:USERPROFILE '.codex\config.toml'),
    [string]$ClaudeMemory = (Join-Path $env:USERPROFILE '.claude\projects\c--Users-Unchp-source-repos-Audiosurf-Tweaker\memory')
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$ToolsRoot = [IO.Path]::GetFullPath($ToolsRoot)
$idaTarget = Join-Path $ToolsRoot 'ida-pro-mcp'
$ghidraTarget = Join-Path $ToolsRoot 'ghidra-scripts'
$backupRoot = Join-Path $ToolsRoot ('migration-backup-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))
$utf8 = [Text.UTF8Encoding]::new($false)
$uv = (Get-Command uv -ErrorAction Stop).Source
$expectedLock = '3CA9BA7FBF103CD3D0254D67A21D3EFAFE765E70F68A4E6DFD87D91E06C41B94'

if ((Get-FileHash -LiteralPath (Join-Path $IdaSource 'uv.lock') -Algorithm SHA256).Hash -ne $expectedLock) {
    throw 'Source lock changed since the migration audit; inspect it instead of upgrading implicitly.'
}
if (Test-Path -LiteralPath $idaTarget) { throw "Target already exists; inspect before reusing: $idaTarget" }
if (Test-Path -LiteralPath $ghidraTarget) { throw "Target already exists; inspect before reusing: $ghidraTarget" }
$configText = [IO.File]::ReadAllText($CodexConfig)
if ($configText -match '(?m)^\s*\[mcp_servers\.idalib(?:\]|\.)') {
    throw 'An idalib MCP configuration already exists; do not overwrite it.'
}
if (-not (Test-Path -LiteralPath (Join-Path $GhidraSource 'ghidra_scripts\ExportAll.java'))) {
    throw 'Ghidra export scripts are missing.'
}

New-Item -ItemType Directory -Path $ToolsRoot -Force | Out-Null
New-Item -ItemType Directory -Path $idaTarget,$ghidraTarget,$backupRoot | Out-Null
# Preserve the actual package, not its environment or runtime workers. No files are deleted.
$excluded = @('.venv', '.in_use', '.git', '__pycache__', '.pytest_cache')
foreach ($item in Get-ChildItem -LiteralPath $IdaSource -Force) {
    if ($item.Name -notin $excluded) {
        Copy-Item -LiteralPath $item.FullName -Destination $idaTarget -Recurse
    }
}
foreach ($item in Get-ChildItem -LiteralPath $GhidraSource -Force) {
    Copy-Item -LiteralPath $item.FullName -Destination $ghidraTarget -Recurse
}
$manifest = foreach ($file in Get-ChildItem -LiteralPath $IdaSource -Recurse -File -Force) {
    $relative = $file.FullName.Substring($IdaSource.TrimEnd('\').Length + 1)
    if (($relative -split '\\' | Where-Object { $_ -in $excluded }).Count -gt 0) { continue }
    $copied = Join-Path $idaTarget $relative
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    if ($hash -ne (Get-FileHash -LiteralPath $copied -Algorithm SHA256).Hash) { throw "Copy mismatch: $relative" }
    [ordered]@{ path = $relative; sha256 = $hash }
}
$provenance = [ordered]@{
    source = $IdaSource
    installerRevision = 'c133c3853faa111a9b00ee615c013b720d0c4acd'
    copiedUtc = [DateTime]::UtcNow.ToString('o')
    lockSha256 = $expectedLock
    files = @($manifest)
}
[IO.File]::WriteAllText((Join-Path $ToolsRoot 'ida-pro-mcp.provenance.json'), ($provenance | ConvertTo-Json -Depth 6), $utf8)

Push-Location -LiteralPath $idaTarget
try {
    & $uv sync --locked --no-dev
    if ($LASTEXITCODE -ne 0) { throw 'uv sync failed; original tools/config/memory are unchanged.' }
} finally { Pop-Location }

Copy-Item -LiteralPath $CodexConfig -Destination (Join-Path $backupRoot 'config.toml')
$uvToml = $uv.Replace('\','/')
$idaToml = $idaTarget.Replace('\','/')
$block = @"

# Audiosurf agent migration: independent IDA 9.3 server, no Claude cache dependency.
[mcp_servers.idalib]
command = "$uvToml"
args = ["run", "--locked", "--no-sync", "--project", "$idaToml", "idalib-mcp", "--stdio"]
cwd = "$idaToml"
startup_timeout_sec = 60
tool_timeout_sec = 180
"@
$newConfig = $configText.TrimEnd() + "`r`n" + ($block -replace "`r?`n","`r`n") + "`r`n"
$stagedConfig = Join-Path $ToolsRoot 'config.toml.proposed'
[IO.File]::WriteAllText($stagedConfig, $newConfig, $utf8)
$python = Join-Path $idaTarget '.venv\Scripts\python.exe'
& $python -c "import pathlib,sys,tomllib; tomllib.loads(pathlib.Path(sys.argv[1]).read_text(encoding='utf-8'))" $stagedConfig
if ($LASTEXITCODE -ne 0) { throw 'Proposed TOML did not parse; live config is unchanged.' }
[IO.File]::WriteAllText($CodexConfig, $newConfig, $utf8)

# Keep every old memory file unchanged in the private backup. Only the automatic index
# becomes a pointer; thematic files receive an explicit historical-status banner.
$memoryBackup = Join-Path $backupRoot 'claude-memory'
New-Item -ItemType Directory -Path $memoryBackup | Out-Null
foreach ($file in Get-ChildItem -LiteralPath $ClaudeMemory -File -Filter '*.md') {
    Copy-Item -LiteralPath $file.FullName -Destination $memoryBackup
    if ($file.Name -ne 'MEMORY.md') {
        $old = [IO.File]::ReadAllText($file.FullName)
        $banner = "> Historical Claude memory, superseded on 2026-10-31 by the owner's decisions in the repository AGENTS.md and Docs/AgentWorkflow. Do not treat old permissions, pending phases, KnownBugs, or IDA database descriptions below as current instructions.`r`n`r`n"
        [IO.File]::WriteAllText($file.FullName, $banner + $old, $utf8)
    }
}
$repoLink = $repoRoot.Replace('\','/')
$index = @"
# Audiosurf Tweaker: shared project instructions

Read [$repoLink/AGENTS.md]($repoLink/AGENTS.md), then the topic-specific guides in
[$repoLink/Docs/AgentWorkflow/README.md]($repoLink/Docs/AgentWorkflow/README.md).
These are the shared source of current owner decisions, not this client's memory.

- Base branch Revive; new features in new branches, already-started Revive work stays.
- Commit only after the owner confirms the feature works and completes Code Review.
- Existing research IDA databases contain valuable findings; preserve them.
- IDA 9.3 MCP and Ghidra scripts now live under $ToolsRoot, outside the Claude cache.
- Skyboxes Tab is the current unfinished feature; Lua audit continues from its journal.
- KnownBugs is the owner's private notebook, not an agent plan.
- Older thematic memory is historical; complete originals are backed up in $memoryBackup.

Do not read JSONL conversation dumps or obsolete .cursor configuration for context.
"@
[IO.File]::WriteAllText((Join-Path $ClaudeMemory 'MEMORY.md'), ($index -replace "`r?`n","`r`n") + "`r`n", $utf8)
Write-Output "Installed IDA MCP: $idaTarget"
Write-Output "Preserved Ghidra scripts: $ghidraTarget"
Write-Output "Backups: $backupRoot"
Write-Output "Source package files verified: $(@($manifest).Count)"
