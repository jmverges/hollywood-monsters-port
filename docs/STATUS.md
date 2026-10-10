# Status

Last updated: 2026-10-10. Everything below was checked with headless scripted runs (`tools/test/`) on
Ubuntu 24.04 (x86-64 host, i386 build), FX Interactive 2010 build of `MONSTERS.EXE`.

## Verified

| Area | Result |
|---|---|
| Pipeline | `pipeline.sh` from a clean clone reproduces the same sources (3,869 generated + 4 overrides) in ~3 min |
| Build | 0 compile errors; Linux i386 binary; Windows i386 `.exe` cross-compiles and links (MinGW) |
| Boot | DirectDraw/DirectSound initialisation, resource files, streaming threads, timers |
| Intro | Plays from the FX/Péndulo logos through the credits and the drive-in sequence (~7 min) without crashing |
| Gameplay | Reaches the first playable scene (Sue outside the mansion), verb line under the cursor |
| Menu | Escape opens the options screen with a live thumbnail |
| Save / load | Saving writes `SAVEGAME.000` with the typed description; a fresh start loads it from the menu |
| Walking | Click-to-walk with screen scrolling; hotspot labels ("Ir a escalera", "Mirar escalera"); right click toggles the verb; walking up the stairs changes room to the terrace |
| Windows | The MinGW build runs under Wine 9.0 headless: intro logo, menu, loading the savegame |
| All chapters | Warping to the start of chapters 1–8 (room id 1000…8000) loads each one; 5-minute random-click runs in every chapter, and four 10-minute runs in chapter 1, finished without a crash |
| Speech | Measured, not heard: in the intro dialogue the output has speech-like modulation (envelope variation 0.75–0.79 vs 0.22 for music, 21–26 % pauses vs 1 %) |
| Against the original | The original `MONSTERS.EXE` runs under Wine/Xvfb with the same scripts (`run_original.py`, including `poke` warps); garden, terrace, menu and the chapter 2 title pan match the port (the black band at the right of that pan is in the original too) |
| Room tour vs original | Warping through all 77 rooms, 8 s after each warp: 44 rooms match within 1 % of pixels, 12 within 15 %, 21 differ. The differences checked so far come from chapter-opening dialogues: in the port they last as long as the speech (chapter 4: ~100 s of continuous speech), while the original under Wine with a null audio device skips through them, so later warps land in different rooms. Delaying the warps past the dialogue makes chapter 4 rooms match |
| Audio | Music streams from `RESOURCE.M09`; sound effects play (measured output levels) |

## Not verified yet

* Speech by ear (only measured so far).
* Inventory, dialogue trees, puzzles, chapter changes, end game.
* The Windows build on real Windows (only Wine so far).
* Long sessions beyond ~7 minutes.

## Known issues / open work

* The 1,000+ functions only reached through rooms not visited yet are compiled but untested; expect more
  decompiler pitfalls there (see the table in ARCHITECTURE.md for the kinds found so far).
* The build is 32-bit only (see PORTING.md for the roadmap to 64-bit).
