#!/bin/bash
# Warp through every room in both the ORIGINAL (Wine) and the port, with no clicks, and compare screenshots.
# Needs: SAVEGAME.000 in the data dir, a 32-bit Wine prefix (WINEPREFIX_BASE, copied per chapter), Xvfb,
# xdotool, tools/test/hmpoke.exe and, for realistic dialogue timing, the snd-dummy kernel module.
# usage: compare_tour.sh <data-dir> [out-dir]
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
DATA="$(realpath "$1")"
OUT="$(realpath -m "${2:-$REPO/../work-compare}")"
BASEPFX="${WINEPREFIX_BASE:-$HOME/.wine-hm}"
mkdir -p "$OUT"
ROOMS=$(grep -oE 'DAT_006ca1be == (0x[0-9a-f]+|[0-9]+)' "$REPO/src/gen/f_0040cd20.c" | awk '{print $3}' | sort -u | tr '\n' ' ')
for ch in 1 2 3 4 5 6 7 8; do
  d="$OUT/ch$ch"; rm -rf "$d"; mkdir -p "$d"
  python3 - "$ch" "$d" "$ROOMS" <<'PY'
import sys
ch, d = int(sys.argv[1]), sys.argv[2]
rooms = sorted({int(x, 0) for x in sys.argv[3].split()})
mine = [r for r in rooms if r // 1000 == ch and r % 10 == 0 and r % 1000 != 0]
lines = ['6000 key 27', '9000 click 172 232', '12000 click 320 60', f'18000 poke16 0x6ca1be {ch*1000}']
t = 32000
for room in mine:
    lines += [f'{t} poke16 0x6ca1be {room}', f'{t+8000} shot room_{room:04x}']
    t += 12000
lines.append(f'{t+500} quit')
open(f'{d}/script.txt', 'w').write('\n'.join(lines) + '\n')
PY
  cp -r "$DATA" "$d/odata"; cp -r "$DATA" "$d/pdata"
  [ -d "$d/wineprefix" ] || cp -r "$BASEPFX" "$d/wineprefix"
  ( WINEPREFIX="$d/wineprefix" python3 "$HERE/run_original.py" "$d/script.txt" "$d/orig" "$d/odata" $((80 + ch)) > "$d/orig.log" 2>&1 ) &
  ( "$HERE/run_script.sh" "$d/script.txt" "$d/port" "$d/pdata" > "$d/port.log" 2>&1 ) &
done
wait
mkdir -p "$OUT/all/orig" "$OUT/all/port"
for ch in 1 2 3 4 5 6 7 8; do cp "$OUT/ch$ch"/orig/*.png "$OUT/all/orig/"; cp "$OUT/ch$ch"/port/*.png "$OUT/all/port/"; done
python3 "$HERE/compare_shots.py" "$OUT/all/orig" "$OUT/all/port" "$OUT/worst.png" 15
