/* Hand-written replacement for FUN_00514a70.
 * The original inlines strcpy/strcat into stack buffers that Ghidra could not place.
 * Builds a text line at 0x6ca140 from three string tables, optionally copies it to 0x6d2220,
 * then shows it with FUN_00514930 using text colour entry param_1 (RGB triplets at 0x696d40). */
#define HM_SELF_FUN_00514a70
#include "hm_common.h"

#define MEM8(a) ((unsigned char *)(a))

void FUN_00514a70(char param_1)
{
    char first[44];
    char second[44];
    char *line = (char *)0x6ca140;
    unsigned int n;

    n = *MEM8(0x6ca0d9) * 0x29u;
    __builtin_strcpy(first, (*MEM8(0x6ca0da) == 1) ? (char *)(0x5e5e00 + n) : (char *)(0x5e7168 + n));
    n = *MEM8(0x6ca0dc) * 0x29u;
    __builtin_strcpy(second, (*MEM8(0x6ca0dd) == 1) ? (char *)(0x5e5e00 + n) : (char *)(0x5e7168 + n));

    __builtin_strcpy(line, (char *)(0x5e4074 + *MEM8(0x6ca0d8) * 0xcu));
    __builtin_strcat(line, first);
    __builtin_strcat(line, (char *)(0x5e4068 + *MEM8(0x6ca0db) * 6u));
    __builtin_strcat(line, second);

    if (param_1 != '\0')
        __builtin_memcpy((void *)0x6d2220, (void *)0x6ca140, 0x7d);
    unsigned c = (unsigned char)param_1 * 3u;
    FUN_00514930(*MEM8(0x696d40 + c), *MEM8(0x696d41 + c), *MEM8(0x696d42 + c), (char *)0x6ca140);
}
