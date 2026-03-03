# sync_to_ue.ps1 — Copy Base-X repo unreal/ plugin to BaseXWorldUE project
# Run from the Base-X Engine repo root:
#   .\unreal\sync_to_ue.ps1

$ErrorActionPreference = "Stop"

$RepoPlugin = Join-Path $PSScriptRoot "BaseXWorldGen"
$UEPlugin   = "C:\Users\Myrtle Beach Ads\source\repos\BaseXWorldUE\Plugins\BaseXWorldGen"

if (!(Test-Path $RepoPlugin)) { Write-Error "Source not found: $RepoPlugin"; exit 1 }
if (!(Test-Path (Split-Path $UEPlugin))) { Write-Error "UE project not found"; exit 1 }

# Mirror the entire plugin directory
robocopy $RepoPlugin $UEPlugin /MIR /XD Intermediate Binaries /NFL /NDL /NJH /NJS /NC /NS

Write-Host "`n=== Synced Base-X repo → BaseXWorldUE ===" -ForegroundColor Green
Write-Host "Source: $RepoPlugin"
Write-Host "Target: $UEPlugin"
