# Runs every automated Hellgirl check headless and prints a pass/fail table.
# Usage (from the project folder, with the editor closed and the project built):
#   powershell -ExecutionPolicy Bypass -File Tests\run-checks.ps1
#   powershell -ExecutionPolicy Bypass -File Tests\run-checks.ps1 -Only Combat,Counter
# Logs go to Logs\Checks. Your save files are backed up first and restored afterwards,
# because some checks collect coins into the real wallet save.
param([string[]]$Only = @(), [int]$TimeoutSeconds = 150)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root 'Hellgirl.uproject'
$engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64'
$logDir = Join-Path $root 'Logs\Checks'
New-Item -ItemType Directory -Force $logDir | Out-Null

$castle = '/Engine/Maps/Entry?StageMap=1'
$queen = '/Engine/Maps/Entry?StageMap=1?CampaignLevel=3'
# Name, map URL, frame rate (0 = variable). Preview checks that need a GPU are left out.
# Known failures on 2026-09-23, before any refactoring (the checks predate later design changes):
#   Energy, EnemyMoveset, Campaign, Terrain, Map, Map2, Map3, Dialogue.
$checks = @(
    @('Combat',         $castle, 60), @('Counter',       $castle, 60), @('RevisedCombat', $castle, 60),
    @('Movement',       $castle, 30), @('Energy',        $castle, 60), @('CombatBalance', $castle, 60),
    @('Physics',        $castle, 60), @('PhysicsPause',  $castle, 0),  @('Pause',         $castle, 0),
    @('EnemyMoveset',   $castle, 60), @('BossDesign',    $castle, 60), @('GoblinStage',   $castle, 60),
    @('Story',          $queen,  60), @('Campaign',      $queen,  60), @('Save',          $castle, 60),
    @('Terrain',        $queen,  60), @('Map',           $queen,  60),
    @('Map',            '/Engine/Maps/Entry?StageMap=2', 0), @('Map', '/Engine/Maps/Entry?StageMap=3', 0),
    @('ImpArena',       '/Engine/Maps/Entry?StageMap=1?CampaignLevel=4', 60),
    @('Hub',            '/Engine/Maps/Entry?ForestHub=1', 60), @('Dialogue', '/Engine/Maps/Entry?ForestHub=1', 0),
    @('MainMenu',       '/Engine/Maps/Entry', 0)
)
if ($Only.Count) {
    # Build the filtered list explicitly; a one-item pipeline result would be unrolled into its strings.
    $filtered = New-Object System.Collections.ArrayList
    foreach ($c in $checks) { if ($Only -contains $c[0]) { [void]$filtered.Add($c) } }
    $checks = $filtered
}

$saves = Join-Path $root 'Saved\SaveGames'
$backup = Join-Path $root 'Saved\SaveGames.before-checks'
if (Test-Path $saves) { Remove-Item $backup -Recurse -Force -ErrorAction SilentlyContinue; Copy-Item $saves $backup -Recurse }

function Wait-Run($process, $name) {
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; return 'TIMEOUT' }
    $process.Refresh(); return $process.ExitCode
}

$results = @()
try {
    foreach ($c in $checks) {
        $name, $url, $fps = $c
        $label = if ($url -match 'StageMap=([23])') { "$name$($matches[1])" } else { $name }
        $log = Join-Path $logDir "$label.log"
        $argList = @("`"$project`"", $url, '-game', '-nullrhi', '-unattended', '-nosound', '-NoSplash', "-Hellgirl$($name)Check", "-abslog=`"$log`"")
        if ($fps -gt 0) { $argList += @('-UseFixedTimeStep', "-FPS=$fps") }
        $started = Get-Date
        $exit = Wait-Run (Start-Process (Join-Path $engine 'UnrealEditor.exe') -ArgumentList $argList -WindowStyle Hidden -PassThru) $label
        $text = if (Test-Path $log) { Get-Content $log -Raw } else { '' }
        $passed = $exit -eq 0 -and $text -match 'PASSED' -and $text -notmatch 'CHECK FAILED|FAILED:|Fatal error'
        $failLine = ([regex]::Matches($text, '.*(FAILED|Fatal error).*') | Select-Object -First 1).Value
        $results += [pscustomobject]@{ Check = $label; Result = if ($passed) { 'PASS' } else { 'FAIL' }; Exit = $exit
            Errors = ([regex]::Matches($text, 'Error:')).Count; Seconds = [int]((Get-Date) - $started).TotalSeconds; Detail = "$failLine".Trim() }
        Write-Host ("{0,-14} {1}" -f $label, $results[-1].Result)
    }

    if (-not $Only.Count -or $Only -contains 'Animation') {
        $log = Join-Path $logDir 'Animation.log'
        $argList = @("`"$project`"", '-unattended', '-nullrhi', '-nosound', '-NoSplash', '-ExecCmds="Automation RunTests Hellgirl.Animation"',
            '-TestExit="Automation Test Queue Empty"', "-abslog=`"$log`"")
        $exit = Wait-Run (Start-Process (Join-Path $engine 'UnrealEditor-Cmd.exe') -ArgumentList $argList -WindowStyle Hidden -PassThru) 'Animation'
        $text = if (Test-Path $log) { Get-Content $log -Raw } else { '' }
        $ok = ([regex]::Matches($text, 'Test Completed\. Result=\{Success\}')).Count
        $bad = ([regex]::Matches($text, 'Test Completed\. Result=\{(Fail|Error)')).Count
        $results += [pscustomobject]@{ Check = 'Animation'; Result = if ($exit -eq 0 -and $ok -gt 0 -and $bad -eq 0) { 'PASS' } else { 'FAIL' }; Exit = $exit
            Errors = ([regex]::Matches($text, 'Error:')).Count; Seconds = 0; Detail = "$ok passed, $bad failed" }
        Write-Host ("{0,-14} {1}" -f 'Animation', $results[-1].Result)
    }
}
finally {
    if (Test-Path $backup) { Remove-Item $saves -Recurse -Force -ErrorAction SilentlyContinue; Move-Item $backup $saves }
}

$results | Format-Table Check, Result, Exit, Errors, Seconds, Detail -AutoSize | Out-String -Width 220
$results | ConvertTo-Json | Set-Content (Join-Path $logDir 'results.json') -Encoding utf8
if ($results.Result -contains 'FAIL') { exit 1 }
