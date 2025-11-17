$file = '.github/workflows/build-windows.yml'
$lines = Get-Content $file
$idx = ($lines | Select-String -Pattern '8.0b Debug: locate/move installer' -SimpleMatch).LineNumber
for ($i = $idx; $i -lt $lines.Length; $i++) { if ($lines[$i] -match '^\s*run:\s*\|') { $start = $i; break } }
$stack = @()
for ($i = $start + 1; $i -le $start + 400; $i++) {
    if ($i -ge $lines.Length) { break }
    $line = $lines[$i]
    # stop at another step
    if ($line -match '^\s*-\sname:') { break }
    for ($j = 0; $j -lt $line.Length; $j++) {
        $c = $line[$j]
        if ($c -eq '{') { $stack += @($i, $j) }
        if ($c -eq '}') {
            if ($stack.Count -gt 0) { $stack = $stack[0..($stack.Count - 3)] } else {
                Write-Host ("Unmatched closing brace at line {0}: {1}" -f $i, $line)
                $context = ($lines[($i - 4)..($i + 2)] -join "`n")
                Write-Host "Context:`n$context"
                exit 0
            }
        }
    }
}
Write-Host "All braces balanced in block"