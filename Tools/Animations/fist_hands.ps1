# Bakes loose fists into the outfit models (fist_hands.py), re-imports the meshes into Unreal, then rebuilds
# their animations (build.ps1 -Outfits) on the re-exported models. Open-handed originals stay in
# <outfit folder>\openhands. Close Unreal Editor first.
#   powershell -ExecutionPolicy Bypass -File Tools\Animations\fist_hands.ps1
#   powershell -ExecutionPolicy Bypass -File Tools\Animations\fist_hands.ps1 -Outfits Rags -Preview C:\Temp\fists
# -Preview only renders close-ups of the new hands into that folder and changes nothing.
param([string[]]$Outfits = @('Rags', 'SuccubusArmor', 'Ghost', 'GoblinQueen', 'ImpMother', 'Rat'), [string]$Preview = '')
$ErrorActionPreference = 'Stop'
$Outfits = @($Outfits | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$gameRoot = Split-Path $project -Parent
$logDir = Join-Path $project 'Logs'
New-Item -ItemType Directory -Force $logDir | Out-Null
$list = $Outfits -join ','

Write-Host "Baking fists in Blender: $list"
$blenderArgs = @('-b', '--factory-startup', '-P', (Join-Path $PSScriptRoot 'fist_hands.py'), '--', $gameRoot, $list)
if ($Preview) { New-Item -ItemType Directory -Force $Preview | Out-Null; $blenderArgs += $Preview }
& 'C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe' @blenderArgs *> (Join-Path $logDir 'FistHands.log')
if (-not (Select-String (Join-Path $logDir 'FistHands.log') -Pattern 'FIST HANDS DONE' -Quiet)) { throw 'Blender step failed; see Logs\FistHands.log' }
if ($Preview) { Write-Host "Preview images in $Preview"; return }

Write-Host 'Re-importing meshes into Unreal...'
$env:HELLGIRL_GAME_ROOT = $gameRoot
$env:HELLGIRL_OUTFITS = $list
$env:HELLGIRL_CLIPS = Join-Path $PSScriptRoot 'clips.json'
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
