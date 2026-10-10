/* Host abstraction: the ONLY interface between the emulated Win32/DirectX layer and the real OS.
 * Uses primitive C types only, so it can be included from code built against the game's types.
 * Porting the game to a new system means re-implementing this file's functions (see host_sdl.c). */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* ---- lifecycle / misc */
int  host_window_open(const char *title, int width, int height);
void host_quit(int code);
void host_log(const char *fmt, ...);
unsigned host_ticks_ms(void);
void host_sleep_ms(unsigned ms);
int  host_is_main_thread(void);

/* ---- video: 8-bit indexed frame + 256-entry RGB palette (r,g,b,flags per entry) */
void host_submit_frame(const unsigned char *pixels, int pitch, int width, int height, const unsigned char *pal4);
void host_render_pending(void);

/* ---- input */
enum { HOST_EV_NONE, HOST_EV_QUIT, HOST_EV_MOUSEMOVE, HOST_EV_MOUSEDOWN, HOST_EV_MOUSEUP, HOST_EV_WHEEL,
       HOST_EV_KEYDOWN, HOST_EV_KEYUP, HOST_EV_CHAR, HOST_EV_FOCUS };
struct host_event {
    int type;
    int x, y;          /* mouse position in game coordinates */
    int button;        /* 1 left, 2 middle, 3 right */
    int vk;            /* Windows virtual-key code for key events */
    int ch;            /* character for HOST_EV_CHAR */
    int wheel;         /* +1 / -1 */
    int focus;         /* for HOST_EV_FOCUS */
};
int  host_poll_event(struct host_event *ev);
void host_get_mouse(int *x, int *y);
void host_set_mouse(int x, int y);
void host_show_cursor(int show);

/* ---- threads and synchronisation */
typedef unsigned (*host_thread_fn)(void *arg);
void *host_thread_create(host_thread_fn fn, void *arg);
void *host_mutex_create(void);
void  host_mutex_lock(void *m);
void  host_mutex_unlock(void *m);
void *host_event_create(int manual_reset, int initial_state);
void  host_event_set(void *e);
void  host_event_reset(void *e);
/* returns index of the signalled event, -1 on timeout. timeout_ms == 0xFFFFFFFF waits forever */
int   host_event_wait_any(int count, void **events, int wait_all, unsigned timeout_ms);

/* ---- audio: the host pulls interleaved stereo S16 frames from mix() */
typedef void (*host_mix_fn)(short *out, int frames, void *user);
int  host_audio_open(int rate, host_mix_fn mix, void *user);
void host_audio_lock(void);
void host_audio_unlock(void);

/* ---- files (paths are game-relative, case-insensitive, '\\' or '/') */
void *host_fopen(const char *path, const char *mode);
int   host_fclose(void *f);
unsigned host_fread(void *buf, unsigned size, unsigned n, void *f);
unsigned host_fwrite(const void *buf, unsigned size, unsigned n, void *f);
int   host_fseek(void *f, long off, int whence);
long  host_ftell(void *f);

/* ---- memory / time */
void *host_malloc(unsigned n);
void  host_free(void *p);
long long host_time(void);

#ifdef __cplusplus
}
#endif
