# Prepares talkbox portraits for the game: cuts away the baked-in checkerboard "transparency" (by flood-filling
# the pale, colourless background from the image border up to the character's ink outline), and names each file
# by its mood. Originals in "Talkbox Images" are never changed; results go to "Talkbox Images\Processed".
#   Talkbox Images\HellGirl\<Outfit>\*.png  ->  Processed\<Outfit>\<Mood>.png   (e.g. Rags\Headache.png)
#   Talkbox Images\<Character>\*.png        ->  Processed\<Character>\<Mood>.png (e.g. GoblinQueen\Angry.png)
# Then run import_portraits.ps1 to bring them into Unreal.
param([string]$Source = '')
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not $Source) { $Source = Join-Path (Split-Path $project -Parent) 'Talkbox Images' }
$out = Join-Path $Source 'Processed'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
public static class PortraitCutout
{
    // Background: bright and nearly colourless (the white and light-grey checker squares).
    static bool IsBackground(byte r, byte g, byte b)
    {
        int max = Math.Max(r, Math.Max(g, b)), min = Math.Min(r, Math.Min(g, b));
        return min > 180 && max - min < 16;
    }
    public static int Cut(string input, string output)
    {
        using (var source = new Bitmap(input))
        using (var image = new Bitmap(source.Width, source.Height, PixelFormat.Format32bppArgb))
        {
            using (var g = Graphics.FromImage(image)) g.DrawImage(source, 0, 0, source.Width, source.Height);
            int w = image.Width, h = image.Height;
            var data = image.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
            var px = new byte[data.Stride * h];
            System.Runtime.InteropServices.Marshal.Copy(data.Scan0, px, 0, px.Length);
            // Images that already have real transparency are kept as they are.
            bool hasAlpha = false;
            for (int i = 3; i < px.Length && !hasAlpha; i += 4 * 97) hasAlpha = px[i] < 250;
            int removed = 0;
            if (!hasAlpha)
            {
                var bg = new bool[w * h];
                var queue = new Queue<int>();
                Action<int, int> Seed = (x, y) =>
                {
                    int o = y * data.Stride + x * 4, k = y * w + x;
                    if (!bg[k] && IsBackground(px[o + 2], px[o + 1], px[o])) { bg[k] = true; queue.Enqueue(k); }
                };
                for (int x = 0; x < w; x++) { Seed(x, 0); Seed(x, h - 1); }
                for (int y = 0; y < h; y++) { Seed(0, y); Seed(w - 1, y); }
                while (queue.Count > 0)
                {
                    int k = queue.Dequeue(), x = k % w, y = k / w;
                    if (x > 0) Seed(x - 1, y);
                    if (x < w - 1) Seed(x + 1, y);
                    if (y > 0) Seed(x, y - 1);
                    if (y < h - 1) Seed(x, y + 1);
                }
                // Soften the cut edge: a pixel next to the background gets partial alpha.
                for (int y = 0; y < h; y++)
                    for (int x = 0; x < w; x++)
                    {
                        int k = y * w + x, o = y * data.Stride + x * 4;
                        if (bg[k]) { px[o + 3] = 0; removed++; continue; }
                        int near = 0;
                        for (int dy = -1; dy <= 1; dy++)
                            for (int dx = -1; dx <= 1; dx++)
                            {
                                int nx = x + dx, ny = y + dy;
                                if (nx >= 0 && ny >= 0 && nx < w && ny < h && bg[ny * w + nx]) near++;
                            }
                        if (near > 0) px[o + 3] = (byte)(255 - near * 22);
                    }
            }
            System.Runtime.InteropServices.Marshal.Copy(px, 0, data.Scan0, px.Length);
            image.UnlockBits(data);
            image.Save(output, ImageFormat.Png);
            return hasAlpha ? -1 : removed * 100 / (w * h);
        }
    }
}
'@

function Mood([string]$name)
{
    $n = $name.ToLower()
    foreach ($pair in @(('headache','Headache'),('sleep','Headache'),('hurt-angry','HurtAngry'),('hurt-determined','HurtDetermined'),
        ('hurt-neutral','HurtNeutral'),('hurt','Hurt'),('evil-smirk','EvilSmirk'),('smirk','Smirk'),('laugh','Laugh'),('angry','Angry'),
        ('surprised','Surprised'),('quiet','Quiet'),('neutral','Neutral'))) { if ($n.Contains($pair[0])) { return $pair[1] } }
    return $null
}

$sets = @()
foreach ($dir in Get-ChildItem $Source -Directory | Where-Object Name -ne 'Processed')
{
    if ($dir.Name -eq 'HellGirl') { $sets += Get-ChildItem $dir.FullName -Directory | ForEach-Object { ,@(($_.Name -replace ' ', ''), $_.FullName) } }
    else { $sets += ,@(($dir.Name -replace ' ', ''), $dir.FullName) }
}
foreach ($set in $sets)
{
    $target = Join-Path $out $set[0]
    New-Item -ItemType Directory -Force $target | Out-Null
    foreach ($file in Get-ChildItem $set[1] -Filter *.png)
    {
        $mood = Mood $file.BaseName
        if (-not $mood) { Write-Host "  skipped $($file.Name): no mood in its name"; continue }
        $cut = [PortraitCutout]::Cut($file.FullName, (Join-Path $target "$mood.png"))
        Write-Host ("  {0}/{1}: {2}" -f $set[0], $mood, $(if ($cut -lt 0) { 'kept its own transparency' } else { "$cut% background removed" }))
    }
}
Write-Host "PORTRAITS PREPARED in $out"
