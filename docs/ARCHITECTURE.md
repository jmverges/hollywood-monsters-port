# Architecture

## The approach: decompile, keep the memory layout, emulate the platform

*Hollywood Monsters* (FX Interactive edition, 2010 build) is a 32-bit Windows program written in C, compiled
with Visual C++ 2005 (optimized, `/GS`), linked incrementally, with a fixed image base (0x400000, no
relocations). Game logic is hard-coded C: one function per room/scene, dispatched from `WinMain`
(`FUN_0040cd20`) by a global room id. There is no script interpreter, so a data-driven engine
reimplementation (ScummVM style) is not an option without first understanding all of that C.

The port therefore keeps the **game code** as decompiled C and replaces only the **platform**:

```
 +-------------------------------------------------------------+
 | src/gen/f_XXXXXXXX.c   3,869 game functions (generated)     |
 | src/override/*.c       hand-written replacements            |
 +-------------------------------------------------------------+
 | platform/win32/        USER32/KERNEL32/WINMM subset,         |
 |                        DirectDraw 7, DirectSound, MSVC CRT   |   built against the game's own types
 +-------------------------------------------------------------+
 | platform/host/host.h   the porting interface (plain C types) |
 | platform/host/host_sdl.c  SDL2 + POSIX implementation        |
 +-------------------------------------------------------------+
```

### Memory model

The generated code addresses every global variable at its **original virtual address**. At start-up
`host_main.c` maps 0x400000–0x709000 and copies the executable's sections there (`.rdata`, `.data`,
`.rsrc`, headers; `.text` too, as plain data). Each generated function file defines, for every global it
touches, a typed macro such as

```c
typedef ushort T_DAT_006c4c82;
#define DAT_006c4c82 (*(T_DAT_006c4c82 *)0x006c4c82U)
```

The type is the one the decompiler used **for that access** in that function, so mixed-width accesses to the
same memory (very common in this game) keep exactly the original semantics. Consequences:

* the build is 32-bit, non-PIE (the binary loads at 0x08048000, well away from the image);
* pointers stored in game data stay valid, because data lives where the original code expects it;
* no game data is linked into the binary; it comes from the user's `MONSTERS.EXE` at run time.

### Calling conventions and prototypes

Everything is compiled by one compiler, so the original conventions do not matter: `__stdcall`,
`__fastcall` etc. are defined away and every function is cdecl. After Ghidra's Decompiler Parameter ID has
committed signatures program-wide, every game function is declared with its **full prototype**, so each
argument is converted to its parameter type. (An earlier version declared them without parameter lists; a
64-bit `CONCAT22(...)` argument then took two stack slots and shifted the following argument — this is how the
mine cart in chapter 5 left a trail.) For callers, a `void` function returns `undefined4`: the original code
sometimes reads whatever the callee left in EAX. Declaring a function `void` in its own file and
`undefined4` for its callers is a mismatch the i386 cdecl ABI tolerates; it would not survive link-time
optimisation, so do not enable `-flto` without regenerating with `gen.py --strict-void`.

### Register leftovers

The decompiler names values the C code cannot express: `extraout_ECX`/`extraout_EDX` (a register after a
call, often passed on as an argument), `extraout_var` (the upper bytes of EAX after a function that returns a
`char`), `in_EAX` (EAX on entry) and `unaff_ESI` etc. (callee-saved registers). In the original they hold
whatever the register held; in C they are uninitialised locals, which the compiler may treat as any value.
`gen.py` initialises them to 0 (about 3,000 of them), so the port is deterministic. The ones checked so far
are dead: the callee reads only the low 16 bits, or masks the result (`FUN_0040fec0` passes the upper half of
its EDX argument to `FUN_00513da0`, which only uses `(b - a + 1) & 0xffff`). `gen.py --no-zero-regs` leaves them
uninitialised for comparison.

### Stack frames

Ghidra splits stack buffers into separate variables (`local_14`, `local_c`, `local_8`), but the original code
often relies on their adjacency, e.g. building `"RESOURCE.M09"` across three locals. `gen.py` therefore
places every stack local at its **original frame offset** inside one byte array (`hm_frame`) and turns the
locals into macros. Raw `stack0x…` references map into the same array.

### Platform emulation

`platform/win32` implements only what the game calls (54 imports; see `src/gen/imports.tsv`):

* **USER32**: one window, a message queue fed by host input (mouse, wheel, keys, chars, focus),
  `DispatchMessageA` → the game's `WndProc` (`FUN_0040e870`), cursor functions.
* **WINMM**: `timeSetEvent`/`timeKillEvent` on one timer thread, as on Windows (the game uses dozens of
  10 ms periodic timers for animation).
* **KERNEL32**: threads, events, `WaitForMultipleObjects` (used by the audio streaming threads).
* **DirectDraw 7**: 8-bit palettized surfaces in memory; primary + back buffer; `Flip`, `Lock`, `Blt`,
  `BltFast` (colour key, fill, stretch), palettes. Frames go to the host as 8bpp + palette.
* **DirectSound**: software mixer for PCM 8/16-bit buffers, looping, volume/pan/frequency, play cursors, and
  `IDirectSoundNotify` position events that drive the game's streaming threads (music, speech, ambience).
* **CRT**: stdio over the host, `rand` with the MSVC generator, math helpers.

The COM objects are plain C structs whose first member is the vtable pointer; the vtable layouts come from
`tools/ghidra/types/dx_min.h`, the same header used to type the decompilation.

## The pipeline (`tools/recomp/pipeline.sh`)

1. **Import and analyse** `MONSTERS.EXE` in Ghidra.
2. **Create every function** from the incremental-link thunk table (`ilt.py`): 1,448 of the 3,876 functions are
   only ever used as callbacks and are invisible to normal analysis.
3. **Type the platform boundary**: `dx_min.h` (DirectDraw 7, DirectSound, notify interfaces) applied to the
   globals listed in `globals_dx.tsv`; without it, COM calls are decompiled without arguments.
4. **CRT helpers** (`SetSigs.java`): names for unrecognised CRT functions, and custom storage for x87
   intrinsics (`_CIsqrt` takes and returns ST0, `__ftol2` returns EDX:EAX) so their arguments are visible.
5. **Decompiler Parameter ID** commits parameter/return storage program-wide.
6. **EAX returns** (`SetReturns.java` + `void_returns_all.txt`): functions the decompiler believes are `void`
   but whose callers use EAX (e.g. `FUN_00515250` returns `rand() % 5` in AL) get an explicit return.
7. **Export** each function's C plus, per token, the accessed address and type (`ExportRecomp.java`).
8. **Generate** (`gen.py`): globals → address macros, thunks → targets, partial accesses (`x._1_3_`) →
   pointer arithmetic, stack frames, prototypes, imports list.

### Texts and translations

`gen.py --exe` turns string literals into `HM_STR(address)` when the text occurs once in the executable's data
(`HM_TEXT("...")` otherwise), and addresses of texts passed as arguments (`&DAT_005d061c`) into `HM_STR` too.
Both go through `hm_text()` (`platform/win32/lang.c`), which returns the text itself or, when the data
directory has an `hm_lang.tsv`, its translation. Entries of tables the code indexes (the verbs at 0x5e4074)
are written over the executable's data at start-up instead. This also means the generated code no longer
carries the game's texts.

## Bugs of the original game

Some bugs are in the original program, not in the recompilation; the port reproduces them faithfully.
`tools/recomp/fixups.py` lists the ones worth fixing: each replaces an exact snippet in one generated function
with a call into `platform/win32/fixes.c`, and generation fails if the snippet disappears. The fixed code only
changes behaviour where the original would hang or crash, and `HM_ORIGINAL_BUGS=1` turns the fixes off.

| Bug | Effect in the original | Fix |
|---|---|---|
| Start-up shuffle in `FUN_00512c00` (48 values, slot i never gets i) can be left with only 48 for slot 48 | freezes before the first frame for about 2 % of `srand(time)` seeds (seed 1007 reproduces it with `HM_SEED`) | move 48 into a random earlier slot first |

## Known decompiler pitfalls (and what handles them)

| Pitfall | Example | Handled by |
|---|---|---|
| Callback-only functions missing | `&LAB_00405443` timer callback | ILT step |
| COM calls without arguments | `(**(code **)(*DAT + 0x2c))()` | DirectX types |
| x87 argument passed in ST0 lost | `_CIsqrt()` | custom storage |
| `void` function whose EAX is used | `FUN_00515250` | EAX returns |
| Access inside a larger symbol | `ram0x00668cc3`, `unique0x…` | representative address / name address |
| Locals relied upon to be adjacent | `"RESOURCE" + ".M09"` | stack frame layout |
| Integer counter sharing a slot with a `double` | `FUN_0051e9a0` | hand-written override |
| Inlined `strcpy` into an unplaced buffer | `FUN_00514a70` | hand-written override |
| SDK version probe | `FUN_0040a770` (GetDXVersion) | hand-written override |
| Parameter accessed wider than declared | `_param_2` on a `byte` parameter | parameter becomes a 4-byte slot, typed view macro |
| 64-bit helper result passed as an argument | `CONCAT22(a,b)` | 32-bit `CONCATxy` for x+y ≤ 4, full prototypes |
| Uninitialised heap read by the game | sprite data past the loaded block | `malloc` fills like the MSVC debug heap (0xCD, 0xFD guards, 0xDD on free) |
| Named structure used through a field | `g_ddsd.lpSurface`: the export gives the field's address (0x691044) | the macro sits at the symbol's start (0x691020) |
| Store of another size to a merged variable | `_DAT_006ca0d8 = CONCAT12(1, x)` through a 2-byte type | `HM_STORE` of exactly the CONCAT's bytes; declaration widened to 4 bytes for reads; `X = (uint)X` artefacts dropped |
| Piece of a wider variable read through a narrow type | `(undefined2)DAT_0068f956` on a 1-byte declaration | read N bytes at the address: `(*(undefined2 *)&DAT_0068f956)` (106 reads, mostly the 0x668cc0 animation byte pattern) |
| Register with no C value | `FUN_0040fec0(extraout_ECX, extraout_EDX)` | initialised to 0 (see Register leftovers) |

When a new problem shows up at run time, the usual loop is: reproduce with a script (`tools/test/`), catch
it in gdb (the binary has debug info and maps 1:1 to `src/gen/f_<address>.c`), compare with the original
disassembly (`objdump -D -b binary -mi386 --adjust-vma=0x401000` on `.text`), then fix the cause in the
pipeline if it is a pattern, or add an override if it is a one-off.
