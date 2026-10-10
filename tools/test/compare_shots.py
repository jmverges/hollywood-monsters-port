#!/usr/bin/env python3
"""Compare two screenshot directories (original vs port) image by image.
Prints the share of differing pixels (with a colour fuzz) per pair, sorted, and writes a side-by-side sheet of
the most different pairs. usage: compare_shots.py <orig-dir> <port-dir> <out.png> [threshold-percent]"""
import os, subprocess, sys
a, b, out = sys.argv[1], sys.argv[2], sys.argv[3]
thr = float(sys.argv[4]) if len(sys.argv) > 4 else 20.0
rows = []
for n in sorted(os.listdir(a)):
    if not n.endswith('.png') or not os.path.exists(os.path.join(b, n)):
        continue
    r = subprocess.run(['compare', '-metric', 'AE', '-fuzz', '12%', os.path.join(a, n), os.path.join(b, n), 'null:'],
                       capture_output=True, text=True)
    try:
        ae = float(r.stderr.split()[0])
    except (ValueError, IndexError):
        continue
    rows.append((100.0 * ae / (640 * 480), n))
rows.sort(reverse=True)
for pct, n in rows:
    print(f'{pct:6.1f}%  {n}')
bad = [n for pct, n in rows if pct >= thr]
if bad:
    args = []
    for n in bad[:24]:
        args += [os.path.join(a, n), os.path.join(b, n)]
    subprocess.run(['montage', *args, '-tile', '4x', '-geometry', '240x180+2+2', '-label', '%f', out])
print(f'{len(bad)} of {len(rows)} pairs differ by >= {thr}%')
