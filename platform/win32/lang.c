/* Translations of the game's texts (hm_lang.tsv in the data directory, made by tools/lang/english_patch.py).
 *
 * Lines are   original<TAB>translation   (texts the code uses directly, matched by content in hm_text)
 * or          @address<TAB>translation   (entries of tables the code indexes, e.g. the verbs at 0x5e4074,
 *                                          written over the executable's data when they fit their slot).
 * Escapes: \t \n \\ \xNN. Texts are in the game's own code page. Without the file nothing changes. */
#include "hm_platform.h"

struct entry { char *from, *to; };
static struct entry *g_map;
static int g_n;

static int unescape(char *s)
{
    char *o = s;
    for (char *p = s; *p; p++) {
        if (*p != '\\' || !p[1]) { *o++ = *p; continue; }
        p++;
        if (*p == 't') *o++ = '\t';
        else if (*p == 'n') *o++ = '\n';
        else if (*p == 'x' && p[1] && p[2]) {
            int v = 0;
            for (int k = 1; k <= 2; k++) {
                char c = p[k];
                v = v * 16 + (c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 0);
            }
            *o++ = (char)v; p += 2;
        } else *o++ = *p;
    }
    *o = 0;
    return (int)(o - s);
}

static char *dup(const char *s)
{
    unsigned n = 0;
    while (s[n]) n++;
    char *d = host_malloc(n + 1);
    for (unsigned i = 0; i <= n; i++) d[i] = s[i];
    return d;
}

static void patch_in_place(unsigned addr, const char *text, int len)
{
    char *dst = (char *)(addr + host_image_delta());
    int cap = 0;
    while (dst[cap]) cap++;                          /* the original text ... */
    while (cap < 256 && dst[cap] == 0) cap++;        /* ... plus the zero padding up to the next datum */
    if (len + 1 > cap) { host_log("hm_lang: %#x: \"%s\" does not fit (%d bytes)", addr, text, cap); return; }
    for (int i = 0; i <= len; i++) dst[i] = text[i];
    for (int i = len + 1; i < cap; i++) dst[i] = 0;
}

void hm_lang_load(void)
{
    void *f = host_fopen("hm_lang.tsv", "rb");
    if (!f) return;
    static char buf[1 << 16];
    unsigned n = host_fread(buf, 1, sizeof buf - 1, f);
    host_fclose(f);
    buf[n] = 0;
    int cap = 0, patched = 0;
    for (char *line = buf; line && *line; ) {
        char *next = line;
        while (*next && *next != '\n') next++;
        if (*next) *next++ = 0; else next = 0;
        int ll = 0;
        while (line[ll]) ll++;
        if (ll && line[ll - 1] == '\r') line[ll - 1] = 0;
        char *tab = line;
        while (*tab && *tab != '\t') tab++;
        if (*line && *line != '#' && *tab) {
            *tab = 0;
            char *to = tab + 1;
            int tolen = unescape(to);
            if (line[0] == '@') {
                unsigned addr = 0;
                for (char *p = line + 1; *p; p++)
                    addr = addr * 16 + (unsigned)(*p >= '0' && *p <= '9' ? *p - '0' : (*p | 32) - 'a' + 10);
                patch_in_place(addr, to, tolen);
                patched++;
            } else {
                unescape(line);
                if (g_n == cap) {
                    struct entry *m = host_malloc((unsigned)(cap ? cap * 2 : 64) * sizeof *m);
                    for (int i = 0; i < g_n; i++) m[i] = g_map[i];
                    if (g_map) host_free(g_map);
                    g_map = m; cap = cap ? cap * 2 : 64;
                }
                g_map[g_n].from = dup(line);
                g_map[g_n].to = dup(to);
                g_n++;
            }
        }
        line = next;
    }
    host_log("hm_lang.tsv: %d texts, %d table entries", g_n, patched);
}

char *hm_text(const char *s)
{
    for (int i = 0; i < g_n; i++) {
        const char *a = g_map[i].from, *b = s;
        while (*a && *a == *b) { a++; b++; }
        if (*a == *b) return g_map[i].to;
    }
    return (char *)s;
}
