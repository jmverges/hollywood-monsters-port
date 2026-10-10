#!/usr/bin/env python3
"""Find each chunk of the port's audio inside a recording of the original game.

usage: compare_audio.py <original.wav|.raw> <port.wav|.raw> [chunk-seconds] [quiet-rms]

.raw files are 44.1 kHz stereo signed 16-bit (what SDL's disk audio driver writes).
For every chunk of the port's recording above the quiet threshold, prints where the same samples occur in
the original (normalised cross-correlation) and the offset between the two timelines. A constant offset with
correlation near 1.0 means the port plays the same audio at the same pace; a jump in the offset means a clip
started at a different moment (random choices such as background shouts do this between any two runs).
Needs numpy.
"""
import sys
import wave

import numpy as np

SR = 44100


def load(path):
    if path.lower().endswith('.wav'):
        w = wave.open(path)
        if w.getframerate() != SR or w.getsampwidth() != 2:
            sys.exit('%s: need 44.1 kHz 16-bit' % path)
        data = np.frombuffer(w.readframes(w.getnframes()), np.int16).reshape(-1, w.getnchannels())
    else:
        data = np.frombuffer(open(path, 'rb').read(), np.int16).reshape(-1, 2)
    return data.astype(np.float64).mean(axis=1)


def best_match(seg, ref):
    n = len(seg) + len(ref)
    size = 1 << int(np.ceil(np.log2(n)))
    corr = np.fft.irfft(np.fft.rfft(ref, size) * np.conj(np.fft.rfft(seg, size)), size)[:len(ref) - len(seg)]
    cs = np.concatenate([[0.0], np.cumsum(ref ** 2)])
    energy = np.sqrt(np.maximum(cs[len(seg):len(seg) + len(corr)] - cs[:len(corr)], 0)) + 1e-9
    nc = corr / (energy * np.linalg.norm(seg) + 1e-9)
    i = int(np.argmax(nc))
    return i / SR, float(nc[i])


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    orig, port = load(sys.argv[1]), load(sys.argv[2])
    step = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
    quiet = float(sys.argv[4]) if len(sys.argv) > 4 else 300.0
    n = int(step * SR)
    matched = []
    for start in range(0, len(port) - n, n):
        seg = port[start:start + n]
        rms = float(np.sqrt((seg ** 2).mean()))
        t = start / SR
        if rms < quiet:
            print('port %6.1f s  rms %5d  quiet' % (t, rms))
            continue
        at, c = best_match(seg, orig)
        print('port %6.1f s  rms %5d  -> original %7.2f s  corr %.2f  offset %+.2f' % (t, rms, at, c, at - t))
        matched.append(c)
    if matched:
        good = sum(1 for c in matched if c >= 0.9)
        print('%d of %d audible chunks found in the original with correlation >= 0.9' % (good, len(matched)))


if __name__ == '__main__':
    main()
