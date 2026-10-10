#!/bin/bash
# Smoke test: warp through every room of every chapter with a few random clicks, in parallel, and fail on a
# crash. Needs SAVEGAME.000 in the data directory (tools/test/make_savegame.sh). ~6 minutes on 8 cores.
# usage: smoke.sh <data-dir> [out-dir]
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
DATA="$(realpath "$1")"
OUT="$(realpath -m "${2:-$REPO/../work-smoke}")"
mkdir -p "$OUT"
ls "$DATA" | grep -qi '^savegame\.000$' || { echo "no SAVEGAME.000 in $DATA (run make_savegame.sh)"; exit 2; }
# room ids from the WinMain dispatcher in the generated code
ROOMS=$(grep -oE 'DAT_006ca1be == (0x[0-9a-f]+|[0-9]+)' "$REPO/src/gen/f_0040cd20.c" | awk '{print $3}' | sort -u | tr '\n' ' ')
pids=()
for ch in 1 2 3 4 5 6 7 8; do
  d="$OUT/ch$ch"; rm -rf "$d"; mkdir -p "$d"; cp -r "$DATA" "$d/data"
  python3 - "$ch" "$d" "$ROOMS" <<'PY'
import random, sys
ch, d = int(sys.argv[1]), sys.argv[2]
rooms = sorted({int(x, 0) for x in sys.argv[3].split()})
mine = [r for r in rooms if r // 1000 == ch and r % 10 == 0 and r % 1000 != 0]
rnd = random.Random(ch)
lines = ['6000 key 27', '9000 click 172 232', '12000 click 320 60', f'18000 poke16 0x6ca1be {ch*1000}']
t = 32000
for room in mine:
    lines.append(f'{t} poke16 0x6ca1be {room}')
    lines.append(f'{t+8000} shot room_{room:04x}')
    tt = t + 8500
    while tt < t + 15000:
        lines.append(f'{tt} {"click" if rnd.random() < 0.7 else "rclick"} {rnd.randint(5, 635)} {rnd.randint(5, 440)}')
        tt += rnd.randint(700, 1800)
    t += 16000
lines.append(f'{t+1000} quit')
open(f'{d}/script.txt', 'w').write('\n'.join(lines) + '\n')
PY
  ( "$HERE/run_script.sh" "$d/script.txt" "$d/shots" "$d/data" > "$d/log.txt" 2>&1 ) &
  pids+=($!)
done
wait "${pids[@]}"
fail=0
for ch in 1 2 3 4 5 6 7 8; do
  log="$OUT/ch$ch/log.txt"
  if grep -q 'quit at' "$log" && ! grep -qE '\*\*\* signal|exception' "$log"; then echo "chapter $ch: ok ($(ls "$OUT/ch$ch/shots" | wc -l) rooms)"
  else echo "chapter $ch: FAILED"; grep -A12 -E '\*\*\* signal|exception' "$log" | head -14; fail=1; fi
done
exit $fail
