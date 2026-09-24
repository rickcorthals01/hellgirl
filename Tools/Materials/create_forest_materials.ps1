# Runs create_forest_materials.py in Unreal (headless). Close Unreal Editor first.
#   powershell -ExecutionPolicy Bypass -File Tools\Materials\create_forest_materials.ps1
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$logDir = Join-Path $project 'Logs'
New-Item -ItemType Directory -Force $logDir | Out-Null
# The Python commandlet cannot take a script path with spaces.
$script = Join-Path $env:TEMP 'hellgirl_create_forest_materials.py'
Copy-Item (Join-Path $PSScriptRoot 'create_forest_materials.py') $script -Force
$log = Join-Path $logDir 'ForestMaterials.log'
$p = Start-Process 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList @("`"$project\Hellgirl.uproject`"", '-run=pythonscript', "-script=$script", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"") -WindowStyle Hidden -PassThru
$p.WaitForExit()
$result = Select-String $log -Pattern 'FOREST MATERIALS (PASSED|FAILED)' | Select-Object -Last 1
if (-not $result -or $result.Line -match 'FAILED') { throw 'Material creation failed; see Logs\ForestMaterials.log' }
Write-Host $result.Matches[0].Value
