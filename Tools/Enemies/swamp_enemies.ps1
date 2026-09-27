# Builds the swamp enemies (World II: the rat, the frog and the Rat Queen) from their rigged Meshy models. Close Unreal
# Editor first.
#   0. The Rat Queen: her HD model is rigged onto the old Rat Queen's skeleton (Tools/Animations/rig_hq_outfit.py) if
#      that has not been done yet, and her HD textures are copied beside it at 2048 px under Meshy's usual names.
#   1. Blender (Tools/Animations/prepare_clips.py): fits each enemy's clips (<name>_clips.json) onto its rig.
#   2. Their contact points go to [HellgirlAnimationContact] in Config/DefaultGame.ini.
#   3. Unreal (import_swamp_enemies.py): model, textures, material, physics asset and clips into /Game/Enemies/Swamp/<Name>.
#   powershell -ExecutionPolicy Bypass -File Tools\Enemies\swamp_enemies.ps1 [-Only RatQueen]
param([string[]]$Only = @('Rat', 'Frog', 'RatQueen'))
$ErrorActionPreference = 'Stop'
$Only = @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$gameRoot = Split-Path $project -Parent
$outRoot = Join-Path $gameRoot 'Animation Testing\SwampEnemies'
$logDir = Join-Path $project 'Logs'
New-Item -ItemType Directory -Force $outRoot, $logDir | Out-Null
$blender = 'C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe'
$enemies = [ordered]@{}
$contact = [ordered]@{}
# The Rat Queen's HD model and textures.
$hdFolder = Join-Path $gameRoot 'Meshy Models\High Quality Models\Rat Queen HD'
$hdStem = Join-Path $hdFolder 'Meshy_AI_rat_queen_enemy_0926133013_image-to-3d-texture_fbx\Meshy_AI_rat_queen_enemy_0926133013_image-to-3d-texture'
$rigged = Join-Path $hdFolder 'rigged\RatQueen.fbx'
if ($Only -contains 'RatQueen') {
    if (-not (Test-Path $rigged)) {
        Write-Host 'Rigging the HD Rat Queen onto her old skeleton in Blender...'
        $old = Join-Path $gameRoot 'Meshy Models\Bosses\Rat Queen\Meshy_models_20260918_172739\c9f32cb2-4169-496e-a3cb-0f0d2becb976\Meshy_AI_Character_output.fbx'
        & $blender -b --factory-startup -P (Join-Path $project 'Tools\Animations\rig_hq_outfit.py') -- $gameRoot $old "$hdStem.fbx" 150000 $rigged *> (Join-Path $logDir 'RatQueenRig.log')
        if (-not (Test-Path $rigged)) { throw 'Rigging failed; see Logs\RatQueenRig.log' }
    }
    Add-Type -AssemblyName System.Drawing
    foreach ($pair in @(@('', ''), @('_normal', '_normal'), @('_metallic', '_metallic'), @('_roughness', '_roughness'))) {
        $source = [System.Drawing.Image]::FromFile("$hdStem$($pair[0]).png")
        $size = [Math]::Min(2048, $source.Width)
        $small = New-Object System.Drawing.Bitmap $size, $size
        $g = [System.Drawing.Graphics]::FromImage($small); $g.InterpolationMode = 'HighQualityBicubic'; $g.DrawImage($source, 0, 0, $size, $size); $g.Dispose()
        $small.Save((Join-Path (Split-Path $rigged) "Meshy_AI_texture_0$($pair[1]).png"), [System.Drawing.Imaging.ImageFormat]::Png)
        $small.Dispose(); $source.Dispose()
    }
}
foreach ($name in $Only) {
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
