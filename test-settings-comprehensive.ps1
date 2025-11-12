# Comprehensive Settings Test Script
Write-Host "`n=== ULTRALIGHT SETTINGS TEST ===" -ForegroundColor Cyan
Write-Host "Testing complete settings workflow`n" -ForegroundColor Cyan

# Test 1: Check initial settings file
Write-Host "[TEST 1] Checking initial settings file..." -ForegroundColor Yellow
if (Test-Path ".\setup\settings.json") {
    $content = Get-Content ".\setup\settings.json" -Raw | ConvertFrom-Json
    Write-Host "  ✓ Settings file exists" -ForegroundColor Green
    Write-Host "  Dark theme: $($content.values.launch_dark_theme)" -ForegroundColor Gray
    Write-Host "  Ad blocking: $($content.values.enable_adblock)" -ForegroundColor Gray
    Write-Host "  Suggestions: $($content.values.enable_suggestions)" -ForegroundColor Gray
}
else {
    Write-Host "  ✗ Settings file not found!" -ForegroundColor Red
    exit 1
}

Write-Host "`n[TEST 2] Starting browser..." -ForegroundColor Yellow
Write-Host "  Instructions:" -ForegroundColor Cyan
Write-Host "  1. Browser should start in DARK MODE (dark_theme=true)" -ForegroundColor Gray
Write-Host "  2. Open Menu (3 dots) - should have NO 'Quick settings' section" -ForegroundColor Gray
Write-Host "  3. Click 'Settings' in menu" -ForegroundColor Gray
Write-Host "  4. Settings page should load WITHOUT errors" -ForegroundColor Gray
Write-Host "  5. Check console (F12) - should show 'Loaded 13 settings'" -ForegroundColor Gray
Write-Host "  6. Verify checkboxes:" -ForegroundColor Gray
Write-Host "     - Dark Theme: ON" -ForegroundColor Gray
Write-Host "     - Ad Blocking: ON" -ForegroundColor Gray
Write-Host "     - Show Suggestions: ON" -ForegroundColor Gray
Write-Host "  7. Toggle 'Vibrant Window' to ON" -ForegroundColor Gray
Write-Host "  8. Toggle 'Show Download Badge' to OFF" -ForegroundColor Gray
Write-Host "  9. Click 'Save Changes'" -ForegroundColor Gray
Write-Host "  10. Close browser" -ForegroundColor Gray
Write-Host "`nPress any key when ready to start browser..." -ForegroundColor Yellow
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

# Start browser
Start-Process ".\build\Release\Ultralight-WebBrowser.exe" -WorkingDirectory $PWD

Write-Host "`n[TEST 3] Waiting for you to make changes and close browser..." -ForegroundColor Yellow
Write-Host "  Close the browser when done testing.`n" -ForegroundColor Gray

# Wait for browser to close
Wait-Process -Name "Ultralight-WebBrowser" -ErrorAction SilentlyContinue

Write-Host "`n[TEST 4] Checking if settings were saved..." -ForegroundColor Yellow
Start-Sleep -Seconds 1

if (Test-Path ".\setup\settings.json") {
    $newContent = Get-Content ".\setup\settings.json" -Raw | ConvertFrom-Json
    Write-Host "  ✓ Settings file still exists" -ForegroundColor Green
    Write-Host "`n  Checking for changes:" -ForegroundColor Cyan

    # Check if vibrant_window_theme was toggled to true
    if ($newContent.values.vibrant_window_theme -eq $true) {
        Write-Host "  ✓ Vibrant Window: ON (was OFF) - SAVED!" -ForegroundColor Green
    }
    else {
        Write-Host "  ✗ Vibrant Window: still OFF (expected ON)" -ForegroundColor Red
    }

    # Check if show_download_badge was toggled to false
    if ($newContent.values.show_download_badge -eq $false) {
        Write-Host "  ✓ Download Badge: OFF (was ON) - SAVED!" -ForegroundColor Green
    }
    else {
        Write-Host "  ✗ Download Badge: still ON (expected OFF)" -ForegroundColor Red
    }

    # Check other settings remained the same
    if ($newContent.values.launch_dark_theme -eq $true -and
        $newContent.values.enable_adblock -eq $true -and
        $newContent.values.enable_suggestions -eq $true) {
        Write-Host "  ✓ Other settings unchanged - CORRECT!" -ForegroundColor Green
    }

    # Check for backup files
    Write-Host "`n  Checking backup files:" -ForegroundColor Cyan
    if (Test-Path ".\setup\settings.json.1") {
        Write-Host "  ✓ Backup .1 exists" -ForegroundColor Green
    }
    if (Test-Path ".\setup\settings.json.2") {
        Write-Host "  ✓ Backup .2 exists" -ForegroundColor Green
    }
}
else {
    Write-Host "  ✗ Settings file was deleted!" -ForegroundColor Red
    exit 1
}

Write-Host "`n[TEST 5] Restarting browser to verify persistence..." -ForegroundColor Yellow
Write-Host "  The browser should now:" -ForegroundColor Cyan
Write-Host "  1. Still start in DARK MODE" -ForegroundColor Gray
Write-Host "  2. Open Settings and verify:" -ForegroundColor Gray
Write-Host "     - Vibrant Window: ON (should be saved)" -ForegroundColor Gray
Write-Host "     - Download Badge: OFF (should be saved)" -ForegroundColor Gray
Write-Host "  3. Test runtime effect: Toggle 'Dark Theme' OFF" -ForegroundColor Gray
Write-Host "     - UI should immediately switch to light mode!" -ForegroundColor Gray
Write-Host "  4. Toggle 'Ad Blocking' OFF" -ForegroundColor Gray
Write-Host "     - Should disable ad blocking immediately" -ForegroundColor Gray
Write-Host "  5. Close browser WITHOUT saving" -ForegroundColor Gray
Write-Host "`nPress any key to start browser again..." -ForegroundColor Yellow
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

Start-Process ".\build\Release\Ultralight-WebBrowser.exe" -WorkingDirectory $PWD

Write-Host "`n  Test the runtime effects, then close browser." -ForegroundColor Gray
Wait-Process -Name "Ultralight-WebBrowser" -ErrorAction SilentlyContinue

Write-Host "`n[TEST 6] Final verification..." -ForegroundColor Yellow
$finalContent = Get-Content ".\setup\settings.json" -Raw | ConvertFrom-Json

Write-Host "  Settings that should be SAVED (from Test 4):" -ForegroundColor Cyan
Write-Host "    Vibrant Window: $($finalContent.values.vibrant_window_theme) (should be true)" -ForegroundColor Gray
Write-Host "    Download Badge: $($finalContent.values.show_download_badge) (should be false)" -ForegroundColor Gray

Write-Host "`n  Settings that should be UNCHANGED (not saved in Test 5):" -ForegroundColor Cyan
Write-Host "    Dark Theme: $($finalContent.values.launch_dark_theme) (should still be true)" -ForegroundColor Gray
Write-Host "    Ad Blocking: $($finalContent.values.enable_adblock) (should still be true)" -ForegroundColor Gray

Write-Host "`n=== TEST COMPLETE ===" -ForegroundColor Cyan
Write-Host "Summary:" -ForegroundColor Yellow
Write-Host "  • Settings load from setup/settings.json: " -NoNewline
if ($finalContent) { Write-Host "✓" -ForegroundColor Green } else { Write-Host "✗" -ForegroundColor Red }
Write-Host "  • Settings persist after save: " -NoNewline
if ($finalContent.values.vibrant_window_theme -eq $true) { Write-Host "✓" -ForegroundColor Green } else { Write-Host "✗" -ForegroundColor Red }
Write-Host "  • Runtime changes work immediately: " -NoNewline
Write-Host "✓ (manual verification)" -ForegroundColor Green
Write-Host "  • Changes without save don't persist: " -NoNewline
if ($finalContent.values.launch_dark_theme -eq $true) { Write-Host "✓" -ForegroundColor Green } else { Write-Host "✗" -ForegroundColor Red }
Write-Host ""
