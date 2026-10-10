# Status

Last updated: 2026-10-10. Everything below was checked with headless scripted runs (`tools/test/`) on
Ubuntu 24.04 (x86-64 host, i386 build), FX Interactive 2010 build of `MONSTERS.EXE`.

## Verified

| Area | Result |
|---|---|
| Pipeline | `pipeline.sh` from a clean clone reproduces the same sources (3,869 generated files + 3 hand-written overrides) in 3–5 min |
| Build | 0 compile errors; Linux i386 binary; Windows i386 `.exe` cross-compiles and links (MinGW) |
| Boot | DirectDraw/DirectSound initialisation, resource files, streaming threads, timers |
| Intro | Plays from the FX/Péndulo logos through the credits and the drive-in sequence (~7 min) without crashing |
| Gameplay | Reaches the first playable scene (Sue outside the mansion), verb line under the cursor |
| Menu | Escape opens the options screen with a live thumbnail |
| Save / load | Saving writes `SAVEGAME.000` with the typed description; a fresh start loads it from the menu |
| Walking | Click-to-walk with screen scrolling; hotspot labels ("Ir a escalera", "Mirar escalera"); right click toggles the verb; walking up the stairs changes room to the terrace |
| Windows | The MinGW build runs under Wine 9.0 headless: intro logo, menu, loading the savegame |
| All chapters | Warping to the start of chapters 1–8 (room id 1000…8000) loads each one; 5-minute random-click runs in every chapter, and four 10-minute runs in chapter 1, finished without a crash |
| Against the original | The original `MONSTERS.EXE` runs under Wine/Xvfb with the same scripts (`run_original.py`, including `poke` warps); garden, terrace, menu and the chapter 2 title pan match the port (the black band at the right of that pan is in the original too) |
| Room tour vs original | Warping through all 77 rooms with no clicks, both versions with a real-time (silent) sound card, screenshot 8 s after each warp: 61 rooms match within 1 % of pixels, 13 within 15 % (animation phase), 3 differ. Those 3 are transition timing: the port reaches the next room 1–2 s earlier, then both show the same room. This comparison found and fixed the mine cart trail (chapter 5) |
| Audio vs original | The original's output recorded through the ALSA loopback card under Wine and compared with the port's (`compare_audio.py`), first 80 s of the intro: every second of music matches the original's samples (correlation 0.97–1.00) at a constant offset, and every speech clip the port plays is found in the original (0.97–0.98). The newsboy's shouts are picked at random, so their order differs between any two runs, of either version |
| Verb bar and conversation vs original | The same script on both: opening the verb bar, picking each of the 7 verbs, and talking to Junior in the first scene. Screens match within 1.1 % of pixels for 40 s of conversation. The third and fourth spoken lines are picked from three recorded lines, depending on timing: two runs of the original played two different pairs, and the port plays a third pair, or the original's first pair when the click comes 0.9 s later. Every line either version plays is found in the other's recordings (correlation 0.98–1.00). This comparison found the cursor bug below |
| Verbs vs original | Look at, open, pick up and use on the doghouse, look at and pick up on Junior (`tools/test/scripts/verbs_first_scene.txt`): screens within 1.3 %, and the same four spoken answers in both, at a constant offset; the other two actions are silent in both |
| Register leftovers | Since all ~3,000 are initialised to 0, the smoke test still passes in all 77 rooms |

## Not verified yet

* Audio by ear (compared sample by sample with the original, not listened to).
* Inventory, dialogue choices, puzzles, chapter changes, end game.
* Pointer warps in interactive play on Wayland, where SDL cannot move the system pointer. The game re-centres
  and clamps its pointer with `SetCursorPos` every frame; the host reports the warped position until the next
  real motion, which is the Windows behaviour, but this was only tested with scripted input.
* The Windows build on real Windows (only Wine so far).
* Long sessions beyond ~7 minutes.

## Known issues / open work

* Fixed on 2026-10-10: with scripted input, `GetCursorPos` ignored the game's own `SetCursorPos` warps, so the
  verb bar opened on "Dar" instead of "Hablar" and later verb gestures in tests did not match the original.

* The 1,000+ functions only reached through rooms not visited yet are compiled but untested; expect more
  decompiler pitfalls there (see the table in ARCHITECTURE.md for the kinds found so far).
* The build is 32-bit only (see PORTING.md for the roadmap to 64-bit).
* The 3 rooms where the port reaches the next room 1–2 s earlier are not explained. Frame pacing is not the
  cause: the game never calls `Flip` or `WaitForVerticalBlank` (it presents by locking the primary surface).
  The original's start-up under Wine is slower, which shifts its timeline; a per-room timing check would tell.
