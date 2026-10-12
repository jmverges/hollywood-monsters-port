/* Language: the game's own Spanish, or the English fan translation installed in the en/ subdirectory of the
 * data directory by tools/lang/english_patch.py. The player switches in the port's menu (menu.c);
 * the choice is kept in hm_lang.txt ("en" or "es"), and HM_LANG=en|es overrides it.
 *
 * English is two things:
 *   - en/RESOURCE.*: the translated dialogue, object names and pictures. The host reads files from en/ first
 *     (host_set_overlay_dir). The game reads a room's texts from RESOURCE.003 when it enters the room or loads
 *     a saved game, and keeps them while it stays there.
 *   - en/hm_lang.tsv: texts inside the executable. Lines are
 *        original<TAB>translation   texts the code uses directly, matched by content in hm_text
 *        @address<TAB>translation   entries of tables the code indexes, e.g. the verbs at 0x5e4074, written over
 *                                   the executable's data when they fit their slot (and restored on the way back)
 *     Escapes: \t \n \\ \xNN. Texts are in the game's own code page.
 *
 * So that a room never mixes the two languages, a switch waits for the game to open RESOURCE.003 (the next room
 * or loaded game) and then changes everything at once; a notice says so meanwhile.
 *
 * Both tables stay in memory for the whole run: the game may keep a pointer hm_text returned. A data directory
 * from older versions of the script (a full English copy with hm_lang.tsv at the top) still works, in English
 * only. */
#include "hm_platform.h"

struct entry { char *from, *to; };
struct patch { char *dst, *orig, *to; int cap; };
static struct entry *g_map;
static int g_n;
static struct patch *g_patch;
static int g_np;
static int g_have_en;            /* en/ is installed: the player can switch */
static int g_en;                 /* English is on */
static int g_want;               /* the player's choice, applied on the next room load (hm_lang_file_opening) */

/* The menus' art (verb bar, inventory and options panels, 1024 x 480) as start-up loads it. The options panel
 * draws its labels straight onto that art each time it opens; the same words over the same words do not show,
 * but after a switch the new ones land on the old ones. A switch puts the clean art back. */
#define MENU_ART_SIZE 0x78000
static unsigned char *g_art, *g_art_clean;

void hm_lang_menu_art_loaded(void *art)
{
    g_art = art;
    if (!g_art || g_art_clean) return;
    g_art_clean = host_malloc(MENU_ART_SIZE);
    for (unsigned i = 0; i < MENU_ART_SIZE; i++) g_art_clean[i] = g_art[i];
}

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

static char *dup(const char *s, int n)
{
    char *d = host_malloc((unsigned)n + 1);
    for (int i = 0; i < n; i++) d[i] = s[i];
    d[n] = 0;
    return d;
}

static void *grow(void *old, int n, int *cap, unsigned size)
{
    if (n < *cap) return old;
    int c = *cap ? *cap * 2 : 64;
    char *m = host_malloc((unsigned)c * size), *o = old;
    for (unsigned i = 0; i < (unsigned)n * size; i++) m[i] = o[i];
    if (old) host_free(old);
    *cap = c;
    return m;
}

static void add_patch(unsigned addr, const char *text, int len)
{
    static int cap;
    char *dst = (char *)(addr + host_image_delta());
    int room = 0;
    while (dst[room]) room++;                        /* the original text ... */
    while (room < 256 && dst[room] == 0) room++;     /* ... plus the zero padding up to the next datum */
    if (len + 1 > room) { host_log("hm_lang: %#x: \"%s\" does not fit (%d bytes)", addr, text, room); return; }
    g_patch = grow(g_patch, g_np, &cap, sizeof *g_patch);
    struct patch *p = &g_patch[g_np++];
    p->dst = dst; p->cap = room;
    p->orig = dup(dst, room);
    p->to = dup(text, len);
}

static int load_table(const char *path)
{
    void *f = host_fopen(path, "rb");
    if (!f) return 0;
    static char buf[1 << 16];
    unsigned n = host_fread(buf, 1, sizeof buf - 1, f);
    host_fclose(f);
    buf[n] = 0;
    int cap = 0;
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
                add_patch(addr, to, tolen);
            } else {
                int fromlen = unescape(line);
                g_map = grow(g_map, g_n, &cap, sizeof *g_map);
                g_map[g_n].from = dup(line, fromlen);
                g_map[g_n].to = dup(to, tolen);
                g_n++;
            }
        }
        line = next;
    }
    host_log("%s: %d texts, %d table entries", path, g_n, g_np);
    return 1;
}

/* The verb bar: start-up writes the seven verbs onto its art (0x50fc80) and keeps a plain and a highlighted copy
 * of each button (7 x 27 rows of 0x58 bytes at 0x6d9c00 and 0x6ddcf8). After a switch the port does the same
 * again, from the art as it was before the verbs were written. */
#define VERB_ART_SIZE (0xbd * 0x280)
static unsigned char *g_verbs, *g_verbs_clean;

void hm_lang_verb_art_loaded(void *art)
{
    g_verbs = art;
    if (!g_verbs || g_verbs_clean) return;
    g_verbs_clean = host_malloc(VERB_ART_SIZE);
    for (unsigned i = 0; i < VERB_ART_SIZE; i++) g_verbs_clean[i] = g_verbs[i];
}

static void redraw_verbs(void)
{
    if (!g_verbs || !g_verbs_clean) return;
    for (unsigned i = 0; i < VERB_ART_SIZE; i++) g_verbs[i] = g_verbs_clean[i];
    /* as start-up: x of each label (centred there), the text at its address in the executable or its literal */
    static const struct { unsigned short x; unsigned addr; const char *text; } verb[7] = {
        { 0x33, 0x5d05b8, 0 }, { 0x8c, 0, "Coger" }, { 0xe5, 0, "Mirar" }, { 0x13f, 0x5d05a0, 0 },
        { 0x199, 0, "Abrir" }, { 499, 0, "Cerrar" }, { 0x24c, 0x5d058c, 0 },
    };
    for (int k = 0; k < 7; k++) {
        const char *t = verb[k].addr ? (const char *)(verb[k].addr + host_image_delta()) : verb[k].text;
        FUN_00515480(verb[k].x, 0xb, 0xf1, 0, (byte *)hm_text(t), 1);
    }
    static const unsigned char width[7] = { 0x57, 0x57, 0x58, 0x58, 0x58, 0x57, 0x57 };
    const unsigned short *x = (const unsigned short *)(0x5e4054 + host_image_delta());
    unsigned char *lit = (unsigned char *)(0x6ddcf8 + host_image_delta());
    for (int b = 0; b < 7; b++) {
        const unsigned char *src = g_verbs + 0x1400 + x[b];
        for (int row = 0; row < 27; row++, src += 0x280, lit += 0x58) {
            unsigned char *plain = lit - 0x40f8;
            for (int i = 0; i < 0x58; i++) plain[i] = lit[i] = src[i];
            for (int i = 0; i < width[b]; i++) if (lit[i]) lit[i] += 9;
        }
    }
    *(unsigned short *)(0x688a42 + host_image_delta()) = width[6];
}

static void apply(int en)
{
    for (int i = 0; i < g_np; i++) {
        const char *src = en ? g_patch[i].to : g_patch[i].orig;
        int k = 0;
        for (; src[k] && k < g_patch[i].cap; k++) g_patch[i].dst[k] = src[k];
        for (; k < g_patch[i].cap; k++) g_patch[i].dst[k] = en ? 0 : g_patch[i].orig[k];
    }
    if (g_art && g_art_clean && g_en != en)
        for (unsigned i = 0; i < MENU_ART_SIZE; i++) g_art[i] = g_art_clean[i];
    int switched = g_en != en;
    g_en = en;
    host_set_overlay_dir(en && g_have_en ? "en" : 0);
    if (switched) redraw_verbs();
}

static int wants_english(void)
{
    const char *env = hm_getenv("HM_LANG");
    char saved[4] = "";
    if (!env) {
        void *f = host_fopen("hm_lang.txt", "rb");
        if (f) { host_fread(saved, 1, 2, f); host_fclose(f); }
        env = saved;
    }
    return (env[0] | 32) == 'e' && (env[1] | 32) == 'n';
}

void hm_lang_load(void)
{
    if (load_table("en/hm_lang.tsv")) {
        g_have_en = 1;
        apply(g_want = wants_english());
    } else if (load_table("hm_lang.tsv")) {          /* older full English copy of the data */
        apply(g_want = 1);
    }
    host_log("language: %s%s", g_en ? "English" : "Spanish", g_have_en ? " (the port's menu switches)" : "");
}

void hm_lang_toggle(void)
{
    if (!g_have_en) {
        host_log("language: no en/ directory with the English translation (tools/lang/english_patch.py)");
        host_osd(g_en ? "ENGLISH ONLY" : "SOLO ESPAÑOL");
        return;
    }
    g_want = !g_want;
    void *f = host_fopen("hm_lang.txt", "wb");
    if (f) { host_fwrite(g_want ? "en\n" : "es\n", 1, 3, f); host_fclose(f); }
    host_log("language: %s%s", g_want ? "English" : "Spanish", g_want != g_en ? " from the next room" : "");
    if (g_want == g_en) host_osd(g_en ? "ENGLISH" : "ESPAÑOL");
    else host_osd(g_want ? "ENGLISH IN THE NEXT ROOM" : "ESPAÑOL EN LA SIGUIENTE SALA");
}

void hm_lang_file_opening(const char *name)
{
    if (g_want == g_en) return;
    const char *p = name, *base = name;
    for (; *p; p++) if (*p == '\\' || *p == '/' || *p == ':') base = p + 1;
    const char *want = "RESOURCE.003";
    int i = 0;
    while (want[i] && (base[i] == want[i] || (base[i] >= 'a' && base[i] <= 'z' && base[i] - 32 == want[i]))) i++;
    if (want[i] || base[i]) return;
    apply(g_want);
    host_log("language: %s now", g_en ? "English" : "Spanish");
}

char *hm_text(const char *s)
{
    /* "Introduce una descripción" (save prompt): the game now reads typed characters */
    if (s == (const char *)(0x005d06b8u + host_image_delta())) {
        static unsigned last;
        unsigned now = host_ticks_ms();
        if (now - last > 1000) host_request_text_input();
        last = now;
    }
    if (!g_en) return (char *)s;
    for (int i = 0; i < g_n; i++) {
        const char *a = g_map[i].from, *b = s;
        while (*a && *a == *b) { a++; b++; }
        if (*a == *b) return g_map[i].to;
    }
    return (char *)s;
}

int hm_lang_english(void) { return g_want; }
