param(
  [Parameter(Mandatory = $true)]
  [string]$Url,

  [string]$OutDir = "$PSScriptRoot\..\build\_deps\cef_auto\download",
  [string]$ExtractTo = "$PSScriptRoot\..\build\cef_auto_src",
  [switch]$Extract
)

Write-Host "CEF download helper"
Write-Host "URL: $Url"
Write-Host "OutDir: $OutDir"

if (-not (Test-Path $OutDir)) {
  New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
}

$fileName = Split-Path -Path $Url -Leaf
$outPath = Join-Path $OutDir $fileName

Write-Host "Downloading to: $outPath"
try {
  # Prefer Invoke-WebRequest; handle PowerShell Core which doesn't support -UseBasicParsing
  if (Get-Command Invoke-WebRequest -ErrorAction SilentlyContinue) {
    try {
      # Try with the legacy flag (works in Windows PowerShell)
      Invoke-WebRequest -Uri $Url -OutFile $outPath -UseBasicParsing -ErrorAction Stop
    }
    catch {
      try {
        # Try without the legacy flag (PowerShell Core)
        Invoke-WebRequest -Uri $Url -OutFile $outPath -ErrorAction Stop
      }
      catch {
        # Fallback to curl.exe if available
        if (Get-Command curl.exe -ErrorAction SilentlyContinue) {
          curl.exe -L $Url -o $outPath
        }
        else {
          throw $_
        }
      }
    }
  }
  else {
    if (Get-Command curl.exe -ErrorAction SilentlyContinue) {
      curl.exe -L $Url -o $outPath
    }
    else {
      throw "No download tool available (Invoke-WebRequest or curl.exe)"
    }
  }
}
catch {
  Write-Error "Download failed: $_"
  exit 1
}

if (-not (Test-Path $outPath)) {
  Write-Error "Download did not produce expected file: $outPath"
  exit 2
}

# Compute SHA256
$hash = Get-FileHash -Algorithm SHA256 -Path $outPath
Write-Host "SHA256: $($hash.Hash)"

Write-Host "To configure CMake to use this URL and its SHA256, run:" -ForegroundColor Yellow
Write-Host "cmake -S . -B build -DCEF_AUTO_DOWNLOAD=ON -DCEF_AUTO_URL=\"$Url\" -DCEF_AUTO_URL_HASH=SHA256=$($hash.Hash) -DBUILD_SIDECAR=ON -DUSE_CEF=ON"

if ($Extract) {
  Write-Host "Extracting archive to: $ExtractTo"
  $dest = $ExtractTo
  if (-not (Test-Path $dest)) { New-Item -ItemType Directory -Path $dest -Force | Out-Null }
  try {
    # Prefer Expand-Archive for zip files
    try {
      Expand-Archive -Path $outPath -DestinationPath $dest -Force -ErrorAction Stop
      Write-Host "Extracted zip to: $dest"
    }
    catch {
      # Fallback: try tar (available on recent Windows builds) for tar.* archives
      if (Get-Command tar -ErrorAction SilentlyContinue) {
        & tar -xvf $outPath -C $dest
        Write-Host "Extracted archive with tar to: $dest"
      }
      else {
        Write-Warning "Extraction failed or archive type not supported by Expand-Archive. You may need to extract manually. $_"
      }
    }
  }
  catch {
    Write-Warning "Extraction failed: $_"
  }
}

Write-Host "Done."
