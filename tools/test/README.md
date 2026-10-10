# Headless testing

All tools take a **data directory**: a copy of the game folder with `MONSTERS.EXE` and its data files.
Tests write savegames and settings into it, so use a copy, never your only install. When omitted, scripts
default to `<repo>/../gamedata-fx`. Output directories default to siblings of the checkout
(`../work-smoke`, `../work-compare`); pass an explicit path to keep them elsewhere.

## Running the port with scripted input

`run_script.sh <script> <shots-dir> [data-dir]` runs `build/hollywood-monsters` with offscreen video and dummy
audio, replays scripted input, and saves screenshots as PNG. A crash prints a backtrace from the signal handler
in `host_main.c`; function names are `FUN_<original address>`, matching `src/gen/f_<address>.c`. Set `HM_GDB=1`
to run under gdb instead (slow, only for short scripts).

Script format: one event per line, `#` comments. Times are milliseconds since start, or `+N` relative to the
previous line. Coordinates are game pixels (640x480). The same format drives the original (below).

| Command | Arguments | Effect |
|---|---|---|
| `move` | x y | move the mouse |
| `click`, `rclick` | x y | left or right click (move, down, up) |
| `down`, `up`, `rdown`, `rup` | x y | press or release a button (hold the right button for the verb bar) |
| `key` | vk | Windows virtual-key code (27 Escape, 13 Enter, 32 Space) |
| `char` | c | type a character |
| `shot` | name | save the last submitted frame as `<shots-dir>/<name>.png` |
| `poke8`, `poke16`, `poke32` | addr value | write game memory (debug warps, e.g. room id `0x6ca1be`) |
| `dump`, `dumpp` | addr len | write `len` bytes at `addr` (`dumpp`: at the pointer stored at `addr`) to a file |
| `trap` | | stop in the debugger (port only, with `HM_GDB=1`) |
| `quit` | | exit |

```
4000  shot   intro_logo
6000  key    27
+3000 click  172 232           # menu: Cargar
+3000 click  320 60            # first savegame slot
+6000 rdown  190 400           # hold the right button over Junior: the verb bar opens ...
+800  move   25 462            # ... pick "Hablar" ...
+800  rup    25 462            # ... and release on the bar
+700  click  190 400           # left click on Junior: Sue walks over and talks
60000 quit
```

The game draws its own pointer and moves the real one with `SetCursorPos` every frame, clamping it to where
it is allowed to be (the port's scripted pointer follows those warps, as Windows does). While the verb bar is
open the pointer is held in a narrow band, so the verb depends on the x coordinate of the move, not on where the
verb is drawn:

| Verb | Hablar | Coger | Mirar | Usar | Abrir | Cerrar | Dar |
|---|---|---|---|---|---|---|---|
| x on `move`/`rup` (y 462) | 25 | 35 | 45 | 55 | 65 | 75 | 85 |

Releasing above the bar cancels. The selected verb applies to the next left click.

Environment: `SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE=out.raw` captures audio (44.1 kHz stereo S16),
`HM_TRACE_FILES=1` traces file access, `HM_DEBUG=1` traces the Win32 layer.

`scripts/` holds scripts that have been checked against the original: `talk_junior.txt` and
`verbs_first_scene.txt` start from the savegame below and exercise the verb bar, a conversation and object
actions in the first scene.

## Savegame and smoke test

* `make_savegame.sh <data-dir>` plays the intro (~7 min) and saves "autotest" in slot 0. Most scripts start
  by loading it (Escape, Cargar, first slot).
* `smoke.sh <data-dir> [out-dir]` warps through every room of chapters 1–8 in parallel (room ids are read from
  the generated `WinMain`) with random clicks, and fails if any run crashes. Run it after changing the
  pipeline, the generator or the platform layer.

## Comparing with the original game

The original `MONSTERS.EXE` runs under Wine as a reference. Setup on Debian/Ubuntu:

```
sudo dpkg --add-architecture i386 && sudo apt update
sudo apt install wine wine32:i386 xvfb xdotool x11-apps imagemagick gcc-mingw-w64-i686 python3-numpy
sudo modprobe snd-dummy                 # silent real-time sound card, keeps dialogue pace
sudo usermod -aG audio "$USER"          # access to the card (scripts use sg until you log in again)
```

The Wine prefix (`WINEPREFIX`, default `~/.wine-hm`) must be 32-bit. `compare_tour.sh` creates it if missing;
by hand: `WINEARCH=win32 WINEPREFIX=~/.wine-hm wineboot -i`. Instances running at the same time need separate
prefixes and display numbers; a shared prefix gives black screenshots.

`run_original.py <script> <shots-dir> <data-dir> [display]` plays a script on the original on a 640x480 Xvfb
display: input through xdotool, screenshots through xwd. It sets Wine's `renderer=gdi` and a 640x480 virtual
desktop in the prefix, and points ALSA at the snd-dummy card (or at the null device when that card is absent,
which makes speech-driven dialogues finish instantly). `poke` and `dump` go through `hmpoke.exe` and
`hmpeek.exe` (`WriteProcessMemory`/`ReadProcessMemory`), built with MinGW on first use.

```
python3 tools/test/run_original.py work/cmp1.txt work/orig/shots /path/to/data-copy-1
tools/test/run_script.sh            work/cmp1.txt work/port/shots /path/to/data-copy-2
python3 tools/test/compare_shots.py work/orig/shots work/port/shots work/cmp.png
```

Timing differs between runs (animations, timers, Wine start-up), so compare states, not exact frames.
`compare_shots.py` lists per-image differences and builds a sheet of the worst pairs.

`compare_tour.sh <data-dir> [out-dir]` does the room-by-room comparison: it warps through every room in the
original (one prefix copy per chapter, copied from `WINEPREFIX_BASE`) and in the port, with no clicks, and
prints per-room pixel differences plus a sheet of the worst pairs.

## Comparing audio

Load the ALSA loopback card, play the original into it, and record the other end:

```
sudo modprobe snd-aloop
arecord -D plughw:Loopback,1,0 -f S16_LE -r 44100 -c 2 -d 84 orig.wav &
HM_ALSA_CARD=Loopback python3 tools/test/run_original.py script.txt orig-shots data-copy-1
SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE=$PWD/port.raw tools/test/run_script.sh script.txt port-shots data-copy-2
python3 tools/test/compare_audio.py orig.wav port.raw
```

`compare_audio.py` finds each second of the port's audio in the original's recording and prints the
correlation and the offset between the two timelines. Music and speech clips should match with correlation
near 1.0 at a constant offset (the offset is Wine's start-up time). Clips the game picks at random, such as the
newsboy's shouts in the intro, start at different moments in every run, so their offset jumps.
