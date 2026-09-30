# The whole map as one maze of 6 m (40 px) corridors: the upper-left maze exactly as approved (gen_maze.ps1, seed 1),
# the rest generated to match and joined to it. Rooms: the vortex room and the four spawn rooms, one door each.
# Tries -Seeds layouts and keeps the one whose four spawn-to-vortex walks are most even. Writes edits.txt, curtains.txt
# and layout.txt (the chosen seed and walks).
param([int]$Seeds = 40, [int]$Seed = 0)
$ErrorActionPreference = 'Stop'
$dir = $PSScriptRoot
$P = 40
$X0 = 40; $X1 = 1560; $Y0 = 40; $Y1 = 1080
$dirs = @{ N = @(0, -$P); E = @($P, 0); S = @(0, $P); W = @(-$P, 0) }
function Key($x, $y, $x2, $y2) { if ($x -lt $x2 -or ($x -eq $x2 -and $y -lt $y2)) { "$x,$y|$x2,$y2" } else { "$x2,$y2|$x,$y" } }

# --- The approved upper-left maze (same code and seed as gen_maze.ps1) --------------------------------------------------
$oldAreas = @(@(280, 80, 1000, 320), @(280, 320, 600, 640), @(40, 400, 280, 640))
$old = @{}
foreach ($a in $oldAreas) { for ($x = $a[0]; $x -lt $a[2]; $x += $P) { for ($y = $a[1]; $y -lt $a[3]; $y += $P) { $old["$x,$y"] = $true } } }
$oldCarved = @{}
$rnd = New-Object System.Random 1
$seen = @{ '440,480' = $true }
$stack = New-Object System.Collections.Stack
$stack.Push(@(440, 480))
while ($stack.Count -gt 0) {
    $cur = $stack.Peek(); $x = $cur[0]; $y = $cur[1]
    $next = @()
    foreach ($d in 'N', 'E', 'S', 'W') { $nx = $x + $dirs[$d][0]; $ny = $y + $dirs[$d][1]; if ($old["$nx,$ny"] -and -not $seen["$nx,$ny"]) { $next += , @($nx, $ny) } }
    if ($next.Count -eq 0) { [void]$stack.Pop(); continue }
    $n = $next[$rnd.Next($next.Count)]
    $oldCarved[(Key $x $y $n[0] $n[1])] = $true
    $seen["$($n[0]),$($n[1])"] = $true
    $stack.Push($n)
}
$internal = @()
foreach ($k in ($old.Keys | Sort-Object)) { $parts = $k.Split(','); $x = [int]$parts[0]; $y = [int]$parts[1]
    foreach ($d in 'E', 'S') { $x2 = $x + $dirs[$d][0]; $y2 = $y + $dirs[$d][1]; if ($old["$x2,$y2"] -and -not $oldCarved[(Key $x $y $x2 $y2)]) { $internal += (Key $x $y $x2 $y2) } } }
for ($i = 0; $i -lt 14 -and $internal.Count -gt 0; $i++) { $oldCarved[$internal[$rnd.Next($internal.Count)]] = $true }
foreach ($y in 440, 480, 520) { $oldCarved[(Key 480 $y 520 $y)] = $true } # C1
# Its ways out, now into the new maze (the vortex door is added with the rooms).
$oldLinks = @((Key 360 80 360 40), (Key 640 80 640 40), (Key 240 400 240 360), (Key 280 240 240 240), (Key 40 600 40 640), (Key 200 600 200 640), (Key 440 600 440 640))

# --- Rooms ---------------------------------------------------------------------------------------------------------
$rooms = @(
    @{ Name = 'S1'; R = @(40, 40, 240, 160); Door = (Key 80 120 80 160) },
    @{ Name = 'S2'; R = @(1320, 40, 1560, 160); Door = (Key 1280 80 1320 80) },
    @{ Name = 'S3'; R = @(40, 920, 240, 1080); Door = (Key 200 960 240 960) },
    @{ Name = 'S4'; R = @(1360, 840, 1560, 1080); Door = (Key 1320 920 1360 920) },
    @{ Name = 'Vortex'; R = @(600, 320, 1000, 640); Door = (Key 960 280 960 320) })
$room = @{}
foreach ($r in $rooms) { for ($x = $r.R[0]; $x -lt $r.R[2]; $x += $P) { for ($y = $r.R[1]; $y -lt $r.R[3]; $y += $P) { $room["$x,$y"] = $r.Name } } }
$new = @()
for ($x = $X0; $x -lt $X1; $x += $P) { for ($y = $Y0; $y -lt $Y1; $y += $P) { if (-not $old["$x,$y"] -and -not $room["$x,$y"]) { $new += "$x,$y" } } }
$isNew = @{}; foreach ($c in $new) { $isNew[$c] = $true }
# Curtains: waterfalls falling across a maze wall, always hiding a way through it (x0,y0,x1,y1 on a wall line).
# C1 is part of the upper-left maze; C2-C7 are carved here.
$curtains = [ordered]@{ C1 = @(520, 440, 520, 560); C2 = @(1120, 360, 1120, 480); C3 = @(1320, 560, 1320, 640);
    C4 = @(720, 760, 800, 760); C5 = @(400, 840, 400, 920); C6 = @(1120, 920, 1200, 920); C7 = @(1360, 240, 1440, 240);
    C8 = @(800, 40, 800, 80) } # C8: through the top row's wall between W2 and W3 (the wall stays blocked for the layout)
$curtainKeys = @()
foreach ($c in $curtains.Values) {
    if ($c[0] -eq $c[2]) { for ($y = $c[1]; $y -lt $c[3]; $y += $P) { $curtainKeys += (Key ($c[0] - $P) $y $c[0] $y) } }
    else { for ($x = $c[0]; $x -lt $c[2]; $x += $P) { $curtainKeys += (Key $x ($c[1] - $P) $x $c[1]) } }
}
# The top row is walled into sections, so it is no straight run past the waterfalls to the upper-left maze.
$blocked = @{}
foreach ($k in @((Key 440 40 480 40), (Key 760 40 800 40))) { $blocked[$k] = $true }

function Build([int]$seed) {
    $carved = @{}
    foreach ($k in $oldCarved.Keys) { $carved[$k] = $true }
    foreach ($k in $oldLinks) { $carved[$k] = $true }
    foreach ($k in $curtainKeys) { $carved[$k] = $true }
    $rnd = New-Object System.Random $seed
    $comp = @{}; $nComp = 0
    foreach ($c in ($new | Sort-Object { $rnd.Next() })) {
        if ($comp[$c]) { continue }
        $nComp++; $comp[$c] = $nComp
        $st = New-Object System.Collections.Stack; $p0 = $c.Split(','); $st.Push(@([int]$p0[0], [int]$p0[1]))
        while ($st.Count -gt 0) {
            $cur = $st.Peek(); $x = $cur[0]; $y = $cur[1]
            $next = @()
            foreach ($d in 'N', 'E', 'S', 'W') { $nx = $x + $dirs[$d][0]; $ny = $y + $dirs[$d][1]; if ($isNew["$nx,$ny"] -and -not $comp["$nx,$ny"] -and -not $blocked[(Key $x $y $nx $ny)]) { $next += , @($nx, $ny) } }
            if ($next.Count -eq 0) { [void]$st.Pop(); continue }
            $n = $next[$rnd.Next($next.Count)]
            $carved[(Key $x $y $n[0] $n[1])] = $true
            $comp["$($n[0]),$($n[1])"] = $nComp
            $st.Push($n)
        }
    }
    # Loops among the new cells (about 7%, like the upper left).
    $walls = @()
    foreach ($c in $new) { $parts = $c.Split(','); $x = [int]$parts[0]; $y = [int]$parts[1]
        foreach ($d in 'E', 'S') { $x2 = $x + $dirs[$d][0]; $y2 = $y + $dirs[$d][1]; if ($isNew["$x2,$y2"] -and -not $carved[(Key $x $y $x2 $y2)] -and -not $blocked[(Key $x $y $x2 $y2)]) { $walls += (Key $x $y $x2 $y2) } } }
    $walls = $walls | Sort-Object
    for ($i = 0; $i -lt [int]($new.Count * .07); $i++) { $carved[$walls[$rnd.Next($walls.Count)]] = $true }
    foreach ($r in $rooms) { $carved[$r.Door] = $true }
    return $carved
}
# Walks (in cells) from each spawn room's door to the vortex door over the cell graph.
function Walks($carved) {
    $target = '960,280'
    $dist = @{ $target = 0 }; $q = New-Object System.Collections.Queue; $q.Enqueue($target)
    while ($q.Count -gt 0) {
        $c = $q.Dequeue(); $parts = $c.Split(','); $x = [int]$parts[0]; $y = [int]$parts[1]
        foreach ($d in 'N', 'E', 'S', 'W') { $nx = $x + $dirs[$d][0]; $ny = $y + $dirs[$d][1]; $n = "$nx,$ny"
            if ($dist.ContainsKey($n) -or $room[$n] -or -not ($isNew[$n] -or $old[$n])) { continue }
            if (-not $carved[(Key $x $y $nx $ny)]) { continue }
            $dist[$n] = $dist[$c] + 1; $q.Enqueue($n) }
    }
    $out = @()
    foreach ($r in $rooms | Where-Object { $_.Name -ne 'Vortex' }) {
        $cells = @($r.Door.Split('|') | Where-Object { -not $room[$_] })
        $out += $(if ($dist.ContainsKey($cells[0])) { $dist[$cells[0]] } else { -1 })
    }
    return $out
}
$best = $null; $bestScore = 1e9
foreach ($s in $(if ($Seed) { @($Seed) } else { 1..$Seeds })) {
    $carved = Build $s; $w = Walks $carved
    if ($w -contains -1) { continue }
    $min = ($w | Measure-Object -Minimum).Minimum; $max = ($w | Measure-Object -Maximum).Maximum
    $score = ($max - $min) + [math]::Max(0, 30 - $min) * 3 # even, and none too short
    if ($score -lt $bestScore) { $bestScore = $score; $best = @{ Seed = $s; Carved = $carved; Walks = $w } }
}
"seed $($best.Seed): walks in cells $($best.Walks -join ', ') (x6 m)" | Tee-Object -FilePath "$dir\layout.txt"
# --- Walls ---------------------------------------------------------------------------------------------------------
$carved = $best.Carved
$out = @("add $X0,$Y0,$X1,$Y0", "add $X0,$Y1,$X1,$Y1", "add $X0,$Y0,$X0,$Y1", "add $X1,$Y0,$X1,$Y1")
for ($x = $X0; $x -lt $X1; $x += $P) { for ($y = $Y0; $y -lt $Y1; $y += $P) {
    foreach ($d in 'E', 'S') {
        $x2 = $x + $dirs[$d][0]; $y2 = $y + $dirs[$d][1]
        if ($x2 -ge $X1 -or $y2 -ge $Y1) { continue }
        $a = $room["$x,$y"]; $b = $room["$x2,$y2"]
        if ($a -and $a -eq $b) { continue }                 # inside a room
        if ($carved[(Key $x $y $x2 $y2)]) { continue }       # a passage or door
        if ($d -eq 'E') { $out += "add $x2,$y,$x2,$($y + $P)" } else { $out += "add $x,$y2,$($x + $P),$y2" }
    } } }
$out | Set-Content "$dir\edits.txt"
($curtains.Keys | ForEach-Object { "$_ $($curtains[$_] -join ',')" }) | Set-Content "$dir\curtains.txt"
"$($out.Count) wall edges"
