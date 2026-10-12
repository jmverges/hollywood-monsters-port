"""Fixes for bugs in the ORIGINAL game, applied by gen.py to the generated C.

Each entry replaces an exact snippet of one generated function. If the snippet is no longer found (for example
after a Ghidra or gen.py change), generation fails, so a fix is never silently lost. Keep fixes minimal, call
out to platform/win32/fixes.c for anything longer than a line, and let HM_ORIGINAL_BUGS=1 turn them off at run
time so the original behaviour stays reproducible.
"""

# Mistakes of the decompiler: code that does not do what the original instructions do. Same format, plus "count"
# (how many times the snippet occurs; all are replaced). These are not switched off by HM_ORIGINAL_BUGS.
DECOMPILER_FIXES = [
    # Register values the compiler kept across a call: MSVC knew that the callee leaves ECX alone, so a loop counter
    # stays in CL. Ghidra calls it extraout_ECX, which gen.py starts at 0, so the counter never advances and the loop
    # never ends.
    dict(
        addr=0x004536e0,
        why='Ron places the Dr. Fly gadget on the mill roof: an animation loop counts in CL (DAT_00696d74 4..14); '
            'with the leftover at 0 the game hung on the roof (chapter 14)',
        old='      if ((byte)extraout_ECX < 0xe) {\n        DAT_00696d74 = (byte)extraout_ECX + 1;',
        new='      if (DAT_00696d74 < 0xe) {   /* RECOMP: decompiler fix, CL holds DAT_00696d74 */\n'
            '        DAT_00696d74 = DAT_00696d74 + 1;',
    ),
    dict(
        addr=0x00453530,
        why='the same animation loop (DAT_00696d74 14..20) when Ron climbs down from the mill roof',
        old='      if ((byte)extraout_ECX < 0x14) {\n        DAT_00696d74 = (byte)extraout_ECX + 1;',
        new='      if (DAT_00696d74 < 0x14) {   /* RECOMP: decompiler fix, CL holds DAT_00696d74 */\n'
            '        DAT_00696d74 = DAT_00696d74 + 1;',
    ),
    dict(
        addr=0x00494c00,
        why='an animation counter kept in CL and spilled to local_2e (Australia)',
        old="        uVar6 = extraout_ECX_00;\n        uVar5 = extraout_EDX_00;\n        if ((char)local_2c == '\\x01') {\n"
            "          if (DAT_0063c1c4 == '\\x02') {\n            if ((char)extraout_ECX_00 < '\\x12') {\n"
            "              local_2e = (char)extraout_ECX_00 + '\\x01';",
        new="        uVar6 = (uint)(byte)local_2e;   /* RECOMP: decompiler fix, CL holds the counter local_2e */\n"
            "        uVar5 = extraout_EDX_00;\n        if ((char)local_2c == '\\x01') {\n"
            "          if (DAT_0063c1c4 == '\\x02') {\n            if (local_2e < '\\x12') {\n"
            "              local_2e = local_2e + '\\x01';",
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
