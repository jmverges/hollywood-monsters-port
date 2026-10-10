/* DirectSound emulation: secondary buffers are mixed in software into the host audio stream.
 * Supports PCM 8/16-bit mono/stereo at any rate, looping, volume/pan/frequency, play cursors,
 * and IDirectSoundNotify position events (used by the game's streaming threads). */
#include "hm_platform.h"

#define DS_OK 0
#define DSERR_GENERIC ((long)0x80004005)
#define DSERR_NOINTERFACE ((long)0x80004002)
#define DSBCAPS_PRIMARYBUFFER 0x1
#define DSBPLAY_LOOPING 0x1
#define DSBSTATUS_PLAYING 0x1
#define DSBSTATUS_LOOPING 0x4
#define DSBLOCK_FROMWRITECURSOR 0x1
#define DSBLOCK_ENTIREBUFFER 0x2
#define DSBPN_OFFSETSTOP 0xFFFFFFFFu
#define MAX_NOTIFY 64

struct ds_buffer;
struct ds_notify {
    IDirectSoundNotify iface;
    int refs;
    struct ds_buffer *buf;
};

struct ds_buffer {
    IDirectSoundBuffer iface;
    int refs;
    int primary;
    unsigned char *data;
    unsigned size;               /* bytes */
    int channels, bits, rate, align, base_rate;
    unsigned flags;
    volatile int playing, looping;
    unsigned pos_fp;             /* play position in frames, 16.16 fixed point */
    long volume, pan;
    float gl, gr;
    int nnotify;
    DSBPOSITIONNOTIFY notify[MAX_NOTIFY];
    struct ds_notify *notifier;
    struct ds_buffer *next;
};

static IDirectSoundBufferVtbl g_buf_vtbl;
static IDirectSoundVtbl g_ds_vtbl;
static IDirectSoundNotifyVtbl g_notify_vtbl;
static IDirectSound g_ds;
static struct ds_buffer *g_buffers;
static int g_out_rate = 44100;
static int g_audio_ok;

#define BUF(p) ((struct ds_buffer *)(p))

static long stub_ok(void) { return DS_OK; }

/* hundredths of dB -> linear gain */
static float db100_to_gain(long v)
{
    if (v <= -10000) return 0.0f;
    if (v >= 0) return 1.0f;
    return (float)__builtin_pow(10.0, (double)v / 2000.0);
}

static void update_gains(struct ds_buffer *b)
{
    float g = db100_to_gain(b->volume);
    float l = 1.0f, r = 1.0f;
    if (b->pan > 0) l = db100_to_gain(-b->pan);
    if (b->pan < 0) r = db100_to_gain(b->pan);
    b->gl = g * l;
    b->gr = g * r;
}

/* ---------------------------------------------------------------- mixer (host audio thread) */
static void fire_notifications(struct ds_buffer *b, unsigned from_byte, unsigned to_byte, int wrapped)
{
    for (int i = 0; i < b->nnotify; i++) {
        unsigned off = (unsigned)b->notify[i].dwOffset;
        if (off == DSBPN_OFFSETSTOP) continue;
        int hit = wrapped ? (off >= from_byte || off < to_byte) : (off >= from_byte && off < to_byte);
        if (hit && b->notify[i].hEventNotify) host_event_set(b->notify[i].hEventNotify);
    }
}

static void fire_stop(struct ds_buffer *b)
{
    for (int i = 0; i < b->nnotify; i++)
        if ((unsigned)b->notify[i].dwOffset == DSBPN_OFFSETSTOP && b->notify[i].hEventNotify)
            host_event_set(b->notify[i].hEventNotify);
}

static void mix(short *out, int frames, void *user)
{
    (void)user;
    static int acc[2 * 4096];
    if (frames > 4096) frames = 4096;
    HM_MEMSET(acc, 0, sizeof(int) * 2 * (unsigned)frames);
    for (struct ds_buffer *b = g_buffers; b; b = b->next) {
        if (b->primary || !b->playing || !b->data || b->align == 0) continue;
        unsigned nframes = b->size / (unsigned)b->align;
        if (!nframes) continue;
        unsigned step = (unsigned)(((unsigned long long)b->rate << 16) / (unsigned)g_out_rate);
        unsigned start_byte = (b->pos_fp >> 16) * (unsigned)b->align;
        int wrapped = 0;
        for (int i = 0; i < frames; i++) {
            unsigned f = b->pos_fp >> 16;
            if (f >= nframes) {
                if (b->looping) { b->pos_fp -= nframes << 16; f = b->pos_fp >> 16; wrapped = 1; }
                else { b->playing = 0; b->pos_fp = 0; fire_stop(b); break; }
            }
            int l, r;
            const unsigned char *p = b->data + f * (unsigned)b->align;
            if (b->bits == 16) {
                l = ((const short *)p)[0];
                r = b->channels == 2 ? ((const short *)p)[1] : l;
            } else {
                l = ((int)p[0] - 128) << 8;
                r = b->channels == 2 ? ((int)p[1] - 128) << 8 : l;
            }
            acc[i * 2] += (int)(l * b->gl);
            acc[i * 2 + 1] += (int)(r * b->gr);
            b->pos_fp += step;
        }
        if (b->nnotify) {
            unsigned end_byte = ((b->pos_fp >> 16) % (nframes ? nframes : 1)) * (unsigned)b->align;
            if (b->playing) fire_notifications(b, start_byte, end_byte, wrapped || end_byte < start_byte);
        }
    }
    for (int i = 0; i < frames * 2; i++) {
        int v = acc[i];
        out[i] = (short)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
}

/* ---------------------------------------------------------------- notify */
static ulong nt_AddRef(IDirectSoundNotify *This) { return ++((struct ds_notify *)This)->refs; }
static ulong nt_Release(IDirectSoundNotify *This) { return --((struct ds_notify *)This)->refs; }
static long nt_SetNotificationPositions(IDirectSoundNotify *This, ulong n, DSBPOSITIONNOTIFY *pos)
{
    struct ds_buffer *b = ((struct ds_notify *)This)->buf;
    host_audio_lock();
    b->nnotify = (int)(n > MAX_NOTIFY ? MAX_NOTIFY : n);
    for (int i = 0; i < b->nnotify; i++) b->notify[i] = pos[i];
    host_audio_unlock();
    HM_TRACE("DSB %p SetNotificationPositions n=%lu first=%lu last=%lu", (void *)b, n, n ? pos[0].dwOffset : 0, n ? pos[n - 1].dwOffset : 0);
    return DS_OK;
}

/* ---------------------------------------------------------------- buffers */
static long buf_QueryInterface(IDirectSoundBuffer *This, void *riid, void **ppv)
{
    unsigned first = riid ? *(unsigned *)riid : 0;
    if (first == 0xb0210783u) {                         /* IID_IDirectSoundNotify */
        struct ds_buffer *b = BUF(This);
        if (!b->notifier) {
            b->notifier = host_malloc(sizeof *b->notifier);
            HM_MEMSET(b->notifier, 0, sizeof *b->notifier);
            b->notifier->iface.lpVtbl = &g_notify_vtbl;
            b->notifier->buf = b;
        }
        b->notifier->refs++;
        *ppv = &b->notifier->iface;
        return DS_OK;
    }
    BUF(This)->refs++;
    *ppv = This;
    return DS_OK;
}
static ulong buf_AddRef(IDirectSoundBuffer *This) { return ++BUF(This)->refs; }
static ulong buf_Release(IDirectSoundBuffer *This)
{
    struct ds_buffer *b = BUF(This);
    if (--b->refs > 0) return b->refs;
    host_audio_lock();
    for (struct ds_buffer **pp = &g_buffers; *pp; pp = &(*pp)->next)
        if (*pp == b) { *pp = b->next; break; }
    host_audio_unlock();
    host_free(b->data);
    if (b->notifier) host_free(b->notifier);
    host_free(b);
    return 0;
}

static long buf_GetCaps(IDirectSoundBuffer *This, DSBCAPS *c)
{
    c->dwFlags = BUF(This)->flags;
    c->dwBufferBytes = BUF(This)->size;
    c->dwUnlockTransferRate = 0;
    c->dwPlayCpuOverhead = 0;
    return DS_OK;
}

static unsigned play_byte(struct ds_buffer *b)
{
    if (!b->align) return 0;
    unsigned nframes = b->size / (unsigned)b->align;
    unsigned f = b->pos_fp >> 16;
    if (nframes) f %= nframes;
    return f * (unsigned)b->align;
}

static long buf_GetCurrentPosition(IDirectSoundBuffer *This, ulong *play, ulong *write)
{
    struct ds_buffer *b = BUF(This);
    unsigned p = play_byte(b);
    if (play) *play = p;
    if (write) {
        unsigned ahead = (unsigned)(b->rate / 100) * (unsigned)b->align;
        *write = b->size ? (p + ahead) % b->size : 0;
    }
    return DS_OK;
}

static long buf_GetFormat(IDirectSoundBuffer *This, DXWAVEFORMATEX *f, ulong sz, ulong *written)
{
    struct ds_buffer *b = BUF(This);
    if (f && sz >= 16) {
        f->wFormatTag = 1; f->nChannels = (unsigned short)b->channels; f->nSamplesPerSec = (ulong)b->base_rate;
        f->wBitsPerSample = (unsigned short)b->bits; f->nBlockAlign = (unsigned short)b->align;
        f->nAvgBytesPerSec = (ulong)(b->base_rate * b->align);
        if (sz >= 18) f->cbSize = 0;                    /* WAVEFORMATEX is 18 bytes on Windows, 20 here (padding) */
    }
    if (written) *written = 18;
    return DS_OK;
}

static long buf_GetVolume(IDirectSoundBuffer *This, long *v) { *v = BUF(This)->volume; return DS_OK; }
static long buf_GetPan(IDirectSoundBuffer *This, long *v) { *v = BUF(This)->pan; return DS_OK; }
static long buf_GetFrequency(IDirectSoundBuffer *This, ulong *v) { *v = (ulong)BUF(This)->rate; return DS_OK; }
static long buf_GetStatus(IDirectSoundBuffer *This, ulong *s)
{
    struct ds_buffer *b = BUF(This);
    *s = (b->playing ? DSBSTATUS_PLAYING : 0) | (b->playing && b->looping ? DSBSTATUS_LOOPING : 0);
    return DS_OK;
}

static long buf_Lock(IDirectSoundBuffer *This, ulong off, ulong bytes, void **p1, ulong *n1, void **p2, ulong *n2, ulong flags)
{
    struct ds_buffer *b = BUF(This);
    if (!b->data || !b->size) return DSERR_GENERIC;
    if (flags & DSBLOCK_FROMWRITECURSOR) { ulong w; buf_GetCurrentPosition(This, 0, &w); off = w; }
    if (flags & DSBLOCK_ENTIREBUFFER) { off = 0; bytes = b->size; }
    off %= b->size;
    if (bytes > b->size) bytes = b->size;
    *p1 = b->data + off;
    if (off + bytes <= b->size) {
        *n1 = bytes;
        if (p2) *p2 = 0;
        if (n2) *n2 = 0;
    } else {
        *n1 = b->size - off;
        if (p2) *p2 = b->data;
        if (n2) *n2 = bytes - (b->size - off);
    }
    return DS_OK;
}

static long buf_Unlock(IDirectSoundBuffer *This, void *p1, ulong n1, void *p2, ulong n2)
{
    (void)This; (void)p1; (void)n1; (void)p2; (void)n2;
    return DS_OK;
}

static long buf_Play(IDirectSoundBuffer *This, ulong r1, ulong prio, ulong flags)
{
    (void)r1; (void)prio;
    struct ds_buffer *b = BUF(This);
    host_audio_lock();
    b->looping = (flags & DSBPLAY_LOOPING) != 0;
    b->playing = 1;
    host_audio_unlock();
    HM_TRACE("DSB %p Play flags=%lu size=%u rate=%d notify=%d", (void *)b, flags, b->size, b->rate, b->nnotify);
    return DS_OK;
}

static long buf_Stop(IDirectSoundBuffer *This)
{
    struct ds_buffer *b = BUF(This);
    host_audio_lock();
    int was = b->playing;
    b->playing = 0;
    if (was) fire_stop(b);
    host_audio_unlock();
    HM_TRACE("DSB %p Stop", (void *)b);
    return DS_OK;
}

static long buf_SetCurrentPosition(IDirectSoundBuffer *This, ulong pos)
{
    struct ds_buffer *b = BUF(This);
    host_audio_lock();
    b->pos_fp = b->align ? (unsigned)((pos % (b->size ? b->size : 1)) / (unsigned)b->align) << 16 : 0;
    host_audio_unlock();
    return DS_OK;
}

static long buf_SetVolume(IDirectSoundBuffer *This, long v) { BUF(This)->volume = v; update_gains(BUF(This)); return DS_OK; }
static long buf_SetPan(IDirectSoundBuffer *This, long v) { BUF(This)->pan = v; update_gains(BUF(This)); return DS_OK; }
static long buf_SetFrequency(IDirectSoundBuffer *This, ulong f) { BUF(This)->rate = f ? (int)f : BUF(This)->base_rate; return DS_OK; }
static long buf_SetFormat(IDirectSoundBuffer *This, DXWAVEFORMATEX *f) { (void)This; (void)f; return DS_OK; }

/* ---------------------------------------------------------------- device */
static ulong ds_AddRef(IDirectSound *This) { (void)This; return 1; }
static ulong ds_Release(IDirectSound *This) { (void)This; return 0; }

static long ds_CreateSoundBuffer(IDirectSound *This, DSBUFFERDESC *d, IDirectSoundBuffer **out, void *outer)
{
    (void)This; (void)outer;
    struct ds_buffer *b = host_malloc(sizeof *b);
    HM_MEMSET(b, 0, sizeof *b);
    b->iface.lpVtbl = &g_buf_vtbl;
    b->refs = 1;
    b->flags = (unsigned)d->dwFlags;
    b->primary = (d->dwFlags & DSBCAPS_PRIMARYBUFFER) != 0;
    if (!b->primary) {
        DXWAVEFORMATEX *f = d->lpwfxFormat;
        b->channels = f ? f->nChannels : 2;
        b->bits = f ? f->wBitsPerSample : 16;
        b->base_rate = b->rate = f ? (int)f->nSamplesPerSec : 22050;
        b->align = f && f->nBlockAlign ? f->nBlockAlign : b->channels * b->bits / 8;
        b->size = (unsigned)d->dwBufferBytes;
        b->data = host_malloc(b->size ? b->size : 1);
        HM_MEMSET(b->data, b->bits == 8 ? 0x80 : 0, b->size);
    }
    update_gains(b);
    host_audio_lock();
    b->next = g_buffers;
    g_buffers = b;
    host_audio_unlock();
    HM_TRACE("CreateSoundBuffer flags=%#lx bytes=%u %dHz %dbit %dch", d->dwFlags, b->size, b->rate, b->bits, b->channels);
    *out = &b->iface;
    return DS_OK;
}

static long ds_DuplicateSoundBuffer(IDirectSound *This, IDirectSoundBuffer *orig, IDirectSoundBuffer **out)
{
    (void)This;
    struct ds_buffer *o = BUF(orig);
    DXWAVEFORMATEX wf = { 1, (unsigned short)o->channels, (ulong)o->base_rate, (ulong)(o->base_rate * o->align),
                          (unsigned short)o->align, (unsigned short)o->bits, 0 };
    DSBUFFERDESC d;
    HM_MEMSET(&d, 0, sizeof d);
    d.dwSize = sizeof d; d.dwFlags = o->flags; d.dwBufferBytes = o->size; d.lpwfxFormat = &wf;
    long r = ds_CreateSoundBuffer(This, &d, out, 0);
    if (r == DS_OK) HM_MEMCPY(BUF(*out)->data, o->data, o->size);
    return r;
}

static void fill_vtable(void **slots, int n, void *fn) { for (int i = 0; i < n; i++) slots[i] = fn; }

long DirectSoundCreate(void *pcGuidDevice, IDirectSound **ppDS, void *pUnkOuter)
{
    (void)pcGuidDevice; (void)pUnkOuter;
    static int init;
    if (!init) {
        init = 1;
        fill_vtable((void **)&g_ds_vtbl, sizeof g_ds_vtbl / sizeof(void *), (void *)stub_ok);
        g_ds_vtbl.AddRef = ds_AddRef;
        g_ds_vtbl.Release = ds_Release;
        g_ds_vtbl.CreateSoundBuffer = ds_CreateSoundBuffer;
        g_ds_vtbl.DuplicateSoundBuffer = ds_DuplicateSoundBuffer;

        fill_vtable((void **)&g_buf_vtbl, sizeof g_buf_vtbl / sizeof(void *), (void *)stub_ok);
        g_buf_vtbl.QueryInterface = buf_QueryInterface;
        g_buf_vtbl.AddRef = buf_AddRef;
        g_buf_vtbl.Release = buf_Release;
        g_buf_vtbl.GetCaps = buf_GetCaps;
        g_buf_vtbl.GetCurrentPosition = buf_GetCurrentPosition;
        g_buf_vtbl.GetFormat = buf_GetFormat;
        g_buf_vtbl.GetVolume = buf_GetVolume;
        g_buf_vtbl.GetPan = buf_GetPan;
        g_buf_vtbl.GetFrequency = buf_GetFrequency;
        g_buf_vtbl.GetStatus = buf_GetStatus;
        g_buf_vtbl.Lock = buf_Lock;
        g_buf_vtbl.Play = buf_Play;
        g_buf_vtbl.SetCurrentPosition = buf_SetCurrentPosition;
        g_buf_vtbl.SetFormat = buf_SetFormat;
        g_buf_vtbl.SetVolume = buf_SetVolume;
        g_buf_vtbl.SetPan = buf_SetPan;
        g_buf_vtbl.SetFrequency = buf_SetFrequency;
        g_buf_vtbl.Stop = buf_Stop;
        g_buf_vtbl.Unlock = buf_Unlock;

        fill_vtable((void **)&g_notify_vtbl, sizeof g_notify_vtbl / sizeof(void *), (void *)stub_ok);
        g_notify_vtbl.AddRef = nt_AddRef;
        g_notify_vtbl.Release = nt_Release;
        g_notify_vtbl.SetNotificationPositions = nt_SetNotificationPositions;

        int rate = host_audio_open(44100, mix, 0);
        g_audio_ok = rate > 0;
        if (rate > 0) g_out_rate = rate;
    }
    g_ds.lpVtbl = &g_ds_vtbl;
    *ppDS = &g_ds;
    return DS_OK;
}
