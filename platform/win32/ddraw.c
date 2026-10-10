/* DirectDraw 7 emulation: 8-bit palettized surfaces in memory, primary + back buffer flip chain.
 * Frames are handed to the host as 8bpp + palette on Flip, on Unlock of the primary, and on palette changes. */
#include "hm_platform.h"

#define DD_OK 0
#define DDERR_GENERIC ((long)0x80004005)
#define DDERR_INVALIDPARAMS ((long)0x80070057)
#define DDERR_NOTFOUND ((long)0x887600FF)

#define DDSD_CAPS 0x1
#define DDSD_HEIGHT 0x2
#define DDSD_WIDTH 0x4
#define DDSD_PITCH 0x8
#define DDSD_BACKBUFFERCOUNT 0x20
#define DDSD_PIXELFORMAT 0x1000
#define DDSD_LPSURFACE 0x800
#define DDSCAPS_BACKBUFFER 0x4
#define DDSCAPS_FLIP 0x10
#define DDSCAPS_FRONTBUFFER 0x20
#define DDSCAPS_OFFSCREENPLAIN 0x40
#define DDSCAPS_PRIMARYSURFACE 0x200
#define DDSCAPS_SYSTEMMEMORY 0x800
#define DDSCAPS_VIDEOMEMORY 0x4000
#define DDPF_RGB 0x40
#define DDPF_PALETTEINDEXED8 0x20
#define DDBLT_COLORFILL 0x400
#define DDBLT_KEYSRC 0x8000
#define DDBLTFAST_SRCCOLORKEY 0x1
#define DDCKEY_SRCBLT 0x8

struct dd_palette {
    IDirectDrawPalette iface;
    int refs;
    unsigned char rgbx[1024];     /* r,g,b,flags */
};

struct dd_surface {
    IDirectDrawSurface7 iface;
    int refs;
    int w, h, pitch;
    unsigned char *pixels;
    int primary;
    struct dd_surface *back;      /* for the primary: attached back buffer */
    struct dd_surface *owner;     /* for the back buffer: its primary */
    struct dd_palette *pal;
    int has_ck;
    unsigned ck;
    unsigned caps;
};

struct dd_device {
    IDirectDraw7 iface;
    int refs;
};

static IDirectDrawSurface7Vtbl g_surf_vtbl;
static IDirectDrawPaletteVtbl g_pal_vtbl;
static IDirectDraw7Vtbl g_dd_vtbl;
static struct dd_device g_dd;
static int g_mode_w = 640, g_mode_h = 480;
static struct dd_surface *g_primary;

#define SURF(p) ((struct dd_surface *)(p))
#define PAL(p) ((struct dd_palette *)(p))

/* ---------------------------------------------------------------- presentation */
static const unsigned char *current_palette(void)
{
    static unsigned char grey[1024];
    if (g_primary && g_primary->pal) return g_primary->pal->rgbx;
    for (int i = 0; i < 256; i++) { grey[i * 4] = grey[i * 4 + 1] = grey[i * 4 + 2] = (unsigned char)i; }
    return grey;
}

void hm_ddraw_present_now(void)
{
    if (!g_primary) return;
    host_submit_frame(g_primary->pixels, g_primary->pitch, g_primary->w, g_primary->h, current_palette());
}

/* ---------------------------------------------------------------- generic stubs */
static long stub_ok(void) { return DD_OK; }
static long stub_fail(void) { return DDERR_GENERIC; }

/* ---------------------------------------------------------------- palette */
static ulong pal_AddRef(IDirectDrawPalette *This) { return ++PAL(This)->refs; }
static ulong pal_Release(IDirectDrawPalette *This)
{
    int r = --PAL(This)->refs;
    if (r <= 0) {
        if (g_primary && g_primary->pal == PAL(This)) g_primary->pal = 0;
        host_free(This);
        return 0;
    }
    return r;
}
static long pal_GetEntries(IDirectDrawPalette *This, ulong flags, ulong base, ulong n, DXPALETTEENTRY *e)
{
    (void)flags;
    for (ulong i = 0; i < n && base + i < 256; i++) {
        e[i].peRed = PAL(This)->rgbx[(base + i) * 4];
        e[i].peGreen = PAL(This)->rgbx[(base + i) * 4 + 1];
        e[i].peBlue = PAL(This)->rgbx[(base + i) * 4 + 2];
        e[i].peFlags = PAL(This)->rgbx[(base + i) * 4 + 3];
    }
    return DD_OK;
}
static long pal_SetEntries(IDirectDrawPalette *This, ulong flags, ulong start, ulong n, DXPALETTEENTRY *e)
{
    (void)flags;
    for (ulong i = 0; i < n && start + i < 256; i++) {
        PAL(This)->rgbx[(start + i) * 4] = e[i].peRed;
        PAL(This)->rgbx[(start + i) * 4 + 1] = e[i].peGreen;
        PAL(This)->rgbx[(start + i) * 4 + 2] = e[i].peBlue;
        PAL(This)->rgbx[(start + i) * 4 + 3] = e[i].peFlags;
    }
    if (g_primary && g_primary->pal == PAL(This)) hm_ddraw_present_now();
    return DD_OK;
}

static struct dd_palette *new_palette(DXPALETTEENTRY *init)
{
    struct dd_palette *p = host_malloc(sizeof *p);
    HM_MEMSET(p, 0, sizeof *p);
    p->iface.lpVtbl = &g_pal_vtbl;
    p->refs = 1;
    if (init) pal_SetEntries(&p->iface, 0, 0, 256, init);
    return p;
}

/* ---------------------------------------------------------------- surfaces */
static struct dd_surface *new_surface(int w, int h, unsigned caps)
{
    struct dd_surface *s = host_malloc(sizeof *s);
    HM_MEMSET(s, 0, sizeof *s);
    s->iface.lpVtbl = &g_surf_vtbl;
    s->refs = 1;
    s->w = w; s->h = h;
    s->pitch = (w + 3) & ~3;
    s->pixels = host_malloc((unsigned)(s->pitch * h));
    HM_MEMSET(s->pixels, 0, (unsigned)(s->pitch * h));
    s->caps = caps;
    return s;
}

static long surf_QueryInterface(IDirectDrawSurface7 *This, void *riid, void **ppv) { (void)riid; SURF(This)->refs++; *ppv = This; return DD_OK; }
static ulong surf_AddRef(IDirectDrawSurface7 *This) { return ++SURF(This)->refs; }
static ulong surf_Release(IDirectDrawSurface7 *This)
{
    struct dd_surface *s = SURF(This);
    int r = --s->refs;
    if (r > 0) return r;
    if (s->owner) return 0;                       /* back buffers live as long as their primary */
    if (s->back) { host_free(s->back->pixels); host_free(s->back); }
    if (s == g_primary) g_primary = 0;
    host_free(s->pixels);
    host_free(s);
    return 0;
}

static void fill_desc(struct dd_surface *s, DDSURFACEDESC2 *d)
{
    HM_MEMSET(d, 0, sizeof *d);
    d->dwSize = sizeof *d;
    d->dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH | DDSD_PIXELFORMAT | DDSD_LPSURFACE;
    d->dwWidth = s->w;
    d->dwHeight = s->h;
    d->lPitch = s->pitch;
    d->lpSurface = s->pixels;
    d->dwBackBufferCount = s->back ? 1 : 0;
    d->ddpfPixelFormat.dwSize = sizeof d->ddpfPixelFormat;
    d->ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_PALETTEINDEXED8;
    d->ddpfPixelFormat.dwRGBBitCount = 8;
    d->ddsCaps.dwCaps = s->caps;
}

static long surf_GetSurfaceDesc(IDirectDrawSurface7 *This, DDSURFACEDESC2 *d) { fill_desc(SURF(This), d); return DD_OK; }
static long surf_GetCaps(IDirectDrawSurface7 *This, DDSCAPS2 *c) { HM_MEMSET(c, 0, sizeof *c); c->dwCaps = SURF(This)->caps; return DD_OK; }

static long surf_Lock(IDirectDrawSurface7 *This, DXRECT *r, DDSURFACEDESC2 *d, ulong flags, void *ev)
{
    (void)flags; (void)ev;
    struct dd_surface *s = SURF(This);
    fill_desc(s, d);
    if (r) d->lpSurface = s->pixels + r->top * s->pitch + r->left;
    return DD_OK;
}

static long surf_Unlock(IDirectDrawSurface7 *This, DXRECT *r)
{
    (void)r;
    if (SURF(This)->primary) hm_ddraw_present_now();
    return DD_OK;
}

static long surf_GetAttachedSurface(IDirectDrawSurface7 *This, DDSCAPS2 *caps, IDirectDrawSurface7 **out)
{
    (void)caps;
    struct dd_surface *s = SURF(This);
    if (!s->back) return DDERR_NOTFOUND;
    s->back->refs++;
    *out = &s->back->iface;
    return DD_OK;
}

static long surf_Flip(IDirectDrawSurface7 *This, IDirectDrawSurface7 *target, ulong flags)
{
    (void)target; (void)flags;
    struct dd_surface *s = SURF(This);
    if (s->back) {
        unsigned char *t = s->pixels;
        s->pixels = s->back->pixels;
        s->back->pixels = t;
    }
    hm_ddraw_present_now();
    return DD_OK;
}

static void clip_copy(struct dd_surface *dst, int dx, int dy, struct dd_surface *src, int sx, int sy, int w, int h, int use_ck)
{
    if (dx < 0) { sx -= dx; w += dx; dx = 0; }
    if (dy < 0) { sy -= dy; h += dy; dy = 0; }
    if (sx < 0) { dx -= sx; w += sx; sx = 0; }
    if (sy < 0) { dy -= sy; h += sy; sy = 0; }
    if (dx + w > dst->w) w = dst->w - dx;
    if (dy + h > dst->h) h = dst->h - dy;
    if (sx + w > src->w) w = src->w - sx;
    if (sy + h > src->h) h = src->h - sy;
    if (w <= 0 || h <= 0) return;
    unsigned char key = (unsigned char)src->ck;
    for (int y = 0; y < h; y++) {
        unsigned char *d = dst->pixels + (dy + y) * dst->pitch + dx;
        const unsigned char *sp = src->pixels + (sy + y) * src->pitch + sx;
        if (!use_ck || !src->has_ck) {
            __builtin_memmove(d, sp, (unsigned)w);
        } else {
            for (int x = 0; x < w; x++) if (sp[x] != key) d[x] = sp[x];
        }
    }
}

static long surf_BltFast(IDirectDrawSurface7 *This, ulong x, ulong y, IDirectDrawSurface7 *src, DXRECT *sr, ulong trans)
{
    struct dd_surface *s = SURF(src);
    if (!s) return DDERR_INVALIDPARAMS;
    int sx = 0, sy = 0, w = s->w, h = s->h;
    if (sr) { sx = sr->left; sy = sr->top; w = sr->right - sr->left; h = sr->bottom - sr->top; }
    clip_copy(SURF(This), (int)x, (int)y, s, sx, sy, w, h, (trans & DDBLTFAST_SRCCOLORKEY) != 0);
    if (SURF(This)->primary) hm_ddraw_present_now();
    return DD_OK;
}

static long surf_Blt(IDirectDrawSurface7 *This, DXRECT *dr, IDirectDrawSurface7 *src, DXRECT *sr, ulong flags, DDBLTFX *fx)
{
    struct dd_surface *d = SURF(This);
    int dx = 0, dy = 0, dw = d->w, dh = d->h;
    if (dr) { dx = dr->left; dy = dr->top; dw = dr->right - dr->left; dh = dr->bottom - dr->top; }
    if (flags & DDBLT_COLORFILL) {
        unsigned char c = fx ? (unsigned char)((ulong *)fx)[0x50 / 4] : 0;
        for (int y = dy; y < dy + dh && y < d->h; y++) {
            if (y < 0) continue;
            int x0 = dx < 0 ? 0 : dx, x1 = dx + dw > d->w ? d->w : dx + dw;
            if (x1 > x0) HM_MEMSET(d->pixels + y * d->pitch + x0, c, (unsigned)(x1 - x0));
        }
    } else if (src) {
        struct dd_surface *s = SURF(src);
        int sx = 0, sy = 0, sw = s->w, sh = s->h;
        if (sr) { sx = sr->left; sy = sr->top; sw = sr->right - sr->left; sh = sr->bottom - sr->top; }
        if (sw == dw && sh == dh) {
            clip_copy(d, dx, dy, s, sx, sy, sw, sh, (flags & DDBLT_KEYSRC) != 0);
        } else if (dw > 0 && dh > 0) {                 /* nearest-neighbour stretch */
            for (int y = 0; y < dh; y++) {
                int ty = dy + y, fy = sy + y * sh / dh;
                if (ty < 0 || ty >= d->h || fy < 0 || fy >= s->h) continue;
                for (int x = 0; x < dw; x++) {
                    int tx = dx + x, fxp = sx + x * sw / dw;
                    if (tx < 0 || tx >= d->w || fxp < 0 || fxp >= s->w) continue;
                    unsigned char p = s->pixels[fy * s->pitch + fxp];
                    if ((flags & DDBLT_KEYSRC) && s->has_ck && p == (unsigned char)s->ck) continue;
                    d->pixels[ty * d->pitch + tx] = p;
                }
            }
        }
    }
    if (d->primary) hm_ddraw_present_now();
    return DD_OK;
}

static long surf_SetColorKey(IDirectDrawSurface7 *This, ulong flags, DDCOLORKEY *k)
{
    (void)flags;
    if (k) { SURF(This)->has_ck = 1; SURF(This)->ck = k->dwColorSpaceLowValue; } else SURF(This)->has_ck = 0;
    return DD_OK;
}
static long surf_GetColorKey(IDirectDrawSurface7 *This, ulong flags, DDCOLORKEY *k)
{
    (void)flags;
    if (!SURF(This)->has_ck) return DDERR_NOTFOUND;
    k->dwColorSpaceLowValue = k->dwColorSpaceHighValue = SURF(This)->ck;
    return DD_OK;
}
static long surf_SetPalette(IDirectDrawSurface7 *This, IDirectDrawPalette *p)
{
    SURF(This)->pal = PAL(p);
    if (p) PAL(p)->refs++;
    if (SURF(This)->primary) hm_ddraw_present_now();
    return DD_OK;
}
static long surf_GetPalette(IDirectDrawSurface7 *This, IDirectDrawPalette **p)
{
    if (!SURF(This)->pal) return DDERR_NOTFOUND;
    SURF(This)->pal->refs++;
    *p = &SURF(This)->pal->iface;
    return DD_OK;
}
static long surf_GetPixelFormat(IDirectDrawSurface7 *This, DDPIXELFORMAT *pf)
{
    (void)This;
    HM_MEMSET(pf, 0, sizeof *pf);
    pf->dwSize = sizeof *pf;
    pf->dwFlags = DDPF_RGB | DDPF_PALETTEINDEXED8;
    pf->dwRGBBitCount = 8;
    return DD_OK;
}

/* ---------------------------------------------------------------- device */
static long dd_QueryInterface(IDirectDraw7 *This, void *riid, void **ppv) { (void)riid; g_dd.refs++; *ppv = This; return DD_OK; }
static ulong dd_AddRef(IDirectDraw7 *This) { (void)This; return ++g_dd.refs; }
static ulong dd_Release(IDirectDraw7 *This) { (void)This; return g_dd.refs > 0 ? --g_dd.refs : 0; }

static long dd_SetDisplayMode(IDirectDraw7 *This, ulong w, ulong h, ulong bpp, ulong rate, ulong flags)
{
    (void)This; (void)rate; (void)flags;
    HM_TRACE("SetDisplayMode %lux%lux%lu", w, h, bpp);
    g_mode_w = (int)w; g_mode_h = (int)h;
    hm_screen_w = (int)w; hm_screen_h = (int)h;
    return DD_OK;
}

static long dd_GetDisplayMode(IDirectDraw7 *This, DDSURFACEDESC2 *d)
{
    (void)This;
    struct dd_surface tmp;
    HM_MEMSET(&tmp, 0, sizeof tmp);
    tmp.w = g_mode_w; tmp.h = g_mode_h; tmp.pitch = g_mode_w;
    fill_desc(&tmp, d);
    d->lpSurface = 0;
    return DD_OK;
}

static long dd_CreateSurface(IDirectDraw7 *This, DDSURFACEDESC2 *d, IDirectDrawSurface7 **out, void *outer)
{
    (void)This; (void)outer;
    unsigned caps = (d->dwFlags & DDSD_CAPS) ? d->ddsCaps.dwCaps : 0;
    struct dd_surface *s;
    if (caps & DDSCAPS_PRIMARYSURFACE) {
        if (g_primary) { surf_Release(&g_primary->iface); }
        s = new_surface(g_mode_w, g_mode_h, caps | DDSCAPS_FRONTBUFFER | DDSCAPS_VIDEOMEMORY);
        s->primary = 1;
        if ((d->dwFlags & DDSD_BACKBUFFERCOUNT) && d->dwBackBufferCount > 0) {
            s->back = new_surface(g_mode_w, g_mode_h, DDSCAPS_BACKBUFFER | DDSCAPS_FLIP | DDSCAPS_VIDEOMEMORY);
            s->back->owner = s;
        }
        g_primary = s;
    } else {
        int w = (d->dwFlags & DDSD_WIDTH) ? (int)d->dwWidth : g_mode_w;
        int h = (d->dwFlags & DDSD_HEIGHT) ? (int)d->dwHeight : g_mode_h;
        s = new_surface(w, h, caps ? caps : DDSCAPS_OFFSCREENPLAIN);
    }
    HM_TRACE("CreateSurface caps=%#x %dx%d -> %p", caps, s->w, s->h, (void *)s);
    *out = &s->iface;
    return DD_OK;
}

static long dd_CreatePalette(IDirectDraw7 *This, ulong flags, DXPALETTEENTRY *e, IDirectDrawPalette **out, void *outer)
{
    (void)This; (void)flags; (void)outer;
    *out = &new_palette(e)->iface;
    return DD_OK;
}

static long dd_GetCaps(IDirectDraw7 *This, void *drv, void *hel)
{
    (void)This;
    if (drv) HM_MEMSET((char *)drv + 4, 0, ((ulong *)drv)[0] > 4 ? ((ulong *)drv)[0] - 4 : 0);
    if (hel) HM_MEMSET((char *)hel + 4, 0, ((ulong *)hel)[0] > 4 ? ((ulong *)hel)[0] - 4 : 0);
    return DD_OK;
}

static long dd_WaitForVerticalBlank(IDirectDraw7 *This, ulong flags, void *ev) { (void)This; (void)flags; (void)ev; host_sleep_ms(1); return DD_OK; }

static void fill_vtable(void **slots, int n, void *fn) { for (int i = 0; i < n; i++) slots[i] = fn; }

static void init_vtables(void)
{
    static int done;
    if (done) return;
    done = 1;
    fill_vtable((void **)&g_surf_vtbl, sizeof g_surf_vtbl / sizeof(void *), (void *)stub_ok);
    g_surf_vtbl.QueryInterface = surf_QueryInterface;
    g_surf_vtbl.AddRef = surf_AddRef;
    g_surf_vtbl.Release = surf_Release;
    g_surf_vtbl.Blt = surf_Blt;
    g_surf_vtbl.BltFast = surf_BltFast;
    g_surf_vtbl.Flip = surf_Flip;
    g_surf_vtbl.GetAttachedSurface = surf_GetAttachedSurface;
    g_surf_vtbl.GetCaps = surf_GetCaps;
    g_surf_vtbl.GetColorKey = surf_GetColorKey;
    g_surf_vtbl.GetDC = (void *)stub_fail;
    g_surf_vtbl.GetPalette = surf_GetPalette;
    g_surf_vtbl.GetPixelFormat = surf_GetPixelFormat;
    g_surf_vtbl.GetSurfaceDesc = surf_GetSurfaceDesc;
    g_surf_vtbl.Lock = surf_Lock;
    g_surf_vtbl.SetColorKey = surf_SetColorKey;
    g_surf_vtbl.SetPalette = surf_SetPalette;
    g_surf_vtbl.Unlock = surf_Unlock;

    fill_vtable((void **)&g_pal_vtbl, sizeof g_pal_vtbl / sizeof(void *), (void *)stub_ok);
    g_pal_vtbl.AddRef = pal_AddRef;
    g_pal_vtbl.Release = pal_Release;
    g_pal_vtbl.GetEntries = pal_GetEntries;
    g_pal_vtbl.SetEntries = pal_SetEntries;

    fill_vtable((void **)&g_dd_vtbl, sizeof g_dd_vtbl / sizeof(void *), (void *)stub_ok);
    g_dd_vtbl.QueryInterface = dd_QueryInterface;
    g_dd_vtbl.AddRef = dd_AddRef;
    g_dd_vtbl.Release = dd_Release;
    g_dd_vtbl.CreatePalette = dd_CreatePalette;
    g_dd_vtbl.CreateSurface = dd_CreateSurface;
    g_dd_vtbl.GetCaps = dd_GetCaps;
    g_dd_vtbl.GetDisplayMode = dd_GetDisplayMode;
    g_dd_vtbl.SetDisplayMode = dd_SetDisplayMode;
    g_dd_vtbl.WaitForVerticalBlank = dd_WaitForVerticalBlank;
    g_dd_vtbl.GetGDISurface = (void *)stub_fail;
    g_dd_vtbl.GetSurfaceFromDC = (void *)stub_fail;
}

long DirectDrawCreateEx(void *lpGuid, void **lplpDD, void *iid, void *pUnkOuter)
{
    (void)lpGuid; (void)iid; (void)pUnkOuter;
    init_vtables();
    g_dd.iface.lpVtbl = &g_dd_vtbl;
    g_dd.refs++;
    *lplpDD = &g_dd.iface;
    return DD_OK;
}
