#!/usr/bin/env bash
# Regenerates patches/java-src.patch: the difference between a fresh decompile of the jar and
# java-src/ (Java files only), then checks that it reproduces java-src/ from the fresh decompile.
#   powershell tools/decompile.ps1 -Out <dir>   (a fresh, unpatched decompile)
#   bash tools/make-patch.sh <dir>              (Git Bash or WSL)
set -euo pipefail
cd "$(dirname "$0")/.."
fresh="$1"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

copy_java() {  # copy_java <from> <to>: the .java files, keeping subfolders, with LF line ends
  # (Vineflower writes LF; files edited on Windows may have CRLF, which would make every line differ)
  (cd "$1" && find . -name '*.java' -print0) | while IFS= read -r -d '' f; do
    mkdir -p "$2/$(dirname "$f")"
    tr -d '\r' < "$1/$f" > "$2/$f"
  done
}
copy_java "$fresh" "$work/a"
copy_java java-src "$work/b"
# git diff --no-index exits 1 when the trees differ
(cd "$work" && git -c core.autocrlf=false diff --no-index --no-color -U3 a b || true) |
  sed -e 's#^\(diff --git \)a/a/\(.*\) b/b/#\1a/\2 b/#' -e 's#^--- a/a/#--- a/#' -e 's#^+++ b/b/#+++ b/#' \
  > patches/java-src.patch

# check: fresh decompile + patch == java-src (line endings aside)
cp -r "$work/a" "$work/check"
(cd "$work/check" && git apply --whitespace=nowarn "$OLDPWD/patches/java-src.patch")
diff -r --strip-trailing-cr -q "$work/check" "$work/b"
echo "patches/java-src.patch: $(grep -c '^@@' patches/java-src.patch) hunks, reproduces java-src/"
