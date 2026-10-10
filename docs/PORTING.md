# Porting to another system

Everything above `platform/host/host.h` is plain C with no OS dependencies. A new target needs:

1. **A 32-bit build** (for now). The game code and its data use 32-bit pointers at fixed addresses.
   Any x86 or ARM target with a 32-bit userland works (32-bit Linux, Windows, ARM Linux with an armhf
   toolchain, old consoles/handhelds with 32-bit CPUs). The CPU architecture does not matter: the code is C.
2. **The ability to reserve 0x400000–0x709000** in the process address space and fill it from
   `MONSTERS.EXE` (`host_main.c`, `map_image`). On systems without `mmap(MAP_FIXED)` use a linker script that
   reserves that range, or any loader trick that places a 3 MB block at that address.
3. **An implementation of `host.h`**: window + 8bpp-with-palette presentation, input events translated to
   Windows virtual-key codes, threads/mutexes/events, an audio pull callback (stereo S16), and stdio-like
   file access with case-insensitive names. `host_sdl.c` (~560 lines) is the reference; with SDL2 available
   on the target it may work unchanged.

### Assumptions the generated code makes about the target

These hold on x86 and on 32-bit ARM Linux; check them before starting on anything else.

* **Little-endian.** Partial accesses (`HM_PART3`, `CONCATxy`, a `ushort` view of an `int` global) take the
  low bytes first, as on x86.
* **Unaligned 16/32-bit loads and stores** at arbitrary addresses: the per-access global macros read game data
  wherever the original did. x86 and ARMv7 and later handle this in hardware; older ARM and MIPS do not.
* **i386 structure layout.** `hm_types.h` has no packing pragmas and relies on the i386 System V rules. Only two
  CRT structures (`_CRT_DOUBLE`, `_exception`) contain 8-byte members, where ARM's alignment differs; the
  game's own data is accessed through address macros, not structures.
* **`long double`** stands in for the x87 80-bit `float10`; a target where it is 64-bit loses precision only in
  the few intermediate results that use it.
* **Thread-local storage** (`__thread`) for the CRT `rand` state.
* **1 ms timer granularity** for `Sleep`, event waits and the multimedia timer thread (`winmm.c`). SDL sets
  this on Windows; a host without SDL must call `timeBeginPeriod(1)` or equivalent.
* **Fixed mapping** of 0x400000–0x709000: `mmap` with `MAP_FIXED_NOREPLACE` (Linux 4.17+, `MAP_EXCL` on FreeBSD;
  elsewhere the address is a hint and the result is checked), `VirtualProtect` over a reserved section on
  Windows.
* **SDL 2.0.18 or later** for `host_sdl.c` (`SDL_RenderWindowToLogical`); headless tests use SDL's `offscreen`
  video driver.

## Targets that only run 64-bit code (Apple Silicon, recent Android, consoles)

These need the game to run as 64-bit code. The obstacle is not the CPU but the data model: the game stores
pointers in 4-byte memory slots everywhere (globals, structures, resource tables, the stack frames reproduced
by `gen.py`) and does integer arithmetic on them. Two routes, from least to most work:

**A. 64-bit code, 32-bit addresses ("low 4 GB" model).** Keep every byte the game can point to below 4 GB, so
truncating a pointer to 32 bits is lossless, and make every *memory-resident* pointer a 32-bit integer:

* the image stays at 0x400000; the port binary is linked low (e.g. `-Wl,-Ttext-segment=0x10000000`);
* `crt__malloc` allocates from an arena mapped below 4 GB (`MAP_32BIT` on Linux x86-64, a fixed low
  reservation elsewhere);
* the game runs on its own thread whose stack is allocated in that arena (stack locals live in `hm_frame`
  and their addresses are stored in globals);
* `FILE *` and other host objects become small integer handles;
* `gen.py` must emit 32-bit types for memory: `ulong`/`long` as `uint32_t`/`int32_t` (they are 4 bytes on
  Windows, 8 on LP64), and pointer-typed globals, struct members and frame variables as 32-bit integers
  with explicit conversion at each use. Pointer-typed *registers/locals* (`puVar5`) may stay native.
* The emulated COM objects (vtables) must also be laid out with 32-bit slots.

Size of the job, counted with regular expressions over the generated code (2026-10-10, approximate):

| What | Count | Route A needs |
|---|---|---|
| Globals accessed with a pointer type | 343 of 5,214 globals, 2,216 accesses in 440 functions | a 32-bit slot and a conversion at each access |
| Globals accessed as `long`/`ulong` | 66 | `int32_t`/`uint32_t` |
| Structure types in `hm_types.h` | 120, with 119 pointer and 260 `long` members | 32-bit members |
| Reproduced stack-frame variables of pointer type | 46 (2 of `long` type) | 32-bit slots |
| Address arithmetic on integers, `*(T *)(x + n)` | 2,201 | nothing, if everything lives below 4 GB |
| Casts from integer locals to pointers | 392 | nothing, if everything lives below 4 GB |

The hard part is the 32-bit memory types in `gen.py` and the COM vtables: Ghidra's types decide which members are pointers, and a wrong guess
silently truncates or widens a field. The per-access types exported by `ExportRecomp.java` are the right
starting point, because they already reflect the size of every memory access.

**B. Real C structures.** Name and type the data in Ghidra, replace address macros with C variables loaded
from the executable at start-up, and clean the code module by module (see below). This is the long-term path
and makes A unnecessary.

## Removing the 32-bit / fixed-address constraint (roadmap)

To reach 64-bit and arbitrary memory layouts, the globals have to move from "address macros" to real C
variables. A practical order:

1. Name and type the globals in Ghidra (structures for rooms, actors, animation tables). Every name added
   there flows into the generated code on the next pipeline run.
2. Replace pointer-sized integer fields (`undefined4` holding pointers) with real pointer types.
3. Emit globals as C definitions initialised from the executable's data (a generated `data.c` loader),
   instead of mapping the image.
4. Replace generated functions with hand-cleaned ones module by module (`src/override/`), starting with
   resource loading, rendering, and the room dispatcher.

The `src/override/` mechanism is the bridge: any function can be replaced by a clean implementation without
touching the rest, and the generated version stays available for comparison.
