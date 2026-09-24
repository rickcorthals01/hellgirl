# Builds the forest kit end to end: materials (Tools\Materials), Blender generation (forest_kit.py) and the
# Unreal import (import_forest_kit.py). Close Unreal Editor first.
#   powershell -ExecutionPolicy Bypass -File Tools\Environment\build_forest_kit.ps1
param([switch]$SkipMaterials)
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$logDir = Join-Path $project 'Logs'
$source = Join-Path $project 'Saved\ForestKit'
New-Item -ItemType Directory -Force $logDir, $source | Out-Null

if (-not $SkipMaterials) { & (Join-Path $project 'Tools\Materials\create_forest_materials.ps1') }

Write-Host 'Generating the kit in Blender...'
& 'C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe' -b --factory-startup -P (Join-Path $PSScriptRoot 'forest_kit.py') -- $source *> (Join-Path $logDir 'ForestKit.log')
if (-not (Select-String (Join-Path $logDir 'ForestKit.log') -Pattern 'FOREST KIT DONE' -Quiet)) { throw 'Blender step failed; see Logs\ForestKit.log' }

Write-Host 'Importing into Unreal...'
$env:HELLGIRL_KIT_SOURCE = $source
$script = Join-Path $env:TEMP 'hellgirl_import_forest_kit.py'
Copy-Item (Join-Path $PSScriptRoot 'import_forest_kit.py') $script -Force
$log = Join-Path $logDir 'ForestKitImport.log'
$p = Start-Process 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList @("`"$project\Hellgirl.uproject`"", '-run=pythonscript', "-script=$script", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"") -WindowStyle Hidden -PassThru
$p.WaitForExit()
$result = Select-String $log -Pattern 'FOREST KIT IMPORT (PASSED|FAILED).*' | Select-Object -Last 1
if (-not $result -or $result.Line -match 'FAILED') { throw 'Unreal import failed; see Logs\ForestKitImport.log' }
Write-Host $result.Matches[0].Value
