# Hollywood Monsters (1997, Pendulo Studios) — portable recompilation

This repository turns the **FX Interactive Windows edition** of *Hollywood Monsters* into portable C code that
runs natively on Linux, Windows and the PS Vita, using SDL2.

[![Hollywood Monsters: the port in movie mode, with the English fan translation](https://img.youtube.com/vi/txw7Vk0wMj0/maxresdefault.jpg)](https://youtu.be/txw7Vk0wMj0)

*The port in movie mode, playing the walkthrough by itself with the English fan translation (Linux build;
the Vita runs the same code).*

It contains **no game code and no game data**. The C sources are generated locally from your own copy of the
game by a Ghidra-based pipeline; the game's data files are read at run time from your copy.

| Part | What it is |
|---|---|
| `tools/ghidra/` | Ghidra headless scripts: import, typing (DirectX, CRT), parameter recovery, export |
| `tools/recomp/` | `pipeline.sh` (one command, EXE → C), `gen.py` (decompiler output → compilable C), `ilt.py` |
| `src/override/` | Hand-written replacements for functions the decompiler gets wrong |
| `src/include/` | Runtime support for decompiler idioms (`ghidra_rt.h`) and the common header |
| `platform/win32/` | Emulated Win32 + DirectDraw 7 + DirectSound + MSVC CRT, built against the game's own types |
| `platform/host/` | The only OS-specific code: SDL2 + POSIX (`host.h` is the porting interface) |
| `tools/test/` | Headless scripted runs with screenshots |

Status: work in progress. See [docs/STATUS.md](docs/STATUS.md) for what has been verified so far.

Quick start: see [docs/BUILDING.md](docs/BUILDING.md). How it works: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
Porting to another system: [docs/PORTING.md](docs/PORTING.md).

Legal: you need a legally obtained copy of the game. Never commit or distribute the executable, the `RESOURCE.*`
files, or the generated sources (`src/gen/` is git-ignored for this reason).
