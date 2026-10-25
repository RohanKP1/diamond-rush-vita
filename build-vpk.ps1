# One command from your jar to out\diamondrush.vpk (Windows, with WSL set up).
#   .\build-vpk.ps1
# Needs, once: tools\setup-windows.ps1, and in WSL: bash tools/setup-wsl.sh all
# and your copy of Diamond-Rush_J2ME_EN_v104-Nokia-N6101.jar in this folder.
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'tools\prepare.ps1')
Push-Location $PSScriptRoot
try {
    wsl ./build.sh vita
    if ($LASTEXITCODE -ne 0) { throw 'Vita build failed' }
} finally { Pop-Location }
Write-Host "Done: $(Join-Path $PSScriptRoot 'out\diamondrush.vpk')"
