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
  # The artwork is the repository's own (port/res, port/sce_sys), used as is: nothing is generated
  # here. Check it is all there, and that the LiveArea images are 8-bit palette PNGs (otherwise
  # the Vita refuses to install: VitaShell error 0x8010113D).
  python3 -I - <<'EOF'
import sys
from PIL import Image
bad = []
for p in ["port/res/port_title_wide.png", "port/res/port_btn.png"] + \
         [f"port/res/port_banner_{i}.png" for i in range(3)] + \
         ["port/sce_sys/icon0.png", "port/sce_sys/livearea/contents/bg.png",
          "port/sce_sys/livearea/contents/startup.png"]:
    try:
        if p.startswith("port/sce_sys/") and Image.open(p).mode != "P":
            bad.append(f"{p}: not an 8-bit palette PNG")
    except FileNotFoundError:
        bad.append(f"{p}: missing")
if bad:
    sys.exit("artwork:\n  " + "\n  ".join(bad))
EOF
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
