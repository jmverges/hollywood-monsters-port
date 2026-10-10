/* Shared declarations for the emulated Win32 / DirectX layer.
 * Code in platform/win32 is compiled against the game's own types (hm_common.h) and must not include
 * system headers; it reaches the real OS only through host.h. */
#pragma once
#include "hm_common.h"
#include "../host/host.h"

#define HM_MEMCPY(d, s, n) __builtin_memcpy((d), (s), (n))
#define HM_MEMSET(d, v, n) __builtin_memset((d), (v), (n))

extern int hm_debug;                 /* HM_DEBUG environment flag */
#define HM_TRACE(...) do { if (hm_debug) host_log(__VA_ARGS__); } while (0)

/* Windows message constants used by the layer */
#define WM_DESTROY      0x0002
#define WM_CLOSE        0x0010
#define WM_QUIT         0x0012
#define WM_ACTIVATEAPP  0x001C
#define WM_SETCURSOR    0x0020
#define WM_KEYDOWN      0x0100
#define WM_KEYUP        0x0101
#define WM_CHAR         0x0102
#define WM_MOUSEMOVE    0x0200
#define WM_LBUTTONDOWN  0x0201
#define WM_LBUTTONUP    0x0202
#define WM_RBUTTONDOWN  0x0204
#define WM_RBUTTONUP    0x0205
#define WM_MBUTTONDOWN  0x0207
#define WM_MBUTTONUP    0x0208
#define WM_MOUSEWHEEL   0x020A

/* user32.c */
void hm_post_message(unsigned msg, unsigned wparam, int lparam);
void hm_pump_host_events(void);
extern int hm_screen_w, hm_screen_h;

/* ddraw.c */
void hm_ddraw_present_now(void);

/* misc */
const char *hm_getenv(const char *name);
