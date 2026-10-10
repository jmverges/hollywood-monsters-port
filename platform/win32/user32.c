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

void hm_post_message(unsigned msg, unsigned wparam, int lparam)
{
    int next = (g_qtail + 1) % QSIZE;
    if (next == g_qhead) return;                 /* queue full: drop */
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

void hm_pump_host_events(void)
{
    struct host_event ev;
    while (host_poll_event(&ev)) {
        switch (ev.type) {
        case HOST_EV_QUIT:
            hm_post_message(WM_DESTROY, 0, 0);
            break;
        case HOST_EV_MOUSEMOVE:
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
            hm_post_message(WM_ACTIVATEAPP, ev.focus ? 1 : 0, 0);
            break;
        }
    }
}

BOOL PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
    (void)hWnd; (void)wMsgFilterMin; (void)wMsgFilterMax;
    hm_pump_host_events();
    if (g_qhead == g_qtail) return 0;
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

HCURSOR LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName) { (void)hInstance; return (HCURSOR)(0x2000u + ((unsigned)lpCursorName & 0xffff)); }
HICON LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName) { (void)hInstance; return (HICON)(0x3000u + ((unsigned)lpIconName & 0xffff)); }
HGDIOBJ GetStockObject(int i) { return (HGDIOBJ)(0x4000u + (unsigned)i); }
HCURSOR SetCursor(HCURSOR hCursor) { (void)hCursor; return 0; }

int ShowCursor(BOOL bShow)
{
    g_cursor_count += bShow ? 1 : -1;
    host_show_cursor(g_cursor_count >= 0 && !hm_getenv("HM_HIDE_CURSOR"));
    return g_cursor_count;
}

BOOL SetCursorPos(int X, int Y)
{
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
