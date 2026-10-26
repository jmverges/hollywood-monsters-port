/* Hand-written replacement for FUN_00532ea0, which pauses the music (the game's menu, saving or loading) and
 * remembers where the song was.  The decompilation adds the sound buffer's pointer where the original adds the
 * play cursor GetCurrentPosition returns (0x532ee0), so the song resumed with a length of billions of bytes and
 * never ended: after a save Humerus danced on and Taffy never put her magnifying glass down. */
#define HM_SELF_FUN_00532ea0
#include "hm_common.h"
typedef int T_DAT_00668c74;
#define DAT_00668c74 (*(T_DAT_00668c74 *)HM_ADDR(0x00668c74U))
typedef undefined1 T_DAT_00688c93;
#define DAT_00688c93 (*(T_DAT_00688c93 *)HM_ADDR(0x00688c93U))
typedef int T_DAT_00688f08;
#define DAT_00688f08 (*(T_DAT_00688f08 *)HM_ADDR(0x00688f08U))
typedef int T_DAT_0069d1d8;
#define DAT_0069d1d8 (*(T_DAT_0069d1d8 *)HM_ADDR(0x0069d1d8U))
typedef undefined1 T_DAT_006c4cb4;
#define DAT_006c4cb4 (*(T_DAT_006c4cb4 *)HM_ADDR(0x006c4cb4U))
typedef ushort T_DAT_006c4d40;
#define DAT_006c4d40 (*(T_DAT_006c4d40 *)HM_ADDR(0x006c4d40U))
typedef ushort T_DAT_006c4d44;
#define DAT_006c4d44 (*(T_DAT_006c4d44 *)HM_ADDR(0x006c4d44U))
typedef undefined1 T_DAT_006ca0d6;
#define DAT_006ca0d6 (*(T_DAT_006ca0d6 *)HM_ADDR(0x006ca0d6U))
typedef undefined1 T_DAT_006d1f21;
#define DAT_006d1f21 (*(T_DAT_006d1f21 *)HM_ADDR(0x006d1f21U))
typedef int T_DAT_006d20e4;
#define DAT_006d20e4 (*(T_DAT_006d20e4 *)HM_ADDR(0x006d20e4U))
typedef struct IDirectSoundBuffer *T_PTR_006d1f74;
#define PTR_006d1f74 (*(T_PTR_006d1f74 *)HM_ADDR(0x006d1f74U))
typedef undefined T_DAT_00688d60;
#define DAT_00688d60 (*(T_DAT_00688d60 *)HM_ADDR(0x00688d60U))
typedef undefined T_DAT_00688d64;
#define DAT_00688d64 (*(T_DAT_00688d64 *)HM_ADDR(0x00688d64U))

int FUN_00532ea0(void)
{
    ulong play = 0, write = 0;
    uint track;

    (*PTR_006d1f74->lpVtbl->GetCurrentPosition)(PTR_006d1f74, &play, &write);
    (*PTR_006d1f74->lpVtbl->Stop)(PTR_006d1f74);
    DAT_006c4d40 = DAT_006c4d44;
    track = (uint)DAT_006c4d44;
    DAT_006ca0d6 = DAT_00688c93;
    DAT_006d20e4 = DAT_006d20e4 + (DAT_00688f08 - DAT_0069d1d8) + (int)play;
    DAT_00668c74 = (*(int *)(&DAT_00688d64 + track * 4) - *(int *)(&DAT_00688d60 + track * 4)) - DAT_006d20e4;
    DAT_006d1f21 = 0;
    DAT_006c4cb4 = 0;
    return (int)(track * 4);
}
