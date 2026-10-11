/* Entry from the host into the game: call the original WinMain (FUN_0040cd20). */
#include "hm_platform.h"

int hm_debug;
extern char *getenv(const char *);   /* only libc symbol used on this side, declared by hand */

const char *hm_getenv(const char *name) { return getenv(name); }

int hm_game_start(const char *cmdline)
{
    hm_debug = hm_getenv("HM_DEBUG") != 0;
    hm_lang_load();                      /* optional English: en/ in the data directory */
    int r = (int)FUN_0040cd20((HINSTANCE)0x400000, 0, (undefined4)(unsigned)cmdline, 1);
    host_log("WinMain returned %d", r);
    host_quit(r);
    return r;
}
