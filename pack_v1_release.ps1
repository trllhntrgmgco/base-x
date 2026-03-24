<#
.SYNOPSIS
    Builds and packages Base-X Engine v1.0 release demos.
.DESCRIPTION
    (c) 2026 Keenan Dunham & Troll Hunter Gaming - All Rights Reserved.
    DRO, LOD, and Base-X Game Engine are patent pending (USPTO).

    Builds only the 4 v1.0 release demos in Release configuration,
    then copies executables + runtime DLLs into a clean release folder.
.EXAMPLE
    .\pack_v1_release.ps1
    .\pack_v1_release.ps1 -Toolchain "vcpkg/scripts/buildsystems/vcpkg.cmake"
#>

param(
    [string]$BuildDir  = "build-release",
    [string]$OutputDir = "release/BaseX-v1.0",
    [string]$Toolchain = ""
)

$ErrorActionPreference = "Stop"

# -- The 4 v1.0 demos ---------------------------------------------------------
$Demos = @(
    "SBX_001_WorldPromptEngine_glfw",
    "WLD_011_WorldBuilder_glfw",
    "WLD_012_Ecosystem_glfw",
    "WPE_001_PromptWorld_glfw"
)
$Whitelist = $Demos -join ";"

Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "  Base-X Engine v1.0 - Release Build"                              -ForegroundColor Cyan
Write-Host "  (c) 2026 Keenan Dunham & Troll Hunter Gaming"                    -ForegroundColor Cyan
Write-Host "  DRO, LOD, and Base-X Game Engine are patent pending (USPTO)"     -ForegroundColor DarkCyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "  Demos:" -ForegroundColor White
foreach ($d in $Demos) { Write-Host "    - $d" -ForegroundColor Gray }
Write-Host ""

# -- 1. Configure --------------------------------------------------------------
$cmakeArgs = @(
    "-S", ".",
    "-B", $BuildDir,
    "-DCMAKE_BUILD_TYPE=Release",
    "-DBUILD_DEMOS=ON",
    "-DBUILD_ENGINE=OFF",
    "-DRELEASE_DEMO_WHITELIST=$Whitelist"
)
if ($Toolchain -ne "") {
    $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$Toolchain"
}

Write-Host "[1/4] Configuring CMake..." -ForegroundColor Yellow
cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

# -- 2. Build ------------------------------------------------------------------
Write-Host ""
Write-Host "[2/4] Building Release..." -ForegroundColor Yellow
cmake --build $BuildDir --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

# -- 3. Package ----------------------------------------------------------------
Write-Host ""
Write-Host "[3/4] Packaging into $OutputDir ..." -ForegroundColor Yellow

if (Test-Path $OutputDir) { Remove-Item $OutputDir -Recurse -Force }
New-Item -ItemType Directory -Path $OutputDir | Out-Null

# Locate the bin directory (multi-config generators nest under bin/Release)
$binDir = Join-Path $BuildDir "bin"
$binRelease = Join-Path $binDir "Release"
if (Test-Path $binRelease) { $binDir = $binRelease }

# Copy demo executables
foreach ($demo in $Demos) {
    $exe = Join-Path $binDir "$demo.exe"
    if (Test-Path $exe) {
        Copy-Item $exe $OutputDir
        Write-Host "  + $demo.exe" -ForegroundColor Green
    } else {
        Write-Warning "  ! $demo.exe not found at $exe"
    }
}

# Copy runtime DLLs (GLFW, Vulkan loader, etc.)
$dllSearchPaths = @(
    $binDir,
    (Join-Path $BuildDir "vcpkg_installed/x64-windows/bin"),
    "vcpkg/installed/x64-windows/bin"
)
if ($env:VULKAN_SDK) { $dllSearchPaths += "$env:VULKAN_SDK/Bin" }

$requiredDlls = @("glfw3.dll")

foreach ($dll in $requiredDlls) {
    $found = $false
    foreach ($searchPath in $dllSearchPaths) {
        $dllPath = Join-Path $searchPath $dll
        if (Test-Path $dllPath) {
            Copy-Item $dllPath $OutputDir
            Write-Host "  + $dll" -ForegroundColor Green
            $found = $true
            break
        }
    }
    if (-not $found) {
        Write-Warning "  ! $dll not found - users will need it on PATH"
    }
}

# Copy any additional DLLs from the bin directory
Get-ChildItem -Path $binDir -Filter "*.dll" -ErrorAction SilentlyContinue | ForEach-Object {
    if ($_.Name -notin $requiredDlls) {
        Copy-Item $_.FullName $OutputDir
        Write-Host "  + $($_.Name)" -ForegroundColor DarkGreen
    }
}

# -- Write release README -----------------------------------------------------
@"
# Base-X Engine v1.0 - Demo Release

(c) 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.
DRO, LOD, and Base-X Game Engine are patent pending (USPTO).

## Included Demos

| Executable | Description |
|------------|-------------|
| SBX_001_WorldPromptEngine_glfw | NLP prompt -> procedural world generation with DRO precision |
| WLD_011_WorldBuilder_glfw      | Holodeck world builder with biome editing & atmosphere |
| WLD_012_Ecosystem_glfw         | Living ecosystem: 12 creatures, tile world, dynamic sky |
| WPE_001_PromptWorld_glfw       | Prompt-driven walkable 3D world explorer |

## System Requirements

- Windows 10/11 x64
- Vulkan-capable GPU with up-to-date drivers

## Running

Double-click any .exe or launch from a terminal.
Each demo displays a controls overlay - press H for help.
"@ | Out-File -FilePath (Join-Path $OutputDir "README.md") -Encoding utf8

# -- 4. Summary ----------------------------------------------------------------
Write-Host ""
Write-Host "[4/4] Done!" -ForegroundColor Green
Write-Host ""
$files = Get-ChildItem $OutputDir
Write-Host "Release contents ($OutputDir):" -ForegroundColor Cyan
$files | ForEach-Object {
    $size = if ($_.Length -ge 1MB) { "$([math]::Round($_.Length / 1MB, 2)) MB" } else { "$([math]::Round($_.Length / 1KB, 1)) KB" }
    Write-Host "  $($_.Name)  ($size)" -ForegroundColor White
}
$totalMB = [math]::Round(($files | Measure-Object -Property Length -Sum).Sum / 1MB, 2)
Write-Host ""
Write-Host "Total: $totalMB MB" -ForegroundColor Cyan
Write-Host "Ready to distribute: $((Resolve-Path $OutputDir).Path)" -ForegroundColor Green
