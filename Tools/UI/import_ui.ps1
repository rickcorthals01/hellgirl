# Cuts the placeholder menu art (prepare_ui.ps1) and imports it into Unreal (import_ui.py) as /Game/UI/Placeholder.
# Close Unreal Editor first.
#   powershell -ExecutionPolicy Bypass -File Tools\UI\import_ui.ps1
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
& (Join-Path $PSScriptRoot 'prepare_ui.ps1')
$env:HELLGIRL_UI = Join-Path (Split-Path $project -Parent) 'UI\Processed'
$script = Join-Path $env:TEMP 'hellgirl_import_ui.py'
Copy-Item (Join-Path $PSScriptRoot 'import_ui.py') $script -Force
$log = Join-Path $project 'Logs\UIImport.log'
$p = Start-Process 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList @("`"$project\Hellgirl.uproject`"", '-run=pythonscript', "-script=$script", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"") -WindowStyle Hidden -PassThru
$p.WaitForExit()
$result = Select-String $log -Pattern 'UI IMPORT (PASSED|FAILED).*' | Select-Object -Last 1
if (-not $result -or $result.Line -match 'FAILED') { throw 'UI import failed; see Logs\UIImport.log' }
Write-Host $result.Matches[0].Value
