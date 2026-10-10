/* Hand-written replacement for FUN_0040a770 (GetDXVersion from the DirectX SDK samples).
 * The original probes DDRAW/DINPUT/DSOUND DLLs; the port provides DirectX 8 level features,
 * and the caller only requires >= 0x800. */
#define HM_SELF_FUN_0040a770
#include "hm_common.h"

void FUN_0040a770(undefined4 *param_1)
{
    *param_1 = 0x800;
}
