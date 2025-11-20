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

function Invoke-Tool([string]$Exe, [string[]]$Arguments) {
    Write-Host "Running: $Exe $($Arguments -join ' ')" -ForegroundColor Cyan
    $outFile = [System.IO.Path]::GetTempFileName()
    $errFile = [System.IO.Path]::GetTempFileName()
    try {
        $proc = Start-Process -FilePath $Exe -ArgumentList $Arguments -NoNewWindow -RedirectStandardOutput $outFile -RedirectStandardError $errFile -Wait -PassThru -ErrorAction Stop
        $exit = $proc.ExitCode
    }
    catch {
        Write-Warning "Start-Process failed: $_"
        $orig = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            $output = & $Exe @Arguments 2>&1 | Out-String
            if ($output -and $output.Trim()) { Write-Host $output }
            $exit = $LASTEXITCODE
        }
        finally { $ErrorActionPreference = $orig }
        return $exit
    }
    $stdout = Get-Content -Raw -LiteralPath $outFile -ErrorAction SilentlyContinue
    $stderr = Get-Content -Raw -LiteralPath $errFile -ErrorAction SilentlyContinue
    if ($stdout -and $stdout.Trim()) { Write-Host $stdout }
    if ($stderr -and $stderr.Trim()) { Write-Host $stderr }
    Remove-Item -LiteralPath $outFile, $errFile -ErrorAction SilentlyContinue
    return $exit
}

$buildArgs = @('--build', 'build', '--config', $Configuration)
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
