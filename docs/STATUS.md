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
| Audio | Music streams from `RESOURCE.M09`; sound effects play (measured output levels) |

## Not verified yet

* Speech: the voice stream buffer is created and started; audible speech has not been confirmed by ear.
* Inventory, dialogue trees, puzzles, chapter changes, end game.
* The Windows build on real Windows (only Wine so far).
* Long sessions beyond ~7 minutes.

## Known issues / open work

* The 1,000+ functions only reached through rooms not visited yet are compiled but untested; expect more
  decompiler pitfalls there (see the table in ARCHITECTURE.md for the kinds found so far).
* The build is 32-bit only (see PORTING.md for the roadmap to 64-bit).
