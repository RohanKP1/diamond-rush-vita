# Translate java-src/*.java (the game, excluding the dead bundled com/nokia stubs) to C++
# in port/src/game/gen. Re-run after every change to java-src.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$java = Join-Path $root 'env\jdk\bin\java.exe'
$exports = 'tree', 'code', 'api', 'util' | ForEach-Object { '--add-exports'; "jdk.compiler/com.sun.tools.javac.$_=ALL-UNNAMED" }
$srcs = Get-ChildItem (Join-Path $root 'java-src') -Filter *.java | ForEach-Object FullName
& $java @exports (Join-Path $root 'tools\j2cpp\J2Cpp.java') `
    (Join-Path $root 'port\src\game\gen') `
    (Join-Path $root 'env\freej2me\freej2me_plus.jar') `
    GloftDIRU @srcs
if ($LASTEXITCODE -ne 0) { throw "translation failed" }
