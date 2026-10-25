#!/usr/bin/env bash
# Run the desktop build headlessly with a key script and save screenshots (PPM -> PNG).
#   tools/port-probe.sh <out-dir> "W7 N5 W2 S ..."      (same steps as tools/j2me-probe.ps1)
set -euo pipefail
out="$1"; steps="$2"
bin="$HOME/.cache/diamondrush-build/desktop/diamondrush"
mkdir -p "$out"
rm -f "$out"/*.ppm "$out"/*.png
# dummy video + audio drivers: no window, no sound device needed.
# Widescreen unless DR_WIDESCREEN=0 (ignores the saved setting, so runs are reproducible).
DR_WIDESCREEN="${DR_WIDESCREEN:-1}" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0 \
  DR_SCRIPT="$steps Q" DR_SHOTS="$out" timeout 120 "$bin" 2>&1 | tail -40
python3 - "$out" <<'EOF'
import sys, glob
from PIL import Image
for p in sorted(glob.glob(sys.argv[1] + "/*.ppm")):
    im = Image.open(p)
    im.resize((im.width * 2, im.height * 2), Image.NEAREST).save(p[:-4] + ".png")
EOF
ls "$out"
