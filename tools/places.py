#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
"""Where the places named in shared sources are in another game made from the same program: the list in that
game's family.h.

Sources written for one game name its places as At(0x...) (a place in the ROM or in work RAM) and AtA5(...) (a
variable, by its distance from A5 = FF8000). A game that builds them too lists where each is in it, in its
family.h, between the lines "// places: begin" and "// places: end"; this writes that list:

  places.py --sources games/vampire_savior/src --from work/vampire_savior --to work/vsav2 \\
            --map work/vsav2/portmap.txt --family games/vsav2/family.h

Each place is looked for, in this order: as an instruction tools/portmap.py placed; between two placed
instructions that moved alike; as a place two placed instructions name (with how many named it so, of how
many); as the same bytes once only in the other program (a table); as a variable between two placed ones that
moved alike. A line that ends in "// by hand" is kept as it is (a place found or checked some other way). What
is found no way is listed at the end, and left out: the build then stops where it is used.
"""
import argparse
import bisect
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import m68k  # noqa: E402
import portmap  # noqa: E402
import recomp  # noqa: E402

AT = re.compile(r"\bAt\(\s*(0x[0-9A-Fa-f]+|kVars \+ 0x[0-9A-Fa-f]+)\s*\)")
AT_A5 = re.compile(r"\bAtA5\(\s*(-?0x[0-9A-Fa-f]+)\s*\)")
LINE = re.compile(r"\{\s*0x([0-9A-Fa-f]+),\s*0x([0-9A-Fa-f]+)\s*\},?\s*(//.*)?$")
BEGIN, END = "// places: begin", "// places: end"


def named_places(folder):
    found = set()
    for path in sorted(glob.glob(os.path.join(folder, "*.cpp")) + glob.glob(os.path.join(folder, "*.h"))):
        for line in open(path):
            code = line.split("//")[0]
            for m in AT.finditer(code):
                v = m.group(1)
                found.add(0xFF8000 + int(v.split("+")[1], 16) if v.startswith("kVars") else int(v, 16))
            for m in AT_A5.finditer(code):
                found.add((0xFF8000 + int(m.group(1), 16)) & 0xFFFFFF)
    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sources", required=True, help="the shared sources' folder")
    ap.add_argument("--from", dest="src", required=True, help="the work folder of the game they were written for")
    ap.add_argument("--to", dest="dst", required=True, help="the other game's work folder")
    ap.add_argument("--map", required=True, help="tools/portmap.py's map from the one to the other")
    ap.add_argument("--family", required=True, help="the other game's family.h (its list is written in place)")
    args = ap.parse_args()

    a_prog = open(os.path.join(args.src, "program.bin"), "rb").read()
    a_data = open(os.path.join(args.src, "program.bin.data"), "rb").read()
    b_prog = open(os.path.join(args.dst, "program.bin"), "rb").read()
    b_data = open(os.path.join(args.dst, "program.bin.data"), "rb").read()
    imap, named, tables, others = {}, {}, {}, {}
    for line in open(args.map):
        t = line.split()
        if t and t[0] == "I":
            imap[int(t[1], 16)] = int(t[2], 16)
        elif t and t[0] == "P":
            named[int(t[1], 16)] = (int(t[2], 16), t[3])
        elif t and t[0] == "T":
            tables[int(t[1], 16)] = int(t[2], 16)
        elif t and t[0] == "C":
            others.setdefault(int(t[1], 16), set()).add(int(t[3], 16))
    a_insns, _, _ = recomp.discover(a_prog, portmap.read_entries(os.path.join(args.src, "entries.txt")) + [0x400],
                                    0x100000, True)
    ikeys = sorted(imap)
    ram = sorted((p, q) for p, (q, votes) in named.items() if p >= 0xFF0000 and votes.split("/")[0] == votes.split("/")[1])
    rkeys = [p for p, _ in ram]

    def between(keys, table, place, reach):
        i = bisect.bisect_right(keys, place)
        if 0 < i < len(keys):
            lo, hi = keys[i - 1], keys[i]
            dl, dh = table(lo) - lo, table(hi) - hi
            if dl == dh and hi - lo <= reach:
                return place + dl
        return None

    def alike(place, count=4):
        # moved as the placed instruction before it or the one after it, if what is there does the same
        i = bisect.bisect_right(ikeys, place)
        for k in (i - 1, i):
            if not 0 <= k < len(ikeys) or abs(ikeys[k] - place) > 0x400:
                continue
            q = place + imap[ikeys[k]] - ikeys[k]
            x, y, same = place, q, True
            for _ in range(count):
                ia, ib = m68k.decode(a_prog, x), m68k.decode(b_prog, y)
                if ia.text == "?" or portmap.shape(ia) != portmap.shape(ib):
                    same = False
                    break
                if ia.flow not in ("next", "cond", "dbcc"):
                    break
                x, y = x + ia.length, y + ib.length
            if same:
                return q
        return None

    def score(x, y, count=24):
        # instructions alike from x and y on: they do the same, and the places they name are where the code at
        # large says those are
        n = 0
        for _ in range(count):
            ia, ib = m68k.decode(a_prog, x), m68k.decode(b_prog, y)
            if ia.text == "?" or portmap.shape(ia) != portmap.shape(ib):
                break
            pa, pb = portmap.places(ia), portmap.places(ib)
            if len(pa) != len(pb) or any(p in named and named[p][0] != q and p >= 0xFF0000 for p, q in zip(pa, pb)):
                break
            n += 1
            if ia.flow not in ("next", "cond", "dbcc", "call"):
                break
            x, y = x + ia.length, y + ib.length
        return n

    def once(place):
        for size in (16, 32, 64):
            for a, b in ((a_data, b_data), (a_prog, b_prog)):
                piece = a[place:place + size]
                if len(piece) < size or len(set(piece)) < 3:
                    continue
                at = b.find(piece)
                if at >= 0 and b.find(piece, at + 1) < 0:
                    return at
        return None

    def find(place):
        if place >= 0xFF0000:
            if place in named:
                q, votes = named[place]
                n, total = votes.split("/")
                if int(n) * 2 > int(total):
                    return q, "a variable the code names so (%s)" % votes
            q = between(rkeys, dict(ram).get, place, 0x40)
            if q is not None:
                return q, "a variable between two that moved alike"
            return None, None
        if place in a_insns:
            # Code: of the places it may be, the one where most instructions on do the same as the first game's
            candidates = []
            if place in tables:
                candidates.append((tables[place], "the same entry of a jump table"))
            if place in imap:
                candidates.append((imap[place], "an instruction"))
            for q in sorted(others.get(place, ())):
                candidates.append((q, "an instruction reached another way"))
            q = between(ikeys, imap.get, place, 0x100)
            if q is not None:
                candidates.append((q, "inside code that moved alike"))
            i = bisect.bisect_right(ikeys, place)
            for k in (i - 1, i):
                if 0 <= k < len(ikeys) and abs(ikeys[k] - place) <= 0x400:
                    candidates.append((place + imap[ikeys[k]] - ikeys[k], "moved as the code before or after it"))
            if place in named:
                q, votes = named[place]
                n, total = (int(v) for v in votes.split("/"))
                if 2 * n > total and q != imap.get(place, q):
                    # The code calls or jumps there: in the other game a routine may begin with something new, and
                    # its first instruction of the first game's then is not where it begins
                    return q, "where the code calls or jumps to it (%s), not its first instruction's place" % votes
                candidates.append((q, "a place the code names so (%s)" % votes))
            scored = {}
            for q, how in candidates:
                if q not in scored:
                    scored[q] = (score(place, q), how)
            if scored:
                best = max(scored, key=lambda q: scored[q][0])
                n, how = scored[best]
                if n:
                    rivals = [] if how.startswith("the same entry") else [q for q in scored if q != best and scored[q][0] == n]
                    return best, "%s, %d on alike%s" % (how, n, "; as many at %s: check" %
                                                        " ".join("%06X" % q for q in rivals) if rivals else "")
            return None, None
        if place in named:
            q, votes = named[place]
            n, total = votes.split("/")
            if int(n) * 2 > int(total):
                return q, "a place the code names so (%s)" % votes
        q = once(place)
        if q is not None:
            return q, "the same bytes, once only"
        return None, None

    text = open(args.family).read() if os.path.exists(args.family) else ""
    if BEGIN not in text or END not in text:
        sys.exit("places: %s has no lines '%s' and '%s'" % (args.family, BEGIN, END))
    head, rest = text.split(BEGIN, 1)
    block, tail = rest.split(END, 1)
    by_hand = {}
    for line in block.splitlines():
        m = LINE.search(line)
        if m and m.group(3) and "by hand" in m.group(3):
            by_hand[int(m.group(1), 16)] = line.strip()
    lines, missing, found = [], [], {}
    wanted = sorted(named_places(args.sources))
    for place in wanted:
        if place in by_hand:
            found[place] = (int(LINE.search(by_hand[place]).group(2), 16), None)
            continue
        here, how = find(place)
        if here is not None:
            found[place] = (here, how)
    # A table between two places found that moved alike (in the ROM: not memory)
    tables = sorted(p for p in found if p < 0x400000)
    for place in wanted:
        if place in found or place >= 0x400000:
            continue
        q = between(tables, lambda p: found[p][0], place, 0x1000)
        if q is not None:
            same = a_data[place:place + 16] == b_data[q:q + 16]
            found[place] = (q, "a table between two that moved alike" + (" (the same bytes)" if same else " (other bytes)"))
    for place in wanted:
        if place in by_hand:
            lines.append("    " + by_hand[place])
        elif place in found:
            lines.append("    {0x%06X, 0x%06X}, // %s" % (place, found[place][0], found[place][1]))
        else:
            missing.append(place)
    indent = "    "
    out = head + BEGIN + " (tools/places.py; a line ending \"by hand\" is kept)\n" + "\n".join(lines) + "\n" + indent + END + tail
    open(args.family, "w").write(out)
    print("places: %d listed (%d by hand), %d not found%s" % (len(lines), len(by_hand), len(missing),
          ": " + " ".join("%06X" % m for m in missing) if missing else ""))


if __name__ == "__main__":
    main()
