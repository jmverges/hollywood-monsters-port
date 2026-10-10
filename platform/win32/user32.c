/* USER32 / GDI subset: one window, a message queue fed from host input events, cursor handling. */
#include "hm_platform.h"

#define HM_HWND ((HWND)0x00001001)
int hm_screen_w = 640, hm_screen_h = 480;

static WNDPROC g_wndproc;
static int g_quit_posted;

/* ---------------------------------------------------------------- message queue (main thread only) */
#define QSIZE 256
static MSG g_q[QSIZE];
static int g_qhead, g_qtail;
static int g_mouse_x, g_mouse_y, g_buttons;

static int queue_count(void) { return (g_qtail - g_qhead + QSIZE) % QSIZE; }

void hm_post_message(unsigned msg, unsigned wparam, int lparam)
{
    /* Like Windows, consecutive WM_MOUSEMOVE messages collapse into the latest one. */
    if (msg == WM_MOUSEMOVE && g_qtail != g_qhead) {
        MSG *last = &g_q[(g_qtail - 1 + QSIZE) % QSIZE];
        if (last->message == WM_MOUSEMOVE) {
            last->wParam = wparam; last->lParam = lparam; last->time = host_ticks_ms();
            last->pt.x = g_mouse_x; last->pt.y = g_mouse_y;
            return;
        }
    }
    int next = (g_qtail + 1) % QSIZE;
    if (next == g_qhead) {
        /* full: make room by dropping the oldest mouse move, never a click or a key */
        int i = g_qhead, found = -1;
        while (i != g_qtail) { if (g_q[i].message == WM_MOUSEMOVE) { found = i; break; } i = (i + 1) % QSIZE; }
        if (found < 0) { HM_TRACE("message queue full, dropping %#x", msg); return; }
        for (int j = found; j != g_qhead; j = (j - 1 + QSIZE) % QSIZE) g_q[j] = g_q[(j - 1 + QSIZE) % QSIZE];
        g_qhead = (g_qhead + 1) % QSIZE;
        next = (g_qtail + 1) % QSIZE;
    }
    MSG *m = &g_q[g_qtail];
    m->hwnd = HM_HWND;
    m->message = msg;
    m->wParam = wparam;
    m->lParam = lparam;
    m->time = host_ticks_ms();
    m->pt.x = g_mouse_x;
    m->pt.y = g_mouse_y;
    g_qtail = next;
}

static int mk_lparam(int x, int y) { return (int)(((unsigned)(y & 0xffff) << 16) | (unsigned)(x & 0xffff)); }

/* Windows sends WM_SETCURSOR (synchronously) before each mouse message over the window. The game uses it to
 * pick its cursor and to show the cursor again after hiding it. */
static void send_setcursor(unsigned mouse_msg)
{
    if (g_wndproc) g_wndproc(HM_HWND, WM_SETCURSOR, (WPARAM)HM_HWND, (LPARAM)((mouse_msg << 16) | 1 /* HTCLIENT */));
}

void hm_pump_host_events(void)
{
    struct host_event ev;
    /* leave events in the host queue while ours is nearly full */
    while (queue_count() < QSIZE - 8 && host_poll_event(&ev)) {
        switch (ev.type) {
        case HOST_EV_QUIT:
            hm_post_message(WM_DESTROY, 0, 0);
            break;
        case HOST_EV_MOUSEMOVE:
            send_setcursor(WM_MOUSEMOVE);
            g_mouse_x = ev.x; g_mouse_y = ev.y;
            hm_post_message(WM_MOUSEMOVE, g_buttons, mk_lparam(ev.x, ev.y));
            break;
        case HOST_EV_MOUSEDOWN:
        case HOST_EV_MOUSEUP: {
            int down = ev.type == HOST_EV_MOUSEDOWN;
            unsigned bit = ev.button == 1 ? 1 : ev.button == 3 ? 2 : 0x10;
            g_mouse_x = ev.x; g_mouse_y = ev.y;
            if (down) g_buttons |= bit; else g_buttons &= ~bit;
            unsigned msg = ev.button == 1 ? (down ? WM_LBUTTONDOWN : WM_LBUTTONUP)
                         : ev.button == 3 ? (down ? WM_RBUTTONDOWN : WM_RBUTTONUP)
                                          : (down ? WM_MBUTTONDOWN : WM_MBUTTONUP);
            if (down) send_setcursor(msg);
            hm_post_message(msg, g_buttons, mk_lparam(ev.x, ev.y));
            break;
        }
        case HOST_EV_WHEEL:
            hm_post_message(WM_MOUSEWHEEL, ((unsigned)(short)(ev.wheel * 120) << 16) | g_buttons, mk_lparam(ev.x, ev.y));
            break;
        case HOST_EV_KEYDOWN:
            hm_post_message(WM_KEYDOWN, ev.vk, 1);
            break;
        case HOST_EV_KEYUP:
            hm_post_message(WM_KEYUP, ev.vk, (int)0xC0000001u);
            break;
        case HOST_EV_CHAR:
            hm_post_message(WM_CHAR, ev.ch & 0xff, 1);
            break;
        case HOST_EV_FOCUS:
            /* The game treats deactivation like being minimised from fullscreen: it stops its sounds (the
             * current spoken line ends early) and restores surfaces and palette on return. In a window that
             * happens on every click elsewhere, so it only gets these with HM_FOCUS_PAUSE=1. */
            if (hm_getenv("HM_FOCUS_PAUSE")) hm_post_message(WM_ACTIVATEAPP, ev.focus ? 1 : 0, 0);
            break;
        }
    }
}

BOOL PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
    (void)hWnd; (void)wMsgFilterMin; (void)wMsgFilterMax;
    static int empties = -1;
    hm_pump_host_events();
    if (g_qhead == g_qtail) {
        /* The game busy-waits on PeekMessage (100% of a core, as on Windows). After many empty polls in a
         * row, yield for 1 ms; game timing comes from the timer thread, so this only saves CPU. */
        if (empties < 0) empties = hm_getenv("HM_NO_IDLE_SLEEP") ? -2 : 0;
        if (empties >= 0 && ++empties >= 32) { empties = 0; host_sleep_ms(1); }
        return 0;
    }
    if (empties > 0) empties = 0;
    *lpMsg = g_q[g_qhead];
    if (wRemoveMsg & 1) g_qhead = (g_qhead + 1) % QSIZE;
    return 1;
}

BOOL GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
    (void)hWnd; (void)wMsgFilterMin; (void)wMsgFilterMax;
    for (;;) {
        hm_pump_host_events();
        if (g_qhead != g_qtail) break;
        host_sleep_ms(1);
    }
    *lpMsg = g_q[g_qhead];
    g_qhead = (g_qhead + 1) % QSIZE;
    return lpMsg->message != WM_QUIT;
}

BOOL TranslateMessage(MSG *lpMsg) { (void)lpMsg; return 0; }

LRESULT DispatchMessageA(MSG *lpMsg)
{
    if (lpMsg->message == WM_QUIT) return 0;
    if (g_wndproc) return g_wndproc(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam);
    return 0;
}

void PostQuitMessage(int nExitCode)
{
    if (g_quit_posted) return;
    g_quit_posted = 1;
    hm_post_message(WM_QUIT, (unsigned)nExitCode, 0);
    host_log("PostQuitMessage(%d)", nExitCode);
    host_quit(nExitCode);
}

/* ---------------------------------------------------------------- window */
ATOM RegisterClassExA(WNDCLASSEXA *wc)
{
    g_wndproc = wc->lpfnWndProc;
    return 1;
}

HWND CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y,
                     int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    (void)dwExStyle; (void)lpClassName; (void)dwStyle; (void)X; (void)Y; (void)nWidth; (void)nHeight;
    (void)hWndParent; (void)hMenu; (void)hInstance; (void)lpParam;
    if (!host_window_open(lpWindowName ? (const char *)lpWindowName : "Hollywood Monsters", hm_screen_w, hm_screen_h))
        return 0;
    return HM_HWND;
}

BOOL ShowWindow(HWND hWnd, int nCmdShow) { (void)hWnd; (void)nCmdShow; return 1; }
BOOL UpdateWindow(HWND hWnd) { (void)hWnd; return 1; }

/* ---------------------------------------------------------------- cursor */
static int g_cursor_count;            /* ShowCursor display counter, like Windows */
static RECT g_clip = { 0, 0, 640, 480 };

/* ---------------------------------------------------------------- cursors
 * The game's cursors are RT_GROUP_CURSOR/RT_CURSOR resources of MONSTERS.EXE (32x32 monochrome, AND + XOR
 * masks). They are decoded to ARGB and drawn by the host. System cursors (hInstance 0) stay small fake
 * handles that SetCursor ignores. */
#define HM_IMAGE ((const unsigned char *)0x00400000u)

static unsigned rd16u(const unsigned char *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }
static unsigned rd32u(const unsigned char *p) { return rd16u(p) | (rd16u(p + 2) << 16); }

static const unsigned char *find_resource(unsigned type, unsigned id, unsigned *size)
{
    unsigned rva = host_image_resource_rva();
    if (!rva) return 0;
    const unsigned char *root = HM_IMAGE + rva, *dir = root;
    unsigned want[3] = { type, id, 0xffffffffu };      /* third level: any language */
    for (int level = 0; level < 3; level++) {
        unsigned n = rd16u(dir + 12) + rd16u(dir + 14);
        const unsigned char *e = dir + 16, *hit = 0;
        for (unsigned i = 0; i < n; i++, e += 8)
            if (want[level] == 0xffffffffu || (!(rd32u(e) & 0x80000000u) && rd32u(e) == want[level])) { hit = e; break; }
        if (!hit) return 0;
        unsigned off = rd32u(hit + 4);
        if (level < 2) {
            if (!(off & 0x80000000u)) return 0;
            dir = root + (off & 0x7fffffffu);
        } else {
            const unsigned char *data = root + off;     /* IMAGE_RESOURCE_DATA_ENTRY */
            if (size) *size = rd32u(data + 4);
            return HM_IMAGE + rd32u(data);
        }
    }
    return 0;
}

static void *decode_cursor(unsigned group_id)
{
    const unsigned char *grp = find_resource(12 /* RT_GROUP_CURSOR */, group_id, 0);
    if (!grp || rd16u(grp + 4) == 0) return 0;
    unsigned cur_id = rd16u(grp + 6 + 12);              /* first entry's nID */
    unsigned size = 0;
    const unsigned char *cur = find_resource(1 /* RT_CURSOR */, cur_id, &size);
    if (!cur || size < 4 + 40) return 0;
    int hx = (int)rd16u(cur), hy = (int)rd16u(cur + 2);
    const unsigned char *bi = cur + 4;
    int w = (int)rd32u(bi + 4), h = (int)rd32u(bi + 8) / 2, bpp = (int)rd16u(bi + 14);
    unsigned ncol = rd32u(bi + 32);
    if (w <= 0 || h <= 0 || w > 256 || h > 256 || (bpp != 1 && bpp != 4 && bpp != 8)) return 0;
    if (!ncol) ncol = 1u << bpp;
    const unsigned char *pal = bi + rd32u(bi);
    const unsigned char *xorb = pal + 4 * ncol;
    unsigned xstride = (unsigned)((w * bpp + 31) / 32 * 4), astride = (unsigned)((w + 31) / 32 * 4);
    const unsigned char *andb = xorb + xstride * (unsigned)h;
    if (andb + astride * (unsigned)h > cur + size) return 0;
    unsigned *argb = host_malloc((unsigned)(w * h * 4));
    if (!argb) return 0;
    for (int y = 0; y < h; y++) {
        const unsigned char *xr = xorb + xstride * (unsigned)(h - 1 - y), *ar = andb + astride * (unsigned)(h - 1 - y);
        for (int x = 0; x < w; x++) {
            unsigned idx = bpp == 1 ? (xr[x >> 3] >> (7 - (x & 7))) & 1
                         : bpp == 4 ? (xr[x >> 1] >> ((x & 1) ? 0 : 4)) & 15 : xr[x];
            int transparent = (ar[x >> 3] >> (7 - (x & 7))) & 1;
            const unsigned char *q = pal + 4 * (idx < ncol ? idx : 0);
            unsigned rgb = ((unsigned)q[2] << 16) | ((unsigned)q[1] << 8) | q[0];
            /* AND=1: transparent if XOR is black, inverted screen otherwise (drawn black here) */
            argb[y * w + x] = transparent ? (rgb ? 0xFF000000u : 0) : 0xFF000000u | rgb;
        }
    }
    void *c = host_cursor_create(argb, w, h, hx, hy);
    host_free(argb);
    HM_TRACE("cursor %u: %dx%d %d bpp, hot spot %d,%d -> %p", group_id, w, h, bpp, hx, hy, c);
    return c;
}

HCURSOR LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName)
{
    unsigned id = (unsigned)lpCursorName;
    if (hInstance && id < 0x10000u) {
        static struct { unsigned id; void *c; } cache[16];
        static int n;
        for (int i = 0; i < n; i++) if (cache[i].id == id) return (HCURSOR)cache[i].c;
        void *c = decode_cursor(id);
        if (c && n < 16) { cache[n].id = id; cache[n].c = c; n++; }
        if (c) return (HCURSOR)c;
    }
    return (HCURSOR)(0x2000u + (id & 0xffff));
}
HICON LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName) { (void)hInstance; return (HICON)(0x3000u + ((unsigned)lpIconName & 0xffff)); }
HGDIOBJ GetStockObject(int i) { return (HGDIOBJ)(0x4000u + (unsigned)i); }
HCURSOR SetCursor(HCURSOR hCursor)
{
    static HCURSOR current;
    HCURSOR prev = current;
    if ((unsigned)hCursor >= 0x10000u || !hCursor) {   /* fake system-cursor handles keep the current image */
        if (hCursor != current) HM_TRACE("SetCursor(%p)", (void *)hCursor);
        current = hCursor;
        host_cursor_set((void *)hCursor);
    }
    return prev;
}

int ShowCursor(BOOL bShow)
{
    g_cursor_count += bShow ? 1 : -1;
    HM_TRACE("ShowCursor(%d) -> %d", bShow, g_cursor_count);
    host_show_cursor(g_cursor_count >= 0 && !hm_getenv("HM_HIDE_CURSOR"));
    return g_cursor_count;
}

BOOL SetCursorPos(int X, int Y)
{
    HM_TRACE("SetCursorPos(%d, %d)", X, Y);
    g_mouse_x = X; g_mouse_y = Y;
    host_set_mouse(X, Y);
    return 1;
}

BOOL GetCursorPos(LPPOINT lpPoint)
{
    int x, y;
    host_get_mouse(&x, &y);
    lpPoint->x = x; lpPoint->y = y;
    return 1;
}

BOOL ClipCursor(RECT *lpRect) { if (lpRect) g_clip = *lpRect; return 1; }
BOOL GetClipCursor(LPRECT lpRect) { *lpRect = g_clip; return 1; }

int MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    (void)hWnd; (void)uType;
    host_log("MessageBox [%s]: %s", lpCaption ? (const char *)lpCaption : "", lpText ? (const char *)lpText : "");
    return 1;
}
BOOL MessageBeep(UINT uType) { (void)uType; return 1; }

/* The decompiler lost DefWindowProcA's arguments (no import signature); the port's default handling is 0. */
LRESULT DefWindowProcA() { return 0; }
