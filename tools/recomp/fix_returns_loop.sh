#!/bin/bash
# Iterate: export -> strict-void generate -> compile -> find void callees whose return is used -> set EAX return in Ghidra.
cd ~/hollywood-monsters
G=~/.local/opt/ghidra_12.1.4_PUBLIC
S=/tmp/claude-1000/-home-jmverges/296bc8fc-9b82-538b-a57f-1d928d9b3522/scratchpad
for round in 1 2 3 4 5 6 7 8; do
  work/export_all.sh > /dev/null
  rm -rf work/gen_strict
  python3 hm-port/tools/recomp/gen.py --rx work/rx --out work/gen_strict --override hm-port/src/override --strict-void > /dev/null
  (cd work/gen_strict && ls f_*.c | xargs -P16 -I{} sh -c 'gcc -m32 -std=gnu11 -fsyntax-only -I../../hm-port/src/include -I. -w {} > /dev/null 2>{}.err || true'; cat *.err > $S/strict.txt; rm -f *.err)
  python3 - <<'PY'
import re, os
S='/tmp/claude-1000/-home-jmverges/296bc8fc-9b82-538b-a57f-1d928d9b3522/scratchpad'
os.chdir('/home/jmverges/hollywood-monsters/work/gen_strict')
voids=set()
for line in open('hm_funcs.h'):
    m=re.match(r'^void (?:__\w+ )?(FUN_[0-9a-f]{8})\(',line)
    if m: voids.add(m.group(1))
hits=set()
for l in open(S+'/strict.txt'):
    m=re.match(r'^(f_[0-9a-f]+\.c):(\d+):\d+: error: void value not ignored',l)
    if m:
        src=open(m.group(1)).read().split('\n')[int(m.group(2))-1]
        for fn in re.findall(r'\b(FUN_[0-9a-f]{8})\(',src):
            if fn in voids: hits.add(fn)
# callers that read EAX/AL left over by a void call: extraout_EAX / extraout_var
for f in os.listdir('.'):
    if not f.startswith('f_'): continue
    lines=open(f).read().split('\n')
    for i,ln in enumerate(lines):
        if re.search(r'=\s*extraout_(EAX|var)\w*\b|extraout_(EAX|var)\w*\s*[^=\s;]', ln) and 'undefined' not in ln.split('extraout')[0][-12:]:
            for j in range(i, max(i-6,0), -1):
                calls=[c for c in re.findall(r'\b(FUN_[0-9a-f]{8})\(', lines[j]) if c in voids]
                if calls:
                    hits.add(calls[-1]); break
open(S+'/void_returns.txt','w').write('\n'.join(sorted(h[4:] for h in hits))+('\n' if hits else ''))
print('round candidates:', len(hits))
PY
  n=$(grep -c . $S/void_returns.txt)
  echo "round $round: $n functions"
  [ "$n" = "0" ] && break
  cat $S/void_returns.txt >> hm-port/tools/ghidra/types/void_returns_all.txt
  $G/support/analyzeHeadless ghidra-proj HM -process MONSTERS_FX.exe -noanalysis -scriptPath hm-port/tools/ghidra -postScript SetReturns.java $S/void_returns.txt > /dev/null 2>&1
done
