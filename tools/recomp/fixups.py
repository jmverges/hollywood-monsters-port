"""Fixes for bugs in the ORIGINAL game, applied by gen.py to the generated C.

Each entry replaces an exact snippet of one generated function. If the snippet is no longer found (for example
after a Ghidra or gen.py change), generation fails, so a fix is never silently lost. Keep fixes minimal, call
out to platform/win32/fixes.c for anything longer than a line, and let HM_ORIGINAL_BUGS=1 turn them off at run
time so the original behaviour stays reproducible.
"""

FIXUPS = [
    dict(
        addr=0x00512c00,
        why='Start-up shuffle of a 48-entry table (slot i never gets value i) can deadlock in the last slot: '
            'when the only unused value is 48, the original retries forever. About 2 % of random seeds '
            '(srand(time) at start-up) freeze the game before the first frame.',
        old='''  DAT_006bae56 = 1;
  do {
    do {
      do {
        iVar2 = crt__rand();
''',
        new='''  DAT_006bae56 = 1;
  do {
    hm_fix_shuffle_last_slot((unsigned char *)0x0068fe40U, DAT_006bae56);   /* RECOMP fixup: original bug */
    do {
      do {
        iVar2 = crt__rand();
''',
    ),
]
