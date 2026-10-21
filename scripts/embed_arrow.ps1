Add-Type -AssemblyName System.Drawing
$srcPath = Join-Path $PSScriptRoot "..\317-arrow-right2.png"
$outPath = Join-Path $PSScriptRoot "..\src\gui\newmenu\assets\arrow_right_icon.inl"
$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $srcPath))
$dst = New-Object System.Drawing.Bitmap 16, 16
$g = [System.Drawing.Graphics]::FromImage($dst)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.DrawImage($src, 0, 0, 16, 16)
$g.Dispose()
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("static constexpr int kArrowIconW = 16;")
[void]$sb.AppendLine("static constexpr int kArrowIconH = 16;")
[void]$sb.AppendLine("static constexpr unsigned char kArrowIconRgba[] = {")
$line = New-Object System.Collections.Generic.List[string]
for ($y = 0; $y -lt 16; $y++) {
  for ($x = 0; $x -lt 16; $x++) {
    $c = $dst.GetPixel($x, $y)
    [void]$line.Add([string][int]$c.R)
    [void]$line.Add([string][int]$c.G)
    [void]$line.Add([string][int]$c.B)
    [void]$line.Add([string][int]$c.A)
    if ($line.Count -ge 16) {
      [void]$sb.AppendLine(($line -join ",") + ",")
      $line.Clear()
    }
  }
}
if ($line.Count -gt 0) {
  [void]$sb.AppendLine(($line -join ",") + ",")
}
[void]$sb.AppendLine("};")
$src.Dispose()
$dst.Dispose()
[System.IO.File]::WriteAllText($outPath, $sb.ToString())
Write-Output "wrote $outPath"
