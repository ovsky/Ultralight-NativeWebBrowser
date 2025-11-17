$file = '.github/workflows/build-windows.yml'
$lines = Get-Content $file
$startLine = ($lines | Select-String -Pattern '8.0b Debug: locate/move installer' -SimpleMatch).LineNumber
if (-not $startLine) { write-host 'not found'; exit }
# find the run: | block start
$runStart = ($lines[$startLine..($startLine + 50)] | Select-String -Pattern "run: \|" -SimpleMatch).LineNumber
if (-not $runStart) { write-host 'no run'; exit }
$runStart = $startLine + $runStart - 1
# get block until next unindented line or blank line and actions steps
$block = @()
for ($i = $runStart + 1; $i -lt $lines.Length; $i++) {
    $l = $lines[$i]
    if ($l -match '^\s*-[\w]') { break }
    $block += $l
}
Write-Host "Block lines =" $block.Count
$txt = $block -join "`n"
[regex]::Matches($txt, '\{').Count | Write-Host 'open'
[regex]::Matches($txt, '\}').Count | Write-Host 'close'
Write-Host $txt
