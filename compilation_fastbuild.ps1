#!/usr/bin/env pwsh
<#
.SYNOPSIS
  Fast wrapper to invoke the build step only: `cmake --build build --config <CONFIG>`.

.DESCRIPTION
  This script focuses on the fastest possible incremental build without re-configuring
  or removing any dependencies. It simply runs `cmake --build build --config <CONFIG>`
  where `<CONFIG>` defaults to `Release`. You may pass a different configuration as the
  first argument (for example, `Debug`). Any remaining args are forwarded to the
  underlying `cmake --build` command (for example, `-- -j 8` on supported CMake versions).

.USAGE
  ./compilation_fastbuild.ps1 [Configuration] [-- additional cmake build args]
  Example: ./compilation_fastbuild.ps1 Release
#>

[CmdletBinding()]
param(
    [string]$Configuration = 'Release',
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$RemainingArgs = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Write-Host "Starting compilation_fastbuild.ps1: PowerShell $($PSVersionTable.PSVersion) Host: $($Host.Name) PID: $($PID)" -ForegroundColor Magenta
Write-Host "Working directory: " (Get-Location).Path -ForegroundColor Magenta
Write-Host "Build configuration: $Configuration" -ForegroundColor Magenta
Write-Host "Additional args: $($RemainingArgs -join ' ')" -ForegroundColor Magenta

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $scriptRoot

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "cmake not found in PATH. Please install CMake and add it to PATH."
    exit 2
}

function Write-Log([string]$Level, [string]$Message) {
    $ts = (Get-Date).ToString('HH:mm:ss')
    switch ($Level) {
        'INFO' { Write-Host "[$ts] $Message" -ForegroundColor Cyan }
        'WARN' { Write-Warning "[$ts] $Message" }
        'ERR'  { Write-Error "[$ts] $Message" }
        default { Write-Host "[$ts] $Message" }
    }
}

function Invoke-Tool([string]$Exe, [string[]]$Arguments, [int]$TimeoutSeconds = 3600, [int]$StallTimeoutSeconds = 300) {
    Write-Log INFO "Running: $Exe $($Arguments -join ' ')"
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
    while (-not $proc.HasExited) {
        Start-Sleep -Milliseconds 300
        try {
            $size = (Get-Item $outFile -ErrorAction SilentlyContinue).Length
            if ($null -ne $size -and $size -ne $lastSize) {
                $lastSize = $size
                $lastOutputTime = Get-Date
                # Output any new content
                Get-Content -LiteralPath $outFile -Tail 100 -ErrorAction SilentlyContinue | ForEach-Object { Write-Log INFO $_ }
            }
            else {
                # occasionally tail to show progress
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
            Write-Log WARN "No output for $StallTimeoutSeconds seconds. Sending break to process..."
            try { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue } catch { }
            break
        }
    }

    # Ensure we capture any remaining output
    Start-Sleep -Milliseconds 200
    try {
        if (Test-Path $outFile) { Get-Content -LiteralPath $outFile -ErrorAction SilentlyContinue | ForEach-Object { Write-Log INFO $_ } }
        if (Test-Path $errFile) { Get-Content -LiteralPath $errFile -ErrorAction SilentlyContinue | ForEach-Object { Write-Log ERR $_ } }
    }
    catch { }

    $exit = 1
    try { $exit = $proc.ExitCode } catch { }
    Remove-Item -LiteralPath $outFile, $errFile -ErrorAction SilentlyContinue
    return $exit
}

$buildArgs = @('--build', 'build', '--config', $Configuration)
# If caller didn't provide parallelism flags, add a reasonable default based on CPU count
$hasParallel = $false
foreach ($a in $RemainingArgs) { if ($a -match '^-j' -or $a -match '^/m' -or $a -eq '--' -or $a -eq '/m') { $hasParallel = $true; break } }
$procs = [Environment]::ProcessorCount
if (-not $hasParallel) {
    if ($IsWindows) { $buildArgs += '--'; $buildArgs += ("/m:$procs") }
    else { $buildArgs += '--'; $buildArgs += ("-j $procs") }
}
if ($RemainingArgs) { $buildArgs += $RemainingArgs }

$code = Invoke-Tool 'cmake' $buildArgs
if ($code -ne 0) { exit $code }

Write-Host "Build step completed with exit code $code" -ForegroundColor Green

# After a successful build, locate the executable and launch it (detached by default)
function Find-Executable {
    param([string]$BasePath)
    $search = Get-ChildItem -Path $BasePath -Recurse -File -ErrorAction SilentlyContinue | Where-Object {
        ($_.Extension -ieq '.exe' -and $_.BaseName -eq 'Ultralight-WebBrowser') -or
        ($_.BaseName -eq 'Ultralight-WebBrowser' -and $_.Extension -eq '')
    }
    if ($search) { return $search[0].FullName }
    $win = Join-Path (Join-Path $BasePath 'Release') 'Ultralight-WebBrowser.exe'
    if (Test-Path $win) { return $win }
    $other = Join-Path $BasePath 'Ultralight-WebBrowser'
    if (Test-Path $other) { return $other }
    return $null
}

Write-Host "Searching for executable under: $PWD\build" -ForegroundColor Cyan
$exe = Find-Executable -BasePath (Join-Path $scriptRoot 'build')
if ($exe) {
    Write-Host "Found executable: $exe" -ForegroundColor Green
    try {
        Write-Host "Launching executable (detached)..." -ForegroundColor Yellow
        $proc = Start-Process -FilePath $exe -PassThru -ErrorAction Stop
        Write-Host "Launched PID: $($proc.Id)" -ForegroundColor Yellow
    }
    catch {
        Write-Warning "Failed to launch executable: $_"
        exit 4
    }
}
else {
    Write-Warning "Could not find built executable under build/. Skipping launch."
}
