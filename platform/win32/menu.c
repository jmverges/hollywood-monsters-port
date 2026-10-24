/* The port's own menu (F9, or X/Square on a controller), drawn over the game as a layer of its own
 * (host_overlay). While it is open the game waits: hm_pump_host_events keeps the game's thread here.
 *
 *   Normal mode   the player plays: stops whatever the guide is doing (the action under way finishes)
 *   Easy mode     the walkthrough's steps (guide/hm_actions.txt) section by section, only those of the room the
 *                 player is in and up to the next one somewhere else, with a check on those done;
 *                 choosing one makes the guide play it (guide.c), then the list comes back
 *   Movie mode    the guide plays the walkthrough from the current step to the end, with nothing drawn over the
 *                 game (it can be recorded); opening the menu pauses it, and the menu offers to stop it
 *   The story     FX Interactive's guide, read from hm_story.txt in the data directory ("== title" lines start
 *                 the chapters; tools/guide/story.py writes it)
 *   Language      Spanish or English (the English fan translation, when installed: lang.c)
 *
 * Keys: arrows, Page Up/Down, Enter, Escape (back); the mouse points, clicks, scrolls and right-clicks back.
 * Texts are UTF-8 and drawn in Latin-1 (menu_font.h). */
#include "hm_platform.h"
#include "menu_font.h"

#define W 640
#define H 480
#define GOLD 0xFFE0B860u
#define WHITE 0xFFF2EEE4u
#define GREY 0xFFA09A8Cu

static unsigned g_px[W * H];
enum { SCR_NONE, SCR_MAIN, SCR_EASY, SCR_STORY_LIST, SCR_STORY };
static int g_screen, g_sel, g_top, g_section;
static int g_movie;                          /* the guide is playing the walkthrough to the end */
static int g_easy_step = -1;                 /* a step chosen in easy mode is being played */
static int g_last_step = -1;
static char g_hud[160];                      /* the line shown over the game while the guide plays */
static unsigned g_msg_until;                 /* ... or a message, until then */

static const char *L(const char *es, const char *en) { return hm_lang_english() ? en : es; }

/* ------------------------------------------------------------------ drawing */
static void blend(int x, int y, unsigned c, unsigned a)
{
    if (x < 0 || y < 0 || x >= W || y >= H || !a) return;
    unsigned *d = &g_px[y * W + x], da = *d >> 24;
    unsigned oa = a + da * (255 - a) / 255;
    if (!oa) return;
    unsigned out = oa << 24;
    for (int s = 0; s <= 16; s += 8) {
        unsigned sc = (c >> s) & 255, dc = (*d >> s) & 255;
        out |= ((sc * a + dc * da * (255 - a) / 255) / oa) << s;
    }
    *d = out;
}

static void rect(int x, int y, int w, int h, unsigned argb)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) blend(i, j, argb & 0xFFFFFF, argb >> 24);
}

static void frame_rect(int x, int y, int w, int h, unsigned argb)
{
    rect(x, y, w, 1, argb); rect(x, y + h - 1, w, 1, argb);
    rect(x, y, 1, h, argb); rect(x + w - 1, y, 1, h, argb);
}

/* the next character of UTF-8 text as Latin-1: typographic quotes and dashes become plain ones, anything else
 * outside Latin-1 '?' */
static int next_char(const char **p)
{
    const unsigned char *s = (const unsigned char *)*p;
    int c = *s++;
    if (c >= 0xC0) {
        int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
        c &= 0x3F >> n;
        while (n-- && (*s & 0xC0) == 0x80) c = (c << 6) | (*s++ & 0x3F);
    }
    *p = (const char *)s;
    if (c == 0x2018 || c == 0x2019) return '\'';
    if (c == 0x201C || c == 0x201D) return '"';
    if (c == 0x2013 || c == 0x2014) return '-';
    return c < 32 || c > 255 || (c >= 0x7F && c < 0xA0) ? '?' : c;
}

static int char_w(const struct menu_font *f, int c) { return f->w[c - 32]; }

static int text_w(const struct menu_font *f, const char *s, int bytes)
{
    int w = 0;
    for (const char *p = s; *p && (bytes < 0 || p < s + bytes); ) w += char_w(f, next_char(&p));
    return w;
}

/* text with its top at y, cut at max_w with "..." (max_w <= 0: no limit); returns the width */
static int text(int x, int y, const struct menu_font *f, const char *s, int bytes, unsigned color, int max_w)
{
    int x0 = x, dots = 3 * char_w(f, '.');
    if (max_w > 0 && text_w(f, s, bytes) <= max_w) max_w = 0;
    for (const char *p = s; *p && (bytes < 0 || p < s + bytes); ) {
        int c = next_char(&p), cw = char_w(f, c);
        if (max_w > 0 && x + cw - x0 > max_w - dots) { c = '.'; cw = char_w(f, c); for (int k = 0; k < 3; k++) x += text(x, y, f, ".", 1, color, 0); break; }
        const unsigned char *g = f->bits + f->off[c - 32];
        for (int j = 0; j < f->height; j++)
            for (int i = 0; i < cw; i++)
                if (g[j * cw + i]) blend(x + i, y + j, color & 0xFFFFFF, g[j * cw + i] * (color >> 24) / 255);
        x += cw;
    }
    return x - x0;
}

/* where the line that starts at s ends to fit in w: the byte count, and *next where the following one starts */
static int wrap(const struct menu_font *f, const char *s, int w, const char **next)
{
    const char *p = s, *brk = 0;
    int x = 0;
    while (*p && *p != '\n') {
        const char *q = p;
        int c = next_char(&q);
        if (c == ' ') brk = p;
        x += char_w(f, c);
        if (x > w && p > s) {
            if (brk) { *next = brk + 1; return (int)(brk - s); }
            *next = p; return (int)(p - s);
        }
        p = q;
    }
    *next = *p == '\n' ? p + 1 : p;
    return (int)(p - s);
}

static void check(int x, int y, int on)
{
    frame_rect(x, y, 14, 14, 0xFF8C8270u);
    if (!on) return;
    static const signed char pts[][2] = { {3,7},{4,8},{5,9},{6,10},{7,9},{8,8},{9,7},{10,6},{11,5},{12,4} };
    for (unsigned k = 0; k < sizeof pts / sizeof pts[0]; k++) rect(x + pts[k][0] - 1, y + pts[k][1] - 1, 2, 3, 0xFF7CD65Cu);
}

static void play_icon(int x, int y, int h, unsigned color)
{
    for (int j = 0; j < h; j++) {
        int len = (j < h / 2 ? j : h - 1 - j) * 2 + 1;
        rect(x, y + j, len, 1, color);
    }
}

static void clear(void) { for (int i = 0; i < W * H; i++) g_px[i] = 0; }

/* ------------------------------------------------------------------ the story */
static char *g_story;                        /* hm_story.txt, lines split in place */
static const char *g_chapter[64], *g_chapter_text[64];
static int g_nchapters, g_chapter_sel;

static void load_story(void)
{
    if (g_story) return;
    void *f = host_fopen("hm_story.txt", "rb");
    if (!f) return;
    host_fseek(f, 0, 2);
    long n = host_ftell(f);
    host_fseek(f, 0, 0);
    g_story = host_malloc((unsigned)n + 1);
    n = (long)host_fread(g_story, 1, (unsigned)n, f);
    host_fclose(f);
    g_story[n] = 0;
    for (char *p = g_story; *p; ) {
        char *line = p;
        while (*p && *p != '\n') p++;
        char *end = p;
        if (*p) p++;
        if (end > line && end[-1] == '\r') end[-1] = ' ';
        if (line[0] == '=' && line[1] == '=' && g_nchapters < 64) {
            line[0] = 0;                             /* the previous chapter's text ends here */
            *end = 0;
            char *title = line + 2;
            while (*title == ' ') title++;
            g_chapter[g_nchapters] = title;
            g_chapter_text[g_nchapters++] = p;
        }
    }
}

/* ------------------------------------------------------------------ the walkthrough's sections */
static int section_count(void)
{
    int n = 0, count = hm_guide_count();
    for (int i = 0; i < count; i++) if (!i || hm_guide_section(i) != hm_guide_section(i - 1)) n++;
    return n;
}

/* first step of section s and how many steps it has */
static int section_steps(int s, int *n)
{
    int k = -1, count = hm_guide_count(), first = 0;
    for (int i = 0; i < count; i++) {
        if (!i || hm_guide_section(i) != hm_guide_section(i - 1)) { if (++k == s + 1) break; first = i; }
        if (k == s) { if (i == first) *n = 0; (*n)++; }
    }
    return first;
}

static int section_of(int step)
{
    int k = -1;
    for (int i = 0; i <= step && i < hm_guide_count(); i++) if (!i || hm_guide_section(i) != hm_guide_section(i - 1)) k++;
    return k < 0 ? 0 : k;
}

static int done(int i) { return hm_guide_done(i); }

static const char *step_label(int i) { const char *n = hm_guide_note(i); return *n ? n : hm_guide_step(i); }

/* the step's verb, as the game names it in the menu's language */
static const char *step_verb(int i)
{
    static const char *const v[][3] = {
        { "go", "Ir a", "Go to" }, { "talk", "Hablar", "Talk to" }, { "take", "Coger", "Take" },
        { "look", "Mirar", "Look at" }, { "use", "Usar", "Use" }, { "open", "Abrir", "Open" },
        { "close", "Cerrar", "Close" }, { "give", "Dar", "Give" }, { "click", "Viajar", "Travel" },
        { "walk", "Caminar", "Walk" }, { "wait", "Esperar", "Wait" }, { "key", "Tecla", "Key" },
        { "mine", "Vagoneta", "Mine cart" }, { "mural", "Mural", "Mural" }, { "maze", "Laberinto", "Maze" },
    };
    const char *s = hm_guide_step(i);
    for (unsigned k = 0; k < sizeof v / sizeof v[0]; k++) {
        const char *a = v[k][0], *b = s;
        while (*a && *a == *b) { a++; b++; }
        if (!*a && (*b == ' ' || !*b)) return v[k][hm_lang_english() ? 2 : 1];
    }
    return "";
}

/* ------------------------------------------------------------------ screens */
static const int PX = 36, PY = 28, PW = W - 72, PH = H - 56, ROW = 24;
static int g_rows, g_row_y0;                 /* rows on screen and where the first one is (mouse) */

static void panel(const char *title)
{
    clear();
    rect(0, 0, W, H, 0xB4000000u);
    rect(PX, PY, PW, PH, 0xEC1C1712u);
    frame_rect(PX, PY, PW, PH, GOLD);
    text(PX + 20, PY + 12, &menu_font_title, title, -1, GOLD, PW - 40);
    rect(PX + 20, PY + 44, PW - 40, 1, 0x80E0B860u);
}

/* the main menu's rows */
enum { MAIN_BACK, MAIN_NORMAL, MAIN_EASY, MAIN_MOVIE, MAIN_STORY, MAIN_LANG, MAIN_ROWS };

static void footer(const char *s) { text(PX + 20, PY + PH - 26, &menu_font_body, s, -1, GREY, PW - 40); }

#ifdef __vita__
/* A Vita button drawn in front of its label (the font is Latin-1, so the shapes are drawn): cross, circle,
 * triangle, square, the d-pad's "updown" and "leftright", or a word in a pill (L, R, START). Returns the width. */
static int same(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }

static int button(int x, int y, const char *b)
{
    int cy = y + menu_font_body.ascent / 2 + 3, cx = x + 7;
    if (same(b, "cross")) {
        for (int i = -5; i <= 5; i++) { rect(cx + i, cy + i, 2, 2, 0xFF7CA8E8u); rect(cx + i, cy - i, 2, 2, 0xFF7CA8E8u); }
        return 16;
    }
    if (same(b, "circle")) {
        for (int j = -7; j <= 7; j++)
            for (int i = -7; i <= 7; i++) { int d = i * i + j * j; if (d >= 26 && d <= 44) rect(cx + i, cy + j, 1, 1, 0xFFE86464u); }
        return 16;
    }
    if (same(b, "triangle")) {
        for (int j = 0; j <= 11; j++) { rect(cx - j / 2, cy - 6 + j, 2, 1, 0xFF64C890u); rect(cx + j / 2, cy - 6 + j, 2, 1, 0xFF64C890u); }
        rect(cx - 6, cy + 5, 14, 2, 0xFF64C890u);
        return 16;
    }
    if (same(b, "square")) { frame_rect(cx - 6, cy - 6, 13, 13, 0xFFD890D0u); frame_rect(cx - 5, cy - 5, 11, 11, 0xFFD890D0u); return 16; }
    if (same(b, "updown") || same(b, "leftright")) {   /* two small arrows */
        int v = b[0] == 'u';
        for (int k = 0; k < 5; k++) {
            if (v) { rect(cx - k, cy - 7 + k, 2 * k + 1, 1, WHITE); rect(cx - k, cy + 7 - k, 2 * k + 1, 1, WHITE); }
            else { rect(cx - 6 + k, cy - k, 1, 2 * k + 1, WHITE); rect(cx + 6 - k, cy - k, 1, 2 * k + 1, WHITE); }
        }
        return 16;
    }
    int w = text_w(&menu_font_body, b, -1) + 8;
    rect(x, y + 1, w, menu_font_body.height, 0xFF4A443Cu);
    text(x + 4, y, &menu_font_body, b, -1, WHITE, 0);
    return w;
}

/* button, label, button, label... along a line; an empty label puts the next button right after this one */
static void hints(int y, const char *const *pairs, int n)
{
    int x = PX + 20;
    for (int i = 0; i < n; i++) {
        x += button(x, y, pairs[2 * i]) + 5;
        if (pairs[2 * i + 1][0]) x += text(x, y, &menu_font_body, pairs[2 * i + 1], -1, GREY, 0) + 18;
    }
}
#define FOOTER_HINTS(...) do { const char *const h_[] = { __VA_ARGS__ }; hints(PY + PH - 26, h_, (int)(sizeof h_ / sizeof h_[0]) / 2); } while (0)
#endif

static void draw_main(void)
{
    panel("Hollywood Monsters");
    const char *items[MAIN_ROWS] = {
        [MAIN_BACK] = L("Volver al juego", "Back to the game"),
        [MAIN_NORMAL] = L("Modo normal: juegas tú", "Normal mode: you play"),
        [MAIN_EASY] = L("Modo fácil: la lista de acciones", "Easy mode: the list of actions"),
        [MAIN_MOVIE] = g_movie ? L("Modo película: en pausa (seguir)", "Movie mode: paused (go on)")
                               : L("Modo película: ver la partida sola", "Movie mode: watch the game play itself"),
        [MAIN_STORY] = L("La historia (guía de FX Interactive)", "The story (FX Interactive's guide)"),
        [MAIN_LANG] = L("Idioma: español", "Language: English"),
    };
    g_rows = MAIN_ROWS; g_row_y0 = PY + 64;
    for (int i = 0; i < g_rows; i++) {
        int y = g_row_y0 + i * (ROW + 10);
        if (i == g_sel) rect(PX + 14, y - 4, PW - 28, ROW + 4, 0x60E0B860u);
        /* which mode the game is in: easy mode is not one that stays on, it plays the action chosen */
        if (i == MAIN_NORMAL) check(PX + 24, y + 2, !g_movie);
        if (i == MAIN_MOVIE) check(PX + 24, y + 2, g_movie);
        text(PX + 46, y, &menu_font_body, items[i], -1, i == g_sel ? WHITE : 0xFFD8D0C0u, PW - 80);
    }
    int n = hm_guide_count();
    if (n) {
        char line[200];
        const char *sec = hm_guide_section(hm_guide_current());
        int k = 0;
        for (const char *p = L("Siguiente acción en: ", "Next action in: "); *p && k < 100; ) line[k++] = *p++;
        for (const char *p = sec; *p && k < 199; ) line[k++] = *p++;
        line[k] = 0;
        text(PX + 20, PY + PH - 52, &menu_font_body, line, -1, GREY, PW - 40);
    }
#ifdef __vita__
    /* what the buttons do in the game, which nothing else on screen says */
    int y = PY + PH - 112;
    rect(PX + 20, y - 8, PW - 40, 1, 0x40E0B860u);
    text(PX + 20, y, &menu_font_body, L("En el juego:", "In the game:"), -1, GOLD, 0);
    {
        const char *const h[] = { "cross", L("acción", "action"), "circle", L("cambiar acción", "change action"),
                                  "triangle", L("inventario", "inventory"), "START", L("saltar", "skip") };
        hints(y + 24, h, 4);
    }
    FOOTER_HINTS("cross", L("elegir", "choose"), "circle", L("volver", "back"), "square", L("cerrar", "close"));
#else
    footer(L("Intro: aceptar   Esc: volver   F9: cerrar", "Enter: choose   Esc: back   F9: close"));
#endif
}

/* What easy mode offers from the section shown: the steps played in the room the game is in, those done and the
 * next ones, up to the first step still to do somewhere else (the later ones here come after it). */
static int g_vis[600];

static int easy_list(void)
{
    int n = 0, count = 0, first = section_steps(g_section, &n), cur = hm_guide_current(), blocked = 0;
    for (int k = 0; k < n && count < 600; k++) {
        int i = first + k;
        if (!hm_guide_here(i)) { if (i >= cur && !done(i)) blocked = 1; continue; }
        if (i < cur || done(i) || !blocked) g_vis[count++] = i;
    }
    return count;
}

static void draw_easy(void)
{
    int nsec = section_count();
    if (g_section >= nsec) g_section = nsec - 1;
    if (g_section < 0) g_section = 0;
    int n = easy_list(), first = section_steps(g_section, &(int){ 0 });
    char title[200];
    int k = 0;
    const char *sec = hm_guide_section(first);
    for (const char *p = sec; *p && k < 150; ) title[k++] = *p++;
    title[k] = 0;
    panel(title);
    g_row_y0 = PY + 58;
    g_rows = (PH - 58 - 40) / ROW;
    if (g_sel >= n) g_sel = n - 1;
    if (g_sel < 0) g_sel = 0;
    if (g_sel < g_top) g_top = g_sel;
    if (g_sel >= g_top + g_rows) g_top = g_sel - g_rows + 1;
    if (!n) {                                    /* nothing of this section in this room */
        int y = g_row_y0;
        const char *s = L("Aquí no hay nada que hacer en esta parte de la guía.", "There is nothing to do here in this part of the guide.");
        text(PX + 24, y, &menu_font_body, s, -1, WHITE, PW - 48);
        int next = hm_guide_current();
        if (next >= 0 && next < hm_guide_count()) {
            text(PX + 24, y + 2 * ROW, &menu_font_body, L("La siguiente acción es en otro sitio:", "The next action is somewhere else:"), -1, GREY, PW - 48);
            text(PX + 24, y + 3 * ROW, &menu_font_body, step_verb(next), -1, GOLD, 86);
            text(PX + 116, y + 3 * ROW, &menu_font_body, step_label(next), -1, WHITE, PW - 140);
        }
    }
    for (int r = 0; r < g_rows && g_top + r < n; r++) {
        int i = g_vis[g_top + r], y = g_row_y0 + r * ROW;
        if (g_top + r == g_sel) rect(PX + 14, y - 3, PW - 28, ROW, 0x60E0B860u);
        check(PX + 22, y, done(i));
        unsigned color = done(i) ? GREY : g_top + r == g_sel ? WHITE : 0xFFD8D0C0u;
        if (i == hm_guide_current() && !done(i)) play_icon(PX + 44, y + 2, 11, GOLD);
        text(PX + 58, y, &menu_font_body, step_verb(i), -1, done(i) ? GREY : GOLD, 86);
        text(PX + 150, y, &menu_font_body, step_label(i), -1, color, PW - 182);
    }
    char hint[160];
    int h = 0;
    const char *a = L("Intro: hacerla   <- ->: otra sección (", "Enter: do it   <- ->: other section (");
    while (*a) hint[h++] = *a++;
    int v = g_section + 1, digits[4], nd = 0;
    do { digits[nd++] = v % 10; v /= 10; } while (v);
    while (nd) hint[h++] = (char)('0' + digits[--nd]);
    hint[h++] = '/';
    v = nsec; do { digits[nd++] = v % 10; v /= 10; } while (v);
    while (nd) hint[h++] = (char)('0' + digits[--nd]);
    a = ")   Esc: volver";
    if (hm_lang_english()) a = ")   Esc: back";
    while (*a) hint[h++] = *a++;
    hint[h] = 0;
#ifdef __vita__
    if (g_msg_until && g_hud[0]) footer(g_hud);
    else {
        char sec_n[16];
        int q = 0;
        for (const char *p = hint; *p; p++) if (*p == '(') { while (*++p && *p != ')' && q < 14) sec_n[q++] = *p; break; }
        sec_n[q] = 0;
        char other[48];
        int o = 0;
        for (const char *p = L("otra sección ", "other section "); *p; ) other[o++] = *p++;
        other[o++] = '(';
        for (int i = 0; sec_n[i]; i++) other[o++] = sec_n[i];
        other[o++] = ')';
        other[o] = 0;
        FOOTER_HINTS("cross", L("hacerla", "do it"), "leftright", other, "circle", L("volver", "back"));
    }
#else
    footer(g_msg_until && g_hud[0] ? g_hud : hint);  /* a message about the last action, for a moment */
#endif
}

static void draw_story_list(void)
{
    load_story();
    panel(L("La historia", "The story"));
    g_row_y0 = PY + 62;
    g_rows = (PH - 62 - 40) / ROW;
    if (!g_nchapters) {
        const char *s = L("No está la guía: falta hm_story.txt en la carpeta de datos del juego "
                          "(tools/guide/story.py la escribe).",
                          "The guide is missing: put hm_story.txt in the game's data directory "
                          "(tools/guide/story.py writes it).");
        int y = g_row_y0;
        for (const char *p = s, *next; *p; p = next, y += ROW) {
            int len = wrap(&menu_font_body, p, PW - 40, &next);
            text(PX + 20, y, &menu_font_body, p, len, WHITE, 0);
        }
#ifdef __vita__
        FOOTER_HINTS("circle", L("volver", "back"));
#else
        footer(L("Esc: volver", "Esc: back"));
#endif
        return;
    }
    if (g_sel >= g_nchapters) g_sel = g_nchapters - 1;
    if (g_sel < g_top) g_top = g_sel;
    if (g_sel >= g_top + g_rows) g_top = g_sel - g_rows + 1;
    for (int r = 0; r < g_rows && g_top + r < g_nchapters; r++) {
        int y = g_row_y0 + r * ROW;
        if (g_top + r == g_sel) rect(PX + 14, y - 3, PW - 28, ROW, 0x60E0B860u);
        text(PX + 24, y, &menu_font_body, g_chapter[g_top + r], -1, g_top + r == g_sel ? WHITE : 0xFFD8D0C0u, PW - 50);
    }
#ifdef __vita__
    FOOTER_HINTS("cross", L("leer", "read"), "L", "", "R", L("página", "page"), "circle", L("volver", "back"));
#else
    footer(L("Intro: leer   Esc: volver", "Enter: read   Esc: back"));
#endif
}

/* the chapter's lines, wrapped; with out == NULL only counts them */
static int story_lines(const char **out, int *len, int max)
{
    int n = 0;
    for (const char *p = g_chapter_text[g_chapter_sel], *next; *p; p = next) {
        int l = wrap(&menu_font_body, p, PW - 48, &next);
        if (out && n < max) { out[n] = p; len[n] = l; }
        n++;
        if (next == p) break;
    }
    return n;
}

static void draw_story(void)
{
    panel(g_chapter[g_chapter_sel]);
    static const char *lines[4000];
    static int lens[4000];
    int n = story_lines(lines, lens, 4000);
    if (n > 4000) n = 4000;
    g_rows = (PH - 58 - 40) / 20;
    if (g_top > n - g_rows) g_top = n - g_rows;
    if (g_top < 0) g_top = 0;
    for (int r = 0; r < g_rows && g_top + r < n; r++)
        text(PX + 24, PY + 58 + r * 20, &menu_font_body, lines[g_top + r], lens[g_top + r], WHITE, 0);
    if (n > g_rows) {                        /* where we are in the chapter */
        int bar = PH - 58 - 40, y = PY + 58 + bar * g_top / n, h = bar * g_rows / n;
        rect(PX + PW - 12, PY + 58, 3, bar, 0x40E0B860u);
        rect(PX + PW - 12, y, 3, h < 8 ? 8 : h, GOLD);
    }
#ifdef __vita__
    FOOTER_HINTS("updown", L("leer", "read"), "L", "", "R", L("página", "page"), "leftright", L("otro capítulo", "other chapter"),
                 "circle", L("volver", "back"));
#else
    footer(L("Flechas: leer   <- ->: otro capítulo   Esc: volver", "Arrows: read   <- ->: other chapter   Esc: back"));
#endif
}

/* the line over the game while the guide plays */
static void draw_hud(void)
{
    clear();
    int w = text_w(&menu_font_body, g_hud, -1) + 52;
    if (w > W - 40) w = W - 40;
    int x = (W - w) / 2;
    rect(x, 8, w, 28, 0xC8000000u);
    frame_rect(x, 8, w, 28, 0x80E0B860u);
    play_icon(x + 12, 15, 13, GOLD);
    text(x + 32, 13, &menu_font_body, g_hud, -1, WHITE, w - 44);
}

static void show(void)
{
    switch (g_screen) {
    case SCR_MAIN: draw_main(); break;
    case SCR_EASY: draw_easy(); break;
    case SCR_STORY_LIST: draw_story_list(); break;
    case SCR_STORY: draw_story(); break;
    default:
        if (g_hud[0]) { draw_hud(); break; }
        host_overlay(0, 0, 0);
        return;
    }
    host_overlay(g_px, W, H);
}

/* ------------------------------------------------------------------ actions */
static void set_hud(const char *prefix, const char *s)
{
    int k = 0;
    while (prefix && *prefix && k < 60) g_hud[k++] = *prefix++;
    while (s && *s && k < (int)sizeof g_hud - 1) g_hud[k++] = *s++;
    g_hud[k] = 0;
}

/* "<prefix><verb>: <what>" for step i */
static void set_hud_step(const char *prefix, int i)
{
    char buf[120];
    int k = 0;
    for (const char *p = step_verb(i); *p && k < 30; ) buf[k++] = *p++;
    if (k) { buf[k++] = ':'; buf[k++] = ' '; }
    for (const char *p = step_label(i); *p && k < (int)sizeof buf - 1; ) buf[k++] = *p++;
    buf[k] = 0;
    set_hud(prefix, buf);
}

static void open_screen(int screen, int sel)
{
    g_screen = screen; g_sel = sel; g_top = 0;
    host_menu_input(screen != SCR_NONE);
    show();
}

static void close_menu(void) { open_screen(SCR_NONE, 0); }

static void open_easy(void)
{
    g_section = section_of(hm_guide_current());
    int n = easy_list(), sel = 0;
    while (sel < n - 1 && done(g_vis[sel])) sel++;
    open_screen(SCR_EASY, sel);
}

static void activate(void)
{
    switch (g_screen) {
    case SCR_MAIN:
        if (g_sel == MAIN_BACK) close_menu();
        else if (g_sel == MAIN_NORMAL) {         /* the action under way finishes, then the player plays */
            if (g_movie || g_easy_step >= 0) hm_guide_stop();
            g_movie = 0; g_easy_step = -1; g_hud[0] = 0;
            close_menu();
        }
        else if (g_sel == MAIN_EASY) { if (hm_guide_count()) open_easy(); }
        else if (g_sel == MAIN_MOVIE && g_movie) close_menu();   /* the paused movie goes on */
        else if (g_sel == MAIN_MOVIE) {          /* from the next step */
            if (!hm_guide_count()) break;
            g_movie = 1; g_easy_step = -1;      /* nothing over the game: the movie can be recorded as it is */
            hm_guide_run(hm_guide_current(), -1);
            close_menu();
        }
        else if (g_sel == MAIN_STORY) {          /* the chapter of the next action, if the walkthrough says it */
            load_story();
            int c = hm_guide_chapter(hm_guide_current());
            if (c > 0 && c <= g_nchapters) g_chapter_sel = c - 1;
            open_screen(SCR_STORY_LIST, g_chapter_sel);
        }
        else if (g_sel == MAIN_LANG) { hm_lang_toggle(); show(); }
        break;
    case SCR_EASY: {
        int n = easy_list();
        if (g_sel < 0 || g_sel >= n) break;
        g_easy_step = g_vis[g_sel]; g_movie = 0;
        hm_guide_run(g_easy_step, 1);
        set_hud_step("", g_easy_step);
        close_menu();
        break;
    }
    case SCR_STORY_LIST:
        if (g_nchapters) { g_chapter_sel = g_sel; open_screen(SCR_STORY, 0); }
        break;
    }
}

static void back(void)
{
    if (g_screen == SCR_STORY) open_screen(SCR_STORY_LIST, g_chapter_sel);
    else if (g_screen == SCR_MAIN) close_menu();
    else open_screen(SCR_MAIN, g_screen == SCR_EASY ? MAIN_EASY : MAIN_STORY);
}

static void move(int d)
{
    if (g_screen == SCR_STORY) { g_top += d; show(); return; }
    int n = g_screen == SCR_MAIN ? MAIN_ROWS : g_screen == SCR_STORY_LIST ? g_nchapters : 0;
    if (g_screen == SCR_EASY) n = easy_list();
    g_sel += d;
    if (g_sel < 0) g_sel = 0;
    if (n && g_sel >= n) g_sel = n - 1;
    show();
}

static void side(int d)
{
    if (g_screen == SCR_EASY) {
        int nsec = section_count();
        g_section += d;
        if (g_section < 0) g_section = 0;
        if (g_section >= nsec) g_section = nsec - 1;
        g_sel = 0; g_top = 0;
        show();
    } else if (g_screen == SCR_STORY && g_chapter_sel + d >= 0 && g_chapter_sel + d < g_nchapters) {
        g_chapter_sel += d; g_top = 0;
        show();
    }
}

void hm_menu_toggle(void)
{
    if (g_screen != SCR_NONE) { close_menu(); return; }
    if (g_easy_step >= 0) {                      /* stop the guide; the action under way finishes */
        hm_guide_stop();
        g_easy_step = -1; g_hud[0] = 0;
    }
    /* a movie only waits while the menu is open (the game waits too): "Back to the game" lets it go on */
    open_screen(SCR_MAIN, g_movie ? MAIN_MOVIE : MAIN_BACK);
}

int hm_menu_is_open(void) { return g_screen != SCR_NONE; }

void hm_menu_event(const struct host_event *ev)
{
    switch (ev->type) {
    case HOST_EV_KEYDOWN:
        switch (ev->vk) {
        case 0x26: move(-1); break;
        case 0x28: move(1); break;
        case 0x21: move(g_screen == SCR_STORY ? -(g_rows - 2) : -g_rows); break;
        case 0x22: move(g_screen == SCR_STORY ? g_rows - 2 : g_rows); break;
        case 0x25: side(-1); break;
        case 0x27: side(1); break;
        case 0x0D: case 0x20: activate(); break;
        case 0x1B: case 0x08: back(); break;
        }
        break;
    case HOST_EV_LANG:                           /* the language changed: the menu's texts with it */
        show();
        break;
    case HOST_EV_WHEEL:
        if (g_screen == SCR_STORY) { g_top -= ev->wheel * 3; show(); }
        else move(-ev->wheel);
        break;
    case HOST_EV_MOUSEMOVE:
    case HOST_EV_MOUSEDOWN: {
        if (ev->type == HOST_EV_MOUSEDOWN && ev->button == 3) { back(); break; }
        if (g_screen == SCR_STORY) break;
        int step = g_screen == SCR_MAIN ? ROW + 10 : ROW, top = g_screen == SCR_MAIN ? 0 : g_top;
        int r = (ev->y - g_row_y0 + 4) / step;
        if (ev->y < g_row_y0 - 4 || r >= g_rows || ev->x < PX || ev->x >= PX + PW) break;
        int n = g_screen == SCR_MAIN ? MAIN_ROWS : g_screen == SCR_STORY_LIST ? g_nchapters : 0;
        if (g_screen == SCR_EASY) n = easy_list();
        if (top + r >= n) break;
        if (g_sel != top + r) { g_sel = top + r; show(); }
        if (ev->type == HOST_EV_MOUSEDOWN && ev->button == 1) activate();
        break;
    }
    }
}

/* Each pump of events: notice the guide's progress, put the line over the game in step with it, and bring the
 * easy mode's list back when its action is over. */
void hm_menu_tick(void)
{
    int cur = hm_guide_current();
    if (cur != g_last_step) {
        g_last_step = cur;
    }
    if ((g_movie || g_easy_step >= 0) && !hm_guide_playing()) {
        int failed = hm_guide_failed();
        if (g_easy_step >= 0) {
            g_easy_step = -1; g_hud[0] = 0;
            if (failed) { set_hud(L("No ha salido; puedes intentarlo otra vez", "It did not work; you can try again"), 0); g_msg_until = host_ticks_ms() + 4000; }
            open_easy();
        } else {
            g_movie = 0;                         /* stopped or over: F9 shows where in the menu */
        }
    }
    if (g_msg_until && (int)(host_ticks_ms() - g_msg_until) >= 0) {
        g_msg_until = 0; g_hud[0] = 0;
        show();
    }
}
