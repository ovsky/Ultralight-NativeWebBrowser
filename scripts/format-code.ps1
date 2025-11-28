#!/usr/bin/env pwsh
<#
  Simple formatting script that runs clang-format over the C++ sources.
  It does not install clang-format; ensure it is present in PATH or install
  it via your preferred package manager (choco/scoop on Windows, apt/brew on Linux/macOS).
#>

param(
  [switch] $CheckOnly,
  [switch] $AutoInstall
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
  if ($IsWindows -and $AutoInstall) {
    $chocoCmd = Get-Command choco -ErrorAction SilentlyContinue
    if ($chocoCmd) {
      Write-Host "Attempting to install clang-format via Chocolatey..."
      try { & choco install -y llvm } catch { Write-Warning "choco install failed: $_" }
      # Re-check
      $cmd = Get-Command clang-format -ErrorAction SilentlyContinue
      if ($cmd) { $clangFormatCmd = $cmd.Source }
    }
    if (-not $clangFormatCmd) {
      # Try downloading a portable clang-format release to ./tools/clang-format.
      # Try multiple versions and filename patterns to increase success rate.
      $versions = @('18.0.0','17.0.6','16.0.6','15.0.7','14.0.6')
      $patterns = @(
        'clang+llvm-{version}-win64.zip',
        'clang+llvm-{version}-x86_64-windows-msvc.zip',
        'clang+llvm-{version}-x86_64-windows.zip',
        'clang+llvm-{version}-windows-msvc.zip'
      )
      $toolsDir = Join-Path $root 'tools/clang-format'
      if (-not (Test-Path $toolsDir)) { New-Item -ItemType Directory -Force -Path $toolsDir | Out-Null }
      $zipPath = Join-Path $env:TEMP $zipName
      Write-Host "Downloading clang-format portable binary from $url to $zipPath"
      $downloaded = $false
      foreach ($ver in $versions) {
        foreach ($pat in $patterns) {
          $zipName = $pat -replace '\{version\}',$ver
          $url = "https://github.com/llvm/llvm-project/releases/download/llvmorg-$ver/$zipName"
          Write-Host "Trying URL: $url"
          try {
            Invoke-WebRequest -Uri $url -UseBasicParsing -OutFile $zipPath -ErrorAction Stop
            Expand-Archive -LiteralPath $zipPath -DestinationPath $toolsDir -Force
            Remove-Item $zipPath -ErrorAction SilentlyContinue
            $found = Get-ChildItem -Path $toolsDir -Recurse -Filter clang-format.exe -ErrorAction SilentlyContinue |
              Select-Object -First 1
            if ($found) { $clangFormatCmd = $found.FullName; $downloaded = $true; break }
          } catch {
            # ignore and try next candidate
            Write-Verbose "Candidate $url failed: $_"
          }
        }
        if ($downloaded) { break }
      }
      if (-not $downloaded) {
        Write-Warning "All download attempts failed for clang-format portable binaries."
      }
      # Find clang-format.exe under extracted folder (if not already set)
      if (-not $clangFormatCmd) {
        $found = Get-ChildItem -Path $toolsDir -Recurse -Filter clang-format.exe -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) { $clangFormatCmd = $found.FullName }
      }
    }
  }
  if (-not $clangFormatCmd) {
    Write-Error "clang-format not found. Please install clang-format and ensure it is on PATH. For Windows try 'choco install llvm' or use -AutoInstall to auto-download a portable binary; for Ubuntu: 'sudo apt install clang-format'"; exit 2
  }
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
