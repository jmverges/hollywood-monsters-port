"""Fixes for bugs in the ORIGINAL game, applied by gen.py to the generated C.

Each entry replaces an exact snippet of one generated function. If the snippet is no longer found (for example
after a Ghidra or gen.py change), generation fails, so a fix is never silently lost. Keep fixes minimal, call
out to platform/win32/fixes.c for anything longer than a line, and let HM_ORIGINAL_BUGS=1 turn them off at run
time so the original behaviour stays reproducible.
"""

# Mistakes of the decompiler: code that does not do what the original instructions do. Same format, plus "count"
# (how many times the snippet occurs; all are replaced). These are not switched off by HM_ORIGINAL_BUGS.
DECOMPILER_FIXES = [
    dict(
        addr=0x00413f10,
        why='Ghidra lost track of the stack depth (the first call\'s arguments are still pushed when the second '
            'reads the table: [esp+idx+0x18], then [esp+idx+0x28]) and indexed the frame table from local_14, '
            '8 bytes past its start at local_1c (0, 0, 1, 2, 3, 4, 5, 6, then 7..22 from 0x5c5333). Frames 9..23 '
            'read past the table: the party scene where Quasimodo lifts the grate (chapter 2) crashed.',
        old='(short)local_14[DAT_00649400]',
        new='(short)(&local_1c)[DAT_00649400]  /* RECOMP: decompiler fix */',
        count=4,
    ),
    # FUN_00516060, the travel agenda: two tables of 7 shorts, the chapter of each destination at [esp+0x8]
    # (1000, 2000, 3000, 4000, 5000, 6000, 8000) and its first room at [esp+0x18] (0x3f4 ... 0x1f4a). Ghidra put
    # the lookups 4 bytes too far (local_20, local_10) and dropped the first two stores as dead, so choosing a
    # destination read the wrong chapter and the game quit (the room was not in WinMain's list).
    dict(
        addr=0x00516060,
        why='travel agenda: chapter table starts 4 bytes before local_20; its first two entries were dropped',
        old='  local_20[0] = 3000;',
        new='  ((ushort *)(hm_frame + 76))[0] = 1000;   /* RECOMP: decompiler fix, the table starts at [esp+0x8] */\n'
            '  ((ushort *)(hm_frame + 76))[1] = 2000;\n'
            '  local_20[0] = 3000;',
    ),
    dict(
        addr=0x00516060,
        why='travel agenda: lookups 4 bytes off',
        old='local_20[*(byte *)((int)&DAT_006d6638 + (uint)bVar1)]',
        new='((ushort *)(hm_frame + 76))[*(byte *)((int)&DAT_006d6638 + (uint)bVar1)]  /* RECOMP: decompiler fix */',
        count=1,
    ),
    dict(
        addr=0x00516060,
        why='travel agenda: lookups 4 bytes off',
        old='local_10[*(byte *)((int)&DAT_006d6638 + (uint)bVar1)]',
        new='(&local_14)[*(byte *)((int)&DAT_006d6638 + (uint)bVar1)]  /* RECOMP: decompiler fix */',
        count=1,
    ),
]

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
