# The Frozen Maze plan: the walls from edits.txt (gen_full.ps1) and the elements from elements.txt (elements.ps1).
# Checks every spawn can walk to the vortex (Walk.cs) and draws Saved\Maze\MazeDesign.png (plus regions.png for
# debugging) and walls.txt (the straight walls). One grid square (40 px) is 8 m in the game.
param([double]$Metres = 0.2, [int]$Clear = 1)
$ErrorActionPreference = 'Stop'
$dir = $PSScriptRoot
$out = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'Saved\Maze'
New-Item -ItemType Directory -Force $out | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -Path "$dir\Snap.cs", "$dir\Walk.cs"
[Snap]::L = 40
[Snap]::Empty(1620, 1100)
foreach ($l in Get-Content "$dir\edits.txt") {
    if (-not $l -or $l.StartsWith('#')) { continue }
    $p = $l.Split(' '); $v = $p[1].Split(',') | ForEach-Object { [int]$_ }
    [Snap]::Add($v[0], $v[1], $v[2], $v[3])
}
$runs = [Snap]::Runs()
($runs | ForEach-Object { $_ -join ',' }) | Set-Content "$dir\walls.txt"
$C = 4; $GW = 0; $GH = 0
$wall = [Snap]::Grid($C, 4, 1620, 1100, [ref]$GW, [ref]$GH)
$ok = [Walk]::Walkable($wall, $Clear)
$open = [Walk]::Walkable($wall, 0)
$n = 0; $regions = [Walk]::Regions($open, [ref]$n)
$spawns = @(@(153, 82, 'S1'), @(1487, 70, 'S2'), @(147, 1002, 'S3'), @(1490, 972, 'S4'))
$vortex = @(790, 470)
$report = @(); $routes = @(); $main = @{}
foreach ($s in $spawns) {
    $main[$regions[[int]($s[0] / $C), [int]($s[1] / $C)]] = $true
    $path = [Walk]::Route($ok, [int]($s[0] / $C), [int]($s[1] / $C), [int]($vortex[0] / $C), [int]($vortex[1] / $C), 12)
    $routes += , $path
    $report += "$($s[2]): " + $(if ($path.Count -eq 0) { 'NO ROUTE' } else { "about $([math]::Round($path.Count * $C * $Metres)) m" })
}
$report | Set-Content "$dir\report.txt"; $report
# --- Drawing ---------------------------------------------------------------------------------------------------------
$pad = 40; $side = 440
$bmp = New-Object System.Drawing.Bitmap (1620 + $pad * 2 + $side), (1100 + $pad * 2)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.TextRenderingHint = 'AntiAlias'
function Col($r, $gg, $b, $a = 255) { [System.Drawing.Color]::FromArgb($a, $r, $gg, $b) }
function Brush($c) { New-Object System.Drawing.SolidBrush $c }
$g.Clear((Col 10 14 34))
$g.FillRectangle((Brush (Col 20 28 58)), $pad + 20, $pad + 20, 1560, 1080)
# Floor where she can go; enclosed pockets are solid ice.
$floorB = Brush (Col 58 84 128); $blockB = Brush (Col 120 160 205)
for ($y = 0; $y -lt $GH; $y++) { for ($x = 0; $x -lt $GW; $x++) {
    $id = $regions[$x, $y]
    if ($id -gt 0) { $g.FillRectangle($(if ($main[$id]) { $floorB } else { $blockB }), $pad + $x * $C, $pad + $y * $C, $C, $C) } } }
$g.SmoothingMode = 'AntiAlias'
# The frozen lake under the vortex, its cracks glowing purple.
$g.FillEllipse((Brush (Col 110 150 205 170)), $pad + 640, $pad + 360, 320, 230)
$crackPen = New-Object System.Drawing.Pen (Col 190 140 255 200), 2
$rc = New-Object System.Random 7
for ($k = 0; $k -lt 11; $k++) {
    $a = $k / 11 * 2 * [math]::PI + $rc.NextDouble() * .4; $r = 30; $px = 800 + [math]::Cos($a) * $r; $py = 470 + [math]::Sin($a) * $r * .72
    for ($s = 0; $s -lt 5; $s++) { $r += 16 + $rc.Next(10); $b = $a + ($rc.NextDouble() - .5) * .5
        $nx = 800 + [math]::Cos($b) * $r; $ny = 470 + [math]::Sin($b) * $r * .72
        $g.DrawLine($crackPen, $pad + $px, $pad + $py, $pad + $nx, $pad + $ny); $px = $nx; $py = $ny }
}
# Ice spires, in clusters in the rooms.
$spireB = Brush (Col 200 235 255 230); $spireP = New-Object System.Drawing.Pen (Col 90 140 200), 1
function Spire($x, $y, $s) { $pts = [System.Drawing.PointF[]]@((New-Object System.Drawing.PointF ($pad + $x), ($pad + $y - $s)), (New-Object System.Drawing.PointF ($pad + $x - $s * .38), ($pad + $y)), (New-Object System.Drawing.PointF ($pad + $x + $s * .38), ($pad + $y)))
    $g.FillPolygon($spireB, $pts); $g.DrawPolygon($spireP, $pts) }
function Spires($x, $y) { Spire ($x - 12) ($y + 4) 22; Spire $x $y 32; Spire ($x + 13) ($y + 6) 18 }
foreach ($at in @(@(632, 364), @(965, 610), @(632, 612), @(880, 352), @(215, 150), @(1530, 150), @(70, 1068), @(1530, 1068), @(1390, 870))) { Spires $at[0] $at[1] }
# Walls.
$wallPen = New-Object System.Drawing.Pen (Col 205 232 252), 7
$wallPen.StartCap = 'Square'; $wallPen.EndCap = 'Square'
foreach ($r in $runs) { $g.DrawLine($wallPen, $pad + $r[0], $pad + $r[1], $pad + $r[2], $pad + $r[3]) }
$font = New-Object System.Drawing.Font 'Segoe UI', 13, ([System.Drawing.FontStyle]::Bold)
$small = New-Object System.Drawing.Font 'Segoe UI', 11
$white = [System.Drawing.Brushes]::White
function Label($text, $x, $y) { $g.DrawString($text, $font, $white, $pad + $x, $pad + $y) }
# Routes.
$routeCols = @((Col 255 120 120), (Col 255 210 90), (Col 150 255 150), (Col 230 140 255))
for ($i = 0; $i -lt 4; $i++) {
    $path = $routes[$i]; if ($path.Count -lt 2) { continue }
    $pen = New-Object System.Drawing.Pen $routeCols[$i], 3; $pen.DashStyle = 'Dot'
    $pts = @(); for ($k = 0; $k -lt $path.Count; $k += 3) { $at = $path[$k]; $pts += New-Object System.Drawing.PointF ($pad + ($at % $GW) * $C + 2.5), ($pad + [math]::Floor($at / $GW) * $C + 2.5) }
    $g.DrawLines($pen, [System.Drawing.PointF[]]$pts)
}
# Elements (elements.ps1): the Deprived's lairs, glow crystals, frozen remains, icicle traps.
$tiny = New-Object System.Drawing.Font 'Segoe UI', 10, ([System.Drawing.FontStyle]::Bold)
$shade = Brush (Col 0 0 0 170)
$lairB = Brush (Col 8 4 14); $lairGlow = Brush (Col 200 40 90 90); $lairPen = New-Object System.Drawing.Pen (Col 235 70 120), 2.5
$eyeB = Brush (Col 255 255 255)
$glowB = Brush (Col 120 240 255 70); $crysB = Brush (Col 150 250 255); $crysP = New-Object System.Drawing.Pen (Col 255 255 255), 1.5
$remB = Brush (Col 215 230 240 235); $remP = New-Object System.Drawing.Pen (Col 70 90 110), 1.5; $soulB = Brush (Col 255 190 60)
$trapB = Brush (Col 255 255 255); $trapP = New-Object System.Drawing.Pen (Col 255 140 40), 2.5
function Lair($x, $y) { $g.FillEllipse($lairGlow, $pad + $x - 17, $pad + $y - 13, 34, 26); $g.FillEllipse($lairB, $pad + $x - 13, $pad + $y - 10, 26, 20)
    $g.DrawEllipse($lairPen, $pad + $x - 13, $pad + $y - 10, 26, 20)
    $g.FillEllipse($eyeB, $pad + $x - 6, $pad + $y - 3, 4, 4); $g.FillEllipse($eyeB, $pad + $x + 2, $pad + $y - 3, 4, 4) }
function Crystal($x, $y) { $g.FillEllipse($glowB, $pad + $x - 16, $pad + $y - 16, 32, 32)
    $pts = [System.Drawing.PointF[]]@((New-Object System.Drawing.PointF ($pad + $x), ($pad + $y - 10)), (New-Object System.Drawing.PointF ($pad + $x + 6), ($pad + $y)), (New-Object System.Drawing.PointF ($pad + $x), ($pad + $y + 10)), (New-Object System.Drawing.PointF ($pad + $x - 6), ($pad + $y)))
    $g.FillPolygon($crysB, $pts); $g.DrawPolygon($crysP, $pts) }
function Remains($x, $y) { $g.FillRectangle($remB, $pad + $x - 8, $pad + $y - 11, 16, 22); $g.DrawRectangle($remP, $pad + $x - 8, $pad + $y - 11, 16, 22)
    $g.FillEllipse($soulB, $pad + $x - 4, $pad + $y - 4, 8, 8) }
function Icicle($x, $y) { $pts = [System.Drawing.PointF[]]@((New-Object System.Drawing.PointF ($pad + $x - 10), ($pad + $y - 8)), (New-Object System.Drawing.PointF ($pad + $x + 10), ($pad + $y - 8)), (New-Object System.Drawing.PointF ($pad + $x), ($pad + $y + 11)))
    $g.FillPolygon($trapB, $pts); $g.DrawPolygon($trapP, $pts) }
$elements = @(Get-Content "$dir\elements.txt" | ForEach-Object { , $_.Split(' ') })
foreach ($e in $elements) { $x = [int]$e[2]; $y = [int]$e[3]
    switch ($e[0]) { 'G' { Crystal $x $y } 'R' { Remains $x $y } 'T' { Icicle $x $y } } }
foreach ($e in $elements | Where-Object { $_[0] -eq 'D' }) { $x = [int]$e[2]; $y = [int]$e[3]; Lair $x $y
    $g.DrawString($e[1], $tiny, $shade, $pad + $x + 1, $pad + $y + 10); $g.DrawString($e[1], $tiny, [System.Drawing.Brushes]::White, $pad + $x, $pad + $y + 9) }
# Waterfalls down the outer wall, each into a pool.
$fall = Brush (Col 90 190 255 220); $pool = Brush (Col 60 150 230 150); $fallPen = New-Object System.Drawing.Pen (Col 210 245 255), 2
foreach ($f in @(@(355, 425, 40, 1), @(600, 645, 40, 2), @(945, 1050, 40, 3), @(350, 440, 1080, 4), @(1020, 1230, 1080, 5))) {
    $x0 = $f[0]; $x1 = $f[1]; $top = $f[2] -eq 40
    $py = if ($top) { 42 } else { 1030 }
    $g.FillEllipse($pool, $pad + $x0 - 8, $pad + $py, ($x1 - $x0) + 16, 46)
    $g.FillRectangle($fall, $pad + $x0, $pad + $f[2] - 8, $x1 - $x0, 16); $g.DrawRectangle($fallPen, $pad + $x0, $pad + $f[2] - 8, $x1 - $x0, 16)
    Label "W$($f[3])" ($x0 + ($x1 - $x0) / 2 - 14) $(if ($top) { 52 } else { 1042 })
}
# Curtains (walk through): C1 hides a way through a maze wall, C2 falls in the east corridor.
$curtain = New-Object System.Drawing.Drawing2D.HatchBrush ([System.Drawing.Drawing2D.HatchStyle]::LightVertical), (Col 170 225 255 240), (Col 60 150 230 120)
foreach ($l in Get-Content "$dir\curtains.txt") {
    $name, $v = $l.Split(' '); $v = $v.Split(',') | ForEach-Object { [int]$_ }
    if ($v[0] -eq $v[2]) { $rx = $v[0] - 8; $ry = $v[1]; $rw = 16; $rh = $v[3] - $v[1]; $lx = $v[0] + 10; $ly = ($v[1] + $v[3]) / 2 - 10 }
    else { $rx = $v[0]; $ry = $v[1] - 8; $rw = $v[2] - $v[0]; $rh = 16; $lx = ($v[0] + $v[2]) / 2 - 12; $ly = $v[1] + 10 }
    $g.FillRectangle($curtain, $pad + $rx, $pad + $ry, $rw, $rh); $g.DrawRectangle($fallPen, $pad + $rx, $pad + $ry, $rw, $rh)
    Label $name $lx $ly
}# The vortex.
$vx = $pad + 800; $vy = $pad + 470
for ($k = 0; $k -lt 6; $k++) { $rr = 95 - $k * 15; $pen = New-Object System.Drawing.Pen (Col 170 110 255 (90 + $k * 30)), 4; $g.DrawArc($pen, $vx - $rr, $vy - $rr * .8, $rr * 2, $rr * 1.6, $k * 60, 300) }
$g.FillEllipse((Brush (Col 230 200 255)), $vx - 8, $vy - 8, 16, 16)
Label 'THE VORTEX' 745 580
for ($i = 0; $i -lt 4; $i++) {
    $s = $spawns[$i]
    $g.FillEllipse((Brush $routeCols[$i]), $pad + $s[0] - 14, $pad + $s[1] - 14, 28, 28)
    $g.DrawEllipse((New-Object System.Drawing.Pen ([System.Drawing.Color]::White), 2), $pad + $s[0] - 14, $pad + $s[1] - 14, 28, 28)
    Label $s[2] ($s[0] + 18) ($s[1] - 12)
}
$len = 20 / $Metres
$g.FillRectangle($white, $pad + 60, $pad + 1085, $len, 6); $g.DrawString('20 m', $small, $white, $pad + 60 + $len + 8, $pad + 1076)
# Side panel.
$sx = 1620 + $pad * 2
$nLairs = @($elements | Where-Object { $_[0] -eq 'D' }).Count
$lines = @(
    @('THE FROZEN MAZE - design v6', $font), @('', $small),
    @("About $([math]::Round(1520 * $Metres)) x $([math]::Round(1040 * $Metres)) m: the largest map yet.", $small),
    @('Icy underground cave: tall jagged ice walls', $small),
    @('(can''t see over), cracked frozen floor, mist,', $small),
    @("dark cave roof. Every corridor $([math]::Round(40 * $Metres)) m wide.", $small), @('', $small),
    @('S1-S4  spawn rooms (one picked per run)', $small),
    @('W1-W5  waterfalls into glowing pools', $small),
    @('C1-C8  waterfall curtains: walk through; each', $small), @('            hides a way through a maze wall', $small), @('', $small),
    @('THE DEPRIVED', $font),
    @('Strong shadows with scythes. Only one hunts', $small),
    @('her at a time, stalking her through the maze.', $small),
    @('When it dies, the next rises ~10 s later from', $small),
    @('the nearest lair she can''t see, at least', $small),
    @('~40 m walk away from her.', $small),
    @("D1-D$nLairs  lairs: shadow pools in dead ends,", $small, 'D'), @('none close to the spawn rooms', $small, '-'), @('', $small),
    @('ELEMENTS', $font),
    @('glow crystals at junctions: the only light', $small, 'G'), @('in the corridors', $small, '-'),
    @('frozen remains in dead ends: smash them', $small, 'R'), @('for Souls', $small, '-'),
    @('icicle traps on the ways to the vortex: a', $small, 'T'), @('shadow on the ice, then they fall', $small, '-'),
    @('ice spires in the rooms', $small, 'S'),
    @('The vortex hangs over a cracked frozen lake.', $small)
)
$yy = $pad
foreach ($l in $lines) {
    $ix = $sx - $pad + 12; $iy = $yy - $pad + 12; $tx = $sx
    if ($l.Count -gt 2) { $tx = $sx + 34
        switch ($l[2]) { 'D' { Lair $ix $iy } 'G' { Crystal $ix $iy } 'R' { Remains $ix $iy } 'T' { Icicle $ix $iy } 'S' { Spires $ix ($iy + 10) } } }
    $g.DrawString($l[0], $l[1], $white, $tx, $yy); $yy += $(if ($l[1] -eq $font) { 30 } else { 24 }) }
$yy += 10; $g.DrawString('Walk to the vortex:', $font, $white, $sx, $yy); $yy += 30
for ($i = 0; $i -lt 4; $i++) { $g.FillRectangle((Brush $routeCols[$i]), $sx, $yy + 6, 14, 14); $g.DrawString($report[$i], $small, $white, $sx + 22, $yy); $yy += 24 }
$bmp.Save("$out\MazeDesign.png")
# Debug: walkable regions coloured.
$dbg = New-Object System.Drawing.Bitmap ($GW * 3), ($GH * 3); $dg = [System.Drawing.Graphics]::FromImage($dbg); $dg.Clear([System.Drawing.Color]::Black)
$rid = [Walk]::Regions($ok, [ref]$n); $rnd = New-Object System.Random 3; $cols = @{}
for ($y = 0; $y -lt $GH; $y++) { for ($x = 0; $x -lt $GW; $x++) { $id = $rid[$x, $y]; if ($id -eq 0) { continue }
    if (-not $cols[$id]) { $cols[$id] = Brush (Col $rnd.Next(80, 255) $rnd.Next(80, 255) $rnd.Next(80, 255)) }
    $dg.FillRectangle($cols[$id], $x * 3, $y * 3, 3, 3) } }
$dbg.Save("$out\regions.png"); "regions with clearance: $n"
