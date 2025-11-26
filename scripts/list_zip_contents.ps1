param(
  [string]$ZipPath = "build/Ultralight-WebBrowser-dev-Windows-x64.zip",
  [string]$OutDir = "build/zip_extract"
)

Write-Host "Listing contents of: $ZipPath"
if (-not (Test-Path $ZipPath)) {
  Write-Error "Zip not found: $ZipPath"
  exit 2
}

Remove-Item -Recurse -Force $OutDir -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath $ZipPath -DestinationPath $OutDir -Force
Get-ChildItem -Path $OutDir -Recurse | ForEach-Object { Write-Host $_.FullName }
