# Prepares the game sources from your own copy of the jar:
#   1. checks the jar is the version the port is made for (by SHA-256)
#   2. decompiles it into .\java-src\ (Vineflower, tools\decompile.ps1)
#   3. applies the port's changes (patches\java-src.patch)
#   4. translates java-src to C++ (port\src\game\gen, tools\translate.ps1)
# Run tools\setup-windows.ps1 once first.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$jarName = 'Diamond-Rush_J2ME_EN_v104-Nokia-N6101.jar'
$jarHash = '4871112043A1DBC57EB3560547E5A2B079A6A2F387DA0E466C8039D81ED98E1F'
$jar = Join-Path $root $jarName
$src = Join-Path $root 'java-src'

if (-not (Test-Path $jar)) {
    throw "Put your copy of the game, $jarName, in $root first."
}
$hash = (Get-FileHash $jar -Algorithm SHA256).Hash
if ($hash -ne $jarHash) {
    throw "$jarName is not the expected version (SHA-256 $hash, expected $jarHash). " +
          "The port is made for Diamond Rush v1.0.4, English, Nokia N6101 build."
}
if (-not (Test-Path (Join-Path $root 'env\jdk\bin\java.exe'))) {
    throw 'Run tools\setup-windows.ps1 first (downloads the JDK and Vineflower into env\).'
}

if (Test-Path $src) {
    Write-Host 'java-src already exists: keeping it (delete it to start again from the jar)'
} else {
    Write-Host '[1/3] decompiling the jar'
    & (Join-Path $PSScriptRoot 'decompile.ps1') | Out-Null
    Write-Host '[2/3] applying the port''s changes'
    Push-Location $src
    try {
        git apply --whitespace=nowarn (Join-Path $root 'patches\java-src.patch')
        if ($LASTEXITCODE -ne 0) { throw 'patches\java-src.patch did not apply' }
    } finally { Pop-Location }
}

Write-Host '[3/3] translating to C++'
& (Join-Path $PSScriptRoot 'translate.ps1') | Out-Null
Write-Host 'Sources ready. Build with: wsl ./build.sh vita'
