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
   file access with case-insensitive names. `host_sdl.c` (~500 lines) is the reference; with SDL2 available
   on the target it may work unchanged.

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
