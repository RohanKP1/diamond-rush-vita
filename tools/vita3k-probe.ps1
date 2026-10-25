# Install + boot the vpk in Vita3K, press Vita buttons, capture window screenshots.
#   .\tools\vita3k-probe.ps1 -Out build\probe\vita3k -Keys 'W15','S','CROSS','W2','S'
#   -NoInstall  boot the already-installed DRSH00001 instead of (re)installing out\diamondrush.vpk
# Buttons (Vita3K default keyboard map, env\vita3k\config.yml):
#   UP DOWN LEFT RIGHT CROSS CIRCLE SQUARE TRIANGLE START SELECT L R ; plus W<sec> and S
param(
    [Parameter(Mandatory)][string]$Out,
    [string[]]$Keys = @('W15', 'S'),
    [switch]$NoInstall
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $PSScriptRoot 'WinProbe.psm1') -Force

$keyMap = @{ UP = 0x26; DOWN = 0x28; LEFT = 0x25; RIGHT = 0x27
    CROSS = 0x58; CIRCLE = 0x43; SQUARE = 0x5A; TRIANGLE = 0x56   # X C Z V
    START = 0x0D; SELECT = 0xA1; L = 0x51; R = 0x45 }              # Enter RShift Q E

$v = Join-Path $root 'env\vita3k'
$exe = Join-Path $v 'Vita3K.exe'
if (-not $NoInstall) {
    # A homebrew vpk is just a zip of the app folder: unpack it straight into Vita3K's ux0:app
    # (Vita3K's own CLI install prompts on stdin when the app already exists).
    $vpk = (Resolve-Path (Join-Path $root 'out\diamondrush.vpk')).Path
    $app = Join-Path $env:APPDATA 'Vita3K\Vita3K\ux0\app\DRSH00001'
    if (Test-Path $app) { Remove-Item -Recurse -Force $app }
    $zip = Join-Path $env:TEMP 'diamondrush-vpk.zip'
    Copy-Item $vpk $zip -Force
    Expand-Archive $zip $app -Force
    Remove-Item $zip
    Write-Host "installed to $app"
}
$p = Start-Process $exe -ArgumentList '--frontend', 'sdl', '-r', 'DRSH00001' -WorkingDirectory $v -PassThru `
    -RedirectStandardOutput (Join-Path $v 'run-out.txt')
try {
    Invoke-ProbeSteps -Process $p -KeyMap $keyMap -Out $Out -Steps $Keys
} finally {
    if (-not $p.HasExited) { $p.Kill() }
    Get-Content (Join-Path $v 'run-out.txt') | Select-String -Pattern '\|E\||\|C\||error' | Select-Object -Last 15
}
