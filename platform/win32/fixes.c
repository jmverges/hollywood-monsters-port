/* Fixes for bugs in the ORIGINAL game, called from snippets that tools/recomp/fixups.py puts into the generated
 * code. Each one changes behaviour only in the situation where the original would hang or crash.
 * HM_ORIGINAL_BUGS=1 disables them, to reproduce the original exactly. */
#include "hm_platform.h"

int crt__rand(void);

static int fixes_enabled(void)
{
    static int state = -1;
    if (state < 0) state = hm_getenv("HM_ORIGINAL_BUGS") == 0;
    return state;
}

/* FUN_00512c00 fills table[1..48] with a permutation of 1..48 where table[i] != i, picking values at random and
 * retrying when a value is used or equals the slot. If the last slot is reached with 48 unused, no value fits
 * and the original loops forever. Before that slot is filled, move 48 into a random earlier slot; the value it
 * displaces is then the only free one, and it is not 48, so the last slot accepts it. */
void hm_fix_shuffle_last_slot(unsigned char *table, unsigned slot)
{
    if (slot != 48 || !fixes_enabled()) return;
    for (int i = 1; i < 48; i++)
        if (table[i] == 48) return;
    int j = crt__rand() % 47 + 1;
    HM_TRACE("fix: start-up shuffle would hang; moving 48 into slot %d (was %d)", j, table[j]);
    table[48] = 0;
    table[j] = 48;
}
