# Builds the Frozen Maze (World IV) art end to end. Close Unreal Editor first. Needs the graveyard and swamp art (the
# noise texture, M_GraveMist and M_SwampWater): run build_graveyard_kit.ps1 and build_swamp_kit.ps1 once before this.
#   1. Blender: the kit meshes (maze_kit.py, preview in Saved\MazeKit\Preview.png).
#   2. Unreal: the ice, floor, waterfall, vortex and pool materials and the kit (import_maze.py) into /Game/Environment/Maze.
#   powershell -ExecutionPolicy Bypass -File Tools\Environment\build_maze_kit.ps1 [-SkipBlender] [-SkipImport]
param([switch]$SkipBlender, [switch]$SkipImport)
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$logDir = Join-Path $project 'Logs'
$source = Join-Path $project 'Saved\MazeKit'
New-Item -ItemType Directory -Force $logDir, $source | Out-Null

if (-not $SkipBlender) {
    Write-Host 'Generating the kit in Blender...'
    & 'C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe' -b --factory-startup -P (Join-Path $PSScriptRoot 'maze_kit.py') -- $source (Join-Path $source 'Preview.png') *> (Join-Path $logDir 'MazeKit.log')
    if (-not (Select-String (Join-Path $logDir 'MazeKit.log') -Pattern 'MAZE KIT DONE' -Quiet)) { throw 'Kit step failed; see Logs\MazeKit.log' }
}
if ($SkipImport) { return }

Write-Host 'Importing into Unreal...'
$env:HELLGIRL_KIT_SOURCE = $source
# The Python commandlet cannot take a script path containing spaces, so run a copy from TEMP.
$script = Join-Path $env:TEMP 'hellgirl_import_maze.py'
Copy-Item (Join-Path $PSScriptRoot 'import_maze.py') $script -Force
$log = Join-Path $logDir 'MazeImport.log'
$p = Start-Process 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList @("`"$project\Hellgirl.uproject`"", '-run=pythonscript', "-script=$script", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"") -WindowStyle Hidden -PassThru
$p.WaitForExit()
$result = Select-String $log -Pattern 'MAZE IMPORT (PASSED|FAILED).*' | Select-Object -Last 1
if (-not $result -or $result.Line -match 'FAILED') { throw 'Unreal import failed; see Logs\MazeImport.log' }
Write-Host $result.Matches[0].Value
