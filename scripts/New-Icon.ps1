$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$target = Join-Path $PSScriptRoot '..\resources\app.ico'
$images = @()
foreach ($size in @(16, 24, 32, 48, 64, 128, 256)) {
    $bitmap = [System.Drawing.Bitmap]::new($size, $size)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.ScaleTransform($size / 256.0, $size / 256.0)
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $path.AddArc(0, 0, 96, 96, 180, 90)
    $path.AddArc(160, 0, 96, 96, 270, 90)
    $path.AddArc(160, 160, 96, 96, 0, 90)
    $path.AddArc(0, 160, 96, 96, 90, 90)
    $path.CloseFigure()
    $brush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(23, 26, 31))
    $graphics.FillPath($brush, $path)
    $pen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(171, 201, 248), 16)
    $pen.StartCap = $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $heights = @(38, 84, 132, 84, 38)
    for ($i = 0; $i -lt 5; $i++) { $graphics.DrawLine($pen, [single](56 + $i * 36), [single](128 - $heights[$i] / 2), [single](56 + $i * 36), [single](128 + $heights[$i] / 2)) }
    $stream = [System.IO.MemoryStream]::new()
    $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    $images += ,@($size, $stream.ToArray())
    $stream.Dispose(); $pen.Dispose(); $brush.Dispose(); $path.Dispose(); $graphics.Dispose(); $bitmap.Dispose()
}
$file = [System.IO.File]::Create($target)
$writer = [System.IO.BinaryWriter]::new($file)
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$images.Count)
$offset = 6 + 16 * $images.Count
foreach ($entry in $images) {
    $dimension = if ($entry[0] -eq 256) { 0 } else { $entry[0] }
    $writer.Write([byte]$dimension); $writer.Write([byte]$dimension)
    $writer.Write([byte]0); $writer.Write([byte]0); $writer.Write([uint16]1); $writer.Write([uint16]32)
    $writer.Write([uint32]$entry[1].Length); $writer.Write([uint32]$offset)
    $offset += $entry[1].Length
}
foreach ($entry in $images) { $writer.Write([byte[]]$entry[1]) }
$writer.Dispose(); $file.Dispose()
Write-Host 'Generated resources/app.ico'
