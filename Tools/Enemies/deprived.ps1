# Rigs the user's Meshy Deprived onto Hellgirl's skeleton and brings it into the game.
#   1. Blender (rig_deprived.py): fitted onto her Rags body, each arm fitted, skin and hair weights -> Deprived.fbx
#   2. Its Meshy textures copied at 2048 (from 4096).
#   3. Unreal (import_deprived.py): /Game/Enemies/Deprived on /Game/Hellgirl/Outfits/Rags/Rags_Skeleton, with material
#      and physics asset. The Deprived model slot in Config/DefaultGame.ini points at it and her sword clips.
# Close Unreal Editor first. From the project folder:
#   powershell -ExecutionPolicy Bypass -File Tools\Enemies\deprived.ps1
# Add -Preview <folder> for Blender's fit, skin, rest and posed renders.
param([string]$Preview = '')
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$gameRoot = Split-Path $project -Parent
$model = Join-Path $gameRoot 'Meshy Models\Enemies\Deprived enemy model'
$stem = 'Meshy_AI_shadow_demon_model_0930122909_image-to-3d-texture'
$rags = Join-Path $gameRoot 'Animation Testing\FreshHellgirl\Rags\openhands\Rags.fbx'
$folder = Join-Path $gameRoot 'Animation Testing\Enemies\Deprived'
$blender = 'C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe'
$unreal = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$logDir = Join-Path $project 'Logs'
New-Item -ItemType Directory -Force $folder, $logDir | Out-Null
if (-not $Preview) { $Preview = Join-Path $folder 'Preview' }

Write-Host 'Rigging in Blender...'
$log = Join-Path $logDir 'DeprivedRig.log'
# 60k triangles; arms fitted (0); legs 4 degrees apart; height from the model (0); her right leg back 14 degrees.
& $blender -b --factory-startup -P (Join-Path $PSScriptRoot 'rig_deprived.py') -- $rags (Join-Path $model "$stem.fbx") 60000 (Join-Path $folder 'Deprived.fbx') $Preview rig 0 4 0 14 RightUpLeg *> $log
if (-not (Select-String $log -Pattern 'DEPRIVED RIG DONE' -Quiet)) { throw "Rigging failed; see $log" }
Select-String $log -Pattern '^DEPRIVED: ' | ForEach-Object { Write-Host "  $($_.Line)" }

Write-Host 'Textures at 2048...'
Add-Type -AssemblyName System.Drawing
foreach ($pair in @(@('', 'base'), @('_normal', 'normal'), @('_metallic', 'metallic'), @('_roughness', 'roughness'))) {
    $image = [System.Drawing.Image]::FromFile((Join-Path $model "$stem$($pair[0]).png"))
    $small = New-Object System.Drawing.Bitmap 2048, 2048
    $canvas = [System.Drawing.Graphics]::FromImage($small)
    $canvas.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $canvas.DrawImage($image, 0, 0, 2048, 2048)
    $small.Save((Join-Path $folder "Deprived_$($pair[1]).png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $canvas.Dispose(); $small.Dispose(); $image.Dispose()
}

Write-Host 'Importing into Unreal...'
$env:HELLGIRL_DEPRIVED_FOLDER = $folder
$log = Join-Path $logDir 'DeprivedImport.log'
# The Python commandlet cannot take a script path containing spaces, so run a copy from TEMP.
$script = Join-Path $env:TEMP 'hellgirl_import_deprived.py'
Copy-Item (Join-Path $PSScriptRoot 'import_deprived.py') $script -Force
$p = Start-Process $unreal -ArgumentList @("`"$project\Hellgirl.uproject`"", '-run=pythonscript', "-script=$script", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"") -WindowStyle Hidden -PassThru
$p.WaitForExit()
$result = Select-String $log -Pattern 'DEPRIVED IMPORT (PASSED|FAILED).*' | Select-Object -Last 1
if (-not $result -or $result.Line -match 'FAILED') { throw "Unreal import failed; see $log" }
Write-Host ($result.Matches[0].Value)
