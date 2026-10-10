#!/usr/bin/env python3
"""Turn the Ghidra export (ExportRecomp.java + ExportTypes.java) into compilable C.

Output layout (all generated, never hand-edit; overrides go in src/override/):
  gen/hm_types.h      Ghidra data types, made self-contained
  gen/hm_funcs.h      prototypes of every game function (copied from the definitions)
  gen/hm_imports.h    prototypes of every Win32 / CRT function the game calls
  gen/f_<addr>.c      one file per game function, globals mapped to their original addresses
  gen/imports.tsv     list of imports the platform layer must implement
"""
import argparse, os, re, sys, collections

GAME_LO, GAME_HI = 0x401000, 0x536f60      # game code; CRT/library code starts at GAME_HI
ILT_LO, ILT_HI = 0x401005, 0x405bb9        # incremental-link jump thunks

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fixups import FIXUPS  # noqa: E402  (fixes for bugs in the original game)

ap = argparse.ArgumentParser()
ap.add_argument('--rx', required=True)
ap.add_argument('--out', required=True)
ap.add_argument('--override', default=None, help='dir with hand-written replacements f_<addr>.c')
ap.add_argument('--strict-void', action='store_true', help='keep void return types in caller prototypes (diagnostics)')
ap.add_argument('--no-frame', action='store_true', help='do not reproduce the original stack frame layout')
ap.add_argument('--unprototyped', action='store_true', help='declare game functions without parameter lists (old mode)')
ap.add_argument('--fill-frame', action='store_true', help='fill reproduced stack frames with 0xCC like MSVC /RTC')
ap.add_argument('--exe', default=None, help='the original MONSTERS.EXE: string literals found in it become references to its data (HM_STR)')
ap.add_argument('--no-zero-regs', action='store_true', help='leave register leftovers (extraout_/in_/unaff_) uninitialised')
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)

def ident(s):
    s = s.replace('FID_conflict:', 'FIDc_')
    return re.sub(r'[^A-Za-z0-9_]', '_', s)

# ---------------------------------------------------------------- functions table
funcs = {}          # entry(int) -> dict
byname = {}
with open(os.path.join(a.rx, 'functions.tsv')) as fh:
    next(fh)
    for line in fh:
        p = line.rstrip('\n').split('\t')
        e = int(p[0], 16) if not p[0].startswith('EXTERNAL') else None
        d = dict(entry=e, raw=p[0], name=p[1], size=int(p[2]), thunk=p[3] == 'true', target=p[4],
                 external=p[5] == 'true', cc=p[6], ret=p[7], sig=p[8])
        if e is not None:
            funcs[e] = d
        byname.setdefault(p[1], []).append(d)

symbols = {}
symf = os.path.join(a.rx, 'symbols.tsv')
if os.path.exists(symf):
    for line in open(symf):
        p = line.rstrip('\n').split('\t')
        if len(p) >= 4 and p[0] not in symbols:
            symbols[p[0]] = (p[1], p[2], p[3])

ilt = {}
with open(os.path.join(a.rx, 'ilt.tsv')) as fh:
    for line in fh:
        k, v = line.split()
        ilt[int(k, 16)] = int(v, 16)
if ilt:
    ILT_LO, ILT_HI = min(ilt), max(ilt) + 5

def is_game(e):
    return e is not None and GAME_LO <= e < GAME_HI and not (ILT_LO <= e < ILT_HI)

game = {e: d for e, d in funcs.items() if is_game(e) and not d['thunk'] and not d['external']}

def resolve(e, depth=0):
    """Follow thunks/ILT jumps to the final function entry (int) or external name (str)."""
    if depth > 8:
        return e
    if e in ilt:
        return resolve(ilt[e], depth + 1)
    d = funcs.get(e)
    if d and d['thunk'] and d['target']:
        t = d['target']
        if t.startswith('EXT:'):
            return t[4:]
        return resolve(int(t, 16), depth + 1)
    return e

def cname_for_entry(e):
    r = resolve(e)
    if isinstance(r, str):
        return ident(r), r
    d = funcs.get(r)
    if d is None:
        return None, None
    if is_game(r):
        return ident(d['name']), None
    return 'crt_' + ident(d['name']), d['name']

name_map = {}       # identifier in decompiled text -> C identifier
imports = {}        # C identifier -> (kind, original name, signature)
for e, d in funcs.items():
    c, orig = cname_for_entry(e)
    if c:
        name_map[d['name']] = c
for k in list(name_map):              # Ghidra prints "FID_conflict:x" as "FID_conflict_x"
    if ':' in k:
        name_map.setdefault(k.replace(':', '_'), name_map[k])
for nm, ds in byname.items():          # externals (no numeric entry)
    for d in ds:
        if d['external']:
            name_map[nm] = ident(nm)

# ---------------------------------------------------------------- per-function generation
tok_re = re.compile(r'FID_conflict:[A-Za-z0-9_]+|[A-Za-z_][A-Za-z0-9_]*')
lab_re = re.compile(r'&(?:LAB|DAT|UNK)_([0-9a-f]{8})\b')
stack_re = re.compile(r'\bstack0x[0-9a-f]+\b')

def type_decl(tmpl, name, size):
    t = tmpl
    m = re.match(r'^(string|TerminatedCString|unicode|TerminatedUnicode) @$', t)
    if m:
        return 'char %s[%d]' % (name, max(int(size), 1))
    return t.replace('@', name)

used_imports = collections.Counter()
override = set()
if a.override and os.path.isdir(a.override):
    for f in os.listdir(a.override):
        m = re.match(r'f_([0-9a-f]{8})\.c$', f)
        if m:
            override.add(int(m.group(1), 16))


# ---- string literals that live in the executable's data


def load_image(path):
    import struct
    d = open(path, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    nsec, optsz = struct.unpack_from('<H', d, pe + 6)[0], struct.unpack_from('<H', d, pe + 20)[0]
    base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
    secs = []
    for i in range(nsec):
        name, vsize, va, rsize, rptr = struct.unpack_from('<8sIIII', d, pe + 24 + optsz + 40 * i)
        if not name.startswith(b'.text'):
            secs.append((base + va, d[rptr:rptr + min(vsize, rsize)]))
    return secs


C_ESC = {'a': 7, 'b': 8, 't': 9, 'n': 10, 'v': 11, 'f': 12, 'r': 13, '"': 34, "'": 39, '\\': 92, '?': 63, '0': 0}


def c_literal_bytes(lit):
    out, i = bytearray(), 0
    while i < len(lit):
        c = lit[i]
        if c != '\\':
            out += c.encode('latin-1'); i += 1; continue
        n = lit[i + 1]
        if n == 'x':
            j = i + 2
            while j < len(lit) and j < i + 4 and lit[j] in '0123456789abcdefABCDEF':
                j += 1
            out.append(int(lit[i + 2:j], 16)); i = j
        elif n in C_ESC:
            out.append(C_ESC[n]); i += 2
        else:
            raise ValueError(lit)
    return bytes(out)


def image_strings(text):
    """String literals and the executable's data.

    Text literals become HM_STR(address) when the bytes occur exactly once in the executable's data, and
    HM_TEXT("...") otherwise; both go through hm_text() at run time, which returns a translation when one is
    loaded (platform/win32/lang.c) and the original text otherwise. HM_STR also makes the game read the text
    from the user's executable, like the original. Non-text byte tables point into the executable when unique.
    """
    if not IMAGE:
        return text
    def sub(m):
        try:
            raw = c_literal_bytes(m.group(1))
        except (ValueError, UnicodeEncodeError):
            return m.group(0)
        b = raw + b'\0'
        hits = []
        for va, data in IMAGE:
            k = data.find(b)
            while k >= 0 and len(hits) < 2:
                hits.append(va + k)
                k = data.find(b, k + 1)
        is_text = raw and all(c >= 0x20 for c in raw)
        if len(hits) == 1:
            stats['literals_to_image'] += 1
            return ('HM_STR(0x%08xU)' if is_text else '((char *)0x%08xU)') % hits[0]
        if is_text:
            stats['literals_translatable'] += 1
            return 'HM_TEXT(%s)' % m.group(0)
        stats['literals_kept'] += 1
        return m.group(0)
    def text_at(va):
        for sva, data in IMAGE:
            if sva <= va < sva + len(data):
                k = va - sva
                if k == 0 or data[k - 1] != 0:
                    return None                      # not the start of a string
                end = data.find(b'\0', k, k + 256)
                raw = data[k:end] if end > 0 else b''
                return raw if len(raw) >= 2 and all(0x20 <= c < 0x7f or 0x80 <= c <= 0xa8 for c in raw) and any(65 <= (c | 32) <= 122 for c in raw) else None   # ASCII + CP437 accents
        return None
    def addr_arg(m):
        va = int(m.group(2), 16)
        if text_at(va) is None:
            return m.group(0)
        stats['text_address_args'] += 1
        return '%s(void *)HM_STR(0x%08xU)%s' % (m.group(1), va, m.group(3))
    lines = text.split('\n')
    for i, line in enumerate(lines):
        if line.lstrip().startswith('#'):
            continue
        if '"' in line:
            line = re.sub(r'"((?:[^"\\]|\\.)*)"', sub, line)
        if '&DAT_' in line:
            # a text passed by address: `f(..., &DAT_005d061c, ...)`
            line = re.sub(r'([(,]\s*)&DAT_([0-9a-f]{8})(\s*[,)])', addr_arg, line)
        lines[i] = line
    return '\n'.join(lines)


IMAGE = load_image(a.exe) if a.exe else None   # (va, bytes) of every data section

TYPE_SIZE = {'byte': 1, 'char': 1, 'undefined': 1, 'undefined1': 1, 'bool': 1, 'ushort': 2, 'short': 2,
             'undefined2': 2, 'uint': 4, 'int': 4, 'undefined4': 4, 'ulong': 4, 'long': 4, 'undefined3': 3}


def sized_stores(text):
    """Stores whose size differs from the global's declared type.

    Ghidra merges byte, word and dword accesses to one address into one variable and prints each store with
    the size of its value, e.g. `_DAT_006ca0d8 = CONCAT12(1, x);` for three byte stores (0x6ca0d8..da), but the
    export gives the variable a single type. Writing such a value through a 2-byte type drops bytes; through a
    4-byte type it overwrites the neighbour. Store exactly the CONCAT's bytes, as the original does, and widen
    the declaration to 4 bytes for the reads (the original reads the merged variable with one dword load).
    `X = (uint)X;` after such stores is a decompiler artefact (no instruction writes the extra byte): drop it.
    """
    types = {m.group(2): m.group(1).strip() for m in re.finditer(r'typedef ([\w ]+?) T_(\w+);', text)}
    # `(undefined2)DAT_x` on a global declared 1 byte wide: Ghidra's piece of a wider variable, i.e. a 2-byte
    # read at that address (FUN_0049d810 reads 0x668cc0..cc1 this way). A C cast would read 1 byte and
    # zero-extend it. Read N bytes instead.
    def wide_read(m):
        n, tok = int(m.group(1)), m.group(2)
        ds = TYPE_SIZE.get(types.get(tok))
        if ds is None or n <= ds:
            return m.group(0)
        stats['wide_reads'] += 1
        return '(*(undefined%d *)&%s)' % (n, tok)
    text = re.sub(r'\(undefined(\d)\)(_?DAT_[0-9a-f]{8})\b(?!\s*\[)', wide_read, text)
    store_re = re.compile(r'^(\s*)(_?DAT_[0-9a-f]{8}) = (CONCAT(\d)(\d)\(.*\));[ \t]*$', re.M)
    # pass 1: globals with a store of another size than declared get a 4-byte declaration (reads use it)
    final = {}
    for m in store_re.finditer(text):
        tok, n = m.group(2), int(m.group(4)) + int(m.group(5))
        ds = TYPE_SIZE.get(types.get(tok))
        if ds is not None and ds != n:
            final[tok] = 4
    # pass 2: every CONCAT store to those globals writes exactly its own size
    def store(m):
        indent, tok, expr, n = m.group(1), m.group(2), m.group(3), int(m.group(4)) + int(m.group(5))
        if tok not in final or n == final[tok]:
            return m.group(0)
        stats['sized_stores'] += 1
        return '%sHM_STORE(%s, %d, %s); /* RECOMP: %d-byte store */' % (indent, tok, n, expr, n)
    text = store_re.sub(store, text)
    for tok in sorted(final):
        text, k = re.subn(r'^(\s*)%s = \((?:uint|int|undefined4)\)%s;[ \t]*$' % (tok, tok),
                          r'\1/* RECOMP: widening artefact removed: %s = (uint)%s; */' % (tok, tok), text, flags=re.M)
        stats['dropped_self_widening'] += k
        if types[tok] not in ('uint', 'int', 'undefined4', 'ulong', 'long'):
            text = text.replace('typedef %s T_%s;' % (types[tok], tok),
                                'typedef uint T_%s; /* RECOMP: widened from %s, see sized stores */' % (tok, types[tok]), 1)
    return text

protos = []
stats = collections.Counter()
applied_fixups = set()
func_c_names = {}
for e in sorted(game):
    d = game[e]
    src = os.path.join(a.rx, 'funcs', '%08x.c' % e)
    if not os.path.exists(src):
        stats['missing_c'] += 1
        continue
    body = open(src).read()
    vars_ = []
    vf = os.path.join(a.rx, 'funcs', '%08x.vars' % e)
    if os.path.exists(vf):
        for line in open(vf):
            p = line.rstrip('\n').split('\t')
            if len(p) >= 4:
                vars_.append(p)
    for v in vars_:
        sym = symbols.get(v[0])
        if sym and not v[0].startswith('_') and int(sym[2]) > int(v[3]) and ('[' in sym[1] or sym[1].startswith(('struct', 'union'))):
            v[2], v[3] = sym[1], sym[2]
            stats['var_widened_from_symbol'] += 1
    for v in vars_:
        if re.search(r'[^A-Za-z0-9_]', v[0]):
            safe = ident(v[0])
            body = body.replace(v[0], safe)
            v[0] = safe
            stats['sanitized_names'] += 1
    # Wider-than-declared parameter accesses (_param_N). Each stack argument is a 4-byte slot in the original,
    # so the parameter becomes an undefined4 slot and its declared narrow type is a view of that slot.
    wide_params = sorted(set(re.findall(r'(?<![\w.>])_(param_\d+)\b', body)))
    if wide_params:
        hdr0 = re.search(r'^[^\n{;]*\b' + re.escape(ident(d['name'])) + r'\s*\(([^{;]*)\)\s*\n\{', body, re.M)
        if hdr0:
            plist = hdr0.group(1)
            views = []
            for pn in wide_params:
                pm = re.search(r'([^,()]*?)\s*\b' + pn + r'\b', plist)
                if not pm:
                    continue
                ptype = pm.group(1).strip()
                plist = plist[:pm.start()] + 'undefined4 hm_slot_' + pn[6:] + plist[pm.end():]
                views.append('#define %s (*(%s *)&hm_slot_%s)\n' % (pn, ptype, pn[6:]))
                body = re.sub(r'(?<![\w.>])_' + pn + r'\b', 'hm_slot_' + pn[6:], body)
            new_hdr = hdr0.group(0).replace(hdr0.group(1), plist, 1)
            body = body.replace(hdr0.group(0), new_hdr, 1)
            pos = body.index(new_hdr) + len(new_hdr)
            mb = re.compile(r'\n[ \t]*\n').search(body, pos + 1)      # end of the declaration block
            at = mb.end() if mb else pos + 1
            body = body[:at] + ''.join(views) + body[at:]
            body = body.rstrip() + '\n' + ''.join('#undef %s\n' % pn for pn in wide_params)
            stats['widened_param_slots'] += 1
    # other wider-than-declared accesses (locals live in hm_frame at their original offsets)
    body = re.sub(r'(?<![\w.>])_((?:local|[a-zA-Z]*Stack|in_stack|stack)_?[0-9a-fA-F]+)\b', r'(*(undefined4 *)&\1)', body)
    gvars = {v[0] for v in vars_}
    for t in sorted(set(tok_re.findall(body))):
        m = re.match(r'^(?:DAT|UNK|PTR_DAT)_([0-9a-f]{8})$', t)
        if m and t not in gvars and t not in symbols:
            vars_.append([t, m.group(1), 'undefined @', '1'])
            gvars.add(t)
            stats['fallback_globals'] += 1
            continue
    for t in sorted(set(tok_re.findall(body))):
        if t not in gvars and t in symbols and t not in name_map:
            addr, tmpl, size = symbols[t]
            vars_.append([t, addr, tmpl, size])
            gvars.add(t)
            stats['symbol_globals'] += 1

    def lab_sub(m):
        t = int(m.group(1), 16)
        if not m.group(0).startswith('&LAB_') and not (t in ilt or t in game):
            return m.group(0)
        r = resolve(t)
        if isinstance(r, int) and r in game:
            return '(' + ident(game[r]['name']) + ')'
        if isinstance(r, int) and r in funcs:
            c, orig = cname_for_entry(r)
            if c:
                used_imports[c] += 1
                imports.setdefault(c, ('crt' if orig else 'ext', funcs[r]['name'], funcs[r]['sig']))
                return '(' + c + ')'
        if m.group(0).startswith('&LAB_'):
            stats['unresolved_lab'] += 1
        return m.group(0)
    body = lab_re.sub(lab_sub, body)

    def tok_sub(m):
        t = m.group(0)
        if t in gvars:
            return t
        c = name_map.get(t)
        if c is None:
            return t
        return c
    body = tok_re.sub(tok_sub, body)

    # record imports used by this function
    for t in sorted(set(tok_re.findall(body))):
        if t.startswith('crt_'):
            used_imports[t] += 1

    # partial accesses: X._off_size_  (also on array elements and struct fields)
    def part_sub(m):
        base, off, size = m.group(1), int(m.group(2)), int(m.group(3))
        ptr = '((undefined1 *)&(%s) + %d)' % (base, off)
        if size == 3:
            return 'HM_PART3(%s)' % ptr
        t = {1: 'undefined1', 2: 'undefined2', 4: 'undefined4', 8: 'undefined8'}.get(size)
        if t is None:
            stats['part_odd'] += 1
            return m.group(0)
        return '(*(%s *)%s)' % (t, ptr)
    part_re = re.compile(r'((?:\(\*?[A-Za-z_][\w]*\)|[A-Za-z_][\w]*)(?:\[[^\[\]]*\]|(?:\.|->)[A-Za-z_]\w*)*)\._(\d+)_(\d+)_')
    # 3-byte stores need a helper statement
    body = re.sub(r'((?:[A-Za-z_][\w]*)(?:\[[^\[\]]*\]|(?:\.|->)[A-Za-z_]\w*)*)\._(\d+)_3_ = ([^;]+);',
                  lambda m: 'HM_SET3(((undefined1 *)&(%s) + %s), (%s));' % (m.group(1), m.group(2), m.group(3)), body)
    n_before = body.count('._')
    body = part_re.sub(part_sub, body)
    if '._' in body and re.search(r'\._\d+_\d+_', body):
        stats['part_left'] += 1

    # the function's own name
    fname = ident(d['name'])
    func_c_names[e] = fname
    # prototype = definition header
    hdr_m = re.search(r'^((?:[^\n{;/]+\n)?[^\n{;]*\b' + re.escape(fname) + r'\s*\([^{;]*\))\s*\n\{', body, re.M)
    if not hdr_m:
        stats['no_header'] += 1
        continue
    protos.append((e, fname, ' '.join(hdr_m.group(1).split())))

    # ---- original stack frame layout: stack locals keep their original offsets (see docs/ARCHITECTURE.md)
    open_idx = body.index(hdr_m.group(0)) + len(hdr_m.group(0))
    m_blank = re.compile(r'\n[ \t]*\n').search(body, open_idx + 1)
    decl_end = m_blank.start() if m_blank else -1
    decl_block = body[open_idx:decl_end] if decl_end > 0 else ''
    frame_vars = []
    new_decl_lines = []
    for line in decl_block.split('\n'):
        dm = re.match(r'^\s+(.+?)\s*\b((?:local|[a-zA-Z]+Stack|in_stack)_([0-9a-f]+))\s*((?:\[\d+\]\s*)*);\s*$', line)
        if dm and not a.no_frame:
            typ, name, off, dims = dm.group(1).strip(), dm.group(2), int(dm.group(3), 16), dm.group(4).replace(' ', '')
            if name.startswith('in_stack'):
                off = (0x100000000 - off) if off > 0x80000000 else -off
            frame_vars.append((typ, name, off, dims))
            continue
        # Register leftovers (ECX/EDX after a call, EAX on entry, callee-saved regs) have no C value.
        # The original read whatever the register held; give them a defined value so the port is
        # deterministic and the compiler cannot exploit an uninitialised read (docs/ARCHITECTURE.md).
        rm = re.match(r'^(\s+.+?\b(?:extraout|in|unaff)_[A-Za-z0-9]+(?:_[0-9]+)?)\s*;\s*$', line)
        if rm and not a.no_zero_regs:
            line = rm.group(1) + ' = 0;'
            stats['zeroed_regs'] += 1
        new_decl_lines.append(line)
    stacks = sorted(set(stack_re.findall(body)))
    frame_defs = ''
    if frame_vars or stacks:
        neg = [fv[2] for fv in frame_vars if fv[2] > 0]
        for st in stacks:
            v = int(st[7:], 16)
            if v >= 0x80000000:
                neg.append(0x100000000 - v)
        fsize = (max(neg) if neg else 0) + 64
        fsize = (fsize + 15) & ~15
        frame_defs = '  unsigned char hm_frame[%d] __attribute__((aligned(16))); /* RECOMP: original stack frame */\n' % fsize
        frame_defs += '  __builtin_memset(hm_frame, 0xcc, sizeof hm_frame);\n' if a.fill_frame else ''
        undefs = []
        for typ, name, off, dims in frame_vars:
            if off <= 0:
                frame_defs = ('  %s %s%s;\n' % (typ, name, dims)) + frame_defs
                continue
            if typ in ('undefined3', 'uint3', 'int3') and not dims:
                frame_defs += '#define %s (HM_PART3(hm_frame + %d))\n' % (name, fsize - off)
                undefs.append(name)
                continue
            tn = 'TF_' + name
            frame_defs += '  typedef %s %s%s;\n' % (typ, tn, dims)
            frame_defs += '#define %s (*(%s *)(hm_frame + %d))\n' % (name, tn, fsize - off)
            undefs.append(name)
        for st in stacks:
            v = int(st[7:], 16)
            if v >= 0x80000000:
                frame_defs += '#define %s (*(undefined1 (*)[0x800])(hm_frame + %d))\n' % (st, fsize - (0x100000000 - v))
                undefs.append(st)
            else:
                frame_defs += '  undefined1 %s[0x800]; /* RECOMP: caller-frame reference, not reproduced */\n' % st
                stats['caller_frame_refs'] += 1
        stats['framed_functions'] += 1
        body = body[:open_idx] + '\n'.join(new_decl_lines) + '\n' + frame_defs + body[decl_end:]
        body = body.rstrip() + '\n' + ''.join('#undef %s\n' % u for u in undefs)
    elif decl_block and new_decl_lines != decl_block.split('\n'):
        body = body[:open_idx] + '\n'.join(new_decl_lines) + body[decl_end:]

    for fx in FIXUPS:
        if fx['addr'] == e:
            if fx['old'] not in body:
                sys.exit('fixup for %08x: snippet not found in the generated code (%s)' % (e, fx['why'][:60]))
            body = body.replace(fx['old'], fx['new'], 1)
            stats['fixups'] += 1
            applied_fixups.add(e)

    if e in override:
        stats['overridden'] += 1
        continue

    out = ['/* Generated from Ghidra decompilation of %s @ %08x. Do not edit; override in src/override. */\n' % (d['name'], e),
           '#define HM_SELF_%s\n#include "hm_common.h"\n' % fname]
    defs, undefs = [], []
    for v in vars_:
        tok, addr, tmpl, size = v[0], v[1], v[2], v[3]
        mname = re.search(r'([0-9a-f]{8})$', tok)
        if mname and not tok.startswith(('register', 'unique', 'stack', 'const')) and addr and addr != mname.group(1):
            addr = mname.group(1)          # the address in the name is the accessed one (offset inside a symbol)
            stats['addr_from_name'] += 1
        elif not mname and tok in symbols and addr and int(addr, 16) != int(symbols[tok][0], 16):
            # A named symbol (g_ddsd) used through a field: the export records the field's address
            # (g_ddsd.lpSurface = 0x691044), but the C token denotes the whole symbol, so the macro must sit at
            # the symbol's start. Getting this wrong made Lock() write the descriptor 0x24 bytes too far.
            addr = symbols[tok][0]
            stats['addr_from_symbol'] += 1
        if not addr:
            m = re.search(r'([0-9a-f]{8})$', tok)
            addr = m.group(1) if m else ''
        if not addr:
            stats['global_no_addr'] += 1
            continue
        tn = 'T_' + tok
        if re.search(r'\(\*' + re.escape(tok) + r'\)\s*\(', body) and not re.search(r'\(\*|\*\s*\(|code', tmpl):
            tmpl = 'code *@'
            stats['callable_globals'] += 1
        defs.append('typedef %s;\n#define %s (*(%s *)0x%sU)\n' % (type_decl(tmpl, tn, size), tok, tn, addr))
        undefs.append('#undef %s\n' % tok)
    for fn in sorted(set(re.findall(r'\b(_func_\d+)\b', body))):
        out.append('#ifndef HM_HAVE%s\n#define HM_HAVE%s\ntypedef int %s();\n#endif\n' % (fn, fn, fn))
    out += defs
    out.append('\n' + body + '\n')
    with open(os.path.join(a.out, 'f_%08x.c' % e), 'w') as fh:
        fh.write(image_strings(sized_stores(''.join(out))))
    stats['generated'] += 1

# ---------------------------------------------------------------- imports used anywhere in game code
all_text = ''
for f in os.listdir(a.out):
    if f.startswith('f_') and f.endswith('.c'):
        all_text += open(os.path.join(a.out, f)).read()
idents = collections.Counter(tok_re.findall(all_text))
game_cnames = set(func_c_names.values())
cmap = {}           # C identifier -> (original name, signature, kind)
for e, d in funcs.items():
    if is_game(e) or d['thunk']:
        continue
    c, orig = cname_for_entry(e)
    if c and c not in game_cnames:
        cmap.setdefault(c, (d['name'], d['sig'], 'crt'))
for nm, ds in byname.items():
    for d in ds:
        if d['external']:
            cmap.setdefault(ident(nm), (nm, d['sig'], 'win32'))
import_decls, imp_rows = [], []
for c, (orig, sig, kind) in sorted(cmap.items()):
    if idents.get(c, 0) == 0:
        continue
    sig_c = re.sub(r'(^|[\s*])' + re.escape(orig) + r'\s*\(', lambda m: m.group(1) + c + '(', sig, count=1)
    if kind == 'crt':
        mm = re.match(r'^(.*?)\b' + re.escape(c) + r'\s*\((.*)\)$', sig_c, re.S)
        if mm and not re.search(r'\b(float|double|float10|\.\.\.)\b|\.\.\.', mm.group(2)):
            r = mm.group(1).strip()
            if r in ('void', 'undefined'):
                r = 'undefined4'
            sig_c = '%s %s()' % (r, c)
    import_decls.append(sig_c + ';')
    imp_rows.append('%s\t%s\t%s\t%d\t%s' % (c, orig, kind, idents[c], sig))
with open(os.path.join(a.out, 'hm_imports.h'), 'w') as fh:
    fh.write('/* Generated: Win32 + CRT functions called by game code. Implemented in platform/. */\n#pragma once\n')
    fh.write('\n'.join(sorted(import_decls)) + '\n')
with open(os.path.join(a.out, 'imports.tsv'), 'w') as fh:
    fh.write('c_name\toriginal\tkind\tuses\tsignature\n' + '\n'.join(sorted(imp_rows)) + '\n')

with open(os.path.join(a.out, 'hm_funcs_full.h'), 'w') as fh:
    fh.write('/* Generated: full prototypes of all game functions, used to type-check hand-written overrides. */\n#pragma once\n')
    for e, fname, proto in protos:
        fh.write('#ifndef HM_SELF_%s\n%s;\n#endif\n' % (fname, proto))
with open(os.path.join(a.out, 'hm_funcs.h'), 'w') as fh:
    fh.write('/* Generated: prototypes of all game functions (copied from their definitions). */\n#pragma once\n')
    for e, fname, proto in protos:
        m = re.match(r'^(.*?)\b' + re.escape(fname) + r'\s*\((.*)\)$', proto, re.S)
        ret, params = m.group(1).strip(), m.group(2)
        ret = re.sub(r'\b__(cdecl|stdcall|fastcall|thiscall)\b', '', ret).strip()
        if a.unprototyped and not re.search(r'\b(float|double|float10)\b', params):
            if ret == 'void' and not a.strict_void:
                ret = 'undefined4'
            decl = '%s %s()' % (ret, fname)
        else:
            # Full prototype, so every argument is converted to its parameter type (a 64-bit expression passed
            # to an unprototyped function would occupy two stack slots). Callers see void functions as
            # returning EAX, like the original machine code does.
            if ret == 'void' and not a.strict_void:
                decl = 'undefined4 %s(%s)' % (fname, params)
            else:
                decl = '%s %s(%s)' % (ret, fname, params)
        fh.write('#ifndef HM_SELF_%s\n%s;\n#endif\n' % (fname, decl))

# ---------------------------------------------------------------- types header
def fix_types(src):
    s = src.replace('typedef pointer32 ImageBaseOffset32;', 'typedef void *ImageBaseOffset32;')
    for d in [r'^typedef unsigned char\s+undefined;$', r'^typedef unsigned char\s+undefined1;$',
              r'^typedef unsigned short\s+undefined2;$', r'^typedef unsigned int\s+undefined4;$',
              r'^typedef unsigned long long\s+undefined8;$', r'^#define unk.*$']:
        s = re.sub(d, '', s, flags=re.M)
    s = re.sub(r'typedef (\w+) \(\*(\w+PROC|FARPROC|NEARPROC)\)\(void\);', r'typedef \1 (*\2)();', s)
    # empty enums -> int typedef
    s = re.sub(r'typedef enum (\w+) \{\s*\} (\w+);', r'typedef int \2;', s)
    s = re.sub(r'enum (\w+) \{\s*\};', '', s)
    out, seen = [], set()
    for line in s.split('\n'):
        if re.match(r'^\s*\w+\s*$', line):          # stray builtin names (e.g. "float10")
            continue
        m = re.match(r'^typedef (.*);$', line)
        if m and '{' not in line and '(' not in line:
            names = re.findall(r'[\s*,](\w+)\s*(?:\[\d*\])?\s*(?=,|$)', m.group(1))
            if names and any(n in seen for n in names):
                out.append('/* RECOMP dropped duplicate: ' + line + ' */')
                continue
            seen.update(names)
        out.append(line)
    return '/* Generated from Ghidra data types. */\n#pragma once\n' + '\n'.join(out)
with open(os.path.join(a.rx, 'types.h')) as fh:
    open(os.path.join(a.out, 'hm_types.h'), 'w').write(fix_types(fh.read()))

print(dict(stats))
missing_fixups = [fx for fx in FIXUPS if fx['addr'] not in applied_fixups]
if missing_fixups:
    sys.exit('fixups not applied (function missing from the export?): ' +
             ', '.join('%08x' % fx['addr'] for fx in missing_fixups))
print('imports needed:', len(import_decls))
