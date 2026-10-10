/* Program entry: map the original executable's data sections at their fixed addresses, then start the game.
 * The recompiled code addresses globals by their original virtual addresses (0x400000 base, no relocations). */
#define _GNU_SOURCE
#include "host.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/mman.h>

extern void host_init_main_thread(void);
extern int hm_game_start(const char *cmdline);   /* in platform/win32/hm_start.c */

#define IMAGE_BASE 0x400000u

static unsigned rd32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24; }
static unsigned rd16(const unsigned char *p) { return p[0] | p[1] << 8; }

static int map_image(const char *exe)
{
    FILE *f = fopen(exe, "rb");
    if (!f) { host_log("cannot open %s", exe); return 0; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *d = malloc(sz);
    if (fread(d, 1, sz, f) != (size_t)sz) { fclose(f); return 0; }
    fclose(f);
    unsigned pe = rd32(d + 0x3c);
    if (memcmp(d + pe, "PE\0\0", 4) != 0) { host_log("%s is not a PE file", exe); return 0; }
    unsigned nsec = rd16(d + pe + 6), optsz = rd16(d + pe + 20);
    const unsigned char *opt = d + pe + 24;
    unsigned base = rd32(opt + 28), imgsize = rd32(opt + 56), hdrsize = rd32(opt + 60);
    if (base != IMAGE_BASE) { host_log("unexpected image base %#x", base); return 0; }
    void *m = mmap((void *)(unsigned long)IMAGE_BASE, imgsize, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (m == MAP_FAILED || m != (void *)(unsigned long)IMAGE_BASE) {
        host_log("cannot map image at %#x (size %#x)", IMAGE_BASE, imgsize);
        return 0;
    }
    memcpy(m, d, hdrsize);
    const unsigned char *sh = opt + optsz;
    for (unsigned i = 0; i < nsec; i++, sh += 40) {
        unsigned vsize = rd32(sh + 8), va = rd32(sh + 12), rsize = rd32(sh + 16), rptr = rd32(sh + 20);
        unsigned n = rsize < vsize ? rsize : vsize;
        if (rptr && n) memcpy((char *)m + va, d + rptr, n);
    }
    free(d);
    return 1;
}

static int find_exe(char *out, size_t n)
{
    DIR *dir = opendir(".");
    if (!dir) return 0;
    struct dirent *de;
    int ok = 0;
    while ((de = readdir(dir))) {
        if (strcasecmp(de->d_name, "MONSTERS.EXE") == 0) { snprintf(out, n, "%s", de->d_name); ok = 1; break; }
    }
    closedir(dir);
    return ok;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : getenv("HM_DATA");
    if (dir && chdir(dir) != 0) { host_log("cannot enter data directory %s", dir); return 1; }
    char exe[256];
    if (!find_exe(exe, sizeof exe)) {
        host_log("MONSTERS.EXE (FX Interactive edition) not found in the data directory.\n"
                 "usage: %s <directory with MONSTERS.EXE and RESOURCE.* files>", argv[0]);
        return 1;
    }
    if (!map_image(exe)) return 1;
    host_init_main_thread();
    return hm_game_start("");
}
