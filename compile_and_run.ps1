#!/usr/bin/env pwsh
<#
.SYNOPSIS
  Configure, build, optionally install and then run the built executable.

.DESCRIPTION
  This script is a PowerShell replacement for `compile_and_run.bat`.
  It configures and builds the project using CMake (Release), then locates the built
  executable and runs it, forwarding any additional args passed to this script.

.USAGE
  ./compile_and_run.ps1 -- -DULTRALIGHT_SDK_ROOT=/path/to/sdk "--app-flag"
#>

[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$RemainingArgs = @()
)

# Support a simple --detach flag that makes the script launch the executable and exit immediately
# If called with no args, default to detached mode (good for Explorer double-clicks). If args are present,
# default to not detach (so you get inline logs when invoking from the terminal).
if ($RemainingArgs) { $RemainingArgs = $RemainingArgs | ForEach-Object { $_.Trim('"') } }
$Detach = ($RemainingArgs.Count -eq 0)
if ($RemainingArgs -and ($RemainingArgs -contains '--detach')) {
    $Detach = $true
    $RemainingArgs = $RemainingArgs | Where-Object { $_ -ne '--detach' }
}

# If the script was invoked with a single value that looks like a PowerShell object description (eg: "System.String")
# when called from CMD via an association, treat it as no args (defensive behavior).
if ($RemainingArgs -and ($RemainingArgs.Count -eq 1) -and ($RemainingArgs[0] -eq 'System.String')) {
    $RemainingArgs = @()
}

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Write-Host "Starting compile_and_run.ps1: PowerShell $($PSVersionTable.PSVersion) Host: $($Host.Name) PID: $($PID)" -ForegroundColor Magenta
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

$cmakeArgs = @('-S', '.', '-B', 'build')
function Normalize-RemainingArgs([string[]]$argsList) {
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
} else {
    $normArgs = @()
}
    return $normArgs
}
$normArgs = Normalize-RemainingArgs $RemainingArgs
if ($normArgs) { $cmakeArgs += $normArgs }

    # If ULTRALIGHT_SDK_ROOT isn't set via a -D flag, but an environment variable exists, use it by default
    if (-not ($normArgs -match '^-DULTRALIGHT_SDK_ROOT=' ) -and ($env:ULTRALIGHT_SDK_ROOT)) {
           $cmakeArgs += ('-DULTRALIGHT_SDK_ROOT=' + $env:ULTRIGHT_SDK_ROOT)
        Write-Host "Using ULTRALIGHT_SDK_ROOT from environment: $env:ULTRALIGHT_SDK_ROOT" -ForegroundColor Yellow
           $normArgs += ('-DULTRALIGHT_SDK_ROOT=' + $env:ULTRIGHT_SDK_ROOT)
    }
# Configure
$code = Invoke-Tool 'cmake' $cmakeArgs
Write-Host "CMake configure returned code: $code" -ForegroundColor Yellow
if ($code -ne 0) { exit $code }

Write-Host "Starting build step..." -ForegroundColor Yellow
$code = Invoke-Tool 'cmake' @('--build', 'build', '--config', 'Release')
Write-Host "CMake build returned code: $code" -ForegroundColor Yellow
if ($code -ne 0) { exit $code }

# Optional install (best-effort)
try {
    Write-Host "Attempting install step (may be optional)..." -ForegroundColor Yellow
    $installCode = Invoke-Tool 'cmake' @('--install', 'build', '--config', 'Release')
    Write-Host "CMake install returned code: $installCode" -ForegroundColor Yellow
} catch {
    Write-Warning "cmake --install threw an exception: $_"
}

# Find the executable
function Find-Executable {
    param([string]$BasePath)
    # Search recursively for 'Ultralight-WebBrowser' executable (platform-agnostic)
    # Prefer an executable file (.exe on Windows, or an executable file without extension on Unix)
    $search = Get-ChildItem -Path $BasePath -Recurse -File -ErrorAction SilentlyContinue | Where-Object { ($_.Extension -ieq '.exe' -and $_.BaseName -eq 'Ultralight-WebBrowser') -or ($_.BaseName -eq 'Ultralight-WebBrowser' -and $_.Extension -eq '') }
    if ($search) { return $search[0].FullName }
    # Try common Release folder fallback
    $releaseFolder = Join-Path $BasePath 'Release'
    $win = Join-Path (Join-Path $BasePath 'Release') 'Ultralight-WebBrowser.exe'
    if (Test-Path $win) { return $win }
    $other = Join-Path $BasePath 'Ultralight-WebBrowser'
    if (Test-Path $other) { return $other }
    return $null
}

Write-Host "Searching for executable under: $buildPath" -ForegroundColor Cyan
$exe = Find-Executable -BasePath $buildPath
if (-not $exe) {
    Write-Error "Could not locate Ultralight-WebBrowser executable under $buildPath. Listing build directory for debugging..."
    try {
        Get-ChildItem -Path $buildPath -Recurse -Force -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_.FullName }
    } catch {
        Write-Warning "Unable to list build directory: $_"
    }
    exit 3
}

Write-Host "Found executable: $exe" -ForegroundColor Green
Write-Host "Executable directory: $([IO.Path]::GetDirectoryName($exe))" -ForegroundColor Yellow

# Set working directory to exe's folder (same as original batch file behavior)
Push-Location $([IO.Path]::GetDirectoryName($exe))
try {
    Write-Host "Running executable with args: $($normArgs -join ' ')" -ForegroundColor Cyan
    if ($Detach) {
        Write-Host "Launching in background (detached)." -ForegroundColor Yellow
        if ($normArgs -and $normArgs.Count -gt 0) {
            $proc = Start-Process -FilePath $exe -ArgumentList $normArgs -PassThru -ErrorAction Stop
        } else {
            $proc = Start-Process -FilePath $exe -PassThru -ErrorAction Stop
        }
        Write-Host "Launched PID: $($proc.Id)" -ForegroundColor Yellow
        $ret = 0
    } else {
        # Run via Invoke-Tool so stdout/stderr are captured consistently
        $ret = Invoke-Tool $exe $normArgs
    }
    Write-Host "Executable exited with code: $ret" -ForegroundColor Cyan
} finally {
    Pop-Location
}

exit $ret
