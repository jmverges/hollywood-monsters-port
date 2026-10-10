/* SDL2 + POSIX implementation of host.h. This is the only file that touches the real OS. */
#define _GNU_SOURCE
#include "host.h"
#include <SDL.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>

static SDL_Window *g_win;
static SDL_Renderer *g_ren;
static SDL_Texture *g_tex;
static int g_tw, g_th;
static pthread_t g_main_thread;
static int g_scale = 2;

/* pending frame shared between the game thread(s) and the renderer */
static pthread_mutex_t g_frame_mx = PTHREAD_MUTEX_INITIALIZER;
static unsigned char *g_frame;          /* 8bpp */
static int g_fw, g_fh, g_frame_dirty;
static unsigned char g_pal[1024];

void host_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

int host_is_main_thread(void) { return pthread_equal(pthread_self(), g_main_thread); }

void host_init_main_thread(void)
{
    g_main_thread = pthread_self();
    const char *s = getenv("HM_SCALE");
    if (s && atoi(s) > 0) g_scale = atoi(s);
}

int host_window_open(const char *title, int width, int height)
{
    if (g_win) return 1;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        host_log("SDL_Init failed: %s", SDL_GetError());
        return 0;
    }
    Uint32 flags = SDL_WINDOW_RESIZABLE;
    if (getenv("HM_FULLSCREEN")) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    g_win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width * g_scale, height * g_scale, flags);
    if (!g_win) { host_log("SDL_CreateWindow: %s", SDL_GetError()); return 0; }
    /* No vsync by default: the game pumps messages one at a time and blits to the primary several times per
     * frame, so blocking on vblank there would slow its main loop. Presentation is rate-limited instead. */
    g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED | (getenv("HM_VSYNC") ? SDL_RENDERER_PRESENTVSYNC : 0));
    if (!g_ren) g_ren = SDL_CreateRenderer(g_win, -1, 0);
    if (!g_ren) { host_log("SDL_CreateRenderer: %s", SDL_GetError()); return 0; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_RenderSetLogicalSize(g_ren, width, height);
    SDL_StartTextInput();
    SDL_ShowCursor(SDL_DISABLE);                     /* the game's cursor is drawn by draw_cursor() */
    return 1;
}

static volatile int g_quitting;

void host_quit(int code)
{
    g_quitting = 1;                 /* other threads (the game's timer) must stop calling into SDL video */
    SDL_Quit();
    exit(code);
}

unsigned host_ticks_ms(void) { return SDL_GetTicks(); }

void host_sleep_ms(unsigned ms)
{
    if (host_is_main_thread()) host_render_pending();
    SDL_Delay(ms);
}

/* ------------------------------------------------------------------ video */
void host_submit_frame(const unsigned char *pixels, int pitch, int width, int height, const unsigned char *pal4)
{
    pthread_mutex_lock(&g_frame_mx);
    if (!g_frame || g_fw != width || g_fh != height) {
        free(g_frame);
        g_frame = malloc((size_t)width * height);
        g_fw = width; g_fh = height;
    }
    for (int y = 0; y < height; y++)
        memcpy(g_frame + (size_t)y * width, pixels + (size_t)y * pitch, width);
    memcpy(g_pal, pal4, 1024);
    g_frame_dirty = 1;
    pthread_mutex_unlock(&g_frame_mx);
    if (host_is_main_thread()) host_render_pending();
}

static void dump_frame_if_requested(const Uint32 *argb, int w, int h)
{
    static int counter;
    const char *dir = getenv("HM_DUMP_FRAMES");
    if (!dir) return;
    counter++;
    const char *every_s = getenv("HM_DUMP_EVERY");
    int every = every_s ? atoi(every_s) : 30;
    if (every <= 0 || counter % every) return;
    char path[512];
    snprintf(path, sizeof path, "%s/frame_%06d.ppm", dir, counter);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        unsigned char rgb[3] = { (unsigned char)(argb[i] >> 16), (unsigned char)(argb[i] >> 8), (unsigned char)argb[i] };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static void save_shot(const char *name)
{
    const char *dir = getenv("HM_SHOT_DIR");
    char path[512];
    snprintf(path, sizeof path, "%s/%s.ppm", dir ? dir : ".", name);
    pthread_mutex_lock(&g_frame_mx);
    FILE *f = g_frame ? fopen(path, "wb") : NULL;
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", g_fw, g_fh);
        for (int i = 0; i < g_fw * g_fh; i++) fwrite(&g_pal[g_frame[i] * 4], 1, 3, f);
        fclose(f);
        host_log("shot %s", path);
    }
    pthread_mutex_unlock(&g_frame_mx);
}

/* ------------------------------------------------------------------ software cursor */
struct host_cursor { int w, h, hx, hy; Uint32 *px; SDL_Texture *tex; };
static struct host_cursor *volatile g_cur;
static volatile int g_cur_visible = 1, g_cursor_dirty, g_mouse_in = 1;

void *host_cursor_create(const unsigned *argb, int w, int h, int hot_x, int hot_y)
{
    struct host_cursor *c = calloc(1, sizeof *c);
    if (!c) return 0;
    c->px = malloc((size_t)w * h * 4);
    if (!c->px) { free(c); return 0; }
    memcpy(c->px, argb, (size_t)w * h * 4);
    c->w = w; c->h = h; c->hx = hot_x; c->hy = hot_y;
    return c;
}

void host_cursor_set(void *cursor)
{
    if (g_cur != cursor) { g_cur = cursor; g_cursor_dirty = 1; }
}

void host_show_cursor(int show)
{
    show = show && !getenv("HM_HIDE_CURSOR");
    if (g_cur_visible != show) { g_cur_visible = show; g_cursor_dirty = 1; }
}

static void draw_cursor(void)
{
    struct host_cursor *c = g_cur;
    if (!c || !g_cur_visible || !g_mouse_in) return;
    if (!c->tex) {
        c->tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, c->w, c->h);
        if (!c->tex) return;
        SDL_UpdateTexture(c->tex, NULL, c->px, c->w * 4);
        SDL_SetTextureBlendMode(c->tex, SDL_BLENDMODE_BLEND);
    }
    int x, y;
    host_get_mouse(&x, &y);
    SDL_Rect r = { x - c->hx, y - c->hy, c->w, c->h };   /* logical coordinates: scaled with the frame */
    SDL_RenderCopy(g_ren, c->tex, NULL, &r);
}

void host_render_pending(void)
{
    static Uint32 last_present;
    if (!g_ren) return;
    if (!g_frame_dirty && !g_cursor_dirty) return;
    Uint32 now = SDL_GetTicks();
    if (now - last_present < 14) return;            /* ~70 Hz cap; the dirty frame is shown on a later call */
    last_present = now;
    g_cursor_dirty = 0;
    pthread_mutex_lock(&g_frame_mx);
    if (!g_frame_dirty) {                           /* only the cursor moved: present the last frame again */
        pthread_mutex_unlock(&g_frame_mx);
        if (!g_tex) return;
        SDL_RenderClear(g_ren);
        SDL_RenderCopy(g_ren, g_tex, NULL, NULL);
        draw_cursor();
        SDL_RenderPresent(g_ren);
        return;
    }
    if (!g_tex || g_tw != g_fw || g_th != g_fh) {
        if (g_tex) SDL_DestroyTexture(g_tex);
        g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, g_fw, g_fh);
        g_tw = g_fw; g_th = g_fh;
        SDL_RenderSetLogicalSize(g_ren, g_fw, g_fh);
    }
    void *dst; int dpitch;
    if (SDL_LockTexture(g_tex, NULL, &dst, &dpitch) == 0) {
        Uint32 lut[256];
        for (int i = 0; i < 256; i++)
            lut[i] = 0xFF000000u | ((Uint32)g_pal[i * 4] << 16) | ((Uint32)g_pal[i * 4 + 1] << 8) | g_pal[i * 4 + 2];
        for (int y = 0; y < g_fh; y++) {
            Uint32 *row = (Uint32 *)((unsigned char *)dst + (size_t)y * dpitch);
            const unsigned char *src = g_frame + (size_t)y * g_fw;
            for (int x = 0; x < g_fw; x++) row[x] = lut[src[x]];
        }
        if (getenv("HM_DUMP_FRAMES")) {
            Uint32 *tmp = malloc((size_t)g_fw * g_fh * 4);
            for (int y = 0; y < g_fh; y++) memcpy(tmp + (size_t)y * g_fw, (unsigned char *)dst + (size_t)y * dpitch, (size_t)g_fw * 4);
            dump_frame_if_requested(tmp, g_fw, g_fh);
            free(tmp);
        }
        SDL_UnlockTexture(g_tex);
    }
    g_frame_dirty = 0;
    pthread_mutex_unlock(&g_frame_mx);
    SDL_RenderClear(g_ren);
    SDL_RenderCopy(g_ren, g_tex, NULL, NULL);
    draw_cursor();
    SDL_RenderPresent(g_ren);
}

/* ------------------------------------------------------------------ input */
static int vk_from_sdl(SDL_Keycode k)
{
    if (k >= SDLK_a && k <= SDLK_z) return 'A' + (k - SDLK_a);
    if (k >= SDLK_0 && k <= SDLK_9) return '0' + (k - SDLK_0);
    if (k >= SDLK_F1 && k <= SDLK_F12) return 0x70 + (k - SDLK_F1);
    switch (k) {
    case SDLK_BACKSPACE: return 0x08; case SDLK_TAB: return 0x09; case SDLK_RETURN: case SDLK_KP_ENTER: return 0x0D;
    case SDLK_LSHIFT: case SDLK_RSHIFT: return 0x10; case SDLK_LCTRL: case SDLK_RCTRL: return 0x11;
    case SDLK_LALT: case SDLK_RALT: return 0x12; case SDLK_PAUSE: return 0x13; case SDLK_ESCAPE: return 0x1B;
    case SDLK_SPACE: return 0x20; case SDLK_PAGEUP: return 0x21; case SDLK_PAGEDOWN: return 0x22;
    case SDLK_END: return 0x23; case SDLK_HOME: return 0x24; case SDLK_LEFT: return 0x25; case SDLK_UP: return 0x26;
    case SDLK_RIGHT: return 0x27; case SDLK_DOWN: return 0x28; case SDLK_INSERT: return 0x2D; case SDLK_DELETE: return 0x2E;
    default: return 0;
    }
}

static void to_logical(int wx, int wy, int *lx, int *ly)
{
    float fx, fy;
    SDL_RenderWindowToLogical(g_ren, wx, wy, &fx, &fy);
    int x = (int)fx, y = (int)fy;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (g_tw && x >= g_tw) x = g_tw - 1;
    if (g_th && y >= g_th) y = g_th - 1;
    *lx = x; *ly = y;
}

static int pending_char = -1;

/* ---- scripted input for automated testing (HM_SCRIPT=file)
 * Each line: <time_ms> <command> [args]; time is absolute since start, or "+N" relative to the previous line.
 * Commands: move X Y | click X Y | rclick X Y | down X Y | up X Y | rdown X Y | rup X Y | key VK | char C | shot NAME
 *           | poke8|poke16|poke32 ADDR VALUE (write game memory, e.g. the room id at 0x6ca1be)
 *           | trap (SIGTRAP: stop in gdb at this moment) | dump ADDR LEN (raw memory to the shot dir) | quit */
struct script_ev { unsigned t; char cmd[16]; int a, b; char name[64]; };
static struct script_ev *g_script;
static int g_script_n, g_script_i, g_script_loaded;
static unsigned g_script_t0;
static int g_mx = 320, g_my = 240;
/* SetCursorPos semantics: after a warp, GetCursorPos reports the new position until the pointer really moves.
 * SDL cannot warp on some systems (Wayland) and may report the old position until the next event elsewhere. */
static int g_warp_pending, g_warp_x, g_warp_y;
static volatile int g_warp_request;   /* a warp asked for by another thread, done by the main thread */
static void apply_warp(void);

static struct host_event g_inject[8];
static int g_inject_n;

static void load_script(void)
{
    g_script_loaded = 1;
    const char *path = getenv("HM_SCRIPT");
    if (!path) return;
    FILE *f = fopen(path, "r");
    if (!f) { host_log("HM_SCRIPT %s not found", path); return; }
    char line[256];
    unsigned last = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        struct script_ev ev;
        memset(&ev, 0, sizeof ev);
        char tbuf[32];
        int n = sscanf(line, "%31s %15s %63s %d", tbuf, ev.cmd, ev.name, &ev.b);
        if (n < 2) continue;
        unsigned t = tbuf[0] == '+' ? last + (unsigned)atoi(tbuf + 1) : (unsigned)atoi(tbuf);
        ev.t = last = t;
        ev.a = atoi(ev.name);
        g_script = realloc(g_script, sizeof *g_script * (g_script_n + 1));
        g_script[g_script_n++] = ev;
    }
    fclose(f);
    g_script_t0 = SDL_GetTicks();
    host_log("HM_SCRIPT: %d events", g_script_n);
}

static void save_shot(const char *name);

static void run_script(void)
{
    if (!g_script_loaded) load_script();
    unsigned now = SDL_GetTicks() - g_script_t0;
    while (g_script_i < g_script_n && g_script[g_script_i].t <= now && g_inject_n < 6) {
        struct script_ev *s = &g_script[g_script_i++];
        struct host_event e;
        memset(&e, 0, sizeof e);
        const char *c = s->cmd;
        int right = c[0] == 'r' && strcmp(c, "rclick") != 0 ? 1 : !strcmp(c, "rclick");
        const char *base = (c[0] == 'r' && strcmp(c, "rclick")) ? c + 1 : c;   /* rdown -> down, rup -> up */
        if (!strcmp(base, "move") || !strcmp(base, "click") || !strcmp(base, "rclick") || !strcmp(base, "down") || !strcmp(base, "up")) {
            g_mx = s->a; g_my = s->b;
            e.type = HOST_EV_MOUSEMOVE; e.x = g_mx; e.y = g_my;
            g_inject[g_inject_n++] = e;
            int btn = right ? 3 : 1;
            if (strcmp(base, "move") && strcmp(base, "up")) { e.type = HOST_EV_MOUSEDOWN; e.button = btn; g_inject[g_inject_n++] = e; }
            if (!strcmp(base, "click") || !strcmp(base, "rclick") || !strcmp(base, "up")) { e.type = HOST_EV_MOUSEUP; e.button = btn; g_inject[g_inject_n++] = e; }
        } else if (!strcmp(s->cmd, "key")) {
            e.type = HOST_EV_KEYDOWN; e.vk = s->a; g_inject[g_inject_n++] = e;
            if (s->a == 0x0D || s->a == 0x1B || s->a == 0x20) { e.type = HOST_EV_CHAR; e.ch = s->a; g_inject[g_inject_n++] = e; }
            e.type = HOST_EV_KEYUP; g_inject[g_inject_n++] = e;
        } else if (!strcmp(s->cmd, "char")) {
            e.type = HOST_EV_CHAR; e.ch = (unsigned char)s->name[0]; g_inject[g_inject_n++] = e;
        } else if (!strncmp(s->cmd, "poke", 4)) {          /* poke8|poke16|poke32 ADDR VALUE (debug warps) */
            unsigned long addr = strtoul(s->name, NULL, 0);
            unsigned long val = (unsigned long)s->b;
            int bits = atoi(s->cmd + 4);
            if (bits == 8) *(volatile unsigned char *)addr = (unsigned char)val;
            else if (bits == 16) *(volatile unsigned short *)addr = (unsigned short)val;
            else *(volatile unsigned int *)addr = (unsigned int)val;
            host_log("HM_SCRIPT: poke%d %#lx = %lu", bits, addr, val);
        } else if (!strcmp(s->cmd, "dump") || !strcmp(s->cmd, "dumpp")) {
            /* dump ADDR LEN, or dumpp PTRADDR LEN (memory the 32-bit pointer at PTRADDR points to)
             * -> <shot dir>/dump_<ADDR>_<t>.bin */
            unsigned long addr = strtoul(s->name, NULL, 0);
            if (s->cmd[4] == 'p') addr = *(unsigned int *)addr;
            const char *dir = getenv("HM_SHOT_DIR");
            char path[512];
            snprintf(path, sizeof path, "%s/dump_%08lx_%u.bin", dir ? dir : ".", strtoul(s->name, NULL, 0), s->t);
            FILE *f = fopen(path, "wb");
            if (f) { fwrite((void *)addr, 1, (size_t)s->b, f); fclose(f); host_log("HM_SCRIPT: dump %s", path); }
        } else if (!strcmp(s->cmd, "trap")) {             /* stop in the debugger at this moment */
            host_log("HM_SCRIPT: trap");
#ifdef SIGTRAP
            raise(SIGTRAP);
#else
            __builtin_trap();                             /* Windows: no SIGTRAP; a debugger stops here too */
#endif
        } else if (!strcmp(s->cmd, "shot")) {
            save_shot(s->name);
        } else if (!strcmp(s->cmd, "quit")) {
            host_log("HM_SCRIPT: quit at %u ms", now);
            exit(0);
        }
    }
}

int host_poll_event(struct host_event *ev)
{
    SDL_Event e;
    memset(ev, 0, sizeof *ev);
    if (host_is_main_thread()) host_render_pending();
    run_script();
    if (g_warp_request && host_is_main_thread()) apply_warp();
    if (g_inject_n) { *ev = g_inject[0]; memmove(g_inject, g_inject + 1, sizeof g_inject[0] * --g_inject_n); return 1; }
    if (pending_char >= 0) { ev->type = HOST_EV_CHAR; ev->ch = pending_char; pending_char = -1; return 1; }
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT: host_log("SDL_QUIT (window closed or SIGINT/SIGTERM)"); ev->type = HOST_EV_QUIT; return 1;
        case SDL_MOUSEMOTION: g_warp_pending = 0; g_cursor_dirty = 1; ev->type = HOST_EV_MOUSEMOVE; ev->x = e.motion.x; ev->y = e.motion.y; return 1;
        case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
            ev->type = e.type == SDL_MOUSEBUTTONDOWN ? HOST_EV_MOUSEDOWN : HOST_EV_MOUSEUP;
            ev->x = e.button.x; ev->y = e.button.y;
            if (g_warp_pending) { ev->x = g_warp_x; ev->y = g_warp_y; }   /* Windows: a click happens where the cursor was warped */
            ev->button = e.button.button == SDL_BUTTON_LEFT ? 1 : e.button.button == SDL_BUTTON_MIDDLE ? 2 : 3;
            return 1;
        case SDL_MOUSEWHEEL: ev->type = HOST_EV_WHEEL; ev->wheel = e.wheel.y > 0 ? 1 : -1; host_get_mouse(&ev->x, &ev->y); return 1;
        case SDL_KEYDOWN: case SDL_KEYUP: {
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT)) {
                Uint32 fs = SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                SDL_SetWindowFullscreen(g_win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                break;
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F12) {
                char name[64];
                snprintf(name, sizeof name, "screenshot_%u", SDL_GetTicks());
                save_shot(name);
                break;
            }
            int vk = vk_from_sdl(e.key.keysym.sym);
            if (!vk) break;
            ev->type = e.type == SDL_KEYDOWN ? HOST_EV_KEYDOWN : HOST_EV_KEYUP;
            ev->vk = vk;
            if (e.type == SDL_KEYDOWN && (vk == 0x0D || vk == 0x1B || vk == 0x08 || vk == 0x09)) pending_char = vk;
            return 1;
        }
        case SDL_TEXTINPUT: {
            const unsigned char *s = (const unsigned char *)e.text.text;
            int cp = s[0];
            if (cp >= 0xC0 && cp < 0xE0 && s[1]) cp = ((cp & 0x1F) << 6) | (s[1] & 0x3F);
            else if (cp >= 0x80) break;
            if (cp > 0xFF) break;
            ev->type = HOST_EV_CHAR; ev->ch = cp; return 1;
        }
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_ENTER || e.window.event == SDL_WINDOWEVENT_LEAVE) {
                g_mouse_in = e.window.event == SDL_WINDOWEVENT_ENTER; g_cursor_dirty = 1;
            }
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED || e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                ev->type = HOST_EV_FOCUS; ev->focus = e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED; return 1;
            }
            if (e.window.event == SDL_WINDOWEVENT_EXPOSED) { g_frame_dirty = 1; host_render_pending(); }
            break;
        default: break;
        }
    }
    return 0;
}

void host_get_mouse(int *x, int *y)
{
    if (g_script_n) { *x = g_mx; *y = g_my; return; }
    if (g_warp_pending) { *x = g_warp_x; *y = g_warp_y; return; }
    int wx = 0, wy = 0;
    SDL_GetMouseState(&wx, &wy);
    if (g_ren) to_logical(wx, wy, x, y); else { *x = wx; *y = wy; }
}

static void apply_warp(void)
{
    g_warp_request = 0;
    if (!g_win || !g_ren || g_quitting) return;
    int x = g_warp_x, y = g_warp_y;
    int ww, wh; SDL_GetWindowSize(g_win, &ww, &wh);
    if (!g_tw || !g_th) return;
    float sx = (float)ww / g_tw, sy = (float)wh / g_th, s = sx < sy ? sx : sy;
    int ox = (int)((ww - g_tw * s) / 2), oy = (int)((wh - g_th * s) / 2);
    SDL_WarpMouseInWindow(g_win, ox + (int)(x * s), oy + (int)(y * s));
}

/* SetCursorPos. The game calls it from its timer thread too; SDL video calls belong on the main thread, so
 * those warps are recorded here and applied by the main thread on its next event poll. */
void host_set_mouse(int x, int y)
{
    if (getenv("HM_NO_WARP") || g_quitting) return;
    if (g_script_n) { g_mx = x; g_my = y; }   /* scripted pointer: a warp moves it, as SetCursorPos does on Windows */
    g_warp_x = x; g_warp_y = y; g_warp_pending = 1; g_cursor_dirty = 1;
    if (host_is_main_thread()) apply_warp();
    else g_warp_request = 1;
}


/* ------------------------------------------------------------------ threads + events */
struct thread_start { host_thread_fn fn; void *arg; };
static void *thread_tramp(void *p)
{
    struct thread_start ts = *(struct thread_start *)p;
    free(p);
    ts.fn(ts.arg);
    return NULL;
}
void *host_thread_create(host_thread_fn fn, void *arg)
{
    pthread_t t;
    struct thread_start *ts = malloc(sizeof *ts);
    ts->fn = fn; ts->arg = arg;
    if (pthread_create(&t, NULL, thread_tramp, ts) != 0) { free(ts); return NULL; }
    pthread_detach(t);
    return (void *)(uintptr_t)t;
}

void *host_mutex_create(void)
{
    pthread_mutex_t *m = malloc(sizeof *m);
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(m, &a);
    return m;
}
void host_mutex_lock(void *m) { pthread_mutex_lock((pthread_mutex_t *)m); }
void host_mutex_unlock(void *m) { pthread_mutex_unlock((pthread_mutex_t *)m); }

struct hevent { int manual, state; };
static pthread_mutex_t g_ev_mx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_ev_cv = PTHREAD_COND_INITIALIZER;

void *host_event_create(int manual_reset, int initial_state)
{
    struct hevent *e = calloc(1, sizeof *e);
    e->manual = manual_reset; e->state = initial_state;
    return e;
}
void host_event_set(void *p)
{
    pthread_mutex_lock(&g_ev_mx);
    ((struct hevent *)p)->state = 1;
    pthread_cond_broadcast(&g_ev_cv);
    pthread_mutex_unlock(&g_ev_mx);
}
void host_event_reset(void *p)
{
    pthread_mutex_lock(&g_ev_mx);
    ((struct hevent *)p)->state = 0;
    pthread_mutex_unlock(&g_ev_mx);
}
int host_event_wait_any(int count, void **events, int wait_all, unsigned timeout_ms)
{
    struct timespec dl;
    clock_gettime(CLOCK_REALTIME, &dl);
    if (timeout_ms != 0xFFFFFFFFu) {
        dl.tv_sec += timeout_ms / 1000;
        dl.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
        if (dl.tv_nsec >= 1000000000L) { dl.tv_sec++; dl.tv_nsec -= 1000000000L; }
    }
    pthread_mutex_lock(&g_ev_mx);
    for (;;) {
        if (wait_all) {
            int all = 1;
            for (int i = 0; i < count; i++) if (!((struct hevent *)events[i])->state) all = 0;
            if (all) {
                for (int i = 0; i < count; i++) if (!((struct hevent *)events[i])->manual) ((struct hevent *)events[i])->state = 0;
                pthread_mutex_unlock(&g_ev_mx);
                return 0;
            }
        } else {
            for (int i = 0; i < count; i++) {
                struct hevent *e = events[i];
                if (e && e->state) {
                    if (!e->manual) e->state = 0;
                    pthread_mutex_unlock(&g_ev_mx);
                    return i;
                }
            }
        }
        int r = timeout_ms == 0xFFFFFFFFu ? pthread_cond_wait(&g_ev_cv, &g_ev_mx) : pthread_cond_timedwait(&g_ev_cv, &g_ev_mx, &dl);
        if (r == ETIMEDOUT) { pthread_mutex_unlock(&g_ev_mx); return -1; }
    }
}

/* ------------------------------------------------------------------ audio */
static SDL_AudioDeviceID g_audio;
static host_mix_fn g_mix;
static void *g_mix_user;
static void audio_cb(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    memset(stream, 0, len);
    if (g_mix) g_mix((short *)stream, len / 4, g_mix_user);
}
int host_audio_open(int rate, host_mix_fn mix, void *user)
{
    if (getenv("HM_NO_AUDIO")) return 0;
    SDL_AudioSpec want = {0}, have;
    want.freq = rate; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024; want.callback = audio_cb;
    g_mix = mix; g_mix_user = user;
    g_audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!g_audio) { host_log("audio: %s", SDL_GetError()); return 0; }
    SDL_PauseAudioDevice(g_audio, 0);
    return have.freq;
}
void host_audio_lock(void) { if (g_audio) SDL_LockAudioDevice(g_audio); }
void host_audio_unlock(void) { if (g_audio) SDL_UnlockAudioDevice(g_audio); }

/* ------------------------------------------------------------------ files */
/* Resolve a Windows-style relative path case-insensitively against the current directory. */
static int resolve_path(const char *in, char *out, size_t outsz, int for_create)
{
    char tmp[1024];
    size_t n = strlen(in);
    if (n >= sizeof tmp) return 0;
    strcpy(tmp, in);
    for (char *p = tmp; *p; p++) if (*p == '\\') *p = '/';
    char *s = tmp;
    if (s[0] && s[1] == ':') s += 2;          /* drop drive letter */
    while (*s == '/') s++;
    char cur[1024] = ".";
    char *save, *comp = strtok_r(s, "/", &save);
    while (comp) {
        char *next = strtok_r(NULL, "/", &save);
        char found[1024] = "";
        DIR *d = opendir(cur);
        if (d) {
            struct dirent *de;
            while ((de = readdir(d))) if (strcasecmp(de->d_name, comp) == 0) { snprintf(found, sizeof found, "%s/%s", cur, de->d_name); break; }
            closedir(d);
        }
        if (!found[0]) {
            if (next || !for_create) return 0;
            snprintf(found, sizeof found, "%s/%s", cur, comp);
        }
        snprintf(cur, sizeof cur, "%s", found);
        comp = next;
    }
    snprintf(out, outsz, "%s", cur);
    return 1;
}

void *host_fopen(const char *path, const char *mode)
{
    char real[1024];
    int create = strchr(mode, 'w') || strchr(mode, 'a');
    if (!resolve_path(path, real, sizeof real, create)) {
        if (getenv("HM_TRACE_FILES")) host_log("fopen(%s, %s) -> not found", path, mode);
        return NULL;
    }
    FILE *f = fopen(real, mode);
    if (getenv("HM_TRACE_FILES")) host_log("fopen(%s, %s) -> %s %p", path, mode, real, (void *)f);
    return f;
}
int host_fclose(void *f) { return f ? fclose((FILE *)f) : -1; }
unsigned host_fread(void *buf, unsigned size, unsigned n, void *f) { return f ? (unsigned)fread(buf, size, n, (FILE *)f) : 0; }
unsigned host_fwrite(const void *buf, unsigned size, unsigned n, void *f) { return f ? (unsigned)fwrite(buf, size, n, (FILE *)f) : 0; }
int host_fseek(void *f, long off, int whence) { return f ? fseek((FILE *)f, off, whence) : -1; }
long host_ftell(void *f) { return f ? ftell((FILE *)f) : -1; }

void *host_malloc(unsigned n) { return malloc(n); }
void host_free(void *p) { free(p); }
long long host_time(void) { return (long long)time(NULL); }

void host_localtime(long long t, int *out9)
{
    time_t tt = (time_t)t;
    struct tm tm;
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    out9[0] = tm.tm_sec; out9[1] = tm.tm_min; out9[2] = tm.tm_hour; out9[3] = tm.tm_mday; out9[4] = tm.tm_mon;
    out9[5] = tm.tm_year; out9[6] = tm.tm_wday; out9[7] = tm.tm_yday; out9[8] = tm.tm_isdst;
}
