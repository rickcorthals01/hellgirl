# Builds the swamp enemies (World II: the rat and the frog) from their rigged Meshy models. Close Unreal Editor first.
#   1. Blender (Tools/Animations/prepare_clips.py): fits each enemy's clips (rat_clips.json, frog_clips.json) onto its rig.
#   2. Their contact points go to [HellgirlAnimationContact] in Config/DefaultGame.ini.
#   3. Unreal (import_swamp_enemies.py): model, textures, material, physics asset and clips into /Game/Enemies/Swamp/<Name>.
#   powershell -ExecutionPolicy Bypass -File Tools\Enemies\swamp_enemies.ps1
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$gameRoot = Split-Path $project -Parent
$outRoot = Join-Path $gameRoot 'Animation Testing\SwampEnemies'
$logDir = Join-Path $project 'Logs'
New-Item -ItemType Directory -Force $outRoot, $logDir | Out-Null
$blender = 'C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe'
$enemies = [ordered]@{}
$contact = [ordered]@{}
foreach ($name in 'Rat', 'Frog') {
    $config = Join-Path $PSScriptRoot "$($name.ToLower())_clips.json"
    $out = Join-Path $outRoot $name
    New-Item -ItemType Directory -Force $out | Out-Null
    Write-Host "Fitting the $name's clips in Blender..."
    $log = Join-Path $logDir "SwampEnemy$name.log"
    & $blender -b --factory-startup -P (Join-Path $project 'Tools\Animations\prepare_clips.py') -- $config $gameRoot $out *> $log
    if (-not (Select-String $log -Pattern 'PREPARE CLIPS DONE' -Quiet)) { throw "Blender step failed; see Logs\SwampEnemy$name.log" }
    $report = Get-Content (Join-Path $out 'prepare_report.json') -Raw | ConvertFrom-Json
    foreach ($clip in $report.$name.PSObject.Properties) {
        if ($clip.Value.skipped) { throw "$name/$($clip.Name): $($clip.Value.skipped)" }
        if ($clip.Value.fit_error_deg -gt 1) { throw "$name/$($clip.Name): fit error $($clip.Value.fit_error_deg) degrees" }
        if ($null -ne $clip.Value.contact_fraction) { $contact[$clip.Name] = $clip.Value.contact_fraction.ToString([Globalization.CultureInfo]::InvariantCulture) }
    }
    $enemies[$name] = (Get-Content $config -Raw | ConvertFrom-Json).outfits.$name
}

# Contact points: existing keys are updated in place, new ones added to the section.
$ini = Join-Path $project 'Config\DefaultGame.ini'
$lines = [System.Collections.Generic.List[string]](Get-Content $ini)
$at = $lines.IndexOf('[HellgirlAnimationContact]')
if ($at -lt 0) { throw 'No [HellgirlAnimationContact] section in DefaultGame.ini' }
$end = $at + 1
while ($end -lt $lines.Count -and $lines[$end] -notmatch '^\[') { $end++ }
while ($end -gt $at + 1 -and $lines[$end - 1] -eq '') { $end-- }
foreach ($key in $contact.Keys) {
    $found = -1
    for ($i = $at + 1; $i -lt $end; $i++) { if ($lines[$i] -like "$key=*") { $found = $i } }
    if ($found -ge 0) { $lines[$found] = "$key=$($contact[$key])" } else { $lines.Insert($end, "$key=$($contact[$key])"); $end++ }
}
[IO.File]::WriteAllLines($ini, $lines, (New-Object Text.UTF8Encoding $false))

Write-Host 'Importing into Unreal...'
$env:HELLGIRL_GAME_ROOT = $gameRoot
$env:HELLGIRL_CLIP_DIR = $outRoot
$env:HELLGIRL_ENEMIES = ($enemies | ConvertTo-Json -Compress)
$script = Join-Path $env:TEMP 'hellgirl_import_swamp_enemies.py'
Copy-Item (Join-Path $PSScriptRoot 'import_swamp_enemies.py') $script -Force
$log = Join-Path $logDir 'SwampEnemyImport.log'
$p = Start-Process 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList @("`"$project\Hellgirl.uproject`"", '-run=pythonscript', "-script=$script", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"") -WindowStyle Hidden -PassThru
$p.WaitForExit()
$result = Select-String $log -Pattern 'SWAMP ENEMY IMPORT (PASSED|FAILED).*' | Select-Object -Last 1
if (-not $result -or $result.Line -match 'FAILED') { throw 'Unreal import failed; see Logs\SwampEnemyImport.log' }
Write-Host $result.Matches[0].Value
