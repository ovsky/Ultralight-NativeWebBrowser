# Test script to run Ultralight-WebBrowser and check settings workflow
Write-Host "Starting Ultralight WebBrowser..." -ForegroundColor Green
Write-Host "Expected behavior:" -ForegroundColor Yellow
Write-Host "1. Console should show settings loaded from setup/settings.json" -ForegroundColor Yellow
Write-Host "2. Open Settings (Menu > Settings or press F12 and check console)" -ForegroundColor Yellow
Write-Host "3. Checkboxes should reflect: dark_theme=ON, adblock=OFF, show_download_badge=OFF, vibrant_window_theme=ON" -ForegroundColor Yellow
Write-Host "4. Toggle some settings and click Save" -ForegroundColor Yellow
Write-Host "5. Restart browser and verify settings persisted" -ForegroundColor Yellow
Write-Host "" -ForegroundColor Yellow

# Run the browser
& ".\build\Release\Ultralight-WebBrowser.exe"
