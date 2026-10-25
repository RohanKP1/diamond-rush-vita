# Decompile the jar into .\java-src\ (or -Out) with Vineflower. Must be run into an empty/new folder.
# The port's changes are then applied with tools\prepare.ps1 (patches\java-src.patch).
param([string]$Out)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$java = Join-Path $root 'env\jdk\bin\java.exe'
$jar = Join-Path $root 'Diamond-Rush_J2ME_EN_v104-Nokia-N6101.jar'
$out = if ($Out) { $Out } else { Join-Path $root 'java-src' }
if ((Test-Path $out) -and (Get-ChildItem $out)) { throw "$out is not empty; delete it first" }
New-Item -ItemType Directory -Force $out | Out-Null
# -dgs: decompile generic signatures, -rsy: hide synthetic, -ind: 4-space indent,
# -lit: keep numeric literals as written (colours stay hex-friendly to compare)
# -ren: rename obfuscated members so overloaded names like `a` (field i a; field Display a) become unique
& $java -jar (Join-Path $root 'env\vineflower.jar') -dgs=1 -rsy=1 -lit=1 -ren=1 "-ind=    " $jar $out
# Vineflower writes a jar of sources when given a jar; unpack it.
$srcJar = Join-Path $out (Split-Path $jar -Leaf)
if (Test-Path $srcJar) {
    $tmp = "$srcJar.zip"; Move-Item $srcJar $tmp -Force
    Expand-Archive $tmp $out -Force; Remove-Item $tmp
}
Get-ChildItem $out -Recurse -Filter *.java | ForEach-Object {
    '{0,7} lines  {1}' -f (Get-Content $_.FullName).Count, $_.FullName.Substring($out.Length + 1)
}
