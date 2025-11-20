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
    $outFile = [System.IO.Path]::GetTempFileName()
    $errFile = [System.IO.Path]::GetTempFileName()
    try {
        $proc = Start-Process -FilePath $Exe -ArgumentList $Arguments -NoNewWindow -RedirectStandardOutput $outFile -RedirectStandardError $errFile -Wait -PassThru -ErrorAction Stop
        $exit = $proc.ExitCode
    } catch {
        Write-Warning "Start-Process failed or behaved unexpectedly: $_"
        $orig = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            $output = & $Exe @Arguments 2>&1 | Out-String
            if ($output -and $output.Trim()) { Write-Host $output }
            $exit = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $orig
        }
        return $exit
    }

    $stdout = Get-Content -Raw -LiteralPath $outFile -ErrorAction SilentlyContinue
    $stderr = Get-Content -Raw -LiteralPath $errFile -ErrorAction SilentlyContinue
    if ($stdout -and $stdout.Trim()) { Write-Host $stdout }
    if ($stderr -and $stderr.Trim()) { Write-Host $stderr }
    Remove-Item -LiteralPath $outFile,$errFile -ErrorAction SilentlyContinue
    return $exit
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "cmake not found in PATH. Please install CMake and add it to PATH or run this script from an environment that can run cmake."
    exit 2
}

$buildPath = Join-Path $scriptRoot 'build'
$cachePath = Join-Path $buildPath 'CMakeCache.txt'
if (Test-Path $cachePath) {
    Write-Host "Removing previous CMake cache/build to avoid generator mismatch..." -ForegroundColor Yellow
    try {
        Remove-Item -LiteralPath $buildPath -Recurse -Force -ErrorAction Stop
    } catch {
        Write-Warning "Failed to remove build directory: $_"
    }
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

# If ULTRALIGHT_SDK_ROOT isn't set via a -D flag, but an environment variable exists, use it by default
if (-not ($normArgs -match '^-DULTRALIGHT_SDK_ROOT=' ) -and ($env:ULTRALIGHT_SDK_ROOT)) {
    $cmakeArgs += ('-DULTRALIGHT_SDK_ROOT=' + $env:ULTRALIGHT_SDK_ROOT)
    Write-Host "Using ULTRALIGHT_SDK_ROOT from environment: $env:ULTRALIGHT_SDK_ROOT" -ForegroundColor Yellow
}
$code = Invoke-Tool 'cmake' $cmakeArgs
if ($code -ne 0) { exit $code }

# Build
$code = Invoke-Tool 'cmake' @('--build', 'build', '--config', 'Release')
if ($code -ne 0) { exit $code }

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
