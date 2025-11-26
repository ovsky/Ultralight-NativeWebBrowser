#!/usr/bin/env pwsh
<#
.SYNOPSIS
  Configure, build, optionally install and then run the built executable.

.DESCRIPTION
  Replacement for the old `compile_and_run.ps1` script. Configures and builds the
  project using CMake (Release), then locates the built executable and runs it,
  forwarding any additional args passed to this script.

.USAGE
  ./compilation_complete.ps1 -- -DULTRALIGHT_SDK_ROOT=/path/to/sdk "--app-flag"
#>

[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
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
Write-Host "Starting compilation_complete.ps1: PowerShell $($PSVersionTable.PSVersion) Host: $($Host.Name) PID: $($PID)" -ForegroundColor Magenta
Write-Host "Working directory: " (Get-Location).Path -ForegroundColor Magenta
Write-Host "Script args (remaining): $($RemainingArgs -join ' ')" -ForegroundColor Magenta

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $scriptRoot

function Invoke-Tool([string]$Exe, [string[]]$Arguments) {
    Write-Host "Running: $Exe $($Arguments -join ' ')" -ForegroundColor Cyan
    # Use the improved streaming runner to avoid long hangs and provide progress.
    function Write-Log([string]$Level, [string]$Message) {
        $ts = (Get-Date).ToString('HH:mm:ss')
        switch ($Level) {
            'INFO' { Write-Host "[$ts] $Message" -ForegroundColor Cyan }
            'WARN' { Write-Warning "[$ts] $Message" }
            'ERR'  { Write-Error "[$ts] $Message" }
            default { Write-Host "[$ts] $Message" }
        }
    }

    $outFile = [System.IO.Path]::GetTempFileName()
    $errFile = [System.IO.Path]::GetTempFileName()
    $proc = $null
    try {
        $proc = Start-Process -FilePath $Exe -ArgumentList $Arguments -NoNewWindow -RedirectStandardOutput $outFile -RedirectStandardError $errFile -PassThru -ErrorAction Stop
    }
    catch {
        Write-Log WARN "Start-Process failed to start: $_. Falling back to direct invocation."
        $orig = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            & $Exe @Arguments 2>&1 | ForEach-Object { Write-Log INFO $_ }
            return $LASTEXITCODE
        }
        finally { $ErrorActionPreference = $orig }
    }

    $lastSize = 0
    $lastOutputTime = Get-Date
    $start = Get-Date
    $TimeoutSeconds = 3600
    $StallTimeoutSeconds = 300
    while (-not $proc.HasExited) {
        Start-Sleep -Milliseconds 300
        try {
            $size = (Get-Item $outFile -ErrorAction SilentlyContinue).Length
            if ($null -ne $size -and $size -ne $lastSize) {
                $lastSize = $size
                $lastOutputTime = Get-Date
                Get-Content -LiteralPath $outFile -Tail 100 -ErrorAction SilentlyContinue | ForEach-Object { Write-Log INFO $_ }
            }
            else {
                if ((Get-Date) -gt $lastOutputTime.AddSeconds(60)) {
                    Get-Content -LiteralPath $outFile -Tail 20 -ErrorAction SilentlyContinue | ForEach-Object { Write-Log INFO $_ }
                }
            }
        }
        catch { }

        if ((Get-Date) -gt $start.AddSeconds($TimeoutSeconds)) {
            Write-Log ERR "Process timed out after $TimeoutSeconds seconds. Killing process..."
            try { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue } catch { }
            break
        }

        if ((Get-Date) -gt $lastOutputTime.AddSeconds($StallTimeoutSeconds)) {
            Write-Log WARN "No output for $StallTimeoutSeconds seconds. Killing process to avoid hang..."
            try { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue } catch { }
            break
        }
    }

    Start-Sleep -Milliseconds 200
    try {
        if (Test-Path $outFile) { Get-Content -LiteralPath $outFile -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_ } }
        if (Test-Path $errFile) { Get-Content -LiteralPath $errFile -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_ } }
    }
    catch { }

    $exit = 1
    try { $exit = $proc.ExitCode } catch { }
    Remove-Item -LiteralPath $outFile, $errFile -ErrorAction SilentlyContinue
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
    }
    catch {
        Write-Warning "Failed to remove build directory: $_"
    }
}

# Prepare configure args and normalize any remaining -D flags
$cmakeArgs = @('-S', '.', '-B', 'build')
function Normalize-RemainingArgs([string[]]$argsList) {
    function Join-Args([string[]]$argsList) {
        if (-not $argsList) { return @() }
        $out = @()
        for ($i = 0; $i -lt $argsList.Length; $i++) {
            $arg = $argsList[$i].Trim('"')
            if ($arg -match '^-D[^=]+$' -and ($i + 1) -lt $argsList.Length -and -not ($argsList[$i + 1] -like '-*')) {
                $next = $argsList[$i + 1].Trim('"')
                $out += ($arg + '=' + $next)
                $i++
            }
            else {
                $out += $arg
            }
        }
        return $out
    }

    if ($argsList) {
        $normArgs = Join-Args $argsList
        Write-Host "Normalized remaining args for CMake: $($normArgs -join ' ')" -ForegroundColor Yellow
    }
    else {
        $normArgs = @()
    }

    # If ULTRALIGHT_SDK_ROOT isn't set via a -D flag, but an environment variable exists, use it by default
    if (-not ($normArgs -match '^-DULTRALIGHT_SDK_ROOT=' ) -and ($env:ULTRALIGHT_SDK_ROOT)) {
        $normArgs += ('-DULTRALIGHT_SDK_ROOT=' + $env:ULTRALIGHT_SDK_ROOT)
        Write-Host "Using ULTRALIGHT_SDK_ROOT from environment: $env:ULTRALIGHT_SDK_ROOT" -ForegroundColor Yellow
    }

    return $normArgs
}

$normArgs = Normalize-RemainingArgs $RemainingArgs
if ($normArgs) { $cmakeArgs += $normArgs }
# Configure
$code = Invoke-Tool 'cmake' $cmakeArgs
Write-Host "CMake configure returned code: $code" -ForegroundColor Yellow
if ($code -ne 0) { exit $code }

Write-Host "Starting build step..." -ForegroundColor Yellow
# Build with parallelism by default; allow remaining args to override
$buildArgs = @('--build', 'build', '--config', 'Release')
$hasParallel = $false
foreach ($a in $RemainingArgs) { if ($a -match '^-j' -or $a -match '^/m' -or $a -eq '--') { $hasParallel = $true; break } }
$procs = [Environment]::ProcessorCount
if (-not $hasParallel) {
    if ($IsWindows) { $buildArgs += '--'; $buildArgs += ("/m:$procs") }
    else { $buildArgs += '--'; $buildArgs += ("-j $procs") }
}
if ($RemainingArgs) { $buildArgs += $RemainingArgs }
$code = Invoke-Tool 'cmake' $buildArgs
Write-Host "CMake build returned code: $code" -ForegroundColor Yellow
if ($code -ne 0) { exit $code }

# Optional install (best-effort)
try {
    Write-Host "Attempting install step (may be optional)..." -ForegroundColor Yellow
    $installCode = Invoke-Tool 'cmake' @('--install', 'build', '--config', 'Release')
    Write-Host "CMake install returned code: $installCode" -ForegroundColor Yellow
}
catch {
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
    }
    catch {
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
        }
        else {
            $proc = Start-Process -FilePath $exe -PassThru -ErrorAction Stop
        }
        Write-Host "Launched PID: $($proc.Id)" -ForegroundColor Yellow
        $ret = 0
    }
    else {
        # Run via Invoke-Tool so stdout/stderr are captured consistently
        $ret = Invoke-Tool $exe $normArgs
    }
    Write-Host "Executable exited with code: $ret" -ForegroundColor Cyan
}
finally {
    Pop-Location
}

exit $ret
