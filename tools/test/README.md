# Headless testing

`run_script.sh <script> <shots-dir> [data-dir]` runs the port under gdb with offscreen video and dummy audio,
replays scripted input, saves screenshots as PNG, and prints a backtrace if the game crashes.

Script format (one event per line, `#` comments). Times are milliseconds since start, or `+N` relative to
the previous line. Coordinates are in game pixels (640x480).

```
4000  shot   intro_logo        # save the last presented frame as <shots-dir>/intro_logo.png
6000  key    27                # Windows virtual-key code (27 = Escape, 13 = Enter, 32 = Space)
+1500 click  320 400           # left click (move + down + up)
+1500 rclick 320 400           # right click
+500  move   100 200
+500  char   a
60000 quit
```

Useful environment variables: `SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE=out.raw` to capture audio
(44.1 kHz stereo S16), `HM_TRACE_FILES=1`, `HM_DEBUG=1`.
Without gdb, crashes still print a backtrace (signal handler in `host_main.c`); function names are
`FUN_<original address>`, matching `src/gen/f_<address>.c`.

## Comparing with the original game

`run_original.py <script> <shots-dir> <data-dir>` plays the same script on the **original** `MONSTERS.EXE`
under Wine (32-bit prefix) on a 640x480 Xvfb display, driving input with xdotool and taking screenshots with
xwd. Wine needs `renderer=gdi` (set automatically) and an ALSA device; a null device is configured for the
process. Put the two screenshot sets side by side to check a behaviour against the original:

```
python3 tools/test/run_original.py work/cmp1.txt work/orig/shots /path/to/data-copy-1
tools/test/run_script.sh            work/cmp1.txt work/port/shots /path/to/data-copy-2
```

Timing differs between runs (animations, timers), so compare states, not pixels.
`poke` commands are applied with `tools/test/hmpoke.exe` (build it with
`i686-w64-mingw32-gcc -O2 -o tools/test/hmpoke.exe tools/test/hmpoke.c`), which writes into the
original's memory through `WriteProcessMemory` under Wine.

## Savegame and smoke test

* `make_savegame.sh <data-dir>` plays the intro (~7 min) and saves "autotest" in slot 0. Most scripts start
  by loading it (`Esc`, `Cargar`, first slot).
* `smoke.sh <data-dir>` warps through every room of chapters 1–8 in parallel (room ids are read from the
  generated `WinMain`) with random clicks, and fails if any run crashes. Use it after changing the pipeline.

Caveat: with the null audio device, the original's speech streams finish almost instantly under Wine, so
speech-driven dialogues run much faster than in the port (which plays them in real time). Compare scenes
without dialogue, or wait until the port's dialogue has finished. `compare_shots.py orig/ port/ out.png`
lists per-image differences and builds a sheet of the worst pairs.

`compare_tour.sh <data-dir>` does the whole room-by-room comparison: it warps through every room in the
original (Wine, one prefix copy per chapter, copied from `WINEPREFIX_BASE`) and in the port, with no clicks,
and prints per-room pixel differences plus a sheet of the worst pairs. Load `snd-dummy`
(`sudo modprobe snd-dummy`, user in group `audio`) so the original's dialogues keep their real pace.
