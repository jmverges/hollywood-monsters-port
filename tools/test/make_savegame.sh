#!/bin/bash
# Create SAVEGAME.000 ("autotest") at the first playable scene by playing the intro (~7 minutes).
# usage: make_savegame.sh <data-dir>
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
DATA="$(realpath "$1")"
S="$(mktemp -d)"
python3 - "$S/script.txt" <<'PY'
import sys
lines = [f'{t} click 320 400' for t in range(5000, 400000, 2000)]
lines += ['412000 key 27', '415000 click 80 232', '418000 click 320 60']
lines += [f'{419000 + i * 300} char {c}' for i, c in enumerate('autotest')]
lines += ['423000 key 13', '428000 quit']
open(sys.argv[1], 'w').write('\n'.join(sorted(lines, key=lambda l: int(l.split()[0]))) + '\n')
PY
"$HERE/run_script.sh" "$S/script.txt" "$S/shots" "$DATA" | grep -vE '^shot' || true
rm -rf "$S"
if ls "$DATA" | grep -qi '^savegame\.000$'; then echo "savegame created in $DATA"; else echo "FAILED: no SAVEGAME.000"; exit 1; fi
