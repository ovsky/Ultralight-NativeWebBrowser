param(
  [string]$PackagePath = "build/Ultralight-WebBrowser-dev-Windows-x64.zip"
)

Write-Host "Verifying package: $PackagePath"
if (-not (Test-Path $PackagePath)) {
  Write-Error "Package not found: $PackagePath"
  exit 2
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead((Resolve-Path $PackagePath).Path)
$entries = $zip.Entries | ForEach-Object { $_.FullName }

Write-Host "Package contains (top-level sample):"
$entries | Where-Object { $_ -notlike "*/**" } | Select-Object -First 20 | ForEach-Object { Write-Host "  $_" }

$expected = @(
  "Ultralight-WebBrowser.exe",
  "UltralightWebBrowser/AppCore.dll",
  "UltralightWebBrowser/Ultralight.dll",
  "UltralightWebBrowser/UltralightCore.dll",
  "UltralightWebBrowser/WebCore.dll",
  "UltralightWebBrowser/assets/",
  "UltralightWebBrowser/assets/resources/icudt",
  "UltralightWebBrowser/assets/resources/cacert.pem"
)

$missing = @()
foreach ($pat in $expected) {
  if ($pat -like "*icudt") {
    $found = $entries | Where-Object { $_ -like "*icudt*" }
  } elseif ($pat.EndsWith("/")) {
    $found = $entries | Where-Object { $_ -like ("$pat*") }
  } else {
    $found = $entries | Where-Object { $_ -eq $pat }
  }
  if (-not $found) { $missing += $pat }
}

if ($missing.Count -eq 0) {
  Write-Host "All expected items found in package." -ForegroundColor Green
  exit 0
} else {
  Write-Warning "Missing expected items in package:"
  $missing | ForEach-Object { Write-Host "  $_" }
  exit 3
}
