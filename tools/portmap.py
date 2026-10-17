#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
"""Where one game's code and tables are in another game made from the same program (a sequel, a second version).

Capcom made several games from one program, changed here and there: the places of most routines and tables move
by a few bytes or a few thousand between them. Given the two deciphered programs and the places the first game's
code was seen to start (its entries file), this works out where each of the first's instructions is in the
second, and where the places its instructions name (routines, tables in the ROM, variables in memory) are:

  1. Runs of the two programs that are the same but for the places they name (16 bytes at a time, a long that
     could be a place in the ROM left out) give anchors: a piece of the first is a piece of the second, moved.
  2. The second's code is found from the first's entries moved by the anchors (tools/recomp.py's discovery).
  3. Between two anchors, the two instruction lists are matched by what the instructions do (the places they
     name left out), so code that was changed in the second game still lines up around the change.
  4. The places named by two matched instructions are taken as the same place in the two games, and the
     places in two matched jump tables are paired in their order.

  portmap.py --from work/vampire_savior --to work/vsav2 --out work/vsav2/portmap.txt [--limit 100000]

What it writes is made from the two programs and stays in work/ (never in the repository): the instruction
map (`I a b`), the places jump tables give by their order (`T a b`), places
reached two ways (`C a kept other`), the places named (`P a b votes`), and the entries for the second game's translation
(--entries), which spare a fresh tree most of 'make explore'. A game's sources take the few places they need
from it by hand, checked (see games/vsav2/PROGRESS.md).
"""
import argparse
import bisect
import collections
import difflib
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import m68k  # noqa: E402
import recomp  # noqa: E402

K = 16


def masked(m):
    """The program with every long that could be a place in the ROM (000400-3FFFFF) blotted out"""
    m = bytearray(m)
    for i in range(0, len(m) - 4, 2):
        if m[i] == 0 and m[i + 1] < 0x40 and (m[i + 1] or m[i + 2] >= 0x04):
            m[i + 1] = m[i + 2] = m[i + 3] = 0xEE
    return bytes(m)


def anchors(a, b, lo, hi):
    """Runs of a that are in b once, moved: [(a_start, a_end, delta)] in a's order, b's order kept"""
    def index(m):
        d, dup = {}, set()
        for i in range(lo, min(hi, len(m) - K), 2):
            k = m[i:i + K]
            if k in d:
                dup.add(k)
            else:
                d[k] = i
        for k in dup:
            del d[k]
        return d
    da, db = index(a), index(b)
    pairs = sorted((i, db[k]) for k, i in da.items() if k in db)
    runs = []
    for i, j in pairs:
        if runs and runs[-1][2] == j - i and i - runs[-1][1] <= 64:
            runs[-1][1] = i + K
        else:
            runs.append([i, i + K, j - i])
    # (a piece may have been moved somewhere else in the second: runs are not kept in order, only long enough
    # that a match by chance is unlikely)
    return [r for r in runs if r[1] - r[0] >= 48]


HEX = re.compile(r"\$([0-9A-F]+)")
# (the game's variables are reached from A5 = FF8000: a displacement from it is a place in memory too)
A5 = re.compile(r"(-?0x[0-9A-F]+)\(a5")


def is_place(text, m):
    """A $ number in an instruction that is a place: an address, or an immediate as big as a place in the ROM's
    tables (from 010000) or in work RAM (a table's place loaded into a register)"""
    if m.start() == 0 or text[m.start() - 1] != "#":
        return True
    v = int(m.group(1), 16)
    return 0x10000 <= v < 0x400000 or 0xFF0000 <= v <= 0xFFFFFF


def shape(insn):
    """What an instruction does, the places it names left out (other immediates kept: they are '#$')"""
    t = HEX.sub(lambda m: "@" if is_place(insn.text, m) else m.group(0), insn.text)
    return A5.sub("@(a5", t)


def places(insn):
    """The places an instruction names, in order (its branch target among them; A5's displacements as FF8000 on)"""
    t = insn.text
    found = [(m.start(), int(m.group(1), 16)) for m in HEX.finditer(t) if is_place(t, m)]
    found += [(m.start(), (0xFF8000 + int(m.group(1), 16)) & 0xFFFFFF) for m in A5.finditer(t)]
    return [v for _, v in sorted(found)]


def read_entries(path):
    out = []
    if os.path.exists(path):
        for line in open(path):
            line = line.strip()
            if line and not line.startswith("#"):
                try:
                    out.append(int(line, 16))
                except ValueError:
                    pass
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--from", dest="src", required=True, help="the first game's work folder (program.bin, entries.txt)")
    ap.add_argument("--to", dest="dst", required=True, help="the second game's work folder (program.bin)")
    ap.add_argument("--out", required=True, help="the map")
    ap.add_argument("--entries", help="write the second game's entries here (moved from the first's)")
    ap.add_argument("--limit", default="100000", help="where the code ends (hex)")
    args = ap.parse_args()
    limit = int(args.limit, 16)

    a = open(os.path.join(args.src, "program.bin"), "rb").read()
    b = open(os.path.join(args.dst, "program.bin"), "rb").read()
    runs = anchors(masked(a), masked(b), 0, limit)
    starts = [r[0] for r in runs]

    def moved(x):
        i = bisect.bisect_right(starts, x) - 1
        if i >= 0 and runs[i][0] <= x < runs[i][1]:
            return x + runs[i][2]
        return None

    a_entries = read_entries(os.path.join(args.src, "entries.txt"))
    a_insns, _, _ = recomp.discover(a, a_entries + [0x400], limit, True)
    a_starts = sorted(set(a_entries))
    a_list = sorted(a_insns)

    imap, conflicts = {}, []
    b_known = {}  # the second's instructions decoded so far

    def b_insn(y):
        if y not in b_known:
            b_known[y] = m68k.decode(b, y)
        return b_known[y]

    def walk(seeds):
        """Both programs' code followed side by side from pairs of places taken to be the same, while what the
        instructions do is the same: branches and calls give more pairs"""
        work = list(seeds)
        while work:
            x, y = work.pop()
            while x in a_insns and not y & 1 and y < limit:
                if x in imap:
                    if imap[x] != y:
                        conflicts.append((x, y))
                    break
                ia, ib = a_insns[x], b_insn(y)
                if ib.text == "?" or shape(ia) != shape(ib):
                    break
                imap[x] = y
                if ia.target is not None and ib.target is not None and ia.flow in ("cond", "branch", "call", "dbcc"):
                    work.append((ia.target, ib.target))
                if ia.flow in ("next", "cond", "dbcc", "call"):
                    x, y = x + ia.length, y + ib.length
                else:
                    break

    # 1. Inside anchors, moved as they are
    walk([(x, moved(x)) for x in reversed(a_list) if moved(x) is not None])
    # 2. Jump tables read by code that matched: the n-th place of one is the n-th of the other
    tables = {}

    def table(mem, insns, at, count=None):
        insn = insns(at)
        if insn is None:
            return None
        m = re.match(r"move\.w \$([0-9A-F]+)\(pc,d\d\.w\),(d\d)$", insn.text)
        if m:
            nxt = insns(at + insn.length)
            base = int(m.group(1), 16)
            if nxt is None or nxt.text != "jmp $%X(pc,%s.w)" % (base, m.group(2)):
                return None
            # word offsets from the table, up to where the first place it gives begins
            out, i = [], base
            while not out or i < min(out):
                out.append((base + int.from_bytes(mem[i:i + 2], "big", signed=True)) & 0xFFFFFF)
                i += 2
                if len(out) > 256 or (count and len(out) > count):
                    return None
            return out
        m = re.match(r"movea\.l \$([0-9A-F]+)\(pc,d\d\.w\),a\d$", insn.text)
        if m:
            base, out = int(m.group(1), 16), []
            while len(out) < (count or 256):
                v = int.from_bytes(mem[base + 4 * len(out):base + 4 * len(out) + 4], "big")
                # (a place in the code of either: not every place a table gives was reached in the first)
                if v & 1 or not 0x400 <= v < limit or (count is None and not out and v not in a_insns):
                    break
                out.append(v)
            return out
        return None

    for _ in range(4):
        before = len(imap)
        seeds = []
        for x, y in list(imap.items()):
            ta = table(a, a_insns.get, x)
            tb = table(b, b_insn, y, len(ta)) if ta else None
            if ta and tb and len(ta) == len(tb):
                for p, q in zip(ta, tb):
                    if p not in tables:
                        tables[p] = q
                        seeds.append((p, q))
        walk(seeds)
        # 3. The first's entries moved as the code around them
        if len(imap) == before:
            break

    # 4. What is left between two placed instructions: the lists matched by shape (code changed in the second)
    b_entries = sorted(set(imap.values()) | {m for m in (moved(x) for x in a_starts) if m is not None})
    b_insns, _, _ = recomp.discover(b, b_entries + [0x400], limit, True)
    b_list = sorted(b_insns)
    known = sorted(imap.items())
    filled = {}
    for (pa, pb), (na, nb) in zip(known, known[1:]):
        if na - pa <= a_insns[pa].length or nb - pb <= 0 or nb - pb > 2 * (na - pa) + 0x400:
            continue
        ga = a_list[bisect.bisect_right(a_list, pa):bisect.bisect_left(a_list, na)]
        gb = b_list[bisect.bisect_right(b_list, pb):bisect.bisect_left(b_list, nb)]
        if not ga or not gb or len(ga) > 4000 or len(gb) > 4000:
            continue
        sm = difflib.SequenceMatcher(None, [shape(a_insns[x]) for x in ga], [shape(b_insns[y]) for y in gb],
                                     autojunk=False)
        for i, j, n in sm.get_matching_blocks():
            for k in range(n):
                filled[ga[i + k]] = gb[j + k]
    for x, y in filled.items():
        if x not in imap:
            b_known[y] = b_insns[y]
            imap[x] = y
    if conflicts:
        print("portmap: %d places reached two ways (the first kept)" % len(conflicts), file=sys.stderr)

    votes = collections.Counter()
    for x, y in imap.items():
        pa, pb = places(a_insns[x]), places(b_insn(y))
        if len(pa) == len(pb):
            for p, q in zip(pa, pb):
                votes[(p, q)] += 1
    best = {}
    for (p, q), n in votes.most_common():
        best.setdefault(p, (q, n, sum(v for (pp, _), v in votes.items() if pp == p)))

    with open(args.out, "w") as f:
        f.write("# made by tools/portmap.py from two games' programs: stays in work/\n")
        for x in sorted(imap):
            f.write("I %06X %06X\n" % (x, imap[x]))
        for p in sorted(tables):
            f.write("T %06X %06X\n" % (p, tables[p]))
        for x, y in conflicts:
            f.write("C %06X %06X %06X\n" % (x, imap[x], y))
        for p in sorted(best):
            q, n, total = best[p]
            f.write("P %06X %06X %d/%d\n" % (p, q, n, total))
    if args.entries:
        with open(args.entries, "w") as f:
            f.write("# Places the game's code was seen to reach in ways the translator cannot follow\n")
            f.write("# (moved from another game's by tools/portmap.py)\n")
            for x in a_starts:
                y = imap.get(x)
                if y is not None:
                    f.write("%06X\n" % y)
    print("portmap: %d anchors; %d of %d instructions placed (%.1f%%); %d places named"
          % (len(runs), len(imap), len(a_list), 100.0 * len(imap) / max(1, len(a_list)), len(best)))


if __name__ == "__main__":
    main()
