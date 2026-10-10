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
#elif defined(__vita__)
/* PS Vita (VitaSDK): relocatable build only; data in ux0:data/hollywood-monsters by default. */
int _newlib_heap_size_user = 256 * 1024 * 1024;     /* newlib heap for the game's buffers (default is small) */
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

static unsigned g_rsrc_rva;
unsigned host_image_resource_rva(void) { return g_rsrc_rva; }

/* Where the image lives relative to its original base 0x400000. 0 in the normal build; with HM_RELOCATABLE the
 * image is loaded wherever memory is available (systems that cannot map a fixed address, e.g. the PS Vita) and
 * the generated code adds this to every image address (HM_ADDR in ghidra_rt.h). */
unsigned hm_image_delta;
unsigned host_image_delta(void) { return hm_image_delta; }

#ifdef HM_RELOCATABLE
extern const unsigned hm_relocs[];       /* src/gen/hm_relocs.c: words in the image that hold image addresses */
extern const unsigned hm_nrelocs;
#endif

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
    g_rsrc_rva = rd32(opt + 96 + 2 * 8);            /* data directory 2: resources */
    if (base != IMAGE_BASE) { host_log("unexpected image base %#x", base); return 0; }
#ifdef _WIN32
    void *m = (void *)(unsigned long)IMAGE_BASE;
    DWORD old;
    if (!VirtualProtect((char *)m + 0x1000, imgsize - 0x1000, PAGE_READWRITE, &old)) {
        host_log("cannot unprotect the reserved image range (%lu)", GetLastError());
        return 0;
    }
    (void)hdrsize;     /* 0x400000-0x401000 holds this program's own PE headers: leave them alone */
#elif defined(HM_RELOCATABLE)
    unsigned char *m = calloc(1, imgsize + 0x1000);
    if (!m) { host_log("cannot allocate the image (%#x bytes)", imgsize); return 0; }
    m = (unsigned char *)(((unsigned long)m + 0xfff) & ~0xfffUL);
    hm_image_delta = (unsigned)(unsigned long)m - IMAGE_BASE;
    memcpy(m, d, hdrsize);
#if !defined(_WIN32) && !defined(__vita__) && defined(MAP_FIXED_NOREPLACE)
    /* Test aid: nothing may use the original range any more. A pointer that escaped relocation faults here and
     * the crash handler prints the address. */
    if (!getenv("HM_NO_GUARD"))
        mmap((void *)(unsigned long)IMAGE_BASE, imgsize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
#endif
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
#ifdef HM_RELOCATABLE
    unsigned moved = 0;
    for (unsigned i = 0; i < hm_nrelocs; i++) {
        unsigned *w = (unsigned *)((unsigned char *)m + (hm_relocs[i] - IMAGE_BASE));
        if (*w >= IMAGE_BASE + 0x1000 && *w < IMAGE_BASE + imgsize) { *w += hm_image_delta; moved++; }
    }
    host_log("image at %p (delta %#x), %u of %u relocations applied", (void *)m, hm_image_delta, moved, hm_nrelocs);
#endif
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
#elif defined(__vita__)
static void install_crash_handler(void) { }
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
#ifdef __vita__
    if (!dir) dir = "ux0:data/hollywood-monsters";
#endif
    if (dir && chdir(dir) != 0) { host_log("cannot enter data directory %s", dir); return 1; }
#ifdef __vita__
    /* No shell on the Vita: HM_* variables (HM_DEBUG=1 ...) can be set in hm_env.txt, one per line. */
    FILE *envf = fopen("hm_env.txt", "r");
    if (envf) {
        char line[256];
        while (fgets(line, sizeof line, envf)) {
            line[strcspn(line, "\r\n")] = 0;
            char *eq = strchr(line, '=');
            if (eq && line[0] != '#') { *eq = 0; setenv(line, eq + 1, 1); host_log("hm_env.txt: %s=%s", line, eq + 1); }
        }
        fclose(envf);
    }
#endif
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
