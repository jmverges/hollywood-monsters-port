/* Guide: plays walkthrough actions ("pick up glass", "use glass with punch bowl") the way a player would, with the
 * mouse: the verb from the verb bar (right button held), the object where it is on screen, inventory items from the
 * inventory panel. Everything is read from the game's own state, so the actions work in any language and do not
 * depend on fixed screen positions:
 *
 *   room objects    names at 0x5e7168 + id*41; a 1024-wide map of screen regions (pointer at 0x6c4cd0, scrolled
 *                   by 0x700f62 / 0x6c4c94) and the region -> object table at 0x7010a0
 *   inventory       global item ids, names at 0x5e5e00 + id*41; per character (0x6b61cc) the items at
 *                   0x6c4b80 + c*0x79, how many (0x6ceade + c) and the first one shown (0x68912c + c); the panel
 *                   (open when 0x5e5dfb != 0) has 2 rows of 8 cells of 68 pixels from (50, 338)
 *   verbs           current verb 0x6d2bb8: 1 go to, 2 talk to, 3 pick up, 4 look at, 5 use, 6 open, 7 close, 8 give
 *   control         0x688fe4 is 1 while the player has control (the game's own enable/disable at 0x514d50/0x514dd0)
 *   room            0x6ca1be
 *   dialogue menu   0x6c4cbc is 1 while one is on screen, 0x6d3e75 lines, the option of each line at 0x6d2140,
 *                   the lines at 0x6b59a0 + i*0x65; the panel is 24 + 21 * lines high at the bottom (0x46a960)
 *
 * The walkthrough is hm_actions.txt in the data directory, one step per line:
 *   <verb> <thing> [<thing>] [say a,b,...] [until <condition>]
 *     verb: go talk take look use open close give; thing: oN (room object N) or iN (inventory item N)
 *     say: in the dialogue menus that follow, choose these options (1 = the top one) first, or @address for
 *       the option whose 0-based number the game keeps at that address (the fisherman's quiz); after them, and
 *       without "say", the guide talks about everything: each option not chosen yet, top to bottom, and the
 *       last one (usually goodbye) when nothing new is left
 *     read: if the action opens a reading screen (control does not come back), close it with Escape
 *     nowait: do not wait for the game to give control back afterwards
 *     fast: no waiting for the game to settle before (actions with a short window, like taking the key while
 *       the gorilla drinks): it waits for the object to show up and then goes for it quickly
 *     until: repeat the action (up to 30 times) until the condition holds: "has iN", "room 0xN", "gone oN",
 *       "moved" (in another room than when the step started), "mem 0xADDR = V" (a byte of the game's state),
 *       "named iA iB" (item A shows item B's name)
 *   wait <ms>                  let the game go on (music, animations); with "until", check every <ms> for up to
 *                              a minute
 *   click <x> <y>              a click on a screen without objects (the travel agenda)
 *   walk <x> <y>               walk to that point of the room (room coordinates: the screen's plus the scroll)
 *   key <vk>                   a key, without waiting for control first (27: Escape closes a reading screen)
 *   maze [back]                walk Egypt's maze along the beetle's path (from inside it), or back to the mural
 *   mural                      solve the mural puzzle in Egypt (after using the papyrus with the mural)
 *   mine <room>                set the switch levers of Australia's mine for that room and ride the cart
 *   snap <name>                write the game's data (0x660000..0x710000) to snap_<name>.bin (finding state while
 *                              writing the walkthrough)
 *   == title                   a section of the walkthrough
 *   # comment
 *
 * Commands (from a test script, "guide <command>", and later from the port's menu):
 *   probe                      log the room, the objects on screen and the inventory
 *   do <step>                  one action, written as in the walkthrough
 *   play [n]                   play the walkthrough from the current step (n steps, or to the end)
 *   goto <n>                   make step n (1-based line of steps) the current one
 *   stop                       stop playing after the current action
 *   save <row> <name>          save the game in that row of the game's list (1 = top), through its menu
 *   load <row>                 load the game in that row
 *   find <text>                log the inventory items whose name contains the text (writing the walkthrough)
 */
#include "hm_platform.h"

/* ------------------------------------------------------------------ game state */
#define GA(a) ((a) + host_image_delta())
static unsigned g8(unsigned a) { return *(volatile unsigned char *)GA(a); }
static unsigned g16(unsigned a) { return *(volatile unsigned short *)GA(a); }
static unsigned g32(unsigned a) { return *(volatile unsigned *)GA(a); }

enum { VERB_GO = 1, VERB_TALK, VERB_TAKE, VERB_LOOK, VERB_USE, VERB_OPEN, VERB_CLOSE, VERB_GIVE };
static const char *const verb_names[] = { "", "go", "talk", "take", "look", "use", "open", "close", "give" };

static int room_id(void) { return (int)g16(0x6ca1be); }
static int in_control(void) { return g8(0x688fe4) != 0; }
static int inventory_open(void) { return g8(0x5e5dfb) != 0; }
static int current_verb(void) { return (int)g8(0x6d2bb8); }
static const char *object_name(int id) { return (const char *)GA(0x5e7168 + (unsigned)id * 41); }
static const char *item_name(int id) { return (const char *)GA(0x5e5e00 + (unsigned)id * 41); }

static int object_at(int x, int y)
{
    const unsigned char *map = (const unsigned char *)(unsigned long)g32(0x6c4cd0);
    if (!map || x < 0 || x >= 640 || y < 0 || y > 0x1df) return 0;
    int sx = (short)g16(0x700f62), sy = (int)g16(0x6c4c94);
    long i = x + sx + (long)(sy + y) * 0x400;
    if (i < 0 || i >= 0x78000) return 0;            /* the map is 1024 x 480 (0x40bf70); the ending scrolls further */
    return (int)g8(0x7010a0 + map[i]);
}

/* A point on the object: the pixel of the object closest to the centre of its pixels (an object can be a ring or
 * have holes). The bottom rows are left out: the pointer there opens the inventory. */
static int object_point(int id, int *px, int *py)
{
    long n = 0, sx = 0, sy = 0;
    for (int y = 0; y < 466; y += 2)
        for (int x = 0; x < 640; x += 2)
            if (object_at(x, y) == id) { n++; sx += x; sy += y; }
    if (!n) return 0;
    int cx = (int)(sx / n), cy = (int)(sy / n);
    long best = -1;
    for (int y = 0; y < 466; y += 2)
        for (int x = 0; x < 640; x += 2)
            if (object_at(x, y) == id) {
                long d = (long)(x - cx) * (x - cx) + (long)(y - cy) * (y - cy);
                if (best < 0 || d < best) { best = d; *px = x; *py = y; }
            }
    return 1;
}

/* A point of a raw screen region (the map's value itself, not an object): the mine's switch panel uses them. */
static int region_point(int r, int *px, int *py)
{
    const unsigned char *map = (const unsigned char *)(unsigned long)g32(0x6c4cd0);
    if (!map) return 0;
    int sx = (short)g16(0x700f62);
    long n = 0, cx = 0, cy = 0;
    for (int y = 0; y < 480; y += 2)
        for (int x = 0; x < 640; x += 2)
            if (map[x + sx + y * 0x400] == r) { n++; cx += x; cy += y; }
    if (!n) return 0;
    cx /= n; cy /= n;
    long best = -1;
    for (int y = 0; y < 480; y += 2)
        for (int x = 0; x < 640; x += 2)
            if (map[x + sx + y * 0x400] == r) {
                long d = (x - cx) * (x - cx) + (y - cy) * (y - cy);
                if (best < 0 || d < best) { best = d; *px = x; *py = y; }
            }
    return 1;
}

/* Rooms can be wider than the screen (the map is 1024 wide, scrolled by 0x700f62). Where is an object that is not
 * on screen: -1 to the left, 1 to the right, 0 nowhere. */
static int object_side(int id)
{
    const unsigned char *map = (const unsigned char *)(unsigned long)g32(0x6c4cd0);
    if (!map) return 0;
    int sx = (short)g16(0x700f62);
    long n = 0, sum = 0;
    for (int y = 0; y < 466; y += 4)
        for (int x = 0; x < 1024; x += 4)
            if (g8(0x7010a0 + map[x + y * 0x400]) == (unsigned)id) { n++; sum += x; }
    if (!n) return 0;
    return sum / n < sx + 320 ? -1 : 1;
}

/* A spot of free floor (no object) near the left or right edge of the screen, to walk towards. */
static int edge_spot(int side, int *px, int *py)
{
    for (int dx = 8; dx < 200; dx += 8)
        for (int y = 440; y > 250; y -= 10) {
            int x = side < 0 ? dx : 639 - dx;
            if (!object_at(x, y)) { *px = x; *py = y; return 1; }
        }
    return 0;
}

/* The inventory of the current character: items[0..n) */
static int inventory(unsigned char *items, int max)
{
    unsigned c = g8(0x6b61cc), count = g8(0x6ceade + c);
    int n = 0;
    for (unsigned k = 0; k <= count && n < max; k++) {
        unsigned it = g8(0x6c4b80 + c * 0x79 + k);
        if (it) items[n++] = (unsigned char)it;
    }
    return n;
}

/* Which way to scroll the inventory panel to show an item: -1 up, 1 down, 0 shown or not carried. */
static int item_scroll(int item)
{
    unsigned c = g8(0x6b61cc), first = g8(0x68912c + c), count = g8(0x6ceade + c);
    for (unsigned k = 0; k <= count; k++)
        if ((int)g8(0x6c4b80 + c * 0x79 + k) == item) return k < first ? -1 : k >= first + 16 ? 1 : 0;
    return 0;
}

/* Where an item is in the open inventory panel. */
static int item_point(int item, int *px, int *py)
{
    unsigned c = g8(0x6b61cc), first = g8(0x68912c + c), count = g8(0x6ceade + c);
    for (int row = 0; row < 2; row++)
        for (int col = 0; col < 8; col++) {
            unsigned k = first + (unsigned)(row * 8 + col);
            if (k > count) continue;
            if ((int)g8(0x6c4b80 + c * 0x79 + k) == item) { *px = 50 + col * 68 + 32; *py = 338 + row * 68 + 32; return 1; }
        }
    return 0;
}

static const char *trim(const char *s) { while (*s == ' ') s++; return s; }

/* A game text (code page 850) as UTF-8, for the log: the letters Spanish needs, '?' for anything else. */
static const char *utf8(const char *s)
{
    static char buf[4][96];
    static int k;
    char *o = buf[k = (k + 1) & 3], *end = o + sizeof buf[0] - 3;
    static const unsigned char from[] = { 0xa0, 0x82, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0x81, 0xb5, 0x90, 0xd6, 0xe0, 0xe9, 0xad, 0xa8, 0xae, 0xaf, 0x87 };
    static const char *const to[] = { "á", "é", "í", "ó", "ú", "ñ", "Ñ", "ü", "Á", "É", "Í", "Ó", "Ú", "¡", "¿", "«", "»", "ç" };
    for (const unsigned char *p = (const unsigned char *)trim(s); *p && o < end; p++) {
        if (*p < 0x80) { *o++ = (char)*p; continue; }
        const char *t = "?";
        for (unsigned i = 0; i < sizeof from; i++) if (from[i] == *p) t = to[i];
        while (*t && o < end) *o++ = *t++;
    }
    *o = 0;
    return buf[k];
}

static void probe(void)
{
    host_log("guide: room %#x, verb %d, control %d, inventory %s, scroll %d,%d", room_id(), current_verb(), in_control(),
             inventory_open() ? "open" : "closed", (short)g16(0x700f62), (int)g16(0x6c4c94));
    host_log("guide:   flags 6ca139=%u 69c875=%u 6d2bbc=%u menu=%u", g8(0x6ca139), g8(0x69c875), g8(0x6d2bbc), g8(0x6c4cbc));
    int seen[256] = { 0 };
    for (int y = 0; y < 466; y += 4)
        for (int x = 0; x < 640; x += 4) seen[object_at(x, y)] = 1;
    for (int id = 1; id < 256; id++) {
        int x, y;
        if (!seen[id] || !object_point(id, &x, &y)) continue;
        char verbs[64];
        int k = 0;
        for (int v = 2; v <= 8; v++)                 /* the verbs this object answers to (0x692cbc) */
            if (g16(0x692cbc + (unsigned)(v + id * 8) * 4)) { const char *n = verb_names[v]; while (*n) verbs[k++] = *n++; verbs[k++] = ' '; }
        verbs[k] = 0;
        host_log("guide:   o%d \"%s\" at %d,%d  %s", id, utf8(object_name(id)), x, y, verbs);
    }
    for (int id = 1; id < 64; id++) {                /* in the room, off screen */
        if (seen[id]) continue;
        int side = object_side(id);
        if (side) host_log("guide:   o%d \"%s\" off screen to the %s", id, utf8(object_name(id)), side < 0 ? "left" : "right");
        else if (id < 39 && *utf8(object_name(id)) && *utf8(object_name(id)) != ' ')
            host_log("guide:   o%d \"%s\" not in the room now", id, utf8(object_name(id)));
    }
    unsigned char items[128];
    int n = inventory(items, (int)sizeof items);
    for (int i = 0; i < n; i++) host_log("guide:   i%d \"%s\"", items[i], utf8(item_name(items[i])));
}

/* ------------------------------------------------------------------ dialogue menus */
static int menu_open(void) { return g8(0x6c4cbc) != 0 && g8(0x6d3e75) != 0; }   /* set by 0x46a470, cleared by 0x46a4a0 */

static unsigned line_hash(int line)
{
    unsigned h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)GA(0x6b59a0 + (unsigned)line * 0x65); *p; p++) h = (h ^ *p) * 16777619u;
    return h;
}

static unsigned g_said[64];          /* options chosen in this conversation (hash of their first line) */
static int g_nsaid;
static int g_say[16], g_nsay, g_say_i;

/* The screen line to click: the first line of the chosen option. */
static int choose_line(void)
{
    int lines = (int)g8(0x6d3e75), first[8], nopt = 0;
    for (int i = 0; i < lines && nopt < 8; i++)
        if (i == 0 || g8(0x6d2140 + i) != g8(0x6d2140 + i - 1)) first[nopt++] = i;
    if (!nopt) return -1;
    for (int k = 0; k < nopt; k++)                   /* the whole menu, for writing "say" lists */
        host_log("guide:   menu %d: %s", k + 1, utf8((const char *)GA(0x6b59a0 + (unsigned)first[k] * 0x65)));
    if (g_say_i < g_nsay) {
        int k = g_say[g_say_i++] - 1;
        if (k < -1) k = (int)g8((unsigned)(-(k + 2)));   /* @address: the option the game keeps there (0-based) */
        if (k >= 0 && k < nopt) return first[k];
    }
    for (int k = 0; k < nopt - 1; k++) {
        unsigned h = line_hash(first[k]);
        int said = 0;
        for (int j = 0; j < g_nsaid; j++) if (g_said[j] == h) said = 1;
        if (!said) { if (g_nsaid < 64) g_said[g_nsaid++] = h; return first[k]; }
    }
    return first[nopt - 1];
}

static void line_point(int line, int *x, int *y)
{
    int lines = (int)g8(0x6d3e75), top = 480 - (24 + 21 * lines);
    *x = 200;
    *y = top + 12 + 21 * line + 10;
}

/* ------------------------------------------------------------------ playing an action */
enum { OP_MOVE, OP_DOWN, OP_UP, OP_WAIT, OP_IDLE, OP_INV_OPEN, OP_INV_CLOSE, OP_OBJ, OP_ITEM, OP_VERB, OP_LINE, OP_KEY, OP_CHAR, OP_AT, OP_APPEAR, OP_REGION, OP_VANISH, OP_CONTROL, OP_CLOSE_READING, OP_MURAL, OP_MAZE, OP_CELL, OP_LEAVE };
struct op { int kind, a, b; };
#define MAX_OPS 64
static struct op g_ops[MAX_OPS];
static int g_nops, g_op;
static unsigned g_op_t0, g_stable_since;
static int g_from_x, g_from_y, g_ptr_x = 320, g_ptr_y = 240;
static int g_failed;
static int g_tries;                  /* attempts of the current walkthrough step (0: not playing a step) */
static int g_fast;                   /* the current action is "fast": wait for missing objects */
static int g_walks;                  /* walks towards an off-screen object in this action */

static void insert_op(int at, int kind, int a)
{
    if (g_nops >= MAX_OPS && g_op > 0) {             /* drop the ops already done */
        for (int i = g_op; i < g_nops; i++) g_ops[i - g_op] = g_ops[i];
        g_nops -= g_op; at -= g_op; g_op = 0;
    }
    if (g_nops >= MAX_OPS) { host_log("guide: too many queued ops"); return; }
    for (int i = g_nops; i > at; i--) g_ops[i] = g_ops[i - 1];
    g_nops++;
    g_ops[at].kind = kind; g_ops[at].a = a; g_ops[at].b = 0;
}

static void push(int kind, int a, int b) { if (g_nops >= MAX_OPS) host_log("guide: too many queued ops"); if (g_nops < MAX_OPS) { g_ops[g_nops].kind = kind; g_ops[g_nops].a = a; g_ops[g_nops].b = b; g_nops++; } }

static void send_pointer(int type, int x, int y, int button)
{
    struct host_event e;
    char *p = (char *)&e;
    for (unsigned i = 0; i < sizeof e; i++) p[i] = 0;
    e.type = type; e.x = x; e.y = y; e.button = button;
    g_ptr_x = x; g_ptr_y = y;
    hm_inject_event(&e);
}

int hm_guide_busy(void) { return g_op < g_nops; }

static void fail(const char *why)
{
    host_log("guide: action stopped: %s", why);
    g_failed = 1;
    g_nops = g_op = 0;
}

/* thing: 'o' + room object id, or 'i' + item id */
static void push_thing(char kind, int id)
{
    if (kind == 'o') { push(OP_INV_CLOSE, 0, 0); push(OP_OBJ, id, 0); push(OP_DOWN, 1, 0); push(OP_WAIT, 60, 0); push(OP_UP, 1, 0); }
    else { push(OP_INV_OPEN, 0, 0); push(OP_ITEM, id, 0); push(OP_DOWN, 1, 0); push(OP_WAIT, 60, 0); push(OP_UP, 1, 0); }
    push(OP_WAIT, 250, 0);
}

static int parse_thing(const char **s, char *kind, int *id)
{
    const char *p = trim(*s);
    if (*p != 'o' && *p != 'i') return 0;
    *kind = *p++;
    int v = 0, digits = 0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p++ - '0'); digits++; }
    if (!digits) return 0;
    *id = v; *s = p;
    return 1;
}

/* ------------------------------------------------------------------ conditions */
static int read_num(const char **s)
{
    const char *p = trim(*s);
    int base = 10, v = 0;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
    for (;; p++) {
        int d = *p >= '0' && *p <= '9' ? *p - '0' : base == 16 && (*p | 32) >= 'a' && (*p | 32) <= 'f' ? (*p | 32) - 'a' + 10 : -1;
        if (d < 0) break;
        v = v * base + d;
    }
    *s = p;
    return v;
}

static int has_item(int item)
{
    unsigned char items[128];
    int n = inventory(items, (int)sizeof items);
    for (int i = 0; i < n; i++) if (items[i] == item) return 1;
    return 0;
}

static int on_screen(int id)
{
    for (int y = 0; y < 466; y += 4)
        for (int x = 0; x < 640; x += 4) if (object_at(x, y) == id) return 1;
    return 0;
}

static int g_step_room;              /* the room when the current step started, for "moved" */

/* "has iN", "room 0xN", "gone oN", "moved", "mem 0xADDR = V"; an empty condition holds */
static int condition(const char *c)
{
    c = trim(c);
    if (!*c) return 1;
    int neg = 0;
    if (c[0] == 'n' && c[1] == 'o' && c[2] == 't' && c[3] == ' ') { neg = 1; c = trim(c + 4); }
    int r = 1;
    if (c[0] == 'h' && c[1] == 'a' && c[2] == 's') { c += 3; c = trim(c); if (*c == 'i') c++; r = has_item(read_num(&c)); }
    else if (c[0] == 'r' && c[1] == 'o' && c[2] == 'o' && c[3] == 'm') { c += 4; r = room_id() / 10 == read_num(&c) / 10; }  /* rooms go in tens, the last digit is a state: 0x1b80..0x1b82 */
    else if (c[0] == 'm' && c[1] == 'o' && c[2] == 'v' && c[3] == 'e' && c[4] == 'd') r = room_id() / 10 != g_step_room / 10;
    else if (c[0] == 'n' && c[1] == 'a' && c[2] == 'm' && c[3] == 'e' && c[4] == 'd') {
        /* named iA iB: item A now shows item B's name (the multi-tool knife keeps its number and changes its name) */
        c = trim(c + 5); if (*c == 'i') c++;
        int a = read_num(&c);
        c = trim(c); if (*c == 'i') c++;
        int b = read_num(&c);
        const char *x = trim(item_name(a)), *y = trim(item_name(b));
        while (*x && *x == *y) { x++; y++; }
        r = *x == *y;
    }
    else if (c[0] == 'm' && c[1] == 'e' && c[2] == 'm' && c[3] == ' ') {   /* mem 0xADDR = V: a byte of the game's state */
        c += 4;
        unsigned a = (unsigned)read_num(&c);
        c = trim(c);
        if (*c == '=') c++;
        r = (int)g8(a) == read_num(&c);
    }
    else if (c[0] == 'g' && c[1] == 'o' && c[2] == 'n' && c[3] == 'e') { c = trim(c + 4); if (*c == 'o') c++; r = !on_screen(read_num(&c)); }
    else host_log("guide: unknown condition \"%s\"", c);
    return neg ? !r : r;
}

static int start_action(const char *s)
{
    s = trim(s);
    int walk = s[0] == 'w' && s[1] == 'a' && s[2] == 'l' && s[3] == 'k';
    if (walk || (s[0] == 'c' && s[1] == 'l' && s[2] == 'i' && s[3] == 'c' && s[4] == 'k')) {   /* click <x> <y>: screens without objects */
        s += walk ? 4 : 5;
        int x = read_num(&s), y = read_num(&s);
        if (walk) x -= (short)g16(0x700f62);         /* walk <x> <y>: a point of the room, wherever it has scrolled */
        g_nops = g_op = 0; g_failed = 0; g_fast = 0;
        push(OP_IDLE, 0, 0);
        push(OP_AT, x, y); push(OP_DOWN, 1, 0); push(OP_WAIT, 60, 0); push(OP_UP, 1, 0);
        push(OP_IDLE, 0, 0);
        g_op_t0 = host_ticks_ms();
        return 1;
    }
    if (s[0] == 'm' && s[1] == 'i' && s[2] == 'n' && s[3] == 'e' && s[4] == ' ') {
        /* mine <room>: Australia's mine cart. Where it goes depends on two switch levers through a table the game
         * shuffles for each new game (9 ints at 0x69d240, room = table[left * 3 + right]; levers at 0x6b69e4/5).
         * Set the levers in the switch panel (object 5: regions 1-3 left lever, 4-6 right lever, 7 leaves), then
         * use the cart (object 1). */
        s += 5;
        int room = read_num(&s), at = -1;
        char t[64];
        int k = 0;
        for (int i = 0; i < 9; i++) {
            int v = (int)g32(0x69d240 + (unsigned)i * 4);
            if (v / 10 == room / 10) at = i;
            for (int d = 1000; d; d /= 10) t[k++] = (char)('0' + v / d % 10);
            t[k++] = ' ';
        }
        t[k] = 0;
        host_log("guide: mine table %s", t);
        if (at < 0) { host_log("guide: no switch setting leads to room %#x", room); return 0; }
        host_log("guide: mine: room %#x is levers %d,%d (now %d,%d)", room, at / 3, at % 3, (int)g8(0x6b69e4), (int)g8(0x6b69e5));
        g_nops = g_op = 0; g_failed = 0; g_fast = 0;
        push(OP_IDLE, 0, 0);
        if ((int)g8(0x6b69e4) != at / 3 || (int)g8(0x6b69e5) != at % 3) {
            /* the panel replaces the room (the cart is no longer in the region map) until region 7 closes it */
            push(OP_VERB, VERB_USE, 0); push_thing('o', 5); push(OP_VANISH, 1, 0); push(OP_WAIT, 800, 0);
            push(OP_REGION, 1 + at / 3, 0); push(OP_DOWN, 1, 0); push(OP_UP, 1, 0); push(OP_WAIT, 500, 0); push(OP_CONTROL, 0, 0);
            push(OP_REGION, 4 + at % 3, 0); push(OP_DOWN, 1, 0); push(OP_UP, 1, 0); push(OP_WAIT, 500, 0); push(OP_CONTROL, 0, 0);
            push(OP_REGION, 7, 0); push(OP_DOWN, 1, 0); push(OP_UP, 1, 0); push(OP_APPEAR, 1, 0); push(OP_IDLE, 0, 0);
        }
        push(OP_VERB, VERB_USE, 0); push_thing('o', 1);
        push(OP_LEAVE, 0, 0); push(OP_IDLE, 0, 0);
        g_op_t0 = host_ticks_ms();
        return 1;
    }
    if (s[0] == 'm' && s[1] == 'a' && s[2] == 'z' && s[3] == 'e') {   /* maze [back]: the beetle's path through Egypt's maze */
        s = trim(s + 4);
        g_nops = g_op = 0; g_failed = 0; g_fast = 0;
        push(OP_IDLE, 0, 0);
        push(OP_MAZE, s[0] == 'b' ? -1 : 0, 0);
        push(OP_IDLE, 0, 0);
        g_op_t0 = host_ticks_ms();
        return 1;
    }
    if (s[0] == 'm' && s[1] == 'u' && s[2] == 'r' && s[3] == 'a' && s[4] == 'l') {   /* mural: solve Egypt's tile puzzle */
        g_nops = g_op = 0; g_failed = 0; g_fast = 0;
        push(OP_CONTROL, 0, 0);
        push(OP_MURAL, 0, 0);
        push(OP_WAIT, 2000, 0);
        g_op_t0 = host_ticks_ms();
        return 1;
    }
    if (s[0] == 's' && s[1] == 'n' && s[2] == 'a' && s[3] == 'p' && s[4] == ' ') {
        /* snap <name>: the game's data (0x660000..0x710000) to snap_<name>.bin, to find state while writing the walkthrough */
        char path[96] = "snap_";
        int k = 5;
        for (const char *q = trim(s + 5); *q && *q != ' ' && k < 80; q++) path[k++] = *q;
        path[k] = 0;
        const char *tail = ".bin";
        while (*tail) path[k++] = *tail++;
        path[k] = 0;
        void *f = host_fopen(path, "wb");
        if (f) { host_fwrite((const void *)(unsigned long)GA(0x660000), 1, 0xb0000, f); host_fclose(f); host_log("guide: %s written", path); }
        g_nops = g_op = 0; g_failed = 0; g_fast = 0;
        push(OP_WAIT, 10, 0);
        g_op_t0 = host_ticks_ms();
        return 1;
    }
    if (s[0] == 'p' && s[1] == 'r' && s[2] == 'o' && s[3] == 'b' && s[4] == 'e') {   /* probe: log the state (writing) */
        probe();
        g_nops = g_op = 0; g_failed = 0; g_fast = 0;
        push(OP_WAIT, 10, 0);
        g_op_t0 = host_ticks_ms();
        return 1;
    }
    if (s[0] == 'k' && s[1] == 'e' && s[2] == 'y' && s[3] == ' ') {   /* key <vk>: e.g. 27 closes a reading screen */
        s += 4;
        int vk = read_num(&s);
        g_nops = g_op = 0; g_failed = 0; g_fast = 0;
        push(OP_KEY, vk, 0); push(OP_WAIT, 500, 0); push(OP_IDLE, 0, 0);
        g_op_t0 = host_ticks_ms();
        return 1;
    }
    if (s[0] == 'w' && s[1] == 'a' && s[2] == 'i' && s[3] == 't') {   /* wait <ms>: let the game go on */
        s += 4;
        g_nops = g_op = 0; g_failed = 0;
        push(OP_WAIT, read_num(&s), 0);
        g_op_t0 = host_ticks_ms();
        return 1;
    }
    int verb = 0;
    for (int v = 1; v <= 8; v++) {
        const char *n = verb_names[v];
        int k = 0;
        while (n[k] && s[k] == n[k]) k++;
        if (!n[k] && (s[k] == ' ' || !s[k])) { verb = v; s += k; break; }
    }
    char k1, k2;
    int id1, id2;
    if (!verb || !parse_thing(&s, &k1, &id1)) { host_log("guide: bad action \"%s\"", s); return 0; }
    int two = parse_thing(&s, &k2, &id2);
    g_nsay = g_say_i = 0;
    if (!g_tries) g_nsaid = 0;                       /* a retry of the same step remembers what was said */
    s = trim(s);
    if (s[0] == 's' && s[1] == 'a' && s[2] == 'y' && s[3] == ' ') {
        s += 4;
        while (g_nsay < 16) {
            const char *b = s;
            int at = *s == '@';
            if (at) s++;
            int v = read_num(&s);
            if (s == b + at) break;
            g_say[g_nsay++] = at ? -v - 1 : v;      /* stored so that k = value - 1 = -(addr) - 2 */
            if (*s == ',') s++;
        }
    }
    int fast = 0, nowait = 0, reading = 0;
    for (const char *p = s; *p; p++) {
        if (p[0] == 'r' && p[1] == 'e' && p[2] == 'a' && p[3] == 'd' && (p == s || p[-1] == ' ') && (p[4] == ' ' || !p[4])) reading = 1;
        if (p[0] == 'f' && p[1] == 'a' && p[2] == 's' && p[3] == 't' && (p == s || p[-1] == ' ') && (p[4] == ' ' || !p[4])) fast = 1;
        if (p[0] == 'n' && p[1] == 'o' && p[2] == 'w' && p[3] == 'a' && p[4] == 'i' && p[5] == 't' && (p[6] == ' ' || !p[6])) nowait = 1;
    }
    g_nops = g_op = 0; g_failed = 0;
    if (!fast) push(OP_IDLE, 0, 0);
    else if (k1 == 'o') push(OP_APPEAR, id1, 0);   /* touch nothing until the object shows up */
    if (verb != VERB_GO) push(OP_VERB, verb, 0);
    push_thing(k1, id1);
    if (two) push_thing(k2, id2);
    if (reading) { push(OP_WAIT, 1500, 0); push(OP_CLOSE_READING, 0, 0); push(OP_WAIT, 800, 0); }
    /* conversations pause between lines with control given back for a moment */
    if (nowait) push(OP_WAIT, 2500, 0); else push(OP_IDLE, verb == VERB_TALK ? 2000 : 0, 0);
    g_fast = fast; g_walks = 0;
    g_op_t0 = host_ticks_ms(); g_stable_since = 0;
    return 1;
}

/* Moves glide over a quarter of a second, so the action can be followed on screen. */
static int g_bar;                    /* sliding along the verb bar: never faster than the game follows */

static int glide(int tx, int ty, unsigned elapsed)
{
    const unsigned dur = g_fast && !g_bar ? 90 : 250;
    if (elapsed >= dur) { send_pointer(HOST_EV_MOUSEMOVE, tx, ty, 0); return 1; }
    int x = g_from_x + (int)((long)(tx - g_from_x) * (long)elapsed / (long)dur);
    int y = g_from_y + (int)((long)(ty - g_from_y) * (long)elapsed / (long)dur);
    send_pointer(HOST_EV_MOUSEMOVE, x, y, 0);
    return 0;
}

static int idle_now(void)
{
    return in_control() && !g8(0x6ca139) && !g8(0x69c875) && !inventory_open();   /* 0x6ca139, 0x69c875: walking */
}

static void play_tick(void);

void hm_guide_tick(void)
{
    play_tick();
    while (g_op < g_nops) {
        struct op *o = &g_ops[g_op];
        unsigned now = host_ticks_ms(), el = now - g_op_t0;
        int done = 0;
        switch (o->kind) {
        case OP_WAIT:
            done = el >= (unsigned)o->a;
            break;
        case OP_IDLE:                                /* the game has given control back and nothing moves */
            if (menu_open()) {                       /* a dialogue menu: answer it, then wait again */
                if (!g_stable_since) { g_stable_since = now ? now : 1; return; }
                if (now - g_stable_since < 600) return;
                int line = choose_line(), x, y;
                if (line < 0) return;
                line_point(line, &x, &y);
                host_log("guide: say \"%s\"", utf8((const char *)GA(0x6b59a0 + (unsigned)line * 0x65)));
                /* this wait becomes: glide to the line, click, wait again */
                g_ops[g_op].kind = OP_LINE; g_ops[g_op].a = x; g_ops[g_op].b = y;
                insert_op(g_op + 1, OP_DOWN, 1);
                insert_op(g_op + 2, OP_UP, 1);
                insert_op(g_op + 3, OP_IDLE, 0);
                if (line == 0 && g8(0x6d3e75) > 1) {
                    /* The game never highlights the top option when the pointer comes straight to it (0x46aaa0
                     * compares it with the option of "no line", 0x6d2140[255], which is 0): come from below. */
                    int bx, by;
                    line_point((int)g8(0x6d3e75) - 1, &bx, &by);
                    g_ops[g_op].kind = OP_AT; g_ops[g_op].a = bx; g_ops[g_op].b = by;
                    insert_op(g_op + 1, OP_WAIT, 200);
                    insert_op(g_op + 2, OP_LINE, 0);
                    g_ops[g_op + 2].a = x; g_ops[g_op + 2].b = y;
                }
                host_get_mouse(&g_from_x, &g_from_y);
                g_op_t0 = now; g_stable_since = 0;
                return;
            }
            if (!idle_now()) g_stable_since = 0;
            else if (!g_stable_since) g_stable_since = now ? now : 1;
            done = g_stable_since && now - g_stable_since >= (unsigned)(o->a ? o->a : 500);
            if (!done && el > 600000) { fail("the game did not give control back"); return; }   /* chapter scenes are long */
            break;
        case OP_VERB: {                              /* hold the right button, slide to the verb, release */
            static int stage;
            if (stage == 0) { host_get_mouse(&g_ptr_x, &g_ptr_y); send_pointer(HOST_EV_MOUSEDOWN, g_ptr_x, g_ptr_y, 3); stage = 1; g_op_t0 = now; return; }
            /* While the bar is open the game keeps the pointer at x = verb * 10 + 5 and reads the verb from x
             * (0x517580), so sliding there walks the highlight through the verbs. */
            int vx = o->a * 10 + 5, vy = g_from_y;
            if (stage == 1) { if (el < 300) return; host_get_mouse(&g_from_x, &g_from_y); stage = 2; g_op_t0 = now; return; }
            if (stage == 2) { g_bar = 1; int arrived = glide(vx, vy, el); g_bar = 0; if (!arrived) return; stage = 3; g_op_t0 = now; return; }
            if (stage == 3) { if (el < 150) return; send_pointer(HOST_EV_MOUSEUP, vx, vy, 3); stage = 0; done = 1;
                              host_log("guide: verb %d at %d,%d -> %d", o->a, vx, vy, current_verb()); }
            break;
        }
        case OP_INV_OPEN:                            /* the inventory opens with the pointer at the bottom edge */
            if (inventory_open()) { done = 1; break; }
            if (o->b == 0) { o->b = 1; o->a = g_ptr_x < 50 || g_ptr_x > 590 ? 320 : g_ptr_x; host_get_mouse(&g_from_x, &g_from_y); g_op_t0 = now; return; }
            if (glide(o->a, 479, el) && el > 3000) { fail("the inventory did not open"); return; }
            return;
        case OP_CONTROL:                             /* only "the player has control" (screens where Ron is not shown) */
            if (!in_control()) g_stable_since = 0;
            else if (!g_stable_since) g_stable_since = now ? now : 1;
            done = g_stable_since && now - g_stable_since >= 300;
            if (!done && el > 60000) { fail("the game did not give control back"); return; }
            break;
        case OP_MURAL: {
            /* Egypt's mural: 48 tiles, the tile at each position in 0x68fe40[1..48] (solved when tile == position);
             * a click selects a position (the region map's value), a second click swaps it with another. */
            int p = 0, q = 0;
            for (int i = 1; i <= 48 && !p; i++) if ((int)g8(0x68fe40 + (unsigned)i) != i) p = i;
            if (!p) { host_log("guide: mural solved"); done = 1; break; }
            for (int i = 1; i <= 48 && !q; i++) if ((int)g8(0x68fe40 + (unsigned)i) == p) q = i;
            if (!q || el > 900000) { fail("the mural cannot be solved"); return; }
            /* this op stays: select p, select q, wait, then look again */
            insert_op(g_op, OP_REGION, p);
            insert_op(g_op + 1, OP_DOWN, 1);
            insert_op(g_op + 2, OP_UP, 1);
            insert_op(g_op + 3, OP_WAIT, 300);
            insert_op(g_op + 4, OP_REGION, q);
            insert_op(g_op + 5, OP_DOWN, 1);
            insert_op(g_op + 6, OP_UP, 1);
            insert_op(g_op + 7, OP_WAIT, 500);
            insert_op(g_op + 8, OP_CONTROL, 0);
            g_op_t0 = now;
            return;
        }
        case OP_MAZE: {
            /* Egypt's maze: a 6-wide grid, the cell at 0x68912e (0xff once out); every passage moves +-1 or +-6. The
             * beetle's path is a table of (cell, direction) pairs at 0x5aaf24, in order. The room (0x80c..0x811) is a
             * view of the corridor; learn which passage of each view moves how, and use that to follow the path and
             * to undo a wrong turn. */
            static signed char learned[6][16];
            static unsigned short tried[256];
            static int from_cell = -1, from_view, from_obj, trail[64], ntrail;
            int c = (int)g8(0x68912e), view = room_id() - 0x80c;
            if (c == 0xff || view < 0 || view > 5) { host_log("guide: out of the maze"); from_cell = -1; ntrail = 0; done = 1; break; }
            if (el > 900000) { fail("lost in the maze"); return; }
            if (from_cell >= 0 && c != from_cell) {
                learned[from_view][from_obj] = (signed char)(c - from_cell);
                if (ntrail && c == trail[ntrail - 1]) ntrail--;          /* came back one step */
                else if (ntrail < 64) trail[ntrail++] = from_cell;        /* went further off the path */
            }
            if (from_cell >= 0 && c == from_cell) tried[c] |= (unsigned short)(1 << from_obj);   /* it did not move us */
            /* where to go: the next cell of the beetle's path, or back along the way we came */
            int target = -1;
            for (int k = 0; k < 20; k++)
                if ((int)g8(0x5aaf24 + (unsigned)k * 2) == c) {
                    if (o->a >= 0) target = k + 1 < 20 ? (int)g8(0x5aaf24 + (unsigned)(k + 1) * 2) : c + 1;   /* out to the chamber */
                    else target = k > 0 ? (int)g8(0x5aaf24 + (unsigned)(k - 1) * 2) : c - 1;                /* back to the mural */
                }
            if (target >= 0) ntrail = 0;
            else if (ntrail) target = trail[ntrail - 1];
            else { fail("maze: not on the beetle's path"); return; }
            int want = target - c, pick = 0;
            for (int id = 1; id < 16 && !pick; id++)
                if (learned[view][id] == want && on_screen(id)) pick = id;
            for (int id = 1; id < 16 && !pick; id++) {
                const char *n = trim(object_name(id));
                int exit = *n && on_screen(id) && !g16(0x692cbc + (unsigned)(2 + id * 8) * 4) && !g16(0x692cbc + (unsigned)(4 + id * 8) * 4);
                if (exit && !learned[view][id] && !(tried[c] & (1 << id))) pick = id;
            }
            if (!pick) { host_log("guide: maze: no passage for %+d at cell %d (view %d)", want, c, view); fail("maze"); return; }
            host_log("guide: maze cell %d -> %d (view %d, passage o%d)", c, target, view, pick);
            from_cell = c; from_view = view; from_obj = pick;
            insert_op(g_op, OP_OBJ, pick);
            insert_op(g_op + 1, OP_DOWN, 1);
            insert_op(g_op + 2, OP_UP, 1);
            insert_op(g_op + 3, OP_CELL, c);
            insert_op(g_op + 4, OP_CONTROL, 0);
            g_op_t0 = now;
            return;
        }
        case OP_LEAVE:                                 /* wait until we are in another room (the cart has left) */
            if (o->b == 0) { o->b = 1; o->a = room_id(); }
            done = room_id() / 10 != o->a / 10;
            if (!done && el > 90000) { fail("the cart did not leave"); return; }
            break;
        case OP_CELL:                                  /* wait for the maze cell to change (or give up after 12 s) */
            done = (int)g8(0x68912e) != o->a || el > 12000;
            break;
        case OP_CLOSE_READING:                       /* a reading screen holds control until a key: Escape it */
            if (in_control()) { done = 1; break; }
            if (el < 2500) return;
            {
                struct host_event e;
                char *p = (char *)&e;
                for (unsigned i = 0; i < sizeof e; i++) p[i] = 0;
                e.type = HOST_EV_KEYDOWN; e.vk = 27; hm_inject_event(&e);
                e.type = HOST_EV_CHAR; e.ch = 27; hm_inject_event(&e);
                e.type = HOST_EV_KEYUP; hm_inject_event(&e);
            }
            done = 1;
            break;
        case OP_VANISH:                              /* e.g. the mine's switch panel has replaced the room */
            if (!on_screen(o->a)) { done = 1; break; }
            if (el > 15000) { fail("the screen did not change"); return; }
            return;
        case OP_APPEAR:
            if (on_screen(o->a)) { done = 1; break; }
            if (el > 30000) { fail("the object did not appear"); return; }
            return;
        case OP_REGION: {
            static int rx, ry;
            if (o->b == 0) {
                if (!region_point(o->a, &rx, &ry)) {
                    if (el < 5000) return;           /* the panel may still be opening */
                    host_log("guide: region %d not on screen", o->a);
                    fail("region not found");
                    return;
                }
                o->b = 1; host_get_mouse(&g_from_x, &g_from_y); g_op_t0 = now; return;
            }
            done = glide(rx, ry, el);
            break;
        }
        case OP_INV_CLOSE:                           /* the open panel hides the bottom of the room: leave it upwards */
            if (!inventory_open()) { done = 1; break; }
            if (glide(g_ptr_x, 200, el) && el > 3000) { fail("the inventory did not close"); return; }
            return;
        case OP_OBJ:
        case OP_ITEM: {
            static int tx, ty;
            if (o->b == 0) {
                int ok = o->kind == OP_OBJ ? object_point(o->a, &tx, &ty) : item_point(o->a, &tx, &ty);
                if (!ok && g_fast && el < 8000) return;   /* it may be about to appear */
                if (!ok && o->kind == OP_OBJ && !object_side(o->a) && el < 20000) return;   /* someone walking about */
                if (!ok && o->kind == OP_ITEM && item_scroll(o->a) && g_walks < 12) {
                    /* a full inventory: the arrows at the right of the panel scroll it by a row */
                    int up = item_scroll(o->a) < 0;
                    g_walks++;
                    insert_op(g_op, OP_AT, 617); g_ops[g_op].b = up ? 388 : 419;
                    insert_op(g_op + 1, OP_DOWN, 1);
                    insert_op(g_op + 2, OP_UP, 1);
                    insert_op(g_op + 3, OP_WAIT, 400);
                    g_op_t0 = now;
                    return;
                }
                if (!ok && o->kind == OP_OBJ && o->b == 0) {   /* off screen: walk that way so the room scrolls */
                    int side = object_side(o->a), x, y;
                    if (side && g_walks < 4 && edge_spot(side, &x, &y)) {
                        g_walks++;
                        host_log("guide: object %d is %s, walking there", o->a, side < 0 ? "to the left" : "to the right");
                        insert_op(g_op, OP_AT, x); g_ops[g_op].b = y;
                        insert_op(g_op + 1, OP_DOWN, 1);
                        insert_op(g_op + 2, OP_UP, 1);
                        insert_op(g_op + 3, OP_IDLE, 0);
                        g_op_t0 = now;
                        return;
                    }
                }
                if (!ok) {
                    host_log("guide: %s %d is not on screen", o->kind == OP_OBJ ? "object" : "item", o->a);
                    probe();
                    fail("thing not found");
                    return;
                }
                o->b = 1; host_get_mouse(&g_from_x, &g_from_y); g_op_t0 = now; return;
            }
            done = glide(tx, ty, el);
            break;
        }
        case OP_LINE:
        case OP_AT:
            done = glide(o->a, o->b, el);
            break;
        case OP_KEY:
        case OP_CHAR: {                              /* a key as the keyboard sends it: down, character, up */
            struct host_event e;
            char *p = (char *)&e;
            for (unsigned i = 0; i < sizeof e; i++) p[i] = 0;
            if (o->kind == OP_KEY) { e.type = HOST_EV_KEYDOWN; e.vk = o->a; hm_inject_event(&e); }
            e.type = HOST_EV_CHAR; e.ch = o->a; hm_inject_event(&e);
            if (o->kind == OP_KEY) { e.type = HOST_EV_KEYUP; e.vk = o->a; hm_inject_event(&e); }
            done = 1;
            break;
        }
        case OP_DOWN: host_get_mouse(&g_ptr_x, &g_ptr_y); send_pointer(HOST_EV_MOUSEDOWN, g_ptr_x, g_ptr_y, o->a); done = 1; break;
        case OP_UP: host_get_mouse(&g_ptr_x, &g_ptr_y); send_pointer(HOST_EV_MOUSEUP, g_ptr_x, g_ptr_y, o->a); done = 1; break;
        }
        if (!done) return;
        g_op++;
        g_op_t0 = host_ticks_ms(); g_stable_since = 0;
        host_get_mouse(&g_from_x, &g_from_y);       /* the next glide starts where the pointer is */
        if (g_op == g_nops) { g_nops = g_op = 0; host_log("guide: %u action done (room %#x)", host_ticks_ms(), room_id()); }
    }
}

/* Before the action just queued: walk to a free spot of floor (no object) near the middle of the screen. */
static void step_aside(void)
{
    for (int r = 0; r < 300; r += 10)
        for (int a = 0; a < 8; a++) {
            static const int dx[] = { 0, 1, 0, -1, 1, 1, -1, -1 }, dy[] = { 1, 0, -1, 0, 1, -1, 1, -1 };
            int x = 320 + dx[a] * r, y = 380 + dy[a] * r / 2;
            if (x < 20 || x > 620 || y < 200 || y > 455 || object_at(x, y)) continue;
            insert_op(1, OP_AT, x); g_ops[1].b = y;
            insert_op(2, OP_DOWN, 1);
            insert_op(3, OP_UP, 1);
            insert_op(4, OP_IDLE, 0);
            return;
        }
}

/* ------------------------------------------------------------------ the walkthrough */
#define MAX_STEPS 2000
static char *g_text;                 /* hm_actions.txt, lines split in place */
static char *g_steps[MAX_STEPS];     /* action lines (section titles and comments left out) */
static char *g_section[MAX_STEPS];   /* the title each step belongs to */
static char *g_note[MAX_STEPS];      /* the comment after the step, "" if none (the port's menu shows it) */
static int g_nsteps, g_step, g_play_left;
static int g_play_failed;            /* playing stopped because a step's condition never came true */

static void load_steps(void)
{
    if (g_text) return;
    void *f = host_fopen("hm_actions.txt", "rb");
    if (!f) { host_log("guide: no hm_actions.txt"); return; }
    host_fseek(f, 0, 2);
    long n = host_ftell(f);
    host_fseek(f, 0, 0);
    g_text = host_malloc((unsigned)n + 1);
    n = (long)host_fread(g_text, 1, (unsigned)n, f);
    host_fclose(f);
    g_text[n] = 0;
    char *title = "";
    for (char *line = g_text; line && *line && g_nsteps < MAX_STEPS; ) {
        char *next = line;
        while (*next && *next != '\n') next++;
        if (*next) *next++ = 0; else next = 0;
        char *hash = line, *note = "";
        while (*hash && *hash != '#') hash++;
        if (*hash) {                                 /* comments, also at the end of a step */
            note = (char *)trim(hash + 1);
            int nl = 0;
            while (note[nl]) nl++;
            while (nl && (note[nl - 1] == ' ' || note[nl - 1] == '\r' || note[nl - 1] == '\t')) note[--nl] = 0;
        }
        *hash = 0;
        int len = 0;
        while (line[len]) len++;
        while (len && (line[len - 1] == ' ' || line[len - 1] == '\r' || line[len - 1] == '\t')) line[--len] = 0;
        line = (char *)trim(line);
        if (line[0] == '=' && line[1] == '=') title = (char *)trim(line + 2);
        else if (*line) { g_section[g_nsteps] = title; g_note[g_nsteps] = note; g_steps[g_nsteps++] = line; }
        line = next;
    }
    host_log("guide: %d steps in hm_actions.txt", g_nsteps);
}

/* the condition after "until", or "" */
static const char *until_of(const char *step)
{
    for (const char *p = step; *p; p++)
        if (p[0] == ' ' && p[1] == 'u' && p[2] == 'n' && p[3] == 't' && p[4] == 'i' && p[5] == 'l' && p[6] == ' ') return p + 7;
    return "";
}

/* HM_GUIDE_EXIT=1 (tests): when playing stops, log the state, keep a screenshot and quit; with
 * HM_GUIDE_SAVE="<row> <name>" and no failure, save the game first (a checkpoint for the next test) */
static int g_exit_after_save;

static void play_stopped(void)
{
    if (!hm_getenv("HM_GUIDE_EXIT")) return;
    probe();
    host_screenshot("guide_end");
    const char *save = hm_getenv("HM_GUIDE_SAVE");
    if (save && !g_failed) {
        char cmd[64] = "save ";
        int k = 5;
        for (int i = 0; save[i] && k < 63; i++) cmd[k++] = save[i];
        cmd[k] = 0;
        hm_guide_command(cmd);
        g_exit_after_save = 1;
        return;
    }
    host_quit(g_failed ? 3 : 0);
}

static void play_tick(void)
{
    static int was_playing;
    if (g_exit_after_save && !hm_guide_busy()) { host_log("guide: saved, step %d", g_step + 1); host_quit(0); }
    if (was_playing && !g_play_left) play_stopped();
    was_playing = g_play_left != 0;
    if (!g_play_left || hm_guide_busy()) return;
    if (g_failed) { g_play_left = 0; g_play_failed = 1; host_log("guide: playing stopped at step %d", g_step + 1); return; }
    if (g_tries) {                                   /* the last action ran: is its condition met? */
        const char *cond = until_of(g_steps[g_step]);
        if (!condition(cond)) {
            /* "wait <ms> until": check every <ms> for up to a minute; actions: 30 tries */
            const char *w = trim(g_steps[g_step]);
            int max = 30;
            if (w[0] == 'w' && w[1] == 'a' && w[2] == 'i' && w[3] == 't') { w += 4; int ms = read_num(&w); max = ms > 0 ? 60000 / ms : 30; }
            if (g_tries >= max) { host_log("guide: step %d: \"%s\" still not true", g_step + 1, cond); g_play_left = 0; g_play_failed = 1; return; }
            g_tries++;
            /* "go to" an exit Sue already stands at does nothing (after loading a game she starts at one):
             * the next try opens it (doors), the one after first walks away from it */
            const char *st = trim(g_steps[g_step]);
            const char *u = trim(until_of(st));        /* only when the aim is to leave the room */
            if (u[0] == 'n' && u[1] == 'o' && u[2] == 't' && u[3] == ' ' && trim(u + 4)[0] == 'r') u = trim(u + 4);   /* "not room" too */
            int go = st[0] == 'g' && st[1] == 'o' && st[2] == ' ' &&
                     ((u[0] == 'm' && u[1] == 'o' && u[2] == 'v') || (u[0] == 'r' && u[1] == 'o' && u[2] == 'o'));
            if (go && g_tries % 3 == 2) {
                char buf[160] = "open";
                int k = 4;
                for (const char *q = st + 2; *q && k < 159; q++) buf[k++] = *q;
                buf[k] = 0;
                start_action(buf);
            } else {
                start_action(g_steps[g_step]);
                if (go && g_tries % 3 == 0) step_aside();
            }
            return;
        }
        g_tries = 0;
        g_step++;
        if (g_play_left > 0) g_play_left--;
        if (!g_play_left) return;
    }
    if (g_step >= g_nsteps) { g_play_left = 0; host_log("guide: end of the walkthrough"); return; }
    /* Conditions and the room are only meaningful once the game has settled (after loading a game the room
     * takes a moment to appear); "fast" steps are meant to start while something is going on. */
    {
        const char *p = g_steps[g_step];
        int fast = (p[0] == 'k' && p[1] == 'e' && p[2] == 'y' && p[3] == ' ') ||   /* keys close screens that hold control */
                   (p[0] == 'm' && p[1] == 'u' && p[2] == 'r');                     /* the mural is a screen of its own */
        for (; *p; p++) if (p[0] == ' ' && p[1] == 'f' && p[2] == 'a' && p[3] == 's' && p[4] == 't' && (p[5] == ' ' || !p[5])) fast = 1;
        static unsigned settled;
        unsigned now = host_ticks_ms();
        if (!fast) {
            if (!idle_now()) { settled = 0; return; }
            if (!settled) settled = now ? now : 1;
            if (now - settled < 500) return;
        }
        settled = 0;
    }
    g_step_room = room_id();
    if (*until_of(g_steps[g_step]) && condition(until_of(g_steps[g_step]))) {   /* already done: skip it */
        host_log("guide: room %#x step %d already done: %s", room_id(), g_step + 1, g_steps[g_step]);
        g_step++;
        if (g_play_left > 0) g_play_left--;
        return;
    }
    host_log("guide: %u room %#x step %d [%s] %s", host_ticks_ms(), room_id(), g_step + 1, g_section[g_step], g_steps[g_step]);
    if (!start_action(g_steps[g_step])) { g_play_left = 0; return; }   /* it reads the action, not the "until" */
    g_tries = 1;
}

void hm_guide_command(const char *text)
{
    const char *s = trim(text);
    host_log("guide: %s", s);
    if (s[0] == 'p' && s[1] == 'r') probe();
    else if (s[0] == 'd' && s[1] == 'o' && s[2] == ' ') start_action(s + 3);
    else if (s[0] == 'p' && s[1] == 'l') {
        load_steps();
        s += 4;
        int n = read_num(&s);
        g_play_left = n > 0 ? n : -1;
        g_tries = 0; g_failed = 0;
    }
    else if (s[0] == 'g' && s[1] == 'o' && s[2] == 't') { load_steps(); s += 4; g_step = read_num(&s) - 1; if (g_step < 0) g_step = 0; }
    else if (s[0] == 's' && s[1] == 't') g_play_left = 0;
    else if (s[0] == 'f' && s[1] == 'i' && s[2] == 'n') {   /* find <text>: item ids whose name has it (authoring) */
        const char *t = trim(s + 4);
        for (int id = 1; id < 256; id++) {
            const char *n = utf8(item_name(id));
            for (const char *p = n; *p; p++) {
                int k = 0;
                while (t[k] && p[k] == t[k]) k++;
                if (!t[k]) { host_log("guide:   i%d \"%s\"", id, n); break; }
            }
        }
    }
    else if ((s[0] == 's' && s[1] == 'a' && s[2] == 'v') || (s[0] == 'l' && s[1] == 'o' && s[2] == 'a')) {
        /* save <row> <name> / load <row>: through the game's own menu (Escape); row 1 is the top of its list */
        int save = s[0] == 's';
        s += 4;
        int row = read_num(&s);
        s = trim(s);
        g_nops = g_op = 0; g_failed = 0;
        push(OP_IDLE, 0, 0);
        push(OP_KEY, 27, 0); push(OP_WAIT, 1500, 0);
        push(OP_AT, save ? 80 : 172, 232); push(OP_DOWN, 1, 0); push(OP_UP, 1, 0); push(OP_WAIT, 1200, 0);
        push(OP_AT, 320, (49 + 43 * row) / 2);   /* rows 21.5 pixels apart from y 46 */
        push(OP_DOWN, 1, 0); push(OP_UP, 1, 0); push(OP_WAIT, 1200, 0);
        if (save) {
            for (int i = 0; s[i] && i < 12; i++) push(OP_CHAR, (unsigned char)s[i], 0);
            push(OP_KEY, 13, 0); push(OP_WAIT, 1500, 0);
        } else push(OP_WAIT, 3000, 0);
        push(OP_IDLE, 0, 0);
        g_op_t0 = host_ticks_ms(); g_stable_since = 0;
    }
    else host_log("guide: unknown command");
}

/* ------------------------------------------------------------------ for the port's menu (menu.c) */
int hm_guide_count(void) { load_steps(); return g_nsteps; }
const char *hm_guide_step(int i) { return i >= 0 && i < g_nsteps ? g_steps[i] : ""; }
const char *hm_guide_note(int i) { return i >= 0 && i < g_nsteps ? g_note[i] : ""; }
const char *hm_guide_section(int i) { return i >= 0 && i < g_nsteps ? g_section[i] : ""; }
int hm_guide_current(void) { return g_step; }
int hm_guide_playing(void) { return g_play_left != 0 || hm_guide_busy(); }
int hm_guide_failed(void) { return g_play_failed; }

/* play n steps from step i (n < 0: to the end) */
void hm_guide_run(int i, int n)
{
    load_steps();
    if (i < 0 || i >= g_nsteps) return;
    if (i != g_step) g_tries = 0;                    /* a step left half done is started again */
    g_step = i;
    g_play_left = n ? n : 1;
    g_failed = 0; g_play_failed = 0;
}

void hm_guide_stop(void) { g_play_left = 0; }       /* the action under way finishes */

/* The step's condition says it has been done already: the item it gives is in the inventory (has, named).
 * Conditions about rooms or missing things say nothing outside the moment the step is played. */
int hm_guide_done(int i)
{
    if (i < 0 || i >= g_nsteps) return 0;
    const char *c = trim(until_of(g_steps[i]));
    if (!((c[0] == 'h' && c[1] == 'a' && c[2] == 's') || (c[0] == 'n' && c[1] == 'a' && c[2] == 'm'))) return 0;
    return condition(c);
}
