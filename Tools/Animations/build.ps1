# Rebuilds Hellgirl's combat animations from Mixamo downloads.
#   1. Blender fits each clip in clips.json onto the six same-rig outfits (Frog uses its own rig and is skipped).
#   2. Unreal imports them as /Game/Hellgirl/Outfits/<Outfit>/Animations/<Clip>.
# Close Unreal Editor first. Usage (from the project folder):
#   powershell -ExecutionPolicy Bypass -File Tools\Animations\build.ps1
#   powershell -ExecutionPolicy Bypass -File Tools\Animations\build.ps1 -Only RightPunch,LeftPunch
#   powershell -ExecutionPolicy Bypass -File Tools\Animations\build.ps1 -Outfits GoblinQueen,Rat
param([string[]]$Only = @(), [string[]]$Outfits = @())
$ErrorActionPreference = 'Stop'
# With -File, "-Only A,B" arrives as one string.
$Only = @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$Outfits = @($Outfits | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$gameRoot = Split-Path $project -Parent
$outDir = Join-Path $gameRoot 'Animation Testing\CombatClips'
$blender = 'C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe'
$unreal = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$logDir = Join-Path $project 'Logs'
New-Item -ItemType Directory -Force $outDir, $logDir | Out-Null

Write-Host 'Fitting clips onto outfits in Blender...'
& $blender -b --factory-startup -P (Join-Path $PSScriptRoot 'prepare_clips.py') -- (Join-Path $PSScriptRoot 'clips.json') $gameRoot $outDir $(if ($Only.Count) { $Only -join ',' } else { '-' }) $(if ($Outfits.Count) { $Outfits -join ',' } else { '-' }) *> (Join-Path $logDir 'AnimationPrepare.log')
if (-not (Select-String (Join-Path $logDir 'AnimationPrepare.log') -Pattern 'PREPARE CLIPS DONE' -Quiet)) { throw 'Blender step failed; see Logs\AnimationPrepare.log' }

$report = Get-Content (Join-Path $outDir 'prepare_report.json') -Raw | ConvertFrom-Json
$rows = foreach ($outfit in $report.PSObject.Properties) { foreach ($clip in $outfit.Value.PSObject.Properties) {
    [pscustomobject]@{ Outfit = $outfit.Name; Clip = $clip.Name; Frames = $clip.Value.frames; Mirrored = $clip.Value.mirrored
        FitErrorDeg = $clip.Value.fit_error_deg; Skipped = $clip.Value.skipped } } }
$rows | Where-Object { -not $_.Skipped -and $_.Outfit -eq 'Rags' } | Format-Table Clip, Frames, Mirrored, FitErrorDeg -AutoSize | Out-String | Write-Host
$waiting = @($rows | Where-Object { $_.Skipped -and $_.Outfit -eq 'Rags' } | ForEach-Object Clip)
if ($waiting.Count) { Write-Host "Waiting for source files: $($waiting -join ', ')" }
$bad = @($rows | Where-Object { $_.FitErrorDeg -gt 1 })
if ($bad.Count) { throw "Fit error above 1 degree: $(($bad | ForEach-Object { "$($_.Outfit)/$($_.Clip)" }) -join ', ')" }

# Contact points are the same for every outfit (same source clip), so record them once per clip in
# Config/DefaultGame.ini. The game aligns each clip's contact point with its attack's damage moment.
$ini = Join-Path $project 'Config\DefaultGame.ini'
$section = '[HellgirlAnimationContact]'
$lines = [System.Collections.Generic.List[string]](Get-Content $ini)
$contact = [ordered]@{}
$at = $lines.IndexOf($section)
if ($at -ge 0) {
    $end = $at + 1
    while ($end -lt $lines.Count -and $lines[$end] -notmatch '^\[') { if ($lines[$end] -match '^(\w+)=(.+)$') { $contact[$matches[1]] = $matches[2] }; $end++ }
    $lines.RemoveRange($at, $end - $at)
}
foreach ($row in $rows | Where-Object { $_.Outfit -eq 'Rags' -and -not $_.Skipped }) {
    $fraction = $report.Rags.($row.Clip).contact_fraction
    if ($null -ne $fraction) { $contact[$row.Clip] = $fraction.ToString([Globalization.CultureInfo]::InvariantCulture) }
}
while ($lines.Count -and $lines[$lines.Count - 1] -eq '') { $lines.RemoveAt($lines.Count - 1) }
$lines.Add(''); $lines.Add($section); $lines.Add('; Written by Tools/Animations/build.ps1: fraction of each clip where its hit connects.')
foreach ($k in $contact.Keys) { if ($k -ne '') { $lines.Add("$k=$($contact[$k])") } }
[IO.File]::WriteAllLines($ini, $lines, (New-Object Text.UTF8Encoding $false))

Write-Host 'Importing into Unreal...'
$env:HELLGIRL_CLIP_DIR = $outDir
$log = Join-Path $logDir 'AnimationImport.log'
# The Python commandlet cannot take a script path containing spaces, so run a copy from TEMP.
$script = Join-Path $env:TEMP 'hellgirl_import_clips.py'
Copy-Item (Join-Path $PSScriptRoot 'import_clips.py') $script -Force
$p = Start-Process $unreal -ArgumentList @("`"$project\Hellgirl.uproject`"", '-run=pythonscript', "-script=$script", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"") -WindowStyle Hidden -PassThru
$p.WaitForExit()
$result = Select-String $log -Pattern 'CLIP IMPORT (PASSED|FAILED).*' | Select-Object -Last 1
if (-not $result -or $result.Line -match 'FAILED') { throw "Unreal import failed; see Logs\AnimationImport.log" }
Write-Host ($result.Matches[0].Value)
