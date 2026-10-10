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
