#!/usr/bin/env python3
"""Run the ORIGINAL MONSTERS.EXE under Wine on a virtual X display with the same script format as the port
(tools/test/README.md), to compare behaviour. Needs: wine (32-bit), Xvfb, xdotool, xwd, ImageMagick.

usage: run_original.py <script.txt> <shots-dir> <data-dir-with-MONSTERS.EXE> [display-number]

The game runs fullscreen 640x480 inside a 640x480 Xvfb screen, so script coordinates are screen coordinates.
Audio: if the snd-dummy kernel module is loaded (sudo modprobe snd-dummy; user in group "audio"), Wine plays
into that silent real-time card, so dialogues keep their pace; otherwise ALSA's null device is used.
"""
import os, subprocess, sys, time

script, shots, data = sys.argv[1], sys.argv[2], sys.argv[3]
disp = ':' + (sys.argv[4] if len(sys.argv) > 4 else '79')
os.makedirs(shots, exist_ok=True)
shots = os.path.abspath(shots)
work = os.path.abspath(os.path.join(shots, '..'))
env = dict(os.environ, DISPLAY=disp, WINEDEBUG='-all', WINEARCH='win32',
           WINEPREFIX=os.environ.get('WINEPREFIX', os.path.expanduser('~/.wine-hm')))
# Audio: prefer the snd-dummy card (real-time clock, silent) so speech-driven dialogues keep their real pace;
# fall back to ALSA's null device, which consumes audio instantly. Never use a real card.
asound = os.path.join(work, 'asound_hm.conf')
dummy = 'Dummy' in open('/proc/asound/cards').read() if os.path.exists('/proc/asound/cards') else False
dev = '{ type hw card Dummy }' if dummy else '{ type null }'
ctl = '{ type hw card Dummy }' if dummy else '{ type hw card 0 }'
with open('/usr/share/alsa/alsa.conf') as f, open(asound, 'w') as o:
    o.write(f.read() + '\npcm.!default %s\nctl.!default %s\n' % (dev, ctl))
env['ALSA_CONFIG_PATH'] = asound
print('audio device:', 'snd-dummy' if dummy else 'null (dialogues will run fast)')

xvfb = subprocess.Popen(['Xvfb', disp, '-screen', '0', '640x480x24'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(2)
for k, v in (('HKCU\\Software\\Wine\\Direct3D', ('renderer', 'gdi')),
             ('HKCU\\Software\\Wine\\Explorer', ('Desktop', 'Default')),
             ('HKCU\\Software\\Wine\\Explorer\\Desktops', ('Default', '640x480'))):
    subprocess.run(['wine', 'reg', 'add', k, '/v', v[0], '/d', v[1], '/f'], env=env, capture_output=True)
exe = next(n for n in os.listdir(data) if n.lower() == 'monsters.exe')
cmd = ['wine', exe]
import grp, shlex
try:
    audio_gid = grp.getgrnam('audio').gr_gid
    if dummy and audio_gid not in os.getgroups():
        cmd = ['sg', 'audio', '-c', shlex.join(cmd)]     # group added but not active in this session yet
except KeyError:
    pass
game = subprocess.Popen(cmd, cwd=data, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
t0 = time.time()

def x(*a):
    subprocess.run(['xdotool', *a], env=env, capture_output=True)

events = []
last = 0
for line in open(script):
    p = line.split()
    if not p or p[0].startswith('#'):
        continue
    t = last + int(p[0][1:]) if p[0].startswith('+') else int(p[0])
    last = t
    events.append((t, p[1], p[2:]))
for t, cmd, args in events:
    delay = t / 1000.0 - (time.time() - t0)
    if delay > 0:
        time.sleep(delay)
    if cmd in ('move', 'click', 'rclick', 'down', 'up', 'rdown', 'rup'):
        x('mousemove', args[0], args[1])
        btn = '3' if cmd.startswith('r') else '1'
        if cmd in ('click', 'rclick'):
            x('click', btn)
        elif cmd.endswith('down'):
            x('mousedown', btn)
        elif cmd.endswith('up'):
            x('mouseup', btn)
    elif cmd == 'key':
        names = {27: 'Escape', 13: 'Return', 32: 'space', 8: 'BackSpace'}
        x('key', names.get(int(args[0]), chr(int(args[0]))))
    elif cmd == 'char':
        x('type', args[0])
    elif cmd == 'shot':
        out = os.path.join(shots, args[0] + '.png')
        xwd = subprocess.run(['xwd', '-root', '-silent'], env=env, capture_output=True)
        subprocess.run(['convert', 'xwd:-', out], input=xwd.stdout)
        print('shot', out)
    elif cmd.startswith('poke'):
        poker = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'hmpoke.exe')
        r = subprocess.run(['wine', poker, exe, args[0], cmd[4:], args[1]], env=env, capture_output=True, text=True)
        print(r.stdout.strip() or r.stderr.strip())
    elif cmd in ('dump', 'dumpp'):
        peeker = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'hmpeek.exe')
        out = os.path.join(shots, 'dump_%08x_%d.bin' % (int(args[0], 0), t))
        winpath = 'Z:' + out.replace('/', '\\')
        where = ('*' if cmd == 'dumpp' else '') + args[0]
        r = subprocess.run(['wine', peeker, exe, where, args[1], winpath], env=env, capture_output=True, text=True)
        print(r.stdout.strip() or r.stderr.strip())
    elif cmd == 'quit':
        break
subprocess.run(['wineserver', '-k'], env=env)
game.wait(timeout=10)
xvfb.terminate()
