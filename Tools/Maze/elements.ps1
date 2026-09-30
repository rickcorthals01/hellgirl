# Places the maze's elements on the cell graph (Cells.cs, walls from edits.txt) and writes elements.txt
# ("kind name x y", sketch px, cell centres):
#   D  the Deprived's lairs: shadow pools in dead ends, spread evenly, kept away from the spawn rooms and the vortex door
#   R  frozen remains (souls inside): the other dead ends, spread out
#   G  glow crystals at junctions, the maze's only light
#   T  icicle traps on straight corridors of the routes to the vortex
param([int]$NLairs = 16, [int]$NRemains = 12, [int]$NCrystals = 26, [int]$NTraps = 8)
$ErrorActionPreference = 'Stop'
$dir = $PSScriptRoot
Add-Type -Path "$dir\Cells.cs"
[Cells]::Load([string[]](Get-Content "$dir\edits.txt"))
[Cells]::SetRoom('S1', 40, 40, 240, 160); [Cells]::SetRoom('S2', 1320, 40, 1560, 160)
[Cells]::SetRoom('S3', 40, 920, 240, 1080); [Cells]::SetRoom('S4', 1360, 840, 1560, 1080)
[Cells]::SetRoom('Vortex', 600, 320, 1000, 640)
$doors = @([Cells]::Idx(80, 160), [Cells]::Idx(1280, 80), [Cells]::Idx(240, 960), [Cells]::Idx(1320, 920)) # just outside each spawn room
$vDoor = [Cells]::Idx(960, 280)
# Nothing goes in the waterfalls' pools (the top and bottom rows under W1-W5) or on or beside a curtain.
$avoid = @{}
foreach ($f in @(@(355, 425, 60), @(600, 645, 60), @(945, 1050, 60), @(350, 440, 1060), @(1020, 1230, 1060))) {
    for ($x = 40; $x -lt 1560; $x += 40) { if ($x + 40 -gt $f[0] - 20 -and $x -lt $f[1] + 20) { $avoid[[Cells]::Idx($x, $f[2] - 20)] = $true } } }
foreach ($l in Get-Content "$dir\curtains.txt") { $v = $l.Split(' ')[1].Split(',') | ForEach-Object { [int]$_ }
    $cells = if ($v[0] -eq $v[2]) { for ($y = $v[1]; $y -lt $v[3]; $y += 40) { @(($v[0] - 40), $y), @($v[0], $y) } } else { for ($x = $v[0]; $x -lt $v[2]; $x += 40) { @($x, ($v[1] - 40)), @($x, $v[1]) } }
    foreach ($at in $cells) { foreach ($o in @(@(0, 0), @(40, 0), @(-40, 0), @(0, 40), @(0, -40))) { $x = $at[0] + $o[0]; $y = $at[1] + $o[1]
        if ($x -ge 40 -and $x -lt 1560 -and $y -ge 40 -and $y -lt 1080) { $avoid[[Cells]::Idx($x, $y)] = $true } } } }
$maze = @(0..(38 * 26 - 1) | Where-Object { -not [Cells]::Room[$_] })
$free = @($maze | Where-Object { -not $avoid[$_] })
$fromSpawns = [Cells]::Bfs([int[]]$doors)
$toVortex = [Cells]::Bfs([int[]]@($vDoor))
$dead = @($free | Where-Object { [Cells]::Degree($_) -eq 1 })
$junctions = @($free | Where-Object { [Cells]::Degree($_) -ge 3 })
# Lairs: dead ends at least 7 cells' walk (42 m) from every spawn room and 4 from the vortex door, picked to cover the maze.

$lairCand = @($dead | Where-Object { $fromSpawns[$_] -ge 7 -and $toVortex[$_] -ge 4 })
$lairs = [Cells]::Cover([int[]]$lairCand, [int[]]$maze, $NLairs)
$toLair = [Cells]::Bfs([int[]]$lairs)
$cover = ($maze | ForEach-Object { $toLair[$_] } | Measure-Object -Maximum -Average)
$avg = [math]::Round($cover.Average, 1); $cover = $cover.Maximum
# Remains: the other dead ends, 3+ cells from a lair.
$remCand = @($dead | Where-Object { $lairs -notcontains $_ -and $toLair[$_] -ge 3 -and $fromSpawns[$_] -ge 3 })
$remains = [Cells]::Spread([int[]]$remCand, $toLair, $NRemains)
# Crystals: junctions, spread; the rooms have their own light.
$none = [int[]]@(0..(38 * 26 - 1) | ForEach-Object { [int]::MaxValue })
$crystals = [Cells]::Spread([int[]]$junctions, $none, $NCrystals)
# Traps: straight corridor cells on the four routes, away from the doors.
$route = @{}
foreach ($d in $doors) { foreach ($c in [Cells]::Route($d, $toVortex)) { $route[$c] = $true } }
$trapCand = @($route.Keys | Where-Object { -not $avoid[$_] -and [Cells]::Straight($_) -and $fromSpawns[$_] -ge 5 -and $toVortex[$_] -ge 4 })
$traps = [Cells]::Spread([int[]]$trapCand, [Cells]::Min($fromSpawns, $toVortex), $NTraps)
# Number the lairs top to bottom, left to right.
$out = @(); $k = 0
foreach ($c in ($lairs | Sort-Object { [Cells]::Cy($_) }, { [Cells]::Cx($_) })) { $k++; $out += "D D$k $([Cells]::Cx($c)) $([Cells]::Cy($c))" }
foreach ($c in $remains) { $out += "R R $([Cells]::Cx($c)) $([Cells]::Cy($c))" }
foreach ($c in $crystals) { $out += "G G $([Cells]::Cx($c)) $([Cells]::Cy($c))" }
foreach ($c in $traps) { $out += "T T $([Cells]::Cx($c)) $([Cells]::Cy($c))" }
$out | Set-Content "$dir\elements.txt"
"maze cells $($maze.Count), dead ends $($dead.Count), junctions $($junctions.Count), route cells $($route.Count)"
"lairs $($lairs.Count) (every maze cell within $cover cells of one, $avg on average), remains $($remains.Count), crystals $($crystals.Count), traps $($traps.Count)"
