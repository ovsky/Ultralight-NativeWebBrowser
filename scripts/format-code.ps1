#!/usr/bin/env pwsh
<#
  Simple formatting script that runs clang-format over the C++ sources.
  It does not install clang-format; ensure it is present in PATH or install
  it via your preferred package manager (choco/scoop on Windows, apt/brew on Linux/macOS).
#>

param(
  [switch] $CheckOnly
)

$root = Split-Path -Path $PSScriptRoot -Parent
$files = Get-ChildItem -Path (Join-Path $root 'src') -Recurse -Include *.cpp,*.h | Select-Object -ExpandProperty FullName
$files += Get-ChildItem -Path (Join-Path $root 'tests') -Recurse -Include *.cpp,*.h | Select-Object -ExpandProperty FullName
foreach ($f in $files) {
  Write-Host "Formatting: $f"
  if ($CheckOnly) {
    & clang-format -n "$f" 2>$null
    if ($LASTEXITCODE -ne 0) { Write-Host "Mismatch: $f"; exit 1 }
  } else {
    & clang-format -i "$f"
  }
}

Write-Host "Formatting complete"
