# Install + boot the built vpk in Vita3K.
#   .\run-vita3k.ps1            install out\diamondrush.vpk and run it
#   .\run-vita3k.ps1 -NoInstall run the already-installed DRSH00001
# (This Vita3K build has no LiveArea screen: check bg.png / startup.png on a real Vita.)
param([switch]$NoInstall)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$vita3k = Join-Path $root 'env\vita3k\Vita3K.exe'
$titleId = 'DRSH00001'

# Firmware is needed once; installing it from the CLI skips the wizard step.
$fwMarker = Join-Path $root 'env\vita3k\.fw-installed'
if (-not (Test-Path $fwMarker)) {
    & $vita3k -z --firmware (Join-Path $root 'env\PSVUPDAT.PUP') | Out-Host
    New-Item -ItemType File $fwMarker | Out-Null
}

if (-not $NoInstall) {
    # A homebrew vpk is a zip of the app folder; unpack it into Vita3K's ux0:app (Vita3K's own
    # CLI install stops to ask on stdin when the app is already installed).
    $vpk = Join-Path $root 'out\diamondrush.vpk'
    if (-not (Test-Path $vpk)) { throw "Build first: wsl ./build.sh vita" }
    $app = Join-Path $env:APPDATA "Vita3K\Vita3K\ux0\app\$titleId"
    if (Test-Path $app) { Remove-Item -Recurse -Force $app }
    $zip = Join-Path $env:TEMP 'diamondrush-vpk.zip'
    Copy-Item $vpk $zip -Force
    Expand-Archive $zip $app -Force
    Remove-Item $zip
}
& $vita3k --frontend sdl -r $titleId
