param(
  [switch]$Force
)

Write-Host "PowerShell: Attempting to install libcurl or vcpkg curl for development (Windows)..."

function Install-VcpkgAndCurl {
  $tempRoot = $env:RUNNER_TEMP
  if (-not $tempRoot) { $tempRoot = $env:TEMP }
  if (-not $tempRoot) { $tempRoot = $env:USERPROFILE }
  if (-not $tempRoot) { $tempRoot = "C:\vcpkg_temp" }
  $vcpkg = Join-Path $tempRoot 'vcpkg'
  if (-not (Test-Path $vcpkg)) {
    Write-Host "Cloning vcpkg to $vcpkg"
    git clone https://github.com/microsoft/vcpkg.git $vcpkg
  }
  Push-Location $vcpkg
  if (-not (Test-Path ./vcpkg.exe)) {
    & .\bootstrap-vcpkg.bat
  }
  # Install curl for x64
  & .\vcpkg.exe install curl:x64-windows
  $env:VCPKG_ROOT = $vcpkg
  # Persist VCPKG_ROOT to GitHub Actions environment file if present
  if ($env:GITHUB_ENV) { "VCPKG_ROOT=$vcpkg" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append }
  Pop-Location
  return $true
}

try {
  Write-Host "Attempting vcpkg install for curl (preferred)", 'VCPKG_ROOT=' + $vcpkg
  try {
    Install-VcpkgAndCurl
    $installed = $true
  } catch {
    Write-Warning "vcpkg install failed: $_"
    $installed = $false
  }
  if (-not $installed) {
    if (Get-Command choco -ErrorAction SilentlyContinue) {
      Write-Host "Chocolatey present; attempting to install curl client via choco as fallback..."
      choco install curl -y --no-progress
      $installed = $true
    }
  }
  Write-Host "Install script finished. If build does not detect curl, consider enabling vcpkg toolchain for CMake by setting -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
  exit 0
} catch {
  Write-Error "Failed to install libcurl: $_"
  exit 1
}
