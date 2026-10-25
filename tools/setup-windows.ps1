# One-time Windows-side setup (no admin needed). Everything lands in .\env\
#   - Vita3K       : PS Vita emulator for testing the .vpk
#   - PSVUPDAT.PUP : official Vita firmware, required by Vita3K (install via File > Install Firmware)
#   - Temurin JDK  : portable Java, to run the reference emulator and the decompiler
#   - Vineflower   : Java decompiler (jar -> readable .java)
#   - FreeJ2ME-Plus: J2ME emulator, to play the original jar as a behaviour reference
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$root = Split-Path -Parent $PSScriptRoot
$env_ = Join-Path $root 'env'
New-Item -ItemType Directory -Force $env_ | Out-Null

function Get-LatestAsset($repo, $pattern) {
    $rel = Invoke-RestMethod "https://api.github.com/repos/$repo/releases/latest"
    $asset = $rel.assets | Where-Object { $_.name -match $pattern } | Select-Object -First 1
    if (-not $asset) { throw "No asset matching '$pattern' in $repo $($rel.tag_name)" }
    return $asset
}

function Get-File($url, $out) {
    if (Test-Path $out) { Write-Host "  have $out"; return }
    Write-Host "  downloading $url"
    Invoke-WebRequest $url -OutFile $out
}

Write-Host '[1/5] Vita3K'
$vita3k = Join-Path $env_ 'vita3k'
if (-not (Test-Path (Join-Path $vita3k 'Vita3K.exe'))) {
    $rel = Invoke-RestMethod 'https://api.github.com/repos/Vita3K/Vita3K/releases/tags/continuous'
    $asset = $rel.assets | Where-Object { $_.name -match '^windows-latest\.zip$' } | Select-Object -First 1
    $zip = Join-Path $env_ $asset.name
    Get-File $asset.browser_download_url $zip
    Expand-Archive $zip $vita3k -Force
    Remove-Item $zip
}

Write-Host '[2/5] PS Vita firmware (for Vita3K)'
Get-File 'http://dus01.psv.update.playstation.net/update/psv/image/2022_0209/rel_f2c7b12fe85496ec88a0391b514d6e3b/PSVUPDAT.PUP' (Join-Path $env_ 'PSVUPDAT.PUP')

Write-Host '[3/5] Temurin JDK 21 (portable)'
$jdk = Join-Path $env_ 'jdk'
if (-not (Test-Path (Join-Path $jdk 'bin\java.exe'))) {
    $zip = Join-Path $env_ 'jdk.zip'
    Get-File 'https://api.adoptium.net/v3/binary/latest/21/ga/windows/x64/jdk/hotspot/normal/eclipse' $zip
    $tmp = Join-Path $env_ 'jdk-tmp'
    Expand-Archive $zip $tmp -Force
    Move-Item (Get-ChildItem $tmp -Directory | Select-Object -First 1).FullName $jdk
    Remove-Item $tmp, $zip -Recurse
}

Write-Host '[4/5] Vineflower decompiler'
$a = Get-LatestAsset 'Vineflower/vineflower' '^vineflower-[\d.]+\.jar$'
Get-File $a.browser_download_url (Join-Path $env_ 'vineflower.jar')

Write-Host '[5/5] FreeJ2ME-Plus (reference J2ME emulator)'
$fj = Join-Path $env_ 'freej2me'
if (-not (Test-Path $fj)) {
    $a = Get-LatestAsset 'TASEmulators/freej2me-plus' '\.(zip|jar)$'
    $out = Join-Path $env_ $a.name
    Get-File $a.browser_download_url $out
    New-Item -ItemType Directory -Force $fj | Out-Null
    if ($a.name -like '*.zip') { Expand-Archive $out $fj -Force; Remove-Item $out }
    else { Move-Item $out (Join-Path $fj $a.name) }
}

Write-Host "`nDone. Tools are in $env_"
