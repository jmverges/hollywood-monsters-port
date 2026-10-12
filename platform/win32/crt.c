/* Microsoft C runtime subset used by the game, mapped onto host services / compiler builtins. */
#include "hm_platform.h"

/* memory */
/* The game is linked against the debug CRT: _malloc fills new blocks with 0xCD, surrounds them with 4-byte
 * 0xFD guards, and _free fills released blocks with 0xDD. The game reads uninitialised memory in places (for
 * example past the loaded sprite data in the chapter 5 mine), and what it draws depends on those values, so
 * the port reproduces the same fill patterns. */
#define HM_GUARD 4
void *crt__malloc(size_t n)
{
    unsigned char *p = host_malloc((unsigned)(n + 2 * HM_GUARD + sizeof(size_t)));
    if (!p) return 0;
    *(size_t *)p = n;
    p += sizeof(size_t);
    __builtin_memset(p, 0xFD, HM_GUARD);
    __builtin_memset(p + HM_GUARD, 0xCD, n);
    __builtin_memset(p + HM_GUARD + n, 0xFD, HM_GUARD);
    return p + HM_GUARD;
}
undefined4 crt__free(void *q)
{
    if (!q) return 0;
    unsigned char *p = (unsigned char *)q - HM_GUARD - sizeof(size_t);
    size_t n = *(size_t *)p;
    __builtin_memset(p, 0xDD, n + 2 * HM_GUARD + sizeof(size_t));
    host_free(p);
    return 0;
}
void *crt__memcpy(void *d, const void *s, size_t n) { return __builtin_memcpy(d, s, n); }
void *crt__memmove(void *d, const void *s, size_t n) { return __builtin_memmove(d, s, n); }
void *crt__memset(void *d, int c, size_t n) { return __builtin_memset(d, c, n); }

/* files: the FILE* handed to the game is an opaque host handle */
/* Saved games (SAVEGAME.nnn) are watched so the port's guide can keep its place with them (guide.c). */
static struct { void *f; char name[16]; int write; unsigned read; } g_save;

static int is_save(const char *name)
{
    const char *base = name, *want = "SAVEGAME.";
    for (const char *p = name; *p; p++) if (*p == '\\' || *p == '/' || *p == ':') base = p + 1;
    for (int i = 0; want[i]; i++) if ((base[i] & ~32) != (want[i] & ~32) && base[i] != want[i]) return 0;
    return 1;
}

FILE *crt__fopen(const char *name, const char *mode)
{
    hm_lang_file_opening(name);
    void *f = host_fopen(name, mode);
    if (f && is_save(name)) {
        g_save.f = f; g_save.read = 0; g_save.write = mode[0] == 'w';
        int k = 0;
        while (name[k] && k < 15) { g_save.name[k] = name[k]; k++; }
        g_save.name[k] = 0;
    }
    return (FILE *)f;
}

int crt__fclose(FILE *f)
{
    int r = host_fclose(f);
    if (f && (void *)f == g_save.f) {
        g_save.f = 0;
        if (g_save.write) hm_guide_saved(g_save.name);
        else if (g_save.read > 0x1000) hm_guide_loaded(g_save.name);   /* the whole game, not its description */
    }
    return r;
}

size_t crt__fread(void *buf, size_t size, size_t n, FILE *f)
{
    size_t r = host_fread(buf, size, n, f);
    if (f && (void *)f == g_save.f) g_save.read += (unsigned)(r * size);
    return r;
}
size_t crt__fwrite(const void *buf, size_t size, size_t n, FILE *f) { return host_fwrite(buf, size, n, f); }
int crt__fseek(FILE *f, long off, int whence) { return host_fseek(f, off, whence); }
long crt__ftell(FILE *f) { return host_ftell(f); }

/* rand: same LCG as MSVC, per-thread state like the original CRT */
static __thread unsigned holdrand = 1;
undefined4 crt__srand(unsigned seed)
{
    const char *fixed = hm_getenv("HM_SEED");          /* tests: replace the game's srand(time) seed */
    if (fixed) {
        unsigned long long v = 0;
        const char *c = fixed;
        for (; *c >= '0' && *c <= '9' && v <= 0xffffffffull; c++) v = v * 10 + (unsigned)(*c - '0');
        if (c == fixed || *c || v > 0xffffffffull)
            host_log("HM_SEED=%s ignored: expected a decimal number below 2^32", fixed);
        else
            seed = (unsigned)v;
    }
    HM_TRACE("srand(%u)", seed);
    holdrand = seed;
    return 0;
}
int crt__rand(void) { holdrand = holdrand * 214013u + 2531011u; return (int)((holdrand >> 16) & 0x7fff); }

/* time */
__time64_t crt___time64(__time64_t *t)
{
    long long now = host_time();
    if (t) *t = now;
    return now;
}

char *crt___itoa(int value, char *buf, int radix)
{
    char tmp[34];
    int i = 0, neg = 0;
    unsigned v;
    if (radix == 10 && value < 0) { neg = 1; v = (unsigned)(-value); } else v = (unsigned)value;
    do { unsigned d = v % (unsigned)radix; tmp[i++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); v /= (unsigned)radix; } while (v);
    char *p = buf;
    if (neg) *p++ = '-';
    while (i) *p++ = tmp[--i];
    *p = 0;
    return buf;
}

/* floating point helpers */
longlong crt___ftol2(double x) { return (longlong)x; }
double crt__CIsqrt(double x) { return __builtin_sqrt(x); }
double crt__ceil(double x) { return __builtin_ceil(x); }
double crt__floor(double x) { return __builtin_floor(x); }

/* /GS stack cookie check: nothing to do in the port */
undefined4 crt___security_check_cookie(void) { return 0; }

/* localtime (MSVC struct tm has the same first nine int fields as the host's) */
struct tm *crt_FIDc___localtime64(__time32_t *t)
{
    static int out[9];
    extern void host_localtime(long long t, int *out9);
    host_localtime(t ? (long long)*t : host_time(), out);
    return (struct tm *)out;
}
