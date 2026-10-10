#!/usr/bin/env python3
"""Decode the MSVC incremental-link thunk table (ILT) at the start of .text.

Every function in an incrementally linked executable gets a 5-byte `jmp rel32` slot here, so the table
is also a complete list of function entry points, including callbacks Ghidra never sees being called.
Writes: <out>/ilt.tsv (thunk address -> target) and <out>/ilt_targets.txt (one target per line).
"""
import struct, sys, os

exe, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
d = open(exe, 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]
nsec = struct.unpack_from('<H', d, pe + 6)[0]
optsz = struct.unpack_from('<H', d, pe + 20)[0]
base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
text = None
for i in range(nsec):
    o = pe + 24 + optsz + i * 40
    name = d[o:o + 8].rstrip(b'\0')
    vsize, va, rsize, rptr = struct.unpack_from('<IIII', d, o + 8)
    if name == b'.text':
        text = (base + va, rptr, rsize)
va0, off0, size = text
a = va0
while d[off0 + (a - va0)] != 0xE9 and a < va0 + 64:      # skip padding before the first slot
    a += 1
ilt = {}
while d[off0 + (a - va0)] == 0xE9:
    rel = struct.unpack_from('<i', d, off0 + (a - va0) + 1)[0]
    ilt[a] = a + 5 + rel
    a += 5
with open(os.path.join(out, 'ilt.tsv'), 'w') as f:
    for k, v in ilt.items():
        f.write('%08x\t%08x\n' % (k, v))
with open(os.path.join(out, 'ilt_targets.txt'), 'w') as f:
    for v in sorted(set(ilt.values())):
        f.write('%08x\n' % v)
print('ILT: %d thunks %#x-%#x, %d distinct targets' % (len(ilt), min(ilt), a, len(set(ilt.values()))))
