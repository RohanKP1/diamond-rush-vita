#!/usr/bin/env bash
# Retranslate-free helper: rebuild the desktop port and run a scripted tour of the screens.
#   tools/port-tour.sh <out-dir> "<steps>"     (see tools/port-probe.sh)
set -euo pipefail
root="/mnt/c/Users/rohan/Documents/Development/diamond rush vita"
bash "$root/build.sh" desktop 2>&1 | grep -E "error|FAILED" || true
bash "$root/tools/port-probe.sh" "$1" "$2" 2>&1 | grep -vE "\.ppm|\.png$" || true
