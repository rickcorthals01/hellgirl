# Cuts the placeholder menu art ("Hellgirl Game\UI\*.png", one flat mockup per menu) into the pieces the menus draw:
# each mockup whole (the menu's background) and its buttons in their plain (dark) and selected (red) looks. Text baked
# into a piece is painted over with the piece's own colour so the game can write its own. Results go to UI\Processed;
# import_ui.ps1 brings them into Unreal as /Game/UI/Placeholder/T_<Name>.
#   powershell -ExecutionPolicy Bypass -File Tools\UI\prepare_ui.ps1
param([string]$Source = '')
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not $Source) { $Source = Join-Path (Split-Path $project -Parent) 'UI' }
$out = Join-Path $Source 'Processed'
New-Item -ItemType Directory -Force $out | Out-Null
Add-Type -AssemblyName System.Drawing

# Name, mockup, x, y, width, height, and an optional area (x, y, w, h inside the piece) whose baked text is painted over.
$pieces = @(
    @('PortalBarRed',   'Blue Portal Menu.png', 140, 40, 750, 200),
    @('PortalBarDark',  'Blue Portal Menu.png', 160, 222, 704, 152),
    @('PortalRow',      'Blue Portal Menu.png', 32, 446, 970, 212),
    @('DeathRed',       'Death Retry Return to camp.png', 228, 168, 290, 66),
    @('DeathDark',      'Death Retry Return to camp.png', 230, 243, 282, 60),
    @('ShopTabRed',     'Shop menu.png', 106, 168, 254, 106, @(22, 20, 210, 66)),
    @('ShopTabDark',    'Shop menu.png', 362, 170, 250, 100, @(22, 18, 206, 64)),
    @('ShopCellRed',    'Shop menu.png', 108, 303, 182, 182),
    @('ShopCellDark',   'Shop menu.png', 296, 305, 178, 178),
    @('StageCardRed',   'Stage Select.png', 103, 58, 152, 275),
    @('StageCardDark',  'Stage Select.png', 303, 58, 148, 275),
    @('StageButtonRed', 'Stage Select.png', 150, 356, 230, 58),
    @('StageButtonDark','Stage Select.png', 390, 358, 200, 50),
    @('WorldItemRed',   'World Select.png', 80, 72, 178, 80),
    @('WorldItemDark',  'World Select.png', 80, 152, 178, 76),
    @('WorldButton',    'World Select.png', 326, 432, 300, 60)
)
$backgrounds = @(@('PortalMenu', 'Blue Portal Menu.png'), @('DeathMenu', 'Death Retry Return to camp.png'), @('ShopMenu', 'Shop menu.png'),
    @('StageSelect', 'Stage Select.png'), @('WorldSelect', 'World Select.png'))

foreach ($b in $backgrounds) {
    $img = [System.Drawing.Image]::FromFile((Join-Path $Source $b[1]))
    $img.Save((Join-Path $out "$($b[0]).png"), [System.Drawing.Imaging.ImageFormat]::Png); $img.Dispose()
}
# The shop background loses its baked tab names (the tabs are drawn over it anyway, with the game's text).
foreach ($p in $pieces) {
    $src = New-Object System.Drawing.Bitmap (Join-Path $Source $p[1])
    $rect = New-Object System.Drawing.Rectangle $p[2], $p[3], $p[4], $p[5]
    $piece = $src.Clone($rect, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    if ($p.Count -gt 6) {
        $r = $p[6]
        # The colour just inside the text area's left edge, spread over the text.
        $sample = $piece.GetPixel($r[0] + 2, $r[1] + [int]($r[3] / 2))
        $g = [System.Drawing.Graphics]::FromImage($piece)
        $brush = New-Object System.Drawing.SolidBrush $sample
        $g.FillRectangle($brush, $r[0], $r[1], $r[2], $r[3]); $g.Dispose(); $brush.Dispose()
    }
    $piece.Save((Join-Path $out "$($p[0]).png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $piece.Dispose(); $src.Dispose()
}
Write-Host "UI PREPARED: $($pieces.Count) pieces and $($backgrounds.Count) backgrounds in $out"
