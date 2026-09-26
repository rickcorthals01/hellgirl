# Rigs the Meshy mini succubus and brings her into the game.
#   1. Blender (rig_mini_succubus.py): Hellgirl's skeleton fitted onto her, wing bones, weights -> MiniSuccubus.fbx
#   2. Blender (mini_succubus_flight.py): her flying clips (she only flies) keyframed on that rig.
#   3. Unreal (import_mini_succubus.py): /Game/Enemies/MiniSuccubus with material, physics asset and clips.
# Close Unreal Editor first. From the project folder:
#   powershell -ExecutionPolicy Bypass -File Tools\Enemies\rig_mini_succubus.ps1
# Add -Preview <folder> to also render her weight map (colour per bone) and a test pose from Blender.
# To see the clips in game (needs a GPU): Hellgirl.uproject /Engine/Maps/Entry?ForestHub=1 -game -windowed
#   -HellgirlMiniSuccubusPreview  -> Saved/Screenshots/MiniSuccubus_Row.png and _Close.png
param([string]$Preview = '')
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$gameRoot = Split-Path $project -Parent
$folder = Join-Path $gameRoot 'Animation Testing\Enemies\MiniSuccubus'
$blender = 'C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe'
$unreal = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$logDir = Join-Path $project 'Logs'
New-Item -ItemType Directory -Force $folder, $logDir | Out-Null

Write-Host 'Rigging in Blender...'
$log = Join-Path $logDir 'MiniSuccubusRig.log'
$rigArgs = @('-b', '--factory-startup', '-P', (Join-Path $PSScriptRoot 'rig_mini_succubus.py'), '--', $gameRoot, (Join-Path $folder 'MiniSuccubus.fbx'))
if ($Preview) { $rigArgs += (Join-Path $Preview 'rig') }
& $blender @rigArgs *> $log
if (-not (Select-String $log -Pattern 'RIG MINI SUCCUBUS DONE' -Quiet)) { throw "Rigging failed; see $log" }

Write-Host 'Keyframing flying clips...'
$log = Join-Path $logDir 'MiniSuccubusClips.log'
$clips = Join-Path $folder 'Clips\MiniSuccubus'
Remove-Item (Join-Path $clips '*.fbx') -ErrorAction SilentlyContinue
& $blender -b --factory-startup -P (Join-Path $PSScriptRoot 'mini_succubus_flight.py') -- (Join-Path $folder 'MiniSuccubus.fbx') $clips *> $log
if (-not (Select-String $log -Pattern 'FLIGHT CLIPS DONE' -Quiet)) { throw "Flying clips failed; see $log" }
Select-String $log -Pattern '^FLIGHT CLIP ' | ForEach-Object { Write-Host "  $($_.Line)" }

Write-Host 'Importing into Unreal...'
$env:HELLGIRL_GAME_ROOT = $gameRoot
$log = Join-Path $logDir 'MiniSuccubusImport.log'
# The Python commandlet cannot take a script path containing spaces, so run a copy from TEMP.
$script = Join-Path $env:TEMP 'hellgirl_import_mini_succubus.py'
Copy-Item (Join-Path $PSScriptRoot 'import_mini_succubus.py') $script -Force
$p = Start-Process $unreal -ArgumentList @("`"$project\Hellgirl.uproject`"", '-run=pythonscript', "-script=$script", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"") -WindowStyle Hidden -PassThru
$p.WaitForExit()
$result = Select-String $log -Pattern 'MINI SUCCUBUS IMPORT (PASSED|FAILED).*' | Select-Object -Last 1
if (-not $result -or $result.Line -match 'FAILED') { throw "Unreal import failed; see $log" }
Write-Host ($result.Matches[0].Value)
