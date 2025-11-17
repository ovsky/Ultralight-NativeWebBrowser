# Parse NSIS/CPack logs and print a helpful summary
param(
    [string]$BuildDir = "build",
    [int]$HeadLines = 200,
    [int]$TailLines = 200
)
function Print-HeadTail($path, $nHead, $nTail) {
    Write-Host "==== HEAD($path) ===="
    Get-Content -Path $path -ErrorAction SilentlyContinue | Select-Object -First $nHead | ForEach-Object { Write-Host "  $_" }
    Write-Host "==== TAIL($path) ===="
    Get-Content -Path $path -ErrorAction SilentlyContinue | Select-Object -Last $nTail | ForEach-Object { Write-Host "  $_" }
}

$logs = @("$BuildDir\nsis-cpack.log", "$BuildDir\zip-cpack.log")
$summary = @()
foreach ($l in $logs) {
    if (Test-Path $l) {
        Print-HeadTail $l $HeadLines $TailLines
        $errs = Select-String -Path $l -Pattern '(?i)error|fatal|exception|rc2135|cannot find|not found' -AllMatches -ErrorAction SilentlyContinue
        if ($errs) {
            $summary += @{ File = $l; Matches = $errs | ForEach-Object { $_.Line } }
        }
    }
}
# NSISOutput.log
$nsisOutputs = Get-ChildItem -Path "$BuildDir\_CPack_Packages" -Recurse -Filter NSISOutput.log -ErrorAction SilentlyContinue
if ($nsisOutputs) {
    foreach ($nsf in $nsisOutputs) {
        Print-HeadTail $nsf.FullName $HeadLines $TailLines
        $errs = Select-String -Path $nsf.FullName -Pattern '(?i)error|fatal|exception|rc2135|cannot find|not found' -AllMatches -ErrorAction SilentlyContinue
        if ($errs) {
            $summary += @{ File = $nsf.FullName; Matches = $errs | ForEach-Object { $_.Line } }
        }
    }
}

Write-Host "==== Summary of potential errors found ===="
if ($summary.Count -eq 0) { Write-Host "No error-like lines found matching common patterns." } else {
    foreach ($s in $summary) {
        Write-Host "File: $($s.File)"
        $s.Matches | Select-Object -Unique | ForEach-Object { Write-Host "  - $_" }
    }
}

Write-Host "Note: presence of 'error' lines is not always fatal; check the logs above for context."

# Don't fail the build; this is a diagnostic tool.