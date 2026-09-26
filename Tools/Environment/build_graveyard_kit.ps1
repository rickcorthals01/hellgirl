# Builds the graveyard (World IV) art end to end. Close Unreal Editor first.
#   1. Blender: the tiling mist noise (graveyard_noise.py) and the kit meshes (graveyard_kit.py, preview in
#      Saved\GraveyardKit\Preview.png).
#   2. Unreal: textures, ground and mist materials, kit meshes (import_graveyard.py) into /Game/Environment/Graveyard.
# The ground textures are Poly Haven CC0 downloads kept in "Desktop\Hellgirl Game\Textures Poly Haven".
#   powershell -ExecutionPolicy Bypass -File Tools\Environment\build_graveyard_kit.ps1
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$logDir = Join-Path $project 'Logs'
$source = Join-Path $project 'Saved\GraveyardKit'
New-Item -ItemType Directory -Force $logDir, $source | Out-Null
$blender = 'C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe'

Write-Host 'Generating the noise and the kit in Blender...'
& $blender -b --factory-startup -P (Join-Path $PSScriptRoot 'graveyard_noise.py') -- (Join-Path $source 'T_GraveNoise.png') *> (Join-Path $logDir 'GraveyardNoise.log')
if (-not (Select-String (Join-Path $logDir 'GraveyardNoise.log') -Pattern 'GRAVE NOISE DONE' -Quiet)) { throw 'Noise step failed; see Logs\GraveyardNoise.log' }
& $blender -b --factory-startup -P (Join-Path $PSScriptRoot 'graveyard_kit.py') -- $source (Join-Path $source 'Preview.png') *> (Join-Path $logDir 'GraveyardKit.log')
if (-not (Select-String (Join-Path $logDir 'GraveyardKit.log') -Pattern 'GRAVEYARD KIT DONE' -Quiet)) { throw 'Kit step failed; see Logs\GraveyardKit.log' }

Write-Host 'Importing into Unreal...'
$env:HELLGIRL_KIT_SOURCE = $source
$env:HELLGIRL_TEXTURE_SOURCE = Join-Path (Split-Path $project -Parent) 'Textures Poly Haven'
# The Python commandlet cannot take a script path containing spaces, so run a copy from TEMP.
$script = Join-Path $env:TEMP 'hellgirl_import_graveyard.py'
Copy-Item (Join-Path $PSScriptRoot 'import_graveyard.py') $script -Force
$log = Join-Path $logDir 'GraveyardImport.log'
$p = Start-Process 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList @("`"$project\Hellgirl.uproject`"", '-run=pythonscript', "-script=$script", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"") -WindowStyle Hidden -PassThru
$p.WaitForExit()
$result = Select-String $log -Pattern 'GRAVEYARD IMPORT (PASSED|FAILED).*' | Select-Object -Last 1
if (-not $result -or $result.Line -match 'FAILED') { throw 'Unreal import failed; see Logs\GraveyardImport.log' }
Write-Host $result.Matches[0].Value
