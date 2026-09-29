# Imports Hellgirl's swords into /Game/Weapons (import_weapons.py). Close Unreal Editor first.
#   powershell -ExecutionPolicy Bypass -File Tools\Weapons\import_weapons.ps1 [-Source <the Weapons folder>]
# The source defaults to "Weapons" next to the project ("Bat Sword" and "Infernal Sword" folders inside).
param([string]$Source = '')
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not $Source) { $Source = Join-Path (Split-Path $project -Parent) 'Weapons' }
$logDir = Join-Path $project 'Logs'
New-Item -ItemType Directory -Force $logDir | Out-Null
$env:HELLGIRL_WEAPON_SOURCE = $Source
# The Python commandlet cannot take a script path containing spaces, so run a copy from TEMP.
$script = Join-Path $env:TEMP 'hellgirl_import_weapons.py'
Copy-Item (Join-Path $PSScriptRoot 'import_weapons.py') $script -Force
$log = Join-Path $logDir 'WeaponImport.log'
$p = Start-Process 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList @("`"$project\Hellgirl.uproject`"", '-run=pythonscript', "-script=$script", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"") -WindowStyle Hidden -PassThru
$p.WaitForExit()
$result = Select-String $log -Pattern 'WEAPON IMPORT (PASSED|FAILED).*' | Select-Object -Last 1
if (-not $result -or $result.Line -match 'FAILED') { throw 'Unreal import failed; see Logs\WeaponImport.log' }
Write-Host $result.Matches[0].Value
