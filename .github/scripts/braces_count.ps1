$file = '.github/workflows/build-windows.yml'
$lines = Get-Content $file
# find '8.0b Debug' step
$idx = ($lines | Select-String -Pattern '8.0b Debug: locate/move installer' -SimpleMatch).LineNumber
if (-not $idx) { Write-Host 'no step' ; exit 1 }
# find start of run: | block
for ($i = $idx; $i -lt $lines.Length; $i++) { if ($lines[$i] -match '^\s*run:\s*\|') { $runStart = $i; break } }
$block = @()
for ($i = $runStart + 1; $i -lt $lines.Length; $i++) {
    $l = $lines[$i]
    # stop at the next step line starting with two spaces then '-' but not ' - ' inside script
    if ($l -match '^\s{0,2}-\sname:') { break }
    $block += $l
}
$txt = $block -join "`n"
$open = [regex]::Matches($txt, '\{').Count
$close = [regex]::Matches($txt, '\}').Count
Write-Host "Open braces: $open. Close braces: $close"
Write-Host "Block lines:"
$block | Select-Object -First 200 | ForEach-Object { $line = $_.Replace("`t", "    "); Write-Host $line }
