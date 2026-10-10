#!/bin/bash
# Find functions the decompiler declares void although callers use their EAX value, mark them in Ghidra,
# and repeat until none are left. Appends the addresses to tools/ghidra/types/void_returns_all.txt.
# Needs an existing Ghidra project (run pipeline.sh first). Usage: find_void_returns.sh [rounds]
set -euo pipefail
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
GHIDRA_HOME="${GHIDRA_HOME:-$HOME/.local/opt/ghidra_12.1.4_PUBLIC}"
WORK="$(realpath "${WORK:-$REPO/../work-recomp}")"
ROUNDS="${1:-8}"
STRICT="$WORK/gen_strict"
EXE="$(cat "$WORK/exe_path.txt")"
NAME="$(basename "$EXE")"
for round in $(seq 1 "$ROUNDS"); do
  STEP=export "$REPO/tools/recomp/pipeline.sh" "$EXE" > /dev/null
  rm -rf "$STRICT"
  python3 "$REPO/tools/recomp/gen.py" --rx "$WORK/rx" --out "$STRICT" --override "$REPO/src/override" --strict-void > /dev/null
  (cd "$STRICT" && ls f_*.c | xargs -P"$(nproc)" -I{} sh -c 'gcc -m32 -std=gnu11 -fsyntax-only -I'"$REPO"'/src/include -I. -w {} > /dev/null 2>{}.err || true'; cat ./*.err > errors.txt; rm -f ./*.err)
  python3 - "$STRICT" > "$WORK/void_returns.txt" <<'PY'
import re, os, sys
d = sys.argv[1]
voids = set()
for line in open(os.path.join(d, 'hm_funcs.h')):
    m = re.match(r'^void (?:__\w+ )?(FUN_[0-9a-f]{8})\(', line)
    if m: voids.add(m.group(1))
hits = set()
for l in open(os.path.join(d, 'errors.txt')):
    m = re.match(r'^(f_[0-9a-f]+\.c):(\d+):\d+: error: void value not ignored', l)
    if m:
        src = open(os.path.join(d, m.group(1))).read().split('\n')[int(m.group(2)) - 1]
        hits.update(fn for fn in re.findall(r'\b(FUN_[0-9a-f]{8})\(', src) if fn in voids)
for f in os.listdir(d):
    if not f.startswith('f_'): continue
    lines = open(os.path.join(d, f)).read().split('\n')
    for i, ln in enumerate(lines):
        if re.search(r'=\s*extraout_(EAX|var)\w*\b|extraout_(EAX|var)\w*\s*[^=\s;]', ln):
            for j in range(i, max(i - 6, 0), -1):
                calls = [c for c in re.findall(r'\b(FUN_[0-9a-f]{8})\(', lines[j]) if c in voids]
                if calls:
                    hits.add(calls[-1]); break
for h in sorted(hits): print(h[4:])
PY
  n=$(grep -c . "$WORK/void_returns.txt" || true)
  echo "round $round: $n functions"
  [ "$n" = "0" ] && break
  cat "$WORK/void_returns.txt" >> "$REPO/tools/ghidra/types/void_returns_all.txt"
  sort -u -o "$REPO/tools/ghidra/types/void_returns_all.txt" "$REPO/tools/ghidra/types/void_returns_all.txt"
  "$GHIDRA_HOME/support/analyzeHeadless" "$WORK/ghidra-proj" HM -process "$NAME" -noanalysis \
    -scriptPath "$REPO/tools/ghidra" -postScript SetReturns.java "$WORK/void_returns.txt" > /dev/null 2>&1
done
