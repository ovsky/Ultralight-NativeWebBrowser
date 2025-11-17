# find_installer.ps1
# Locates an installer executable or MSI under build/ and copies it to the workspace root with a versioned name.
# Useful to avoid inlining complex PowerShell in workflow YAML where brace parsing can be problematic.

param(
    [string] $ManifestPath = 'build\file-manifest.csv'
)

Write-Host "Listing build/ candidates from file manifest (if present) or shallow scan (depth 2)"
if (Test-Path $ManifestPath) {
    Import-Csv -Path $ManifestPath | Sort-Object { [int]$_.Length } -Descending | Select-Object -First 200 | ForEach-Object { Write-Host (' - {0} ({1:N2} KB)' -f $_.FullName, ($_.Length / 1KB) ) }
}
else {
    Get-ChildItem -Path build -Recurse -Depth 2 -File | Sort-Object Length -Descending | Select-Object -First 200 | ForEach-Object { Write-Host (' - {0} ({1:N2} KB)' -f $_.FullName, ($_.Length / 1KB) ) }
}

# Robust search for installer candidates: prefer names starting with project prefix,
# then names containing 'installer' or 'setup', then fall back to largest exe/msi.
$candidates = Get-ChildItem -Path build -Recurse -Depth 2 -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in '.exe', '.msi' }
$matchPrefixed = $candidates | Where-Object { $_.Name -match '^Ultralight-WebBrowser-.*\.(exe|msi)$' }
$matchInstaller = $candidates | Where-Object { $_.Name -match '(installer|setup|setup-?x?)' -or $_.Name -match '\bSetup\b' }

$chosen = $null
if ($matchPrefixed) { $chosen = $matchPrefixed | Sort-Object Length -Descending | Select-Object -First 1 }
elseif ($matchInstaller) { $chosen = $matchInstaller | Sort-Object Length -Descending | Select-Object -First 1 }
elseif ($candidates) { $chosen = $candidates | Sort-Object Length -Descending | Select-Object -First 1 }

if ($chosen) {
    $version = "$env:WEBBROWSER_VERSION"
    $ext = $chosen.Extension
    $finalName = "Ultralight-WebBrowser-$version-Windows-Installer$ext"
    Write-Host "Found installer candidate: $($chosen.FullName) -> copying as $finalName"
    Copy-Item -Force $chosen.FullName (Join-Path $PWD $finalName)
    Write-Host "Workspace root now contains:"; Get-ChildItem -Path $PWD -Filter "Ultralight-WebBrowser-*Installer.*" -File | ForEach-Object { Write-Host (' - {0} ({1:N2} KB)' -f $_.FullName, ($_.Length / 1KB) ) }
    exit 0
}

Write-Host "No installer candidates found under build/. Trying to parse CPack logs for produced package paths..."
$possibleLogFiles = @('build/nsis-cpack.log', 'build/zip-cpack.log')
$foundFromLog = $null
foreach ($log in $possibleLogFiles) {
    if (Test-Path $log) {
        Write-Host "Parsing log: $log"
        $match = Select-String -Path $log -Pattern 'CPack: - package: (.+)' -AllMatches | Select-Object -First 1
        if ($match) {
            $pkgPath = $match.Matches[0].Groups[1].Value.Trim()
            Write-Host "Found package entry in log: $pkgPath"
            # Try absolute path first, then relative to build dir, then workspace root
            if (Test-Path $pkgPath) { $foundFromLog = Get-Item $pkgPath; break }
            $candidate = Join-Path (Join-Path $PWD 'build') $pkgPath
            if (Test-Path $candidate) { $foundFromLog = Get-Item $candidate; break }
            $candidate2 = Join-Path $PWD $pkgPath
            if (Test-Path $candidate2) { $foundFromLog = Get-Item $candidate2; break }
        }
    }
    if ($foundFromLog) { break }
}

if ($foundFromLog) {
    $version = "$env:WEBBROWSER_VERSION"
    $ext = $foundFromLog.Extension
    $finalName = "Ultralight-WebBrowser-$version-Windows-Installer$ext"
    Write-Host "Copying package found in logs: $($foundFromLog.FullName) -> $finalName"
    Copy-Item -Force $foundFromLog.FullName (Join-Path $PWD $finalName)
    Write-Host "Workspace root now contains:"; Get-ChildItem -Path $PWD -Filter "Ultralight-WebBrowser-*Installer.*" -File | ForEach-Object { Write-Host (' - {0} ({1:N2} KB)' -f $_.FullName, ($_.Length / 1KB) ) }
}
else {
    Write-Host "CPack logs did not reveal a package path either. No installer will be uploaded."
}
