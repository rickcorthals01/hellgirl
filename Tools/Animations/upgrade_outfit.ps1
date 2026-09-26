# Swaps an outfit for a new, unrigged high-quality Meshy model while keeping its skeleton, so every animation still
# works. Close Unreal Editor first.
#   1. Blender (rig_hq_outfit.py): reduce the model, fit it onto the old one, give it the old skin weights and bind
#      it to the old skeleton; it becomes the outfit's open-handed original (old files kept in <outfit>\lowpoly).
#   2. Unreal (import_outfit_textures.py): the new textures replace the old ones in place, plus a normal map.
#   3. fist_hands.ps1: bakes the fists, re-imports the mesh onto the skeleton and rebuilds the outfit's clips.
# Usage (from the project folder):
#   powershell -ExecutionPolicy Bypass -File Tools\Animations\upgrade_outfit.ps1 -Outfit Rags -Model "<path to .fbx>"
#   add -Preview <folder> to only render the fit and a posed test there.
param([Parameter(Mandatory)][string]$Outfit, [Parameter(Mandatory)][string]$Model, [int]$TargetTris = 250000, [string]$Preview = '')
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$gameRoot = Split-Path $project -Parent
$logDir = Join-Path $project 'Logs'
New-Item -ItemType Directory -Force $logDir | Out-Null

Write-Host "Rigging $Outfit's new model with its old skeleton in Blender..."
$blenderArgs = @('-b', '--factory-startup', '-P', (Join-Path $PSScriptRoot 'rig_hq_outfit.py'), '--', $gameRoot, $Outfit, $Model, $TargetTris)
if ($Preview) { New-Item -ItemType Directory -Force $Preview | Out-Null; $blenderArgs += $Preview }
& 'C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe' @blenderArgs *> (Join-Path $logDir 'HQRig.log')
Select-String (Join-Path $logDir 'HQRig.log') -Pattern '^HQ ' | ForEach-Object { Write-Host $_.Line }
if (-not (Select-String (Join-Path $logDir 'HQRig.log') -Pattern 'HQ RIG (DONE|PREVIEW DONE)' -Quiet)) { throw 'Blender step failed; see Logs\HQRig.log' }
if ($Preview) { Write-Host "Preview images in $Preview"; return }

Write-Host 'Replacing the textures in Unreal...'
$env:HELLGIRL_OUTFIT = $Outfit
$env:HELLGIRL_TEXTURE_DIR = Split-Path $Model -Parent
$env:HELLGIRL_TEXTURE_STEM = [IO.Path]::GetFileNameWithoutExtension($Model)
$script = Join-Path $env:TEMP 'hellgirl_import_outfit_textures.py'
Copy-Item (Join-Path $PSScriptRoot 'import_outfit_textures.py') $script -Force
$log = Join-Path $logDir 'OutfitTextures.log'
$p = Start-Process 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList @("`"$project\Hellgirl.uproject`"", '-run=pythonscript', "-script=$script", '-unattended', '-nullrhi', '-nosplash', '-nosound', "-abslog=`"$log`"") -WindowStyle Hidden -PassThru
$p.WaitForExit()
$result = Select-String $log -Pattern 'OUTFIT TEXTURES (PASSED|FAILED).*' | Select-Object -Last 1
if (-not $result -or $result.Line -match 'FAILED') { throw 'Texture import failed; see Logs\OutfitTextures.log' }
Write-Host $result.Matches[0].Value

& (Join-Path $PSScriptRoot 'fist_hands.ps1') -Outfits $Outfit
