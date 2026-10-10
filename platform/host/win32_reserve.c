/* Windows only: an uninitialised ("bss") section that cmake/mingw-hm.ld places at 0x401000, reserving the
 * original game's image range 0x401000-0x709000 inside this executable (linked at 0x400000 without ASLR).
 * host_main.c makes it writable and fills it from MONSTERS.EXE at start-up. */
__asm__(".section .hmimg,\"bw\"\n"
        ".balign 4096\n"
        ".globl _hm_game_image_reserve\n"
        "_hm_game_image_reserve:\n"
        ".space 0x308000\n"
        ".text\n");
