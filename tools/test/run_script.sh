#!/bin/bash
# Run the port headless with a scripted input file and convert screenshots to PNG.
# usage: run_script.sh <script.txt> <shots-dir> [data-dir] [extra env...]
set -u
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
SCRIPT="$1"; SHOTS="$2"; DATA="${3:-$HERE/../gamedata-fx}"
mkdir -p "$SHOTS"
SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-offscreen} SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-dummy} \
HM_SCRIPT="$SCRIPT" HM_SHOT_DIR="$SHOTS" \
  gdb -batch -ex 'handle SIGPIPE nostop noprint' -ex run -ex 'bt 12' --args "$HERE/build/hollywood-monsters" "$DATA" 2>&1 \
  | grep -vE 'libEGL|^\[New Thread|^\[Thread .* exited|^$|^Using host|^\[Thread debugging|^\[New process|^\[Inferior'
for f in "$SHOTS"/*.ppm; do [ -f "$f" ] && convert "$f" "${f%.ppm}.png" && rm -f "$f"; done
