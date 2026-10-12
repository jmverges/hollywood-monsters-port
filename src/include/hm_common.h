/* Included by every generated game function. */
#pragma once
#include "ghidra_rt.h"
#include "hm_types.h"
#include "hm_imports.h"
#ifdef HM_OVERRIDE
#include "hm_funcs_full.h"      /* hand-written code is checked against the exact signatures */
#else
#include "hm_funcs.h"
#endif

/* Fixes for bugs in the original game (tools/recomp/fixups.py, platform/win32/fixes.c). */
void hm_fix_shuffle_last_slot(unsigned char *table, unsigned slot);
/* Port hooks (tools/recomp/fixups.py PORT_HOOKS). */
void hm_lang_menu_art_loaded(void *art);
void hm_lang_verb_art_loaded(void *art);

/* Game texts (tools/recomp/gen.py turns string literals into these): the text at the executable address or
 * the literal, or its translation when one is loaded from hm_lang.tsv (platform/win32/lang.c). */
char *hm_text(const char *s);
#define HM_STR(addr) (hm_text((const char *)HM_ADDR(addr)))
#define HM_TEXT(lit) (hm_text(lit))

