# Generate server/web_resources.h: embed web/ assets as C++ raw string literals
# so that chess_server.exe works as a single standalone exe.
# Usage: powershell -ExecutionPolicy Bypass -File tools\gen_web_res.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot   # project root (parent of tools)

function ReadUtf8($p) { [IO.File]::ReadAllText($p) }
function RawWrap($sep, $content) { "R`"$sep($content)$sep`";" }

$html = ReadUtf8 (Join-Path $root "web\index.html")
$css  = ReadUtf8 (Join-Path $root "web\style.css")
$js   = ReadUtf8 (Join-Path $root "web\app.js")

# content must not contain the raw-string terminator sequence, e.g. )html"
if ($html -match '\)html"' -or $css -match '\)css"' -or $js -match '\)js"') {
    throw "web asset contains raw string terminator sequence; use another delimiter"
}

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("// Auto-generated embedded web resources (by tools/gen_web_res.ps1).")
[void]$sb.AppendLine("// Do not edit by hand. Re-run build.bat after changing files under web/.")
[void]$sb.AppendLine("#pragma once")
[void]$sb.AppendLine("")
[void]$sb.AppendLine('static const char INDEX_HTML[] = ' + (RawWrap "html" $html))
[void]$sb.AppendLine("")
[void]$sb.AppendLine('static const char STYLE_CSS[] = ' + (RawWrap "css" $css))
[void]$sb.AppendLine("")
[void]$sb.AppendLine('static const char APP_JS[] = ' + (RawWrap "js" $js))

$out = Join-Path $root "server\web_resources.h"
$enc = New-Object System.Text.UTF8Encoding($false)   # UTF-8 without BOM
[IO.File]::WriteAllText($out, $sb.ToString(), $enc)
Write-Host ("[gen] server/web_resources.h OK  " + (Get-Item $out).Length + " bytes")
