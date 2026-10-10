/* WINMM multimedia timers. As on Windows, every timeSetEvent callback runs on one dedicated timer thread. */
#include "hm_platform.h"

#define MAX_TIMERS 512
struct mmtimer {
    int used;
    UINT id;
    UINT delay;
    unsigned next;
    int periodic;
    LPTIMECALLBACK cb;
    DWORD_PTR user;
};
static struct mmtimer g_timers[MAX_TIMERS];
static void *g_mx, *g_wake;
static UINT g_next_id = 1;
static int g_started;

static unsigned timer_thread(void *arg)
{
    (void)arg;
    for (;;) {
        host_mutex_lock(g_mx);
        unsigned now = host_ticks_ms();
        int due = -1;
        unsigned wait = 50;
        for (int i = 0; i < MAX_TIMERS; i++) {
            if (!g_timers[i].used) continue;
            int d = (int)(g_timers[i].next - now);
            if (d <= 0) {
                if (due < 0 || (int)(g_timers[i].next - g_timers[due].next) < 0) due = i;
            } else if ((unsigned)d < wait) {
                wait = (unsigned)d;
            }
        }
        if (due >= 0) {
            struct mmtimer t = g_timers[due];
            if (t.periodic) {
                g_timers[due].next += t.delay ? t.delay : 1;
                if ((int)(now - g_timers[due].next) > 200) g_timers[due].next = now + t.delay;   /* don't spiral */
            } else {
                g_timers[due].used = 0;
            }
            host_mutex_unlock(g_mx);
            t.cb(t.id, 0, t.user, 0, 0);
            continue;
        }
        host_mutex_unlock(g_mx);
        void *ev = g_wake;
        host_event_wait_any(1, &ev, 0, wait);
    }
    return 0;
}

static void ensure_started(void)
{
    if (g_started) return;
    g_started = 1;
    g_mx = host_mutex_create();
    g_wake = host_event_create(0, 0);
    host_thread_create(timer_thread, 0);
}

MMRESULT timeSetEvent(UINT uDelay, UINT uResolution, LPTIMECALLBACK fptc, DWORD_PTR dwUser, UINT fuEvent)
{
    (void)uResolution;
    ensure_started();
    host_mutex_lock(g_mx);
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (g_timers[i].used) continue;
        struct mmtimer *t = &g_timers[i];
        t->used = 1;
        t->id = g_next_id++;
        if (g_next_id == 0) g_next_id = 1;
        t->delay = uDelay;
        t->next = host_ticks_ms() + uDelay;
        t->periodic = (fuEvent & 1) != 0;
        t->cb = fptc;
        t->user = dwUser;
        UINT id = t->id;
        host_mutex_unlock(g_mx);
        host_event_set(g_wake);
        return id;
    }
    host_mutex_unlock(g_mx);
    host_log("timeSetEvent: out of timers");
    return 0;
}

MMRESULT timeKillEvent(UINT uTimerID)
{
    if (!g_started || uTimerID == 0) return 97;          /* MMSYSERR_INVALPARAM-like */
    host_mutex_lock(g_mx);
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (g_timers[i].used && g_timers[i].id == uTimerID) {
            g_timers[i].used = 0;
            host_mutex_unlock(g_mx);
            return 0;
        }
    }
    host_mutex_unlock(g_mx);
    return 97;
}

MMRESULT timeGetDevCaps(LPTIMECAPS ptc, UINT cbtc)
{
    (void)cbtc;
    ptc->wPeriodMin = 1;
    ptc->wPeriodMax = 1000000;
    return 0;
}
MMRESULT timeBeginPeriod(UINT uPeriod) { (void)uPeriod; return 0; }
MMRESULT timeEndPeriod(UINT uPeriod) { (void)uPeriod; return 0; }
