#!/bin/bash
# Full recompilation pipeline: original MONSTERS.EXE (FX Interactive edition) -> compilable C in src/gen.
#
#   tools/recomp/pipeline.sh /path/to/MONSTERS.EXE
#
# Environment:
#   GHIDRA_HOME   Ghidra install (default: ~/.local/opt/ghidra_12.1.4_PUBLIC)
#   WORK          scratch directory for the Ghidra project and exports (default: <repo>/../work-recomp)
#   STEP          start from a given step: import | types | export | gen   (default: import)
#
# Steps:
#   import  Ghidra import + auto-analysis, create every function listed in the incremental-link table
#   types   DirectX interfaces, CRT names/FPU signatures, Decompiler Parameter ID, EAX returns, stack purge of
#           functions that end in a tail jump (FixTailPurge.java)
#   export  decompile every function (C + per-access global types), data types, symbols
#   gen     generate src/gen/*.c (tools/recomp/gen.py)
set -euo pipefail
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="$(realpath "${1:?usage: pipeline.sh /path/to/MONSTERS.EXE}")"
GHIDRA_HOME="${GHIDRA_HOME:-$HOME/.local/opt/ghidra_12.1.4_PUBLIC}"
WORK="${WORK:-$REPO/../work-recomp}"
STEP="${STEP:-import}"
AH="$GHIDRA_HOME/support/analyzeHeadless"
SCRIPTS="$REPO/tools/ghidra"
TYPES="$SCRIPTS/types"
mkdir -p "$WORK"
WORK="$(realpath "$WORK")"
PROJ="$WORK/ghidra-proj"
RX="$WORK/rx"
NAME="$(basename "$EXE")"
log() { echo "[pipeline] $*"; }

expected_sha="2f5fda8e5b65e915"
actual_sha="$(sha256sum "$EXE" | cut -c1-16)"
if [ "$actual_sha" != "$expected_sha" ]; then
  log "WARNING: $EXE sha256 prefix $actual_sha differs from the FX Interactive 2010 build ($expected_sha);"
  log "         addresses in tools/ghidra/types/*.tsv and src/override/ will not match."
fi

echo "$EXE" > "$WORK/exe_path.txt"
python3 "$REPO/tools/recomp/ilt.py" "$EXE" "$WORK"

run() { "$AH" "$PROJ" HM "$@" -scriptPath "$SCRIPTS" -max-cpu "$(nproc)" >> "$WORK/ghidra.log" 2>&1; }

case "$STEP" in import)
  log "import + analysis"
  rm -rf "$PROJ"; mkdir -p "$PROJ"
  run -import "$EXE"
  log "create functions for all incremental-link targets + re-analyse"
  run -process "$NAME" -preScript CreateFuncs.java "$WORK/ilt_targets.txt"
  ;& types)
  log "types: DirectX, CRT helpers, parameter ID, EAX returns"
  run -process "$NAME" -noanalysis -postScript ApplyTypes.java "$TYPES/dx_min.h" "$TYPES/globals_dx.tsv" -postScript SetSigs.java
  run -process "$NAME" -noanalysis -postScript RunParamId.java
  run -process "$NAME" -noanalysis -postScript SetReturns.java "$TYPES/void_returns_all.txt"
  run -process "$NAME" -noanalysis -postScript FixTailPurge.java
  ;& export)
  log "export"
  rm -rf "$RX"
  run -process "$NAME" -noanalysis -readOnly \
      -postScript ExportRecomp.java "$RX" \
      -postScript ExportTypes.java "$RX/types.h" \
      -postScript ExportSymbols.java "$RX/symbols.tsv"
  cp "$WORK/ilt.tsv" "$RX/ilt.tsv"
  grep -E 'DONE functions|types written|symbols=' "$WORK/ghidra.log" | tail -3
  ;& gen)
  log "generate C"
  rm -rf "$REPO/src/gen"
  python3 "$REPO/tools/recomp/gen.py" --rx "$RX" --out "$REPO/src/gen" --override "$REPO/src/override" --exe "$EXE"
  ;;
esac
log "done. Build with: cmake -S $REPO -B $REPO/build -G Ninja && ninja -C $REPO/build"
