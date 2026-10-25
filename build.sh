#!/usr/bin/env bash
# Build from WSL:  ./build.sh vita | desktop | run-desktop | all
# (First time: tools/prepare.ps1 on Windows makes java-src/ and port/src/game/gen from your jar;
#  after any change to java-src/, run tools/translate.ps1 again. Or all at once: build-vpk.ps1)
set -euo pipefail
cd "$(dirname "$0")"
export VITASDK="${VITASDK:-$HOME/vitasdk}"
export PATH="$VITASDK/bin:$PATH"

# Build dirs live in the Linux filesystem (much faster than /mnt/c); outputs are copied back.
BUILD="${DR_BUILD_DIR:-$HOME/.cache/diamondrush-build}"

resources() {
  [ -d jar ] || unzip -q Diamond-Rush_J2ME_EN_v104-Nokia-N6101.jar -d jar
  # MIDI -> OGG clips (slow; only when missing)
  if [ "$(ls build/gen-res/snd/*.ogg 2>/dev/null | wc -l)" -lt 21 ] || [ ! -f build/gen-res/snd/lengths.txt ]; then
    python3 -I tools/extract_sounds.py jar/snd.f build/gen-res/snd
  fi
}

vita() {
  resources
  # LiveArea images: basic ones from the jar's icon unless port/sce_sys already has some
  [ -f port/sce_sys/icon0.png ] || python3 -I tools/make_default_livearea.py jar port/sce_sys
  cmake -S port -B "$BUILD/vita" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake"
  cmake --build "$BUILD/vita"
  mkdir -p out
  cp "$BUILD/vita/diamondrush.vpk" out/
  echo "-> out/diamondrush.vpk"
}

desktop() {
  resources
  cmake -S port -B "$BUILD/desktop" -G Ninja -DCMAKE_BUILD_TYPE=Debug
  cmake --build "$BUILD/desktop"
}

case "${1:-all}" in
  vita) vita ;;
  desktop) desktop ;;
  run-desktop) desktop; "$BUILD/desktop/diamondrush" ;;
  all) desktop; vita ;;
  *) echo "usage: $0 vita|desktop|run-desktop|all" >&2; exit 2 ;;
esac
