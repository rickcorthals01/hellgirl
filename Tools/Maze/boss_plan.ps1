# Draws the plan of the Frozen Maze's boss room (Saved\Maze\BossRoomDesign.png) from the user's sketch: a wide open oval
# cavern, the vortex's charred imprint in the middle, big stalagmites (and stalactites above them), frozen bodies that
# drop Souls, and purple lightning pulsing along the walls and across the roof. 1 m = $Px pixels.
param([double]$Px = 10.5)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$out = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'Saved\Maze'
New-Item -ItemType Directory -Force $out | Out-Null
$RoomX = 50.0; $RoomY = 35.0                       # the floor: an oval 100 x 70 m
# The sketch (1618 x 1100) mapped into the room: centre (809, 550), shrunk so everything stands inside the wall.
function FromSketch($x, $y) { @((($x - 809) / 809 * $RoomX * .72), (($y - 550) / 550 * $RoomY * .72)) }
$stalagmites = @((FromSketch 145 185), (FromSketch 480 150), (FromSketch 1260 260), (FromSketch 1350 960))
$bodies = @((FromSketch 190 420), (FromSketch 795 170), (FromSketch 1350 430), (FromSketch 1390 700), (FromSketch 450 820), (FromSketch 130 1010))
$W = 1440; $H = 1060; $cx = $W / 2; $cy = $H / 2 + 10; $side = 470
$bmp = New-Object System.Drawing.Bitmap ($W + $side), $H
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'AntiAlias'
function Col($r, $gg, $b, $a = 255) { [System.Drawing.Color]::FromArgb($a, $r, $gg, $b) }
function Brush($c) { New-Object System.Drawing.SolidBrush $c }
function P($m) { New-Object System.Drawing.PointF ([float]($cx + $m[0] * $Px)), ([float]($cy + $m[1] * $Px)) }
$g.Clear((Col 8 10 22))
$rnd = New-Object System.Random 5
# The cave wall: jagged ice all round, thicker outside the floor.
$ring = @(); $inner = @()
for ($k = 0; $k -lt 180; $k++) {
    $t = 2 * [math]::PI * $k / 180
    $j = 1 + .015 * [math]::Sin($t * 7) + .01 * [math]::Sin($t * 13 + 1)
    $inner += P @(($RoomX * $j * [math]::Cos($t)), ($RoomY * $j * [math]::Sin($t)))
    $spike = 1.12 + .06 * $rnd.NextDouble() + $(if ($k % 3 -eq 0) { .07 } else { 0 })
    $ring += P @(($RoomX * $spike * [math]::Cos($t)), ($RoomY * $spike * [math]::Sin($t)))
}
$g.FillPolygon((Brush (Col 70 110 160)), [System.Drawing.PointF[]]$ring)
$g.FillPolygon((Brush (Col 26 40 70)), [System.Drawing.PointF[]]$inner)
$g.DrawPolygon((New-Object System.Drawing.Pen (Col 190 225 250), 4), [System.Drawing.PointF[]]$inner)
# Faint cracks in the floor.
$crack = New-Object System.Drawing.Pen (Col 70 120 180 150), 1.5
for ($k = 0; $k -lt 70; $k++) {
    $ang = $rnd.NextDouble() * 2 * [math]::PI; $d = .35 + $rnd.NextDouble() * .6
    $p = @(($RoomX * $d * [math]::Cos($ang)), ($RoomY * $d * [math]::Sin($ang)))
    for ($s = 0; $s -lt 3; $s++) { $q = @(($p[0] + ($rnd.NextDouble() - .5) * 6), ($p[1] + ($rnd.NextDouble() - .5) * 6)); $g.DrawLine($crack, (P $p), (P $q)); $p = $q }
}
# The charred imprint of the vortex: a burnt spiral, still smouldering red, orange and yellow at its heart.
$burn = New-Object System.Drawing.Drawing2D.GraphicsPath
$burn.AddEllipse([float]($cx - 17 * $Px), [float]($cy - 10 * $Px), [float](34 * $Px), [float](20 * $Px))
$g.FillPath((Brush (Col 12 10 12 235)), $burn)
$colours = @((Col 60 60 64), (Col 255 120 30), (Col 90 90 96), (Col 230 40 30), (Col 255 225 60), (Col 30 28 30))
for ($arm = 0; $arm -lt 3; $arm++) {
    $pts = @()
    for ($s = 0; $s -le 160; $s++) {
        $r = .6 + $s / 160 * 16; $t = $s / 160 * 4.2 * [math]::PI + $arm * 2 * [math]::PI / 3
        $pts += P @(($r * [math]::Cos($t)), ($r * .6 * [math]::Sin($t)))
    }
    for ($s = 1; $s -lt $pts.Count; $s++) {
        $heat = 1 - $s / $pts.Count
        $c = if ($heat -gt .8) { Col 255 225 70 } elseif ($heat -gt .6) { Col 250 70 30 } elseif ($heat -gt .4) { Col 240 120 30 } else { Col 80 80 88 }
        $g.DrawLine((New-Object System.Drawing.Pen $c, ([float](2 + 5 * $heat))), $pts[$s - 1], $pts[$s])
    }
}
$g.FillEllipse((Brush (Col 255 200 120 120)), [float]($cx - 22), [float]($cy - 14), 44, 28)
# Purple lightning: arcs crawling along the walls, and across the roof between the stalactites (lighter, dashed).
function Bolt($from, $to, $pen) {
    $pts = @((P $from)); $n = 9
    for ($s = 1; $s -lt $n; $s++) { $f = $s / $n; $pts += P @(($from[0] + ($to[0] - $from[0]) * $f + ($rnd.NextDouble() - .5) * 3.5), ($from[1] + ($to[1] - $from[1]) * $f + ($rnd.NextDouble() - .5) * 3.5)) }
    $pts += P $to
    $g.DrawLines($pen, [System.Drawing.PointF[]]$pts)
}
$glowPen = New-Object System.Drawing.Pen (Col 170 80 255 90), 9
$boltPen = New-Object System.Drawing.Pen (Col 215 150 255), 2.5
foreach ($ang in 200, 235, 300, 330, 20, 60, 115, 150) {
    $t0 = $ang * [math]::PI / 180; $t1 = ($ang + 22) * [math]::PI / 180
    $f = @(($RoomX * 1.02 * [math]::Cos($t0)), ($RoomY * 1.02 * [math]::Sin($t0))); $t = @(($RoomX * 1.02 * [math]::Cos($t1)), ($RoomY * 1.02 * [math]::Sin($t1)))
    Bolt $f $t $glowPen; Bolt $f $t $boltPen
}
$roofPen = New-Object System.Drawing.Pen (Col 200 130 255 170), 2; $roofPen.DashStyle = 'Dash'
for ($i = 0; $i -lt $stalagmites.Count; $i++) { $n = $stalagmites[($i + 1) % $stalagmites.Count]; Bolt $stalagmites[$i] $n $roofPen }
Bolt $stalagmites[0] @(0, -3) $roofPen; Bolt $stalagmites[2] @(0, 3) $roofPen
# Stalagmites (black, from the sketch) with the stalactites hanging over them (dashed).
function Rock($m, $size, $seed, $pen, $fill) {
    $r2 = New-Object System.Random $seed; $pts = @()
    for ($k = 0; $k -lt 9; $k++) { $t = 2 * [math]::PI * $k / 9; $d = $size * (.55 + .5 * $r2.NextDouble()); $pts += P @(($m[0] + $d * [math]::Cos($t)), ($m[1] + $d * [math]::Sin($t))) }
    if ($fill) { $g.FillPolygon($fill, [System.Drawing.PointF[]]$pts) }
    $g.DrawPolygon($pen, [System.Drawing.PointF[]]$pts)
}
$dash = New-Object System.Drawing.Pen (Col 160 200 235), 2; $dash.DashStyle = 'Dash'
$font = New-Object System.Drawing.Font 'Segoe UI', 13, ([System.Drawing.FontStyle]::Bold)
$small = New-Object System.Drawing.Font 'Segoe UI', 11
$white = [System.Drawing.Brushes]::White
for ($i = 0; $i -lt $stalagmites.Count; $i++) {
    $m = $stalagmites[$i]
    Rock $m 6.5 (10 + $i) (New-Object System.Drawing.Pen (Col 200 225 245), 2.5) (Brush (Col 6 8 14))
    Rock @(($m[0] + 2), ($m[1] - 2)) 8.5 (30 + $i) $dash $null
    $p = P $m; $g.DrawString("M$($i + 1)", $font, $white, $p.X - 14, $p.Y - 10)
}
# Frozen bodies (drop Souls when smashed).
for ($i = 0; $i -lt $bodies.Count; $i++) {
    $p = P $bodies[$i]
    $g.FillRectangle((Brush (Col 200 230 250 230)), $p.X - 9, $p.Y - 13, 18, 26); $g.DrawRectangle((New-Object System.Drawing.Pen (Col 60 90 120), 1.5), $p.X - 9, $p.Y - 13, 18, 26)
    $g.FillEllipse((Brush (Col 255 190 60)), $p.X - 4, $p.Y - 4, 8, 8)
    $g.DrawString("R$($i + 1)", $small, $white, $p.X + 12, $p.Y - 10)
}
# Where she arrives (out of the vortex, which burns out behind her) and where the boss waits.
$s = P @(-22, 0); $g.FillEllipse((Brush (Col 120 255 170)), $s.X - 12, $s.Y - 12, 24, 24); $g.DrawString('START', $font, $white, $s.X - 26, $s.Y + 14)
$b = P @(28, 0); $g.FillEllipse((Brush (Col 230 40 60)), $b.X - 16, $b.Y - 16, 32, 32); $g.DrawString('BOSS', $font, $white, $b.X - 22, $b.Y + 18)
# Scale: 10 m, and the maze's vortex room for comparison (80 x 64 m).
$len = 10 * $Px
$g.FillRectangle($white, 40, $H - 40, $len, 6); $g.DrawString('10 m', $small, $white, 40 + $len + 8, $H - 50)
$cmp = New-Object System.Drawing.Pen (Col 150 150 170 140), 1.5; $cmp.DashStyle = 'Dot'
$g.DrawRectangle($cmp, [float]($cx - 40 * $Px), [float]($cy - 32 * $Px), [float](80 * $Px), [float](64 * $Px))
$g.DrawString("the maze's vortex room (80 x 64 m), for size", $small, (Brush (Col 170 170 190)), [float]($cx - 40 * $Px + 6), [float]($cy + 32 * $Px - 22))
# Side panel.
$sx = $W + 10; $yy = 30
$lines = @(
    @('THE FROZEN MAZE - BOSS ROOM (design v1)', $font), @('', $small),
    @('Beyond the vortex: a wide open ice cavern,', $small), @('an oval 100 x 70 m (about 5,500 m2 of floor,', $small),
    @('the maze''s vortex room holds 5,100). The roof', $small), @('arches 45 m up. Room for a big boss.', $small), @('', $small),
    @('Centre: the vortex''s charred imprint, a burnt', $small), @('spiral 34 x 20 m burnt into the ice, still', $small),
    @('smouldering red, orange and yellow at its heart.', $small), @('', $small),
    @('M1-M4  huge stalagmites (8-25 m), cover to', $small), @('        fight round; stalactites hang over each', $small),
    @('        (dashed) from the roof', $small), @('', $small),
    @('R1-R6  frozen bodies: smash them for Souls', $small), @('        (like the maze''s frozen remains)', $small), @('', $small),
    @('Purple lightning pulses along the walls and', $small), @('arcs across the roof between the stalactites', $small),
    @('(dashed); the whole room flickers violet.', $small), @('', $small),
    @('START  she steps out of the vortex; it burns out', $small), @('        behind her, leaving the imprint', $small),
    @('BOSS   where the boss waits (to decide)', $small), @('', $small),
    @('Same look as the maze: glossy blue ice,', $small), @('cracked floor, mist, thick fog, icicles.', $small)
)
foreach ($l in $lines) { $g.DrawString($l[0], $l[1], $white, $sx, $yy); $yy += $(if ($l[1] -eq $font) { 30 } else { 23 }) }
$bmp.Save("$out\BossRoomDesign.png"); "saved $out\BossRoomDesign.png"
