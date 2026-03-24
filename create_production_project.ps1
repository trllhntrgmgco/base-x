<#
.SYNOPSIS
    Creates a new Unreal production project with Base-X preloaded.
.DESCRIPTION
    (c) 2026 Keenan Dunham & Troll Hunter Gaming - All Rights Reserved.
    DRO, LOD, and Base-X Game Engine are patent pending (USPTO).

    Sets up a new UE 5.7 C++ project with:
      - BaseXWorldGen plugin pre-installed
      - Track-specific content folders (NEC, BNT, SIM, etc.)
      - Optional VR configuration
      - Git repository initialization
      - Production-ready project structure

.PARAMETER ProjectName
    Name of the project (e.g., "NecrotopiaVR", "BibleNT_VR")

.PARAMETER Track
    Production track: TEC, SPT, NEC, BNT, SIM, HWV, SBX

.PARAMETER VRSupport
    Enable VR settings and plugins

.PARAMETER Template
    UE project template (default: ThirdPerson)

.EXAMPLE
    .\create_production_project.ps1 -ProjectName "NecrotopiaVR" -Track "NEC" -VRSupport

.EXAMPLE
    .\create_production_project.ps1 -ProjectName "BibleNT_VR" -Track "BNT" -VRSupport -Template "Blank"
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$ProjectName,
    
    [Parameter(Mandatory=$true)]
    [ValidateSet("TEC","SPT","NEC","BNT","SIM","HWV","SBX")]
    [string]$Track,
    
    [switch]$VRSupport,
    
    [string]$Template = "ThirdPerson"
)

$ErrorActionPreference = "Stop"

# ═══════════════════════════════════════════════════════════════════
# CONFIGURATION
# ═══════════════════════════════════════════════════════════════════
$UERoot = "C:\Program Files\Epic Games\UE_5.7"
$UEditorCmd = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$ProjectRoot = "C:\Users\Myrtle Beach Ads\source\repos"
$ProjectDir = Join-Path $ProjectRoot $ProjectName
$PluginSource = Join-Path $ProjectRoot "BaseXWorldUE\Plugins\BaseXWorldGen"

# Track-specific metadata
$TrackInfo = @{
    "TEC" = @{ Name = "Technical Demos"; Focus = "Engine Core" }
    "SPT" = @{ Name = "Sports VR"; Focus = "Athletics" }
    "NEC" = @{ Name = "Necrotopia VR"; Focus = "Hell Campaign" }
    "BNT" = @{ Name = "Bible NT VR"; Focus = "New Testament" }
    "SIM" = @{ Name = "Simulation"; Focus = "Consciousness City" }
    "HWV" = @{ Name = "Histories & Wars VR"; Focus = "Historical Combat" }
    "SBX" = @{ Name = "Sandbox"; Focus = "Voice-to-World" }
}

Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "  Base-X Production Project Generator"                            -ForegroundColor Cyan
Write-Host "  (c) 2026 Keenan Dunham & Troll Hunter Gaming"                  -ForegroundColor Cyan
Write-Host "  DRO, LOD, and Base-X Game Engine are patent pending (USPTO)"   -ForegroundColor DarkCyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Project: $ProjectName" -ForegroundColor White
Write-Host "Track:   [$Track] $($TrackInfo[$Track].Name)" -ForegroundColor White
Write-Host "Focus:   $($TrackInfo[$Track].Focus)" -ForegroundColor White
Write-Host "VR:      $(if ($VRSupport) { 'Enabled' } else { 'Disabled' })" -ForegroundColor White
Write-Host ""

# ═══════════════════════════════════════════════════════════════════
# 1. VERIFY PREREQUISITES
# ═══════════════════════════════════════════════════════════════════
Write-Host "[1/7] Verifying prerequisites..." -ForegroundColor Yellow

if (!(Test-Path $UERoot)) {
    throw "Unreal Engine 5.7 not found at $UERoot"
}

if (!(Test-Path $PluginSource)) {
    throw "BaseXWorldGen plugin not found at $PluginSource"
}

if (Test-Path $ProjectDir) {
    Write-Host "  WARNING: Project directory already exists: $ProjectDir" -ForegroundColor Red
    $confirm = Read-Host "  Delete existing project? (yes/no)"
    if ($confirm -ne "yes") {
        throw "Project creation cancelled by user"
    }
    Remove-Item $ProjectDir -Recurse -Force
    Write-Host "  Existing project deleted" -ForegroundColor Yellow
}

Write-Host "  Prerequisites verified" -ForegroundColor Green

# ═══════════════════════════════════════════════════════════════════
# 2. CREATE PROJECT STRUCTURE
# ═══════════════════════════════════════════════════════════════════
Write-Host ""
Write-Host "[2/7] Creating project directories..." -ForegroundColor Yellow

New-Item -ItemType Directory -Path $ProjectDir -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $ProjectDir "Content") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $ProjectDir "Config") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $ProjectDir "Source") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $ProjectDir "Plugins") -Force | Out-Null

Write-Host "  + Project structure created" -ForegroundColor Green

# ═══════════════════════════════════════════════════════════════════
# 3. GENERATE .UPROJECT FILE
# ═══════════════════════════════════════════════════════════════════
Write-Host ""
Write-Host "[3/7] Generating .uproject file..." -ForegroundColor Yellow

$UProjectContent = @{
    FileVersion = 3
    EngineAssociation = "5.7"
    Category = ""
    Description = "$($TrackInfo[$Track].Name) - Powered by Base-X Engine"
    Modules = @(
        @{
            Name = $ProjectName
            Type = "Runtime"
            LoadingPhase = "Default"
            AdditionalDependencies = @("Engine", "CoreUObject")
        }
    )
    Plugins = @(
        @{
            Name = "BaseXWorldGen"
            Enabled = $true
        }
    )
} | ConvertTo-Json -Depth 10

$UProjectFile = Join-Path $ProjectDir "$ProjectName.uproject"
$UProjectContent | Out-File -FilePath $UProjectFile -Encoding UTF8
Write-Host "  + $ProjectName.uproject" -ForegroundColor Green

# ═══════════════════════════════════════════════════════════════════
# 4. CREATE SOURCE MODULE
# ═══════════════════════════════════════════════════════════════════
Write-Host ""
Write-Host "[4/7] Creating C++ source module..." -ForegroundColor Yellow

$SourceDir = Join-Path $ProjectDir "Source\$ProjectName"
New-Item -ItemType Directory -Path $SourceDir -Force | Out-Null

# -- Build.cs
$BuildCS = @"
// Copyright 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.

using UnrealBuildTool;

public class $ProjectName : ModuleRules
{
    public $ProjectName(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        
        PublicDependencyModuleNames.AddRange(new string[] { 
            "Core", 
            "CoreUObject", 
            "Engine", 
            "InputCore",
            "BaseXWorldGen"  // Base-X plugin
        });
        
        PrivateDependencyModuleNames.AddRange(new string[] { });
        
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}
"@
$BuildCS | Out-File -FilePath (Join-Path $SourceDir "$ProjectName.Build.cs") -Encoding UTF8
Write-Host "  + $ProjectName.Build.cs" -ForegroundColor Green

# -- Module header
$ModuleH = @"
// Copyright 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
"@
$ModuleH | Out-File -FilePath (Join-Path $SourceDir "$ProjectName.h") -Encoding UTF8
Write-Host "  + $ProjectName.h" -ForegroundColor Green

# -- Module implementation
$ModuleCPP = @"
// Copyright 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.

#include "$ProjectName.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, $ProjectName, "$ProjectName");
"@
$ModuleCPP | Out-File -FilePath (Join-Path $SourceDir "$ProjectName.cpp") -Encoding UTF8
Write-Host "  + $ProjectName.cpp" -ForegroundColor Green

# -- Target.cs
$TargetCS = @"
// Copyright 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ${ProjectName}Target : TargetRules
{
    public ${ProjectName}Target(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("$ProjectName");
    }
}
"@
$TargetCS | Out-File -FilePath (Join-Path $ProjectDir "Source\$ProjectName.Target.cs") -Encoding UTF8
Write-Host "  + $ProjectName.Target.cs" -ForegroundColor Green

# -- Editor Target.cs
$EditorTargetCS = @"
// Copyright 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ${ProjectName}EditorTarget : TargetRules
{
    public ${ProjectName}EditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("$ProjectName");
    }
}
"@
$EditorTargetCS | Out-File -FilePath (Join-Path $ProjectDir "Source\${ProjectName}Editor.Target.cs") -Encoding UTF8
Write-Host "  + ${ProjectName}Editor.Target.cs" -ForegroundColor Green

# ═══════════════════════════════════════════════════════════════════
# 5. INSTALL BASE-X PLUGIN
# ═══════════════════════════════════════════════════════════════════
Write-Host ""
Write-Host "[5/7] Installing BaseXWorldGen plugin..." -ForegroundColor Yellow

$PluginDest = Join-Path $ProjectDir "Plugins\BaseXWorldGen"
Copy-Item $PluginSource $PluginDest -Recurse
Write-Host "  + BaseXWorldGen plugin installed" -ForegroundColor Green

# ═══════════════════════════════════════════════════════════════════
# 6. CONFIGURE PROJECT SETTINGS
# ═══════════════════════════════════════════════════════════════════
Write-Host ""
Write-Host "[6/7] Configuring project settings..." -ForegroundColor Yellow

# -- DefaultEngine.ini
$EngineIni = Join-Path $ProjectDir "Config\DefaultEngine.ini"
$EngineIniContent = @"
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Game/$Track/Maps/Main.Main
EditorStartupMap=/Game/$Track/Maps/Main.Main

[/Script/Engine.RendererSettings]
r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange=True

[/Script/HardwareTargeting.HardwareTargetingSettings]
TargetedHardwareClass=Desktop
AppliedTargetedHardwareClass=Desktop
DefaultGraphicsPerformance=Maximum
AppliedDefaultGraphicsPerformance=Maximum
"@

if ($VRSupport) {
    $EngineIniContent += @"

[/Script/EngineSettings.GeneralProjectSettings]
bStartInVR=True

[/Script/Engine.RendererSettings]
vr.InstancedStereo=True
vr.MobileMultiView=True
vr.RoundRobinOcclusion=True

[/Script/Engine.Engine]
bUseVRFocus=True
bPauseOnLossOfFocus=False
"@
    Write-Host "  + VR support configured" -ForegroundColor Green
}

$EngineIniContent | Out-File -FilePath $EngineIni -Encoding UTF8
Write-Host "  + DefaultEngine.ini" -ForegroundColor Green

# -- DefaultGame.ini
$GameIni = Join-Path $ProjectDir "Config\DefaultGame.ini"
$GameIniContent = @"
[/Script/EngineSettings.GeneralProjectSettings]
ProjectID=$(New-Guid)
ProjectName=$ProjectName
Description=$($TrackInfo[$Track].Name) - $($TrackInfo[$Track].Focus)
CompanyName=Troll Hunter Gaming
CopyrightNotice=Copyright 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.
"@
$GameIniContent | Out-File -FilePath $GameIni -Encoding UTF8
Write-Host "  + DefaultGame.ini" -ForegroundColor Green

# ═══════════════════════════════════════════════════════════════════
# 7. CREATE TRACK-SPECIFIC CONTENT
# ═══════════════════════════════════════════════════════════════════
Write-Host ""
Write-Host "[7/7] Creating track-specific content folders..." -ForegroundColor Yellow

$ContentDir = Join-Path $ProjectDir "Content\$Track"
New-Item -ItemType Directory -Path $ContentDir -Force | Out-Null

# Standard folders for all tracks
$StandardFolders = @("Maps", "Characters", "Systems", "UI", "Audio")
foreach ($folder in $StandardFolders) {
    New-Item -ItemType Directory -Path (Join-Path $ContentDir $folder) -Force | Out-Null
    Write-Host "  + Content/$Track/$folder" -ForegroundColor Green
}

# Track-specific folders
switch ($Track) {
    "NEC" {
        New-Item -ItemType Directory -Path (Join-Path $ContentDir "Realms") -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $ContentDir "Magic") -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $ContentDir "Combat") -Force | Out-Null
        Write-Host "  + Necrotopia-specific folders (Realms, Magic, Combat)" -ForegroundColor Green
    }
    "BNT" {
        New-Item -ItemType Directory -Path (Join-Path $ContentDir "Miracles") -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $ContentDir "HolyLand") -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $ContentDir "Scripture") -Force | Out-Null
        Write-Host "  + Bible NT-specific folders (Miracles, HolyLand, Scripture)" -ForegroundColor Green
    }
    "SPT" {
        New-Item -ItemType Directory -Path (Join-Path $ContentDir "Courts") -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $ContentDir "Fields") -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $ContentDir "Equipment") -Force | Out-Null
        Write-Host "  + Sports-specific folders (Courts, Fields, Equipment)" -ForegroundColor Green
    }
    "SIM" {
        New-Item -ItemType Directory -Path (Join-Path $ContentDir "City") -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $ContentDir "Consciousness") -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $ContentDir "AGI") -Force | Out-Null
        Write-Host "  + Simulation-specific folders (City, Consciousness, AGI)" -ForegroundColor Green
    }
}

# Create README in content folder
$ReadmeContent = @"
# $ProjectName - $($TrackInfo[$Track].Name)

**Track:** $Track - $($TrackInfo[$Track].Focus)
**Engine:** Unreal Engine 5.7
**Plugin:** Base-X World Generation Engine
**VR Support:** $(if ($VRSupport) { 'Enabled' } else { 'Disabled' })

## Quick Start

1. Open $ProjectName.uproject in Unreal Editor
2. Enable BaseXWorldGen plugin (Edit > Plugins)
3. Drop ABaseXPromptDemo into your level
4. Hit Play and start generating worlds

## Production Schedule

See: Base-X-Game-Engine\docs\Production_Schedule.md

---

Copyright 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.
DRO, LOD, and Base-X Game Engine are patent pending (USPTO).
"@
$ReadmeContent | Out-File -FilePath (Join-Path $ProjectDir "README.md") -Encoding UTF8
Write-Host "  + README.md" -ForegroundColor Green

# ═══════════════════════════════════════════════════════════════════
# SUMMARY & NEXT STEPS
# ═══════════════════════════════════════════════════════════════════
Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "  Project Created Successfully!" -ForegroundColor Green
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Project Location:" -ForegroundColor White
Write-Host "  $ProjectDir" -ForegroundColor Gray
Write-Host ""
Write-Host "Next Steps:" -ForegroundColor Yellow
Write-Host "  1. Open $ProjectName.uproject in Unreal Editor" -ForegroundColor White
Write-Host "  2. Let UE generate Visual Studio project files" -ForegroundColor White
Write-Host "  3. Build the project (Ctrl+F5 in VS)" -ForegroundColor White
Write-Host "  4. Start developing your [$Track] demos" -ForegroundColor White
Write-Host ""
Write-Host "Base-X Integration:" -ForegroundColor Yellow
Write-Host "  - BaseXWorldGen plugin: INSTALLED ✓" -ForegroundColor Green
Write-Host "  - Use UBaseXWorldBuilder in C++ code" -ForegroundColor White
Write-Host "  - Drop ABaseXPromptDemo into levels for instant world gen" -ForegroundColor White
Write-Host ""
$devMode = if ($VRSupport) { "VR" } else { "Desktop" }
Write-Host "Ready for $devMode development" -ForegroundColor Cyan
Write-Host ""
