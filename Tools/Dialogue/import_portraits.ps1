# Prepares the talkbox portraits (prepare_portraits.ps1) and imports them into Unreal (import_portraits.py).
# Close Unreal Editor first.
#   powershell -ExecutionPolicy Bypass -File Tools\Dialogue\import_portraits.ps1
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
& (Join-Path $PSScriptRoot 'prepare_portraits.ps1')
$env:HELLGIRL_PORTRAITS = Join-Path (Split-Path $project -Parent) 'Talkbox Images\Processed'
$script = Join-Path $env:TEMP 'hellgirl_import_portraits.py'
Copy-Item (Join-Path $PSScriptRoot 'import_portraits.py') $script -Force
$log = Join-Path $project 'Logs\PortraitImport.log'
$p = Start-Process 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList @("`"$project\Hellgirl.uproject`"", '-run=pythonscript', "-script=$script", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"") -WindowStyle Hidden -PassThru
$p.WaitForExit()
$result = Select-String $log -Pattern 'PORTRAIT IMPORT (PASSED|FAILED).*' | Select-Object -Last 1
if (-not $result -or $result.Line -match 'FAILED') { throw 'Portrait import failed; see Logs\PortraitImport.log' }
Write-Host $result.Matches[0].Value
