#!/usr/bin/env bash
# One-time WSL (Ubuntu) setup: host packages + VitaSDK.
#
#   Step 1 (needs your sudo password, run once yourself):
#     bash tools/setup-wsl.sh deps
#   Step 2 (no sudo; installs VitaSDK into ~/vitasdk, which must not exist yet):
#     bash tools/setup-wsl.sh vitasdk
#   Or both: bash tools/setup-wsl.sh all
#
set -euo pipefail

VITASDK="${VITASDK:-$HOME/vitasdk}"

deps() {
  sudo apt-get update
  # build-essential/cmake/ninja: both builds; libsdl2-*: desktop test build (shown via WSLg)
  # fluidsynth + fluid-soundfont-gm: pre-render the game's MIDI to OGG at build time
  sudo apt-get install -y \
    build-essential cmake ninja-build pkg-config git curl xz-utils bzip2 unzip \
    python3 python3-pil python3-numpy \
    libsdl2-dev libsdl2-mixer-dev \
    fluidsynth fluid-soundfont-gm
}

vitasdk() {
  local vdpm="$HOME/.cache/vdpm"
  if [ ! -d "$vdpm" ]; then
    git clone --depth 1 https://github.com/vitasdk/vdpm "$vdpm"
  fi
  (
    cd "$vdpm"
    export VITASDK PATH="$VITASDK/bin:$PATH"
    # bootstrap requires that $VITASDK does not exist yet
    if [ ! -x "$VITASDK/bin/vdpm" ]; then
      ./bootstrap-vitasdk.sh
    fi
  )
  # The installed client (not the git checkout's scripts) manages packages.
  # Only what this port links: SDL2 + mixer (OGG via libvorbis) and their deps.
  VITASDK="$VITASDK" "$VITASDK/bin/vdpm" install --force \
    zlib libpng libogg libvorbis flac libmikmod mpg123 opus opusfile sdl2 sdl2_mixer
  if ! grep -q 'VITASDK=' "$HOME/.bashrc"; then
    {
      echo "export VITASDK=$VITASDK"
      echo 'export PATH="$VITASDK/bin:$PATH"'
    } >> "$HOME/.bashrc"
  fi
  "$VITASDK/bin/arm-vita-eabi-gcc" --version | head -1
  echo "VitaSDK installed at $VITASDK"
}

case "${1:-}" in
  deps) deps ;;
  vitasdk) vitasdk ;;
  all) deps; vitasdk ;;
  *) echo "usage: $0 deps|vitasdk|all" >&2; exit 2 ;;
esac
