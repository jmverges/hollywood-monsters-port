#!/bin/bash
# Run the port headless with a scripted input file and convert screenshots to PNG.
# Crashes print a backtrace from the built-in signal handler; set HM_GDB=1 to run under gdb instead.
# HM_BIN=path runs another build (e.g. build-reloc/hollywood-monsters).
# usage: run_script.sh <script.txt> <shots-dir> [data-dir]
set -u
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
SCRIPT="$(realpath "$1")"
SHOTS="$2"
DATA="${3:-$HERE/../gamedata-fx}"
[ -f "$SCRIPT" ] || { echo "script not found: $SCRIPT"; exit 1; }
mkdir -p "$SHOTS"
SHOTS="$(realpath "$SHOTS")"
DATA="$(realpath "$DATA")"
RUNNER=()
if [ -n "${HM_GDB:-}" ]; then
  RUNNER=(gdb -batch -ex 'handle SIGPIPE nostop noprint' -ex run -ex 'bt 12' --args)
fi
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-offscreen}" SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}"
export SDL_RENDER_DRIVER="${SDL_RENDER_DRIVER:-software}" HM_SCALE="${HM_SCALE:-1}"
export HM_SCRIPT="$SCRIPT" HM_SHOT_DIR="$SHOTS"
"${RUNNER[@]}" "${HM_BIN:-$HERE/build/hollywood-monsters}" "$DATA" 2>&1 \
  | grep -vE 'libEGL|^\[New Thread|^\[Thread .* exited|^$|^Using host|^\[Thread debugging|^\[New process|^\[Inferior'
for f in "$SHOTS"/*.ppm; do [ -f "$f" ] && convert "$f" "${f%.ppm}.png" && rm -f "$f"; done
exit 0
