# Imports the painted portal art into /Game/Environment/Portals (import_portals.py). Close Unreal Editor first.
#   powershell -ExecutionPolicy Bypass -File Tools\Environment\import_portals.ps1 [-Source <folder with the PNGs>]
# The PNGs ("BluePortal PNG.png", "PurplePortal PNG.png") default to the folder above the project.
param([string]$Source = '')
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not $Source) { $Source = Split-Path $project -Parent }
$logDir = Join-Path $project 'Logs'
New-Item -ItemType Directory -Force $logDir | Out-Null
$env:HELLGIRL_PORTAL_SOURCE = $Source
# The Python commandlet cannot take a script path containing spaces, so run a copy from TEMP.
$script = Join-Path $env:TEMP 'hellgirl_import_portals.py'
Copy-Item (Join-Path $PSScriptRoot 'import_portals.py') $script -Force
$log = Join-Path $logDir 'PortalImport.log'
$p = Start-Process 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList @("`"$project\Hellgirl.uproject`"", '-run=pythonscript', "-script=$script", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"") -WindowStyle Hidden -PassThru
$p.WaitForExit()
$result = Select-String $log -Pattern 'PORTAL IMPORT (PASSED|FAILED).*' | Select-Object -Last 1
if (-not $result -or $result.Line -match 'FAILED') { throw 'Unreal import failed; see Logs\PortalImport.log' }
Write-Host $result.Matches[0].Value
