#!/bin/bash
# Regenerate C from work/rx and syntax-check everything; errors summary to stdout.
cd ~/hollywood-monsters
rm -rf hm-port/src/gen
python3 hm-port/tools/recomp/gen.py --rx work/rx --out hm-port/src/gen --override hm-port/src/override || exit 1
cd hm-port
S=/tmp/claude-1000/-home-jmverges/296bc8fc-9b82-538b-a57f-1d928d9b3522/scratchpad
ls src/gen/f_*.c | xargs -P16 -I{} sh -c 'gcc -m32 -std=gnu11 -fsyntax-only -Isrc/include -Isrc/gen -w {} > /dev/null 2>{}.err || true'
cat src/gen/*.err > $S/errs.txt; rm -f src/gen/*.err
echo "errors: $(grep -c 'error:' $S/errs.txt)"
grep 'error:' $S/errs.txt | sed -E 's/^[^:]+:[0-9]+:[0-9]+: //; s/‘[^’]*’/X/g' | sort | uniq -c | sort -rn | head -15
