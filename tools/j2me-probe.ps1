# Launch a jar in FreeJ2ME-Plus, send keys, and capture window screenshots.
#   .\tools\j2me-probe.ps1 -Jar build\recompiled.jar -Out build\probe\orig -Keys 'W7','N5','W2','S'
# Keys (per env\freej2me\freej2me_system\freej2me.conf):
#   UP DOWN LEFT RIGHT FIRE LS RS STAR POUND N0..N9 ; plus W<sec> and S (see WinProbe.psm1)
param(
    [Parameter(Mandatory)][string]$Jar,
    [Parameter(Mandatory)][string]$Out,
    [string[]]$Keys = @('W8', 'S'),
    [int]$Width = 128, [int]$Height = 160
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $PSScriptRoot 'WinProbe.psm1') -Force

$keyMap = @{ UP = 0x26; DOWN = 0x28; LEFT = 0x25; RIGHT = 0x27; FIRE = 0x0D
    LS = 0x51; RS = 0x57; STAR = 0x45; POUND = 0x52 }
0..9 | ForEach-Object { $keyMap["N$_"] = 0x60 + $_ }

$jarUrl = ([Uri](Resolve-Path $Jar).Path).AbsoluteUri   # file:///C:/...%20...
$java = Join-Path $root 'env\jdk\bin\java.exe'
$fjDir = Join-Path $root 'env\freej2me'
$fj = Join-Path $fjDir 'freej2me_plus.jar'

# FreeJ2ME-Plus takes the screen size from the per-game config, keyed by MIDlet-Name.
$conf = Join-Path $fjDir 'config\Diamond Rush\game.conf'
if (Test-Path $conf) {
    $lines = Get-Content $conf | Where-Object { $_ -notmatch '^(scrwidth|scrheight|fps):' }
} else {
    New-Item -ItemType Directory -Force (Split-Path $conf) | Out-Null
    $lines = @()
}
# fps:0 = let the game pace itself, like a real phone
($lines + "scrwidth:$Width", "scrheight:$Height", 'fps:0') | Set-Content $conf

# FreeJ2ME re-launches itself unless it already runs with ISO-8859-1.
$p = Start-Process $java -ArgumentList @('-Dfile.encoding=ISO_8859_1', '-jar', "`"$fj`"", $jarUrl) `
    -WorkingDirectory $fjDir -PassThru
try {
    Invoke-ProbeSteps -Process $p -KeyMap $keyMap -Out $Out -Steps $Keys
} finally {
    if (-not $p.HasExited) { $p.Kill() }
}
