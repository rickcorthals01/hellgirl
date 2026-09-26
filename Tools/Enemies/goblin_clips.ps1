# Rebuilds the goblin enemies' attack and hit clips from Mixamo downloads (Tools/Enemies/goblin_clips.json):
#   1. Blender fits each clip onto the Goblin and Goblin Queen rigs (Tools/Animations/prepare_clips.py).
#   2. Their contact points go to [HellgirlAnimationContact] in Config/DefaultGame.ini.
#   3. Unreal imports them as /Game/Enemies/Goblins/<Outfit>/Animations/<Clip>.
# Close Unreal Editor first. Usage (from the project folder):
#   powershell -ExecutionPolicy Bypass -File Tools\Enemies\goblin_clips.ps1
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$gameRoot = Split-Path $project -Parent
$outDir = Join-Path $gameRoot 'Animation Testing\GoblinEnemies\Clips'
$blender = 'C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe'
$unreal = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$logDir = Join-Path $project 'Logs'
New-Item -ItemType Directory -Force $outDir, $logDir | Out-Null

Write-Host 'Fitting goblin clips in Blender...'
& $blender -b --factory-startup -P (Join-Path $project 'Tools\Animations\prepare_clips.py') -- (Join-Path $PSScriptRoot 'goblin_clips.json') $gameRoot $outDir *> (Join-Path $logDir 'GoblinClipsPrepare.log')
if (-not (Select-String (Join-Path $logDir 'GoblinClipsPrepare.log') -Pattern 'PREPARE CLIPS DONE' -Quiet)) { throw 'Blender step failed; see Logs\GoblinClipsPrepare.log' }
$report = Get-Content (Join-Path $outDir 'prepare_report.json') -Raw | ConvertFrom-Json

# Same source clips on both rigs, so the Goblin's contact points stand for both. Existing keys are updated in place.
$ini = Join-Path $project 'Config\DefaultGame.ini'
$lines = [System.Collections.Generic.List[string]](Get-Content $ini)
$at = $lines.IndexOf('[HellgirlAnimationContact]')
if ($at -lt 0) { throw 'No [HellgirlAnimationContact] section in DefaultGame.ini' }
$end = $at + 1
while ($end -lt $lines.Count -and $lines[$end] -notmatch '^\[') { $end++ }
while ($end -gt $at + 1 -and $lines[$end - 1] -eq '') { $end-- }
foreach ($clip in $report.Goblin.PSObject.Properties) {
    $fraction = $clip.Value.contact_fraction
    if ($null -eq $fraction) { continue }
    $line = "$($clip.Name)=$($fraction.ToString([Globalization.CultureInfo]::InvariantCulture))"
    $found = -1
    for ($i = $at + 1; $i -lt $end; $i++) { if ($lines[$i] -like "$($clip.Name)=*") { $found = $i } }
    if ($found -ge 0) { $lines[$found] = $line } else { $lines.Insert($end, $line); $end++ }
}
[IO.File]::WriteAllLines($ini, $lines, (New-Object Text.UTF8Encoding $false))

Write-Host 'Importing into Unreal...'
$env:HELLGIRL_CLIP_DIR = $outDir
$log = Join-Path $logDir 'GoblinClipsImport.log'
# The Python commandlet cannot take a script path containing spaces, so run a copy from TEMP.
$script = Join-Path $env:TEMP 'hellgirl_import_goblin_clips.py'
Copy-Item (Join-Path $PSScriptRoot 'import_goblin_clips.py') $script -Force
$p = Start-Process $unreal -ArgumentList @("`"$project\Hellgirl.uproject`"", '-run=pythonscript', "-script=$script", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"") -WindowStyle Hidden -PassThru
$p.WaitForExit()
$result = Select-String $log -Pattern 'GOBLIN CLIP IMPORT (PASSED|FAILED).*' | Select-Object -Last 1
if (-not $result -or $result.Line -match 'FAILED') { throw "Unreal import failed; see Logs\GoblinClipsImport.log" }
Write-Host ($result.Matches[0].Value)
