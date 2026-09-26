# Builds the swamp (World II) art end to end. Close Unreal Editor first. Needs the graveyard art (its noise texture,
# ground textures and M_GraveGround): run build_graveyard_kit.ps1 once before this.
#   1. Blender: the kit meshes (swamp_kit.py, preview in Saved\SwampKit\Preview.png).
#   2. Unreal: the water, ripple, soul and ground materials and the kit (import_swamp.py) into /Game/Environment/Swamp.
#   powershell -ExecutionPolicy Bypass -File Tools\Environment\build_swamp_kit.ps1
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$logDir = Join-Path $project 'Logs'
$source = Join-Path $project 'Saved\SwampKit'
New-Item -ItemType Directory -Force $logDir, $source | Out-Null

Write-Host 'Generating the kit in Blender...'
& 'C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe' -b --factory-startup -P (Join-Path $PSScriptRoot 'swamp_kit.py') -- $source (Join-Path $source 'Preview.png') *> (Join-Path $logDir 'SwampKit.log')
if (-not (Select-String (Join-Path $logDir 'SwampKit.log') -Pattern 'SWAMP KIT DONE' -Quiet)) { throw 'Kit step failed; see Logs\SwampKit.log' }

Write-Host 'Importing into Unreal...'
$env:HELLGIRL_KIT_SOURCE = $source
# The Python commandlet cannot take a script path containing spaces, so run a copy from TEMP.
$script = Join-Path $env:TEMP 'hellgirl_import_swamp.py'
Copy-Item (Join-Path $PSScriptRoot 'import_swamp.py') $script -Force
$log = Join-Path $logDir 'SwampImport.log'
$p = Start-Process 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList @("`"$project\Hellgirl.uproject`"", '-run=pythonscript', "-script=$script", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"") -WindowStyle Hidden -PassThru
$p.WaitForExit()
$result = Select-String $log -Pattern 'SWAMP IMPORT (PASSED|FAILED).*' | Select-Object -Last 1
if (-not $result -or $result.Line -match 'FAILED') { throw 'Unreal import failed; see Logs\SwampImport.log' }
Write-Host $result.Matches[0].Value
