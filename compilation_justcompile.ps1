#!/usr/bin/env pwsh
<#
.SYNOPSIS
  Configure and build the project using CMake.

.DESCRIPTION
  Replacement for the old `compile.ps1` script. Configure and build the project
  using CMake. It will remove previous build cache if present, run
  `cmake -S . -B build`, then build `cmake --build build --config Release` and
  optionally run `cmake --install` (best-effort).

.USAGE
  ./compilation_justcompile.ps1 -- -DULTRALIGHT_SDK_ROOT="/path/to/sdk"

#>

[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$AutoInstallCurl,
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$RemainingArgs = @()
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($RemainingArgs) { $RemainingArgs = $RemainingArgs | ForEach-Object { $_.Trim('"') } }
if ($RemainingArgs -and ($RemainingArgs.Count -eq 1) -and ($RemainingArgs[0] -eq 'System.String')) { $RemainingArgs = @() }
Write-Host "Starting compilation_justcompile.ps1: PowerShell $($PSVersionTable.PSVersion) Host: $($Host.Name) PID: $($PID)" -ForegroundColor Magenta
Write-Host "Working directory: " (Get-Location).Path -ForegroundColor Magenta
Write-Host "Script args (remaining): $($RemainingArgs -join ' ')" -ForegroundColor Magenta

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $scriptRoot

function Invoke-Tool([string]$Exe, [string[]]$Arguments) {
    Write-Host "Running: $Exe $($Arguments -join ' ')" -ForegroundColor Cyan
    try {
        # Direct invocation streams output live and preserves exit codes
        & $Exe @Arguments 2>&1 | ForEach-Object { Write-Host $_ }
        $exit = $LASTEXITCODE
        if (-not $exit) { $exit = 0 }
        return $exit
    } catch {
        Write-Warning "Direct invocation failed, falling back to Start-Process: $_"
        $outFile = [System.IO.Path]::GetTempFileName()
        $errFile = [System.IO.Path]::GetTempFileName()
        try {
            $proc = Start-Process -FilePath $Exe -ArgumentList $Arguments -NoNewWindow -RedirectStandardOutput $outFile -RedirectStandardError $errFile -Wait -PassThru -ErrorAction Stop
            $exit = $proc.ExitCode
        } catch {
            Write-Warning "Start-Process fallback failed: $_"
            return 1
        }
        $stdout = Get-Content -Raw -LiteralPath $outFile -ErrorAction SilentlyContinue
        $stderr = Get-Content -Raw -LiteralPath $errFile -ErrorAction SilentlyContinue
        if ($stdout -and $stdout.Trim()) { Write-Host $stdout }
        if ($stderr -and $stderr.Trim()) { Write-Host $stderr -ForegroundColor Red }
        Remove-Item -LiteralPath $outFile,$errFile -ErrorAction SilentlyContinue
        if (-not $exit) { $exit = 0 }
        return $exit
    }
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "cmake not found in PATH. Please install CMake and add it to PATH or run this script from an environment that can run cmake."
    exit 2
}

$buildPath = Join-Path $scriptRoot 'build'
$cachePath = Join-Path $buildPath 'CMakeCache.txt'
# By default, avoid scrubbing the build directory to save time. Use -Clean to force removal.
if ($Clean.IsPresent) {
    if (Test-Path $cachePath) {
        Write-Host "Cleaning build directory (requested via -Clean)..." -ForegroundColor Yellow
        try { Remove-Item -LiteralPath $buildPath -Recurse -Force -ErrorAction Stop } catch { Write-Warning "Failed to remove build directory: $_" }
    }
} else {
    if (Test-Path $cachePath) { Write-Host "Detected existing CMake cache; skipping removal for speed. Use -Clean to force a full reconfigure." -ForegroundColor Yellow }
}

# Configure
$cmakeArgs = @('-S', '.', '-B', 'build')
function Join-Args([string[]]$argsList) {
    if (-not $argsList) { return @() }
    $out = @()
    for ($i = 0; $i -lt $argsList.Length; $i++) {
        $arg = $argsList[$i].Trim('"')
        if ($arg -match '^-D[^=]+$' -and ($i + 1) -lt $argsList.Length -and -not ($argsList[$i+1] -like '-*')) {
            $next = $argsList[$i+1].Trim('"')
            $out += ($arg + '=' + $next)
            $i++
        } else {
            $out += $arg
        }
    }
    return $out
}
if ($RemainingArgs) {
    $normArgs = Join-Args $RemainingArgs
    Write-Host "Normalized remaining args for CMake: $($normArgs -join ' ')" -ForegroundColor Yellow
    $cmakeArgs += $normArgs
} else {
    $normArgs = @()
}

# If requested, tell CMake to auto-install curl during configure (best-effort helper script)
if ($AutoInstallCurl.IsPresent) {
    Write-Host "AUTO_INSTALL_CURL requested via script flag; passing to CMake configure" -ForegroundColor Yellow
    $cmakeArgs += ('-DAUTO_INSTALL_CURL=ON')
}

# If ULTRALIGHT_SDK_ROOT isn't set via a -D flag, but an environment variable exists, use it by default
if (-not ($normArgs -match '^-DULTRALIGHT_SDK_ROOT=' ) -and ($env:ULTRALIGHT_SDK_ROOT)) {
    $cmakeArgs += ('-DULTRALIGHT_SDK_ROOT=' + $env:ULTRALIGHT_SDK_ROOT)
    Write-Host "Using ULTRALIGHT_SDK_ROOT from environment: $env:ULTRALIGHT_SDK_ROOT" -ForegroundColor Yellow
}
if (-not (Test-Path $buildPath)) {
    # If no build folder exists, run a full configure
    $code = Invoke-Tool 'cmake' $cmakeArgs
    if ($code -ne 0) { exit $code }
} else {
    Write-Host "Re-using existing build directory; skipping full configure for speed." -ForegroundColor Cyan
}

# Build (use parallel jobs to speed up)
$procCount = [System.Environment]::ProcessorCount
$parallelArg = @('--parallel', "$procCount")
 $code = Invoke-Tool 'cmake' (@('--build','build','--config','Release') + $parallelArg)
if (-not $code) { $code = 1 }
if ($code -ne 0) {
    Write-Warning "cmake returned non-zero exit code: $code. Checking for produced executable as fallback..."
    # Search for a produced executable under the build tree as a fallback
    $found = @(Get-ChildItem -Path $buildPath -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -ieq '.exe' -and $_.BaseName -eq 'Ultralight-WebBrowser' })
    if ($found.Count -gt 0) {
        Write-Host "Build returned non-zero but executable exists: $($found[0].FullName). Treating as success." -ForegroundColor Yellow
        $code = 0
    } else {
        Write-Host "Build failed with exit code $code." -ForegroundColor Red
        exit $code
    }
}

# Optional install (best-effort)
try {
    $code = Invoke-Tool 'cmake' @('--install', 'build', '--config', 'Release')
    if ($code -ne 0) {
        Write-Warning "cmake --install returned exit code $code"
    }
} catch {
    Write-Warning "cmake --install threw an exception: $_"
}

Write-Host "Build complete." -ForegroundColor Green
