/* Hand-written replacement for FUN_0051e9a0: incremental savegame thumbnail builder.
 * Each call averages one block of the 1024-wide back buffer and stores the closest palette index into a
 * 184x138 thumbnail. The original keeps an integer loop counter and a double in the same stack slot, which the
 * decompiler expressed as numeric conversions; this version follows the disassembly instead. */
#define HM_SELF_FUN_0051e9a0
#include "hm_common.h"

#define U8(a)  (*(unsigned char *)HM_ADDR(a))
#define U16(a) (*(unsigned short *)HM_ADDR(a))
#define U32(a) (*(unsigned int *)HM_ADDR(a))
#define F64(a) (*(double *)HM_ADDR(a))

#define ACTIVE      U8(0x6325d3)
#define BLK_X       U16(0x62bc5e)
#define BLK_Y       U16(0x62a5dc)
#define X_ACC       F64(0x632bf8)
#define Y_ACC       F64(0x632be8)
#define BLK_W       U8(0x632be0)
#define BLK_H       U8(0x632c00)
#define COL         U8(0x6325d2)
#define ROW         U8(0x62bc5d)
#define STEP        F64(0x5d0840)          /* 640 / 184 */
#define REMAP_ON    U8(0x6cc2f0)
#define REMAP       ((unsigned char *)HM_ADDR(0x632c20))
#define PAL         ((unsigned char *)HM_ADDR(0x6328e0))
#define THUMB       ((unsigned char *)HM_ADDR(0x62c2a0))

static int iabs(int v) { return v < 0 ? -v : v; }

void FUN_0051e9a0(void)
{
    if (!ACTIVE) return;

    unsigned short x = BLK_X, y = BLK_Y;
    BLK_W = (unsigned char)(int)(X_ACC - (double)x);           /* truncation, like fistp with chop */
    unsigned short sum[3] = { 0, 0, 0 };
    unsigned short y_end = (unsigned short)(y + BLK_H);
    if (y < y_end) {
        unsigned short x_end = (unsigned short)(BLK_W + x);
        unsigned rows = (unsigned short)(y_end - y);
        unsigned row_y = y;
        while (rows--) {
            if (x < x_end) {
                const unsigned char *p = (const unsigned char *)(U32(0x6ca0bc) + U16(0x6325d0) + x
                                                                 + ((unsigned)U16(0x6c4c94) + row_y) * 0x400u);
                unsigned n = (unsigned short)(x_end - x);
                while (n--) {
                    unsigned pix = REMAP_ON ? REMAP[*p] : *p;
                    for (int k = 0; k < 3; k++) sum[k] = (unsigned short)(sum[k] + PAL[pix * 3 + k]);
                    p++;
                }
            }
            row_y++;
        }
    }

    unsigned avg[3];
    for (int k = 0; k < 3; k++) {
        double d = (double)sum[k] / (double)((unsigned)BLK_H * (unsigned)BLK_W);
        double fl = __builtin_floor(d), ce = __builtin_ceil(d);
        avg[k] = (unsigned short)(int)((d - fl <= ce - d) ? fl : ce);
    }

    for (unsigned tol = 0; tol < 0x40; tol++) {
        for (unsigned idx = 0; idx < 0x100; idx++) {
            if (iabs((int)avg[0] - PAL[idx * 3]) <= (int)tol && iabs((int)avg[1] - PAL[idx * 3 + 1]) <= (int)tol &&
                iabs((int)avg[2] - PAL[idx * 3 + 2]) <= (int)tol) {
                THUMB[ROW * 0xb8u + COL] = (unsigned char)idx;
                goto found;
            }
        }
    }
found:
    BLK_X = (unsigned short)(BLK_X + BLK_W);
    X_ACC = X_ACC + STEP;
    COL = (unsigned char)(COL + 1);
    if (COL != 0xb8) return;

    /* row complete */
    if (U8(0x632bf0)) {
        __builtin_memcpy((void *)(U32(0x688a48) + 0x91a4u + (unsigned)ROW * 0x400u), THUMB + ROW * 0xb8u, 0xb8);
        U8(0x5e5dfa) = 1;
    }
    X_ACC = STEP;
    Y_ACC = Y_ACC + STEP;
    unsigned short new_y = (unsigned short)(BLK_Y + BLK_H);
    BLK_X = 0;
    COL = 0;
    BLK_Y = new_y;
    ROW = (unsigned char)(ROW + 1);
    BLK_H = (unsigned char)((unsigned char)(int)Y_ACC - (unsigned char)new_y);
    if (ROW == 0x8a) ACTIVE = 0;
}
