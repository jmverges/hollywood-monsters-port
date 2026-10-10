/* Microsoft C runtime subset used by the game, mapped onto host services / compiler builtins. */
#include "hm_platform.h"

/* memory */
void *crt__malloc(size_t n) { return host_malloc(n ? n : 1); }
undefined4 crt__free(void *p) { host_free(p); return 0; }
void *crt__memcpy(void *d, const void *s, size_t n) { return __builtin_memcpy(d, s, n); }
void *crt__memmove(void *d, const void *s, size_t n) { return __builtin_memmove(d, s, n); }
void *crt__memset(void *d, int c, size_t n) { return __builtin_memset(d, c, n); }

/* files: the FILE* handed to the game is an opaque host handle */
FILE *crt__fopen(const char *name, const char *mode) { return (FILE *)host_fopen(name, mode); }
int crt__fclose(FILE *f) { return host_fclose(f); }
size_t crt__fread(void *buf, size_t size, size_t n, FILE *f) { return host_fread(buf, size, n, f); }
size_t crt__fwrite(const void *buf, size_t size, size_t n, FILE *f) { return host_fwrite(buf, size, n, f); }
int crt__fseek(FILE *f, long off, int whence) { return host_fseek(f, off, whence); }
long crt__ftell(FILE *f) { return host_ftell(f); }

/* rand: same LCG as MSVC, per-thread state like the original CRT */
static __thread unsigned holdrand = 1;
undefined4 crt__srand(unsigned seed) { holdrand = seed; return 0; }
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
