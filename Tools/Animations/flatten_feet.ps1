# Fixes outfits whose Meshy model stands on tiptoe: flattens the feet in Blender (flatten_feet.py),
# re-imports the meshes into Unreal, then rebuilds all of their animations (build.ps1 -Outfits).
# Originals stay in Animation Testing\ExtraSkins\<Outfit>\tiptoe. Close Unreal Editor first.
#   powershell -ExecutionPolicy Bypass -File Tools\Animations\flatten_feet.ps1 -Outfits GoblinQueen,Rat,Ghost
param([string[]]$Outfits = @('GoblinQueen', 'Rat', 'Ghost'))
$ErrorActionPreference = 'Stop'
$Outfits = @($Outfits | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$gameRoot = Split-Path $project -Parent
$logDir = Join-Path $project 'Logs'
$list = $Outfits -join ','

Write-Host "Flattening feet in Blender: $list"
& 'C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe' -b --factory-startup -P (Join-Path $PSScriptRoot 'flatten_feet.py') -- $gameRoot $list *> (Join-Path $logDir 'FlattenFeet.log')
if (-not (Select-String (Join-Path $logDir 'FlattenFeet.log') -Pattern 'FLATTEN FEET DONE' -Quiet)) { throw 'Blender step failed; see Logs\FlattenFeet.log' }

Write-Host 'Re-importing meshes into Unreal...'
$env:HELLGIRL_GAME_ROOT = $gameRoot
$env:HELLGIRL_OUTFITS = $list
$script = Join-Path $env:TEMP 'hellgirl_import_outfit_meshes.py'
Copy-Item (Join-Path $PSScriptRoot 'import_outfit_meshes.py') $script -Force
$log = Join-Path $logDir 'OutfitMeshImport.log'
$p = Start-Process 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList @("`"$project\Hellgirl.uproject`"", '-run=pythonscript', "-script=$script", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"") -WindowStyle Hidden -PassThru
$p.WaitForExit()
$result = Select-String $log -Pattern 'OUTFIT MESH IMPORT (PASSED|FAILED).*' | Select-Object -Last 1
if (-not $result -or $result.Line -match 'FAILED') { throw 'Unreal mesh import failed; see Logs\OutfitMeshImport.log' }
Write-Host $result.Matches[0].Value

Write-Host 'Rebuilding their animations...'
& (Join-Path $PSScriptRoot 'build.ps1') -Outfits $list
