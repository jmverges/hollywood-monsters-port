# Building and running

## Requirements (Ubuntu 24.04 / x86-64)

```
sudo dpkg --add-architecture i386 && sudo apt update
sudo apt install openjdk-21-jdk-headless cmake ninja-build gcc-multilib libsdl2-dev:i386 python3 gdb imagemagick
```

Other distributions need the same pieces: a JDK 21, CMake, Ninja, a GCC that can build `-m32`, and the
**32-bit** SDL2 development package (Fedora `SDL2-devel.i686`, Arch `lib32-sdl2`). CMake looks for the 32-bit
`sdl2.pc` in `/usr/lib/i386-linux-gnu/pkgconfig` or `/usr/lib32/pkgconfig`; elsewhere pass
`-DHM_PKG_CONFIG_LIBDIR=/path/to/32-bit/pkgconfig` (Fedora: `/usr/lib/pkgconfig`).

Ghidra 12.1.x unpacked somewhere (default path used by the scripts: `~/.local/opt/ghidra_12.1.4_PUBLIC`).

The port is a **32-bit (i386) build**: the recompiled code still addresses the original 32-bit memory layout.
On x86-64 Linux it runs as a normal 32-bit process.

## Game data

You need the **FX Interactive** edition of the game (Windows, `MONSTERS.EXE`, 2,015,232 bytes,
sha256 prefix `2f5fda8e5b65e915`), plus its `RESOURCE.*` files. The Dinamic Multimedia 1997 edition is a
different program (DOS, Watcom) and is not supported by this pipeline.

Make one writable directory containing `MONSTERS.EXE` and every `RESOURCE.*` file. The game writes save files
(`SAVEGAME.*`) and, in some cases, into `RESOURCE.00x`, so use a copy.

## Generate, build, run

```
tools/recomp/pipeline.sh /path/to/gamedata/MONSTERS.EXE     # 3-5 minutes; creates ../work-recomp and src/gen
cmake -S . -B build -G Ninja && ninja -C build
build/hollywood-monsters /path/to/gamedata
```

`pipeline.sh` accepts `STEP=export` or `STEP=gen` to resume from a later step once the Ghidra project exists.
Re-run `cmake` after regenerating (the source list is a glob).

## PS Vita (runs on a real Vita and in the Vita3K emulator)

Built with VitaSDK, for example with its Docker image:

```
docker run --rm -u $(id -u):$(id -g) -e HOME=/tmp -v $PWD:/src -w /src vitasdk/vitasdk:latest sh -c \
  'cmake -S . -B build-vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake -DCMAKE_BUILD_TYPE=Release && make -C build-vita -j8'
```

For the LiveArea icon and background, run `tools/vita/make_livearea.sh` first (after the Linux build): it
makes them from frames of the game's intro into `livearea/`, which git ignores; without them the VPK has the
system's default bubble.

This produces `build-vita/hollywood-monsters.vpk` (title id HMON00001); the Vita build is always relocatable
(see PORTING.md). Install it with VitaShell and copy the game data (MONSTERS.EXE, the RESOURCE files, and the
`en` folder for English) to `ux0:data/hollywood-monsters/`. Controls: on the touch screen a tap is a click, dragging
aims and clicks on release, and holding still for half a second is the right button (the verb bar: slide to
choose, lift to confirm), and a tap with two fingers is Escape (menu, skip); or the left stick and d-pad to move the pointer, Cross to click, Circle held for the verb bar, Triangle for the inventory, Start
for the menu, Square for the port's menu (below). The same controls work with any game controller on the other builds. On the Vita, HM_* variables
(HM_DEBUG=1 ...) can be set in `hm_env.txt` in the data folder; messages go to the kernel log (`sceClibPrintf`) and
to `hm_log.txt` in the data folder.

## English

The game only exists in Spanish and Italian. A fan translation (ShinjiGR and A.P.$lasH, v1.0.0, 2012, made for
the 2002 FX CD edition) can be installed for the port; it translates all texts and subtitles, voices stay in
Spanish:

```
tools/lang/english_patch.py /path/to/unpacked-patch /path/to/data
```

The script writes `en/` inside the data directory (about 25 MB) and leaves the Spanish files alone: the
patch's RESOURCE files that differ, and `hm_lang.tsv` with the verbs and menus taken from the patch's own
executable (which the port cannot run). The language is then chosen in the port's menu (F9, or X/Square on
a controller). The switch takes effect when the next room or saved game loads, so a room never shows both
languages; a notice at the top of the screen says so. The choice is kept in `hm_lang.txt` and `HM_LANG=en|es`
overrides it. Saved games load in either language. Choose "Text" or "Both" in the options to see the
subtitles.

## The port's menu: easy mode, movie mode and the story

F9 (X on a controller, Square on the Vita) opens the port's own menu over the game; the game waits while it
is open. Arrows or the d-pad move, Enter or A chooses, Escape or B goes back; the mouse works too.

* **Easy mode**: the walkthrough (`guide/hm_actions.txt`, built into the program)
  section by section, with a check on the steps done. Choosing one makes the guide do it as a player would,
  then the list comes back. Left and right change section.
* **Movie mode**: the guide plays the game from the next step to the end, with nothing drawn over it (it can
  be recorded). Opening the menu pauses it; "Back to the game" or the movie row lets it go on, and
  **Normal mode** hands the game back to the player (the action under way finishes). A check marks which of
  the two the game is in; choosing movie mode again goes on from the next step.

On the Vita the menu's footers show the buttons (cross, circle, square, the d-pad, L/R), and the main menu
also lists what the buttons do in the game.
* **The story**: FX Interactive's walkthrough, written as a story. It is not in this repository; download it
  into the data directory once with

  ```
  tools/guide/story.py /path/to/data
  ```

  It opens at the chapter of the next step.

The guide keeps its place with each saved game, in `SAVEGAME.nnn.guide` next to it, so a loaded game shows
its own progress. Games saved before (or without the file) start from the first step.

## Run-time options (environment variables)

| Variable | Effect |
|---|---|
| `HM_DATA=dir` | data directory when none is given on the command line |
| `HM_LANG=en\|es` | language at start-up (default: the last one chosen, Spanish the first time) |
| `HM_SCALE=n` | window scale factor (default 2) |
| `HM_FULLSCREEN=1` | desktop fullscreen |
| `HM_DEBUG=1` | trace Win32/DirectX calls |
| `HM_TRACE_FILES=1` | trace file opens |
| `HM_NO_AUDIO=1` | disable audio |
| `HM_VSYNC=1` | present with vsync (default: no vsync, presentation limited to one frame per 14 ms) |
| `HM_NO_IDLE_SLEEP=1` | never sleep in `PeekMessage` (default: 1 ms sleep after 32 empty polls, ~17 % CPU instead of 100 %) |
| `HM_SEED=n` | replace the game's `srand(time)` seed, for reproducible runs |
| `HM_ORIGINAL_BUGS=1` | disable the fixes for bugs of the original game (`tools/recomp/fixups.py`) |
| `HM_BIN=path` | (test scripts) run another build, e.g. `build-reloc/hollywood-monsters` |
| `HM_NO_GUARD=1` | relocatable build on Linux: do not make 0x400000-0x709000 inaccessible |
| `HM_HIDE_CURSOR=1` | never draw the game's cursor |
| `HM_FOCUS_PAUSE=1` | tell the game when the window loses focus, as Windows does (it then stops its sounds) |
| `HM_NO_WARP=1` | ignore the game's requests to move the mouse pointer |
| `HM_SCRIPT=file`, `HM_SHOT_DIR=dir` | scripted input and screenshots (see `tools/test/`) |
| `HM_DUMP_FRAMES=dir`, `HM_DUMP_EVERY=n` | dump every n-th presented frame as PPM |

Headless: `SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy` (or `disk` with `SDL_DISKAUDIOFILE`).

## Windows build (cross-compiled from Linux)

```
sudo apt install gcc-mingw-w64-i686
# SDL2 MinGW development files: SDL2-devel-2.32.x-mingw.tar.gz from github.com/libsdl-org/SDL/releases
cmake -S . -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake \
      -DSDL2_MINGW_DIR=/path/to/SDL2-2.32.10/i686-w64-mingw32
ninja -C build-win
```

Ship `build-win/hollywood-monsters.exe` with `SDL2.dll` (from the same SDL2 package). The executable is
linked at 0x400000 without ASLR and contains an empty `.hmimg` section covering 0x401000–0x709000, which is
where the game's data is placed at run time (`cmake/mingw-hm.ld`, `platform/host/win32_reserve.c`).
It has been tested under Wine 9.0; it has not been tested on real Windows yet.
