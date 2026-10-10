#!/bin/bash
# Full re-export from the Ghidra project into work/rx
set -e
G=~/.local/opt/ghidra_12.1.4_PUBLIC
cd ~/hollywood-monsters
rm -rf work/rx
$G/support/analyzeHeadless ghidra-proj HM -process MONSTERS_FX.exe -noanalysis -readOnly -scriptPath hm-port/tools/ghidra \
  -postScript ExportRecomp.java /home/jmverges/hollywood-monsters/work/rx \
  -postScript ExportTypes.java /home/jmverges/hollywood-monsters/work/rx/types.h \
  -postScript ExportSymbols.java /home/jmverges/hollywood-monsters/work/rx/symbols.tsv -max-cpu 8 > work/ghidra_rx.log 2>&1
cp work/ilt.tsv work/rx/ilt.tsv
grep -E 'DONE|types written|symbols=' work/ghidra_rx.log
