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
$files = @()
try { $files += Get-ChildItem -Path (Join-Path $root 'src') -Recurse -Include *.cpp,*.h -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName } catch {}
try { $files += Get-ChildItem -Path (Join-Path $root 'tests') -Recurse -Include *.cpp,*.h -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName } catch {}

# Find clang-format executable; prefer PATH, then common installed locations
$clangFormatCmd = $null
$cmd = Get-Command clang-format -ErrorAction SilentlyContinue
if ($cmd) { $clangFormatCmd = $cmd.Source }
if (-not $clangFormatCmd) {
  if ($IsWindows) {
    $candidates = @(
      'C:\\Program Files\\LLVM\\bin\\clang-format.exe',
      'C:\\Program Files (x86)\\LLVM\\bin\\clang-format.exe',
      'C:\\ProgramData\\chocolatey\\bin\\clang-format.exe',
      'C:\\ProgramData\\chocolatey\\lib\\llvm\\tools\\llvm\\bin\\clang-format.exe'
    )
  } else {
    $candidates = @('/usr/bin/clang-format', '/usr/local/bin/clang-format')
  }
  foreach ($c in $candidates) { if (Test-Path $c) { $clangFormatCmd = $c; break } }
}
if (-not $clangFormatCmd) {
  Write-Error "clang-format not found. Please install clang-format and ensure it is on PATH. For Windows: 'choco install llvm' or 'choco install clang-format'; for Ubuntu: 'sudo apt install clang-format'"; exit 2
}
foreach ($f in $files) {
  Write-Host "Formatting: $f"
  if ($CheckOnly) {
    # Use output-replacements-xml to detect any formatting changes
    $xml = & $clangFormatCmd -style=file -output-replacements-xml "$f" 2>$null
    if ($xml -match '<replacement ') { Write-Host "Mismatch: $f"; exit 1 }
  } else {
    & $clangFormatCmd -style=file -i "$f"
  }
}

Write-Host "Formatting complete"
