Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap 256, 256
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::FromArgb(18, 19, 26))
$body = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(45, 48, 68))
$g.FillRectangle($body, 48, 88, 160, 88)
$g.FillRectangle($body, 24, 108, 36, 52)
$g.FillRectangle($body, 196, 108, 36, 52)
$accent = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(99, 102, 241))
$g.FillRectangle($accent, 78, 118, 14, 44)
$g.FillRectangle($accent, 68, 132, 34, 14)
$g.FillEllipse($accent, 147, 111, 22, 22)
$g.FillEllipse($accent, 171, 131, 22, 22)
$g.FillEllipse($accent, 147, 151, 22, 22)
$g.FillEllipse($accent, 123, 131, 22, 22)
$g.Dispose()
$body.Dispose()
$accent.Dispose()
$out = Join-Path $PSScriptRoot "..\resources\img\icon.jpg"
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Jpeg)
$bmp.Dispose()
Write-Host "wrote $out"
