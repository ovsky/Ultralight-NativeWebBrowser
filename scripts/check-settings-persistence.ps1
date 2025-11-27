# Simple smoke test for settings persistence.
# 1) Backup existing data/settings.json if any
# 2) Launch the app for a few seconds
# 3) Check for presence of settings.json under app data or project data folder

$exe = "$(Resolve-Path ..\build\Release\Ultralight-WebBrowser.exe)"
$settingsPath = "$(Resolve-Path ..\data\settings.json)" -ErrorAction SilentlyContinue
$backupPath = "$settingsPath.bak"

if (Test-Path $settingsPath) { Copy-Item $settingsPath $backupPath -Force }

Write-Host "Launching app to generate/read settings (will auto-close)..."
$proc = Start-Process -FilePath $exe -PassThru
Start-Sleep -Seconds 4
# Close the process gracefully
Stop-Process -Id $proc.Id -ErrorAction SilentlyContinue

if (Test-Path $settingsPath) {
    Write-Host "Settings found at: $settingsPath"
    Get-Content $settingsPath | Select-Object -First 50 | ForEach-Object { Write-Host $_ }
}
else {
    Write-Host "No settings file detected at $settingsPath. That may be expected if no save triggered."
}

# restore backup if present
if (Test-Path $backupPath) { Move-Item -Path $backupPath -Destination $settingsPath -Force }

Write-Host "Done."