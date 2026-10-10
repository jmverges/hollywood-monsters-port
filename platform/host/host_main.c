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
#ifdef _WIN32
#include <windows.h>
/* The Windows executable is linked at 0x400000 with an empty section reserving 0x401000-0x709000
 * (see platform/host/win32_reserve.c), so the game's image range is ours by construction. */
#else
#include <sys/mman.h>
#include <signal.h>
#include <execinfo.h>
#include <ucontext.h>
#endif

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
#ifdef _WIN32
    void *m = (void *)(unsigned long)IMAGE_BASE;
    DWORD old;
    if (!VirtualProtect((char *)m + 0x1000, imgsize - 0x1000, PAGE_READWRITE, &old)) {
        host_log("cannot unprotect the reserved image range (%lu)", GetLastError());
        return 0;
    }
    (void)hdrsize;     /* 0x400000-0x401000 holds this program's own PE headers: leave them alone */
#else
#if !defined(MAP_FIXED_NOREPLACE) && defined(MAP_EXCL)
#define MAP_FIXED_NOREPLACE (MAP_FIXED | MAP_EXCL)      /* FreeBSD */
#elif !defined(MAP_FIXED_NOREPLACE)
#define MAP_FIXED_NOREPLACE 0                            /* address is only a hint; checked below */
#endif
    /* Linux < 4.17 ignores MAP_FIXED_NOREPLACE and treats the address as a hint: also checked below. */
    void *m = mmap((void *)(unsigned long)IMAGE_BASE, imgsize, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (m == MAP_FAILED || m != (void *)(unsigned long)IMAGE_BASE) {
        host_log("cannot map image at %#x (size %#x)", IMAGE_BASE, imgsize);
        return 0;
    }
    memcpy(m, d, hdrsize);
#endif
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

#ifdef _WIN32
static LONG WINAPI crash_filter(EXCEPTION_POINTERS *ep)
{
    fprintf(stderr, "\n*** exception %#lx at eip=%#lx (address %p)\n", ep->ExceptionRecord->ExceptionCode,
            (unsigned long)ep->ContextRecord->Eip, ep->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_CONTINUE_SEARCH;
}
static void install_crash_handler(void) { SetUnhandledExceptionFilter(crash_filter); }
#else
static void crash_handler(int sig, siginfo_t *si, void *uc)
{
    void *frames[48];
    int n = backtrace(frames, 48);
    unsigned eip = 0;
#if defined(__i386__)
    eip = (unsigned)((ucontext_t *)uc)->uc_mcontext.gregs[REG_EIP];
#endif
    fprintf(stderr, "\n*** signal %d (%s) at address %p, eip=%#x\n", sig, strsignal(sig), si->si_addr, eip);
    backtrace_symbols_fd(frames, n, 2);
    signal(sig, SIG_DFL);
    raise(sig);
}

static void install_crash_handler(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = crash_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
}
#endif

int main(int argc, char **argv)
{
    install_crash_handler();
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
