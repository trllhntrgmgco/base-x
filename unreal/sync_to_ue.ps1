# sync_to_ue.ps1 — Copy Base-X repo unreal/ plugin to BaseXWorldUE project
# Run from the Base-X Engine repo root:
#   .\unreal\sync_to_ue.ps1

$ErrorActionPreference = "Stop"

$RepoRoot   = Split-Path $PSScriptRoot -Parent
$RepoPlugin = Join-Path $PSScriptRoot "BaseXWorldGen"
$UEPlugin   = "C:\Users\Myrtle Beach Ads\source\repos\BaseXWorldUE\Plugins\BaseXWorldGen"

if (!(Test-Path $RepoPlugin)) { Write-Error "Source not found: $RepoPlugin"; exit 1 }
if (!(Test-Path (Split-Path $UEPlugin))) { Write-Error "UE project not found"; exit 1 }

# 1. Sync core engine headers into plugin's ThirdParty/BaseX/
$HeaderSrc = Join-Path $RepoRoot "include\BaseX"
$HeaderDst = Join-Path $RepoPlugin "ThirdParty\BaseX"
if (!(Test-Path $HeaderDst)) { New-Item -ItemType Directory -Path $HeaderDst | Out-Null }

$headers = @("DRO.h", "WorldPromptEngine.h")
foreach ($h in $headers) {
    $src = Join-Path $HeaderSrc $h
    if (Test-Path $src) {
        Copy-Item $src $HeaderDst -Force
        Write-Host "  + ThirdParty/BaseX/$h" -ForegroundColor Green
    } else {
        Write-Warning "  ! $h not found at $src"
    }
}

# 2. Mirror the entire plugin directory to the UE project
robocopy $RepoPlugin $UEPlugin /MIR /XD Intermediate Binaries /NFL /NDL /NJH /NJS /NC /NS

Write-Host "`n=== Synced Base-X repo -> BaseXWorldUE ===" -ForegroundColor Green
Write-Host "Source: $RepoPlugin"
Write-Host "Target: $UEPlugin"
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Open BaseXWorldUE.uproject in UE Editor"
Write-Host "  2. It will recompile the plugin automatically"
Write-Host "  3. Drop ABaseXVRWorldBuilder into your level (or use your test actor)"
Write-Host ""
