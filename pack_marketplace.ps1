<#
.SYNOPSIS
    Packages BaseXWorldGen plugin for Unreal Marketplace distribution.
.DESCRIPTION
    (c) 2026 Keenan Dunham & Troll Hunter Gaming - All Rights Reserved.
    DRO, LOD, and Base-X Game Engine are patent pending (USPTO).

    Builds the plugin via RunUAT BuildPlugin for Win64 (Development + Shipping),
    then assembles a clean binary-only distribution:
      - Precompiled DLLs (no source .cpp files)
      - Public headers only (API surface)
      - Content assets (meshes, materials, DataTable)
      - ThirdParty headers (read-only, no .cpp)

    The output is ready to zip and submit to the Unreal Marketplace.
.EXAMPLE
    .\pack_marketplace.ps1
    .\pack_marketplace.ps1 -SkipBuild
#>

param(
    [switch]$SkipBuild,
    [string]$OutputDir = "C:\Users\Myrtle Beach Ads\source\repos\BaseXWorldGen\release\BaseXWorldGen-Marketplace"
)

$ErrorActionPreference = "Stop"

$UERoot     = "C:\Program Files\Epic Games\UE_5.7"
$RunUAT     = Join-Path $UERoot "Engine\Build\BatchFiles\RunUAT.bat"
$ProjectDir = "C:\Users\Myrtle Beach Ads\source\repos\BaseXWorldUE"
$Project    = Join-Path $ProjectDir "BaseXWorldUE.uproject"
$PluginDir  = Join-Path $ProjectDir "Plugins\BaseXWorldGen"
$PluginFile = Join-Path $PluginDir "BaseXWorldGen.uplugin"
$ContentDir = Join-Path $ProjectDir "Content\BaseX"
$TempBuild  = Join-Path $ProjectDir "PluginBuild"

Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "  Base-X World Generation Engine - Marketplace Packaging"         -ForegroundColor Cyan
Write-Host "  (c) 2026 Keenan Dunham & Troll Hunter Gaming"                  -ForegroundColor Cyan
Write-Host "  DRO, LOD, and Base-X Game Engine are patent pending (USPTO)"   -ForegroundColor DarkCyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

# ═══════════════════════════════════════════════════════════════════
# 1. BUILD via RunUAT BuildPlugin
# ═══════════════════════════════════════════════════════════════════
if (-not $SkipBuild)
{
    Write-Host "[1/5] Building plugin via RunUAT..." -ForegroundColor Yellow

    # Kill any running editor
    Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force 2>$null
    Start-Sleep 2

    if (Test-Path $TempBuild) { Remove-Item $TempBuild -Recurse -Force }

    & $RunUAT BuildPlugin `
        -Plugin="$PluginFile" `
        -Package="$TempBuild" `
        -TargetPlatforms=Win64 `
        -Rocket `
        -VS2022

    if ($LASTEXITCODE -ne 0) { throw "RunUAT BuildPlugin failed (exit code $LASTEXITCODE)." }
    Write-Host "  RunUAT build succeeded" -ForegroundColor Green
}
else
{
    Write-Host "[1/5] Skipping build (using existing binaries)" -ForegroundColor DarkYellow
}

# ═══════════════════════════════════════════════════════════════════
# 2. ASSEMBLE CLEAN DISTRIBUTION
# ═══════════════════════════════════════════════════════════════════
Write-Host ""
Write-Host "[2/5] Assembling binary distribution..." -ForegroundColor Yellow

if (Test-Path $OutputDir) { Remove-Item $OutputDir -Recurse -Force }
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$dist = $OutputDir

# -- .uplugin
Copy-Item $PluginFile $dist
Write-Host "  + BaseXWorldGen.uplugin" -ForegroundColor Green

# -- Binaries from RunUAT output (precompiled DLLs, no PDBs)
$builtBin = Join-Path $TempBuild "Binaries"
if (Test-Path $builtBin)
{
    $binDest = Join-Path $dist "Binaries"
    New-Item -ItemType Directory -Path $binDest -Force | Out-Null
    # Copy DLLs and .modules files — NO PDBs, NO source
    Get-ChildItem $builtBin -Recurse -File | Where-Object {
        $_.Extension -in @(".dll", ".modules") -or $_.Name -eq "UnrealEditor.modules"
    } | ForEach-Object {
        $rel = $_.FullName.Replace($builtBin, "")
        $destFile = Join-Path $binDest $rel
        $destDir = Split-Path $destFile -Parent
        if (!(Test-Path $destDir)) { New-Item $destDir -ItemType Directory -Force | Out-Null }
        Copy-Item $_.FullName $destFile
        Write-Host "  + Binaries$rel" -ForegroundColor Green
    }
}
else
{
    # Fallback: copy from plugin's own Binaries
    Write-Host "  Using existing plugin binaries..." -ForegroundColor DarkYellow
    $srcBin = Join-Path $PluginDir "Binaries"
    $binDest = Join-Path $dist "Binaries"
    if (Test-Path $srcBin)
    {
        New-Item -ItemType Directory -Path $binDest -Force | Out-Null
        Get-ChildItem $srcBin -Recurse -File | Where-Object {
            $_.Extension -in @(".dll", ".modules") -or $_.Name -eq "UnrealEditor.modules"
        } | ForEach-Object {
            $rel = $_.FullName.Replace($srcBin, "")
            $destFile = Join-Path $binDest $rel
            $destDir = Split-Path $destFile -Parent
            if (!(Test-Path $destDir)) { New-Item $destDir -ItemType Directory -Force | Out-Null }
            Copy-Item $_.FullName $destFile
            Write-Host "  + Binaries$rel" -ForegroundColor Green
        }
    }
}

# -- Intermediate/Build: only UHT generated headers (not .obj/.lib)
$builtIntermediate = Join-Path $TempBuild "Intermediate"
if (Test-Path $builtIntermediate)
{
    Get-ChildItem $builtIntermediate -Recurse -Filter "*.h" | Where-Object {
        $_.FullName -like "*\Inc\*"
    } | ForEach-Object {
        $rel = $_.FullName.Substring($builtIntermediate.Length)
        $destFile = $dist + "\Intermediate" + $rel
        $destDir = Split-Path $destFile -Parent
        if (!(Test-Path $destDir)) { New-Item $destDir -ItemType Directory -Force | Out-Null }
        Copy-Item $_.FullName $destFile
    }
    $genHeaders = @(Get-ChildItem (Join-Path $dist "Intermediate") -Recurse -Filter "*.h" -ErrorAction SilentlyContinue).Count
    Write-Host "  + Intermediate/ ($genHeaders UHT generated headers)" -ForegroundColor Green
}

# -- Source: Build.cs + PUBLIC headers ONLY (no Private/*.cpp)
$srcDest = Join-Path $dist "Source\BaseXWorldGen"
New-Item -ItemType Directory -Path $srcDest -Force | Out-Null
New-Item -ItemType Directory -Path "$srcDest\Public" -Force | Out-Null
New-Item -ItemType Directory -Path "$srcDest\Private" -Force | Out-Null

# Build.cs is required by UBT
Copy-Item (Join-Path $PluginDir "Source\BaseXWorldGen\BaseXWorldGen.Build.cs") $srcDest
Write-Host "  + Source/BaseXWorldGen/BaseXWorldGen.Build.cs" -ForegroundColor Green

# Public headers (API surface customers need)
Get-ChildItem (Join-Path $PluginDir "Source\BaseXWorldGen\Public") -Filter "*.h" | ForEach-Object {
    Copy-Item $_.FullName "$srcDest\Public\"
    Write-Host "  + Source/Public/$($_.Name)" -ForegroundColor Green
}

# Private: EMPTY stub .cpp files that just include the header
# (UBT needs at least one .cpp per module, the real code is in the DLL)
$stubContent = @"
// BaseXWorldGen - Precompiled binary distribution
// (c) 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.
// DRO, LOD, and Base-X Game Engine are patent pending (USPTO).
// Source code is proprietary. Implementation is in the precompiled DLL.
#include "BaseXWorldGenModule.h"
"@
$stubContent | Out-File -FilePath "$srcDest\Private\BaseXWorldGenModule.cpp" -Encoding utf8
Write-Host "  + Source/Private/BaseXWorldGenModule.cpp (stub)" -ForegroundColor DarkGreen

# -- ThirdParty: headers only (no .cpp implementation)
$tpDest = Join-Path $dist "ThirdParty\BaseX"
New-Item -ItemType Directory -Path $tpDest -Force | Out-Null
Get-ChildItem (Join-Path $PluginDir "ThirdParty\BaseX") -Filter "*.h" | ForEach-Object {
    Copy-Item $_.FullName $tpDest
    Write-Host "  + ThirdParty/BaseX/$($_.Name)" -ForegroundColor Green
}

# ThirdParty README
Copy-Item (Join-Path $PluginDir "ThirdParty\BaseX\README.txt") $tpDest -ErrorAction SilentlyContinue

# -- Content assets (meshes, materials, DataTable)
if (Test-Path $ContentDir)
{
    $contentDest = Join-Path $dist "Content\BaseX"
    New-Item -ItemType Directory -Path $contentDest -Force | Out-Null
    Copy-Item "$ContentDir\*" $contentDest -Recurse
    $assetCount = (Get-ChildItem $contentDest -Recurse -Filter "*.uasset").Count
    Write-Host "  + Content/BaseX/ ($assetCount assets)" -ForegroundColor Green
}

# ═══════════════════════════════════════════════════════════════════
# 3. STRIP DANGEROUS FILES (double-check)
# ═══════════════════════════════════════════════════════════════════
Write-Host ""
Write-Host "[3/5] Verifying no source code leaked..." -ForegroundColor Yellow

$leaked = Get-ChildItem $dist -Recurse -File | Where-Object {
    $_.Extension -eq ".cpp" -and $_.FullName -notlike "*Stub*" -and $_.FullName -notlike "*Module*"
}
$pdbLeaked = Get-ChildItem $dist -Recurse -File -Filter "*.pdb"

if ($leaked.Count -gt 0)
{
    Write-Host "  WARNING: Found .cpp files - removing:" -ForegroundColor Red
    $leaked | ForEach-Object {
        Remove-Item $_.FullName -Force
        Write-Host "    REMOVED: $($_.Name)" -ForegroundColor Red
    }
}
else
{
    Write-Host "  No source .cpp files found (clean)" -ForegroundColor Green
}

if ($pdbLeaked.Count -gt 0)
{
    Write-Host "  Removing PDB debug symbols:" -ForegroundColor Yellow
    $pdbLeaked | ForEach-Object {
        Remove-Item $_.FullName -Force
        Write-Host "    REMOVED: $($_.Name)" -ForegroundColor Yellow
    }
}

# Verify no DRO.cpp leaked (proprietary algorithm)
$droCpp = Get-ChildItem $dist -Recurse -Filter "DRO.cpp"
if ($droCpp.Count -gt 0)
{
    $droCpp | ForEach-Object { Remove-Item $_.FullName -Force }
    Write-Host "  REMOVED: DRO.cpp (proprietary)" -ForegroundColor Red
}

Write-Host "  Source protection verified" -ForegroundColor Green

# ═══════════════════════════════════════════════════════════════════
# 4. WRITE LEGAL NOTICES
# ═══════════════════════════════════════════════════════════════════
Write-Host ""
Write-Host "[4/5] Writing legal notices..." -ForegroundColor Yellow

$readmeLines = @(
    "# Base-X World Generation Engine - Unreal Plugin v1.1.0",
    "",
    "Copyright 2026 Keenan Dunham and Troll Hunter Gaming. All Rights Reserved.",
    "DRO (Dynamic Radix Optimization), LOD, and Base-X Game Engine are patent pending (USPTO).",
    "",
    "## What's New in v1.1",
    "",
    "- **Unified Hub UI** — Single tabbed panel with World Prompt, Asset Mapper, and Settings tabs",
    "- **Bring Your Own Assets** — Scan project, auto-match, and assign your Megascans/Marketplace packs to 186 keywords",
    "- **6 Terrain Sizes** — Tiny (16x16) through Epic (512x512) with automatic world scaling",
    "- **24 Built-In Presets** — Double the one-click worlds (12 new: Fjord, Lava Canyon, Swamp, Highland, Coral Reef, Battlefield, Monastery, Dark Elf, Frontier, Ice Cave, Mesa, Haunted)",
    "",
    "## Overview",
    "",
    "NLP prompt-driven procedural world generation for Unreal Engine 5.7.",
    "Type a natural language description and get a complete 3D world with terrain,",
    "biomes, structures, and weather - all generated in real-time.",
    "",
    "## Features",
    "",
    "- 186 structure types with procedural meshes (trees, buildings, ruins, crystals)",
    "- 26 terrain types (mountain, canyon, volcano, fjord, archipelago)",
    "- 52 biome types with unique color palettes",
    "- 38 weather effects (rain, storm, snow, aurora, eclipse)",
    "- DRO precision scaling - patent-pending radix optimization",
    "- 24 built-in presets - one-click world generation",
    "- 6 terrain size presets (16x16 to 512x512)",
    "- Asset Mapper with project scanner and auto-match",
    "- Tabbed Hub UI - click keywords to build prompts",
    "",
    "## What This Plugin Does (and Doesn't Do)",
    "",
    "**This is a world art generation tool, not a level design system.**",
    "",
    "### Intended Workflow",
    "1. Generate world with Base-X",
    "2. Identify sections you like",
    "3. Copy/paste or manually edit in persistent layers",
    "4. Lock those areas using Unreal's built-in tools",
    "5. Regenerate other sections or start new scenes",
    "",
    "### What It Does Well",
    "- Generates visually interesting starting points in seconds",
    "- Lets artists map existing asset libraries to generation keywords",
    "- Produces variety across multiple terrain sizes and moods",
    "- Creates atmospheric environments for prototyping or backgrounds",
    "",
    "### What It Intentionally Doesn't Do",
    "- Does NOT replace level designers or validate gameplay rules",
    "- Does NOT generate playable layouts (navmesh, encounter design, sightlines)",
    "- Does NOT persist manual edits across regenerations",
    "- Does NOT provide layer locking (use Unreal's World Partition/Layer system)",
    "",
    "### Best Used For",
    "- Concept art and mood boards",
    "- Vertical slices and prototypes",
    "- Non-interactive backgrounds (skyboxes, distant vistas)",
    "- Starting points for manual level design",
    "- Narrative-driven environments where layout doesn't matter",
    "",
    "Use Unreal's existing tools for designer workflow (Layer system, World Partition,",
    "Blueprint actors with bIsEditorOnly, version control for manual edits).",
    "",
    "## Quick Start",
    "",
    "1. Enable the plugin in Edit > Plugins",
    "2. Drop ABaseXPromptDemo into any level",
    "3. Hit Play",
    "4. Use the World Prompt tab to type or click-build a prompt",
    "5. Switch to Asset Mapper to assign your own meshes (optional)",
    "6. Choose a terrain size and Generate World",
    "",
    "## System Requirements",
    "",
    "- Unreal Engine 5.7",
    "- Windows 10/11 x64",
    "- Vulkan-capable GPU",
    "",
    "## Legal",
    "",
    "This plugin is distributed as precompiled binaries.",
    "Source code is proprietary and not included.",
    "Reverse engineering, decompilation, or redistribution is prohibited.",
    "",
    "For licensing inquiries: contact Troll Hunter Gaming"
)
$readmeLines | Out-File -FilePath (Join-Path $dist "README.md") -Encoding utf8
Write-Host "  + README.md" -ForegroundColor Green

# Copy master EULA from repo root
$eulaSource = Join-Path $PSScriptRoot "EULA.txt"
if (Test-Path $eulaSource)
{
    Copy-Item $eulaSource (Join-Path $dist "LICENSE.txt")
    Write-Host "  + LICENSE.txt (Ethical Technology EULA)" -ForegroundColor Green
}
else
{
    Write-Host "  WARNING: EULA.txt not found at $eulaSource" -ForegroundColor Red
}

# ═══════════════════════════════════════════════════════════════════
# 5. SUMMARY
# ═══════════════════════════════════════════════════════════════════
Write-Host ""
Write-Host "[5/5] Package Summary" -ForegroundColor Green
Write-Host "================================================================" -ForegroundColor Cyan

$allFiles = Get-ChildItem $dist -Recurse -File
$categories = @{
    "DLLs"     = ($allFiles | Where-Object { $_.Extension -eq ".dll" })
    "Headers"  = ($allFiles | Where-Object { $_.Extension -eq ".h" })
    "Assets"   = ($allFiles | Where-Object { $_.Extension -eq ".uasset" })
    "Config"   = ($allFiles | Where-Object { $_.Extension -in @(".uplugin", ".cs", ".modules", ".txt", ".md") })
    "Other"    = ($allFiles | Where-Object { $_.Extension -notin @(".dll", ".h", ".uasset", ".uplugin", ".cs", ".modules", ".txt", ".md", ".cpp") })
}

foreach ($cat in $categories.GetEnumerator() | Sort-Object Name)
{
    $count = @($cat.Value).Count
    $size = [math]::Round(($cat.Value | Measure-Object -Property Length -Sum).Sum / 1MB, 2)
    Write-Host "  $($cat.Key.PadRight(12)) $count files  ($size MB)" -ForegroundColor White
}

$totalMB = [math]::Round(($allFiles | Measure-Object -Property Length -Sum).Sum / 1MB, 2)
Write-Host ""
Write-Host "  Total: $($allFiles.Count) files, $totalMB MB" -ForegroundColor Cyan
Write-Host "  Output: $dist" -ForegroundColor Green

# Security check
Write-Host ""
Write-Host "  SECURITY AUDIT:" -ForegroundColor Yellow
$cppFiles = $allFiles | Where-Object { $_.Extension -eq ".cpp" }
$pdbFiles = $allFiles | Where-Object { $_.Extension -eq ".pdb" }
$srcCheck = $cppFiles | Where-Object { $_.Length -gt 1024 }  # real source > 1KB
if ($srcCheck.Count -eq 0) {
    Write-Host "    [PASS] No proprietary source code in package" -ForegroundColor Green
} else {
    Write-Host "    [FAIL] Source code detected!" -ForegroundColor Red
    $srcCheck | ForEach-Object { Write-Host "      $($_.Name) ($($_.Length) bytes)" -ForegroundColor Red }
}
if ($pdbFiles.Count -eq 0) {
    Write-Host "    [PASS] No debug symbols (PDB) in package" -ForegroundColor Green
} else {
    Write-Host "    [FAIL] PDB files detected!" -ForegroundColor Red
}

Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "  Ready for Unreal Marketplace submission!" -ForegroundColor Green
Write-Host "  Zip the folder: $dist" -ForegroundColor White
Write-Host "================================================================" -ForegroundColor Cyan
