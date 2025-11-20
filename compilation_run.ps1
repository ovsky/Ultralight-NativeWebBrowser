#!/usr/bin/env pwsh
<#
.SYNOPSIS
  Run the built Ultralight-WebBrowser executable from the `build` folder.

.DESCRIPTION
  Replacement for the old `run.ps1` script. It locates the `Ultralight-WebBrowser`
  executable below `build` and launches it while forwarding any provided
  command-line arguments.

.USAGE
  ./compilation_run.ps1 -- "--some-app-flag"
#>

[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$RemainingArgs = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

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
Write-Host "Starting compilation_run.ps1: PowerShell $($PSVersionTable.PSVersion) Host: $($Host.Name) PID: $($PID)" -ForegroundColor Magenta
Write-Host "Working directory: " (Get-Location).Path -ForegroundColor Magenta
Write-Host "Script args (remaining): $($RemainingArgs -join ' ')" -ForegroundColor Magenta

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $scriptRoot

function Find-Executable {
    param([string]$BasePath)
    # Prefer an executable file (.exe on Windows, or an executable file without extension on Unix)
    $search = Get-ChildItem -Path $BasePath -Recurse -File -ErrorAction SilentlyContinue | Where-Object { ($_.Extension -ieq '.exe' -and $_.BaseName -eq 'Ultralight-WebBrowser') -or ($_.BaseName -eq 'Ultralight-WebBrowser' -and $_.Extension -eq '') }
    if ($search) { return $search[0].FullName }
    $win = Join-Path (Join-Path $BasePath 'Release') 'Ultralight-WebBrowser.exe'
    if (Test-Path $win) { return $win }
    $other = Join-Path $BasePath 'Ultralight-WebBrowser'
    if (Test-Path $other) { return $other }
    return $null
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
        Write-Warning "Start-Process failed or behaved unexpectedly: $_"
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

$buildPath = Join-Path $scriptRoot 'build'
if (-not (Test-Path $buildPath)) {
    Write-Error "Build folder not found: $buildPath. Run the build script first (./compilation_justcompile.ps1)."
    exit 2
}

Write-Host "Searching for executable under: $buildPath" -ForegroundColor Cyan
$exe = Find-Executable -BasePath $buildPath
if (-not $exe) {
    Write-Error "Could not locate Ultralight-WebBrowser executable under $buildPath. Check the build output and run the compile script first."
    try { Get-ChildItem -Path $buildPath -Recurse -Force -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_.FullName } } catch { }
    exit 3
}

Write-Host "Found executable: $exe" -ForegroundColor Green
Write-Host "Running: $exe $($RemainingArgs -join ' ')" -ForegroundColor Cyan
Push-Location $([IO.Path]::GetDirectoryName($exe))
try {
    if ($Detach) {
        Write-Host "Launching in background (detached)." -ForegroundColor Yellow
        if ($RemainingArgs -and $RemainingArgs.Count -gt 0) {
            $proc = Start-Process -FilePath $exe -ArgumentList $RemainingArgs -PassThru -ErrorAction Stop
        }
        else {
            $proc = Start-Process -FilePath $exe -PassThru -ErrorAction Stop
        }
        Write-Host "Launched PID: $($proc.Id)" -ForegroundColor Yellow
        $ret = 0
    }
    else {
        $ret = Invoke-Tool $exe $RemainingArgs
    }
    Write-Host "Executable exited with code: $ret" -ForegroundColor Cyan
}
finally {
    Pop-Location
}
exit $ret
