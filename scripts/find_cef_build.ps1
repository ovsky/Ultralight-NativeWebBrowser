param(
  [string]$IndexUrl = "https://opensource.spotify.com/cefbuilds/index.html"
)

function Get-ContentViaInvoke {
  param([string]$Url)
  try {
    $resp = Invoke-WebRequest -Uri $Url -UseBasicParsing -ErrorAction Stop
    return $resp.Content
  }
  catch {
    try {
      # PowerShell Core may not support -UseBasicParsing
      $resp = Invoke-WebRequest -Uri $Url -ErrorAction Stop
      return $resp.Content
    }
    catch {
      return $null
    }
  }
}

Write-Host "Detecting latest CEF Windows x64 build from CEF builds hosts"

$candidates = @(
  "https://opensource.spotify.com/cefbuilds/index.html",
  "https://cef-builds.spotifycdn.com/index.html",
  "https://opensource.spotify.com/cefbuilds/"
)

$content = $null
$usedUrl = $null
foreach ($u in $candidates) {
  $c = Get-ContentViaInvoke -Url $u
  if ($c) { $content = $c; $usedUrl = $u; break }
}

if (-not $content) {
  Write-Error "Failed to fetch any CEF builds index from known hosts."
  exit 2
}

# Try to find a Windows x64 build reference. Match full hrefs or filenames.
$regex = '([A-Za-z0-9_\-\./:]*cef_binary_[0-9]+(?:\.[0-9]+)*_windows64\.(?:zip|7z|tar\.gz))'
$m = [regex]::Matches($content, $regex)
if ($m.Count -eq 0) {
  Write-Error "No Windows x64 CEF build filename found on index page."
  exit 3
}

# Prefer the first match (index usually lists newest first)
$file = $m[0].Groups[1].Value

# Resolve to absolute URL using the index URL as base
try {
  $baseUri = New-Object System.Uri($usedUrl)
  $abs = New-Object System.Uri($baseUri, $file)
  $url = $abs.AbsoluteUri
}
catch {
  # Fallback: if file already absolute, use it; otherwise prefix host
  if ($file -match '^[a-zA-Z]+://') {
    $url = $file
  }
  else {
    $host = [System.Uri]::GetLeftPart($baseUri, [System.UriPartial]::Authority)
    if ($file.StartsWith('/')) { $url = "$host$file" } else { $url = "$host/$file" }
  }
}

Write-Output $url
exit 0
