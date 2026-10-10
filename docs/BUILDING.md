# Building and running

## Requirements (Ubuntu 24.04 / x86-64)

```
sudo dpkg --add-architecture i386 && sudo apt update
sudo apt install openjdk-21-jdk-headless cmake ninja-build gcc-multilib libsdl2-dev:i386 python3 gdb imagemagick
```

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
tools/recomp/pipeline.sh /path/to/gamedata/MONSTERS.EXE     # ~5 minutes; creates ../work-recomp and src/gen
cmake -S . -B build -G Ninja && ninja -C build
build/hollywood-monsters /path/to/gamedata
```

`pipeline.sh` accepts `STEP=export` or `STEP=gen` to resume from a later step once the Ghidra project exists.
Re-run `cmake` after regenerating (the source list is a glob).

## Run-time options (environment variables)

| Variable | Effect |
|---|---|
| `HM_SCALE=n` | window scale factor (default 2) |
| `HM_FULLSCREEN=1` | desktop fullscreen |
| `HM_DEBUG=1` | trace Win32/DirectX calls |
| `HM_TRACE_FILES=1` | trace file opens |
| `HM_NO_AUDIO=1` | disable audio |
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
