#!/usr/bin/env bash
# Recompile java-src/ (the patched game) into build/recompiled.jar, for testing in FreeJ2ME.
# Run from Git Bash on Windows (uses the portable JDK in env/).
set -euo pipefail
cd "$(dirname "$0")/.."
rm -rf build/javac/classes build/recompiled
env/jdk/bin/javac.exe -nowarn --release 8 -encoding UTF-8 -cp env/freej2me/freej2me_plus.jar \
  -d build/javac/classes java-src/*.java
mkdir -p build/recompiled
cp -r build/javac/classes/. build/recompiled/
(cd jar && find . -type f ! -name '*.class' ! -path './META-INF/*' ! -path './com/*' -exec cp --parents {} ../build/recompiled/ \;)
env/jdk/bin/jar.exe cfm build/recompiled.jar jar/META-INF/MANIFEST.MF -C build/recompiled .
echo "-> build/recompiled.jar"
