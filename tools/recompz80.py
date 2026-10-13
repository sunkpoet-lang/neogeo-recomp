#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
"""Translates the game's Z80 sound driver to C++ (as recomp.py does the 68000 program).

  recompz80.py --program work/<id>/sound.bin --entries work/<id>/entries_z80.txt --out work/<id>/gen

The program is the sound ROM's fixed part (the first 0x8000 bytes; the port writes it: --dump-program).
Starting from the reset and interrupt addresses and the entries file (targets of computed jumps, noted by
the port with --record-entries), every instruction that fixed jumps and calls lead to is decoded, cut into
blocks and gathered into routines.

Output: z80_NNN.cpp (the routines), z80_decl.h, z80_table.cpp.
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import z80  # noqa: E402

LIMIT = 0x8000
ROUTINE_BLOCKS = 400
FILE_LINES = 12000


def discover(mem, entries):
    insns, starts, calls, work = {}, set(), set(), []

    def start(addr):
        if addr < LIMIT and addr not in starts:
            starts.add(addr)
            work.append(addr)

    for e in entries:
        start(e)
    while work:
        addr = work.pop()
        while addr not in insns and addr < LIMIT:
            insn = insns[addr] = z80.decode(mem, addr)
            nxt = addr + insn.length
            flow = insn.flow
            if flow in ("cond", "djnz", "branch"):
                start(insn.target)
            elif flow in ("call", "ccall"):
                start(insn.target)
                if insn.target < LIMIT:
                    calls.add(insn.target)
                start(nxt)
            elif flow == "end":
                start(nxt)
            elif flow == "repeat":
                starts.add(addr)  # (it goes round by coming back to its own start)
            if flow in ("branch", "call", "return", "jump", "stop", "end"):
                break
            addr = nxt
    return insns, starts, calls


class Block:
    def __init__(self, addr):
        self.addr = addr
        self.insns = []
        self.end = addr
        self.succ = []
        self.routine = None


def make_blocks(insns, starts):
    blocks = {}
    for s in sorted(starts):
        if s not in insns:
            continue
        block = Block(s)
        addr = s
        while True:
            insn = insns[addr]
            block.insns.append(insn)
            nxt = addr + insn.length
            block.end = nxt
            if insn.flow in ("cond", "djnz", "branch"):
                block.succ.append(insn.target)
            if insn.flow in ("branch", "return", "jump", "stop"):
                break
            if insn.flow in ("call", "end") or nxt in starts or nxt not in insns:
                block.succ.append(nxt)
                break
            addr = nxt
        blocks[s] = block
    return blocks


def make_routines(blocks, first, rest):
    routines = []
    pending = list(first) + list(rest)
    done = set()
    for head in first:
        if head in blocks:
            blocks[head].routine = head
    i = 0
    while i < len(pending):
        head = pending[i]
        i += 1
        if head not in blocks or head in done:
            continue
        done.add(head)
        if blocks[head].routine is None:
            blocks[head].routine = head
        elif blocks[head].routine != head:
            continue
        members, work = [head], [head]
        while work:
            for s in blocks[work.pop()].succ:
                target = blocks.get(s)
                if target is None or target.routine is not None:
                    continue
                if len(members) >= ROUTINE_BLOCKS:
                    target.routine = s
                    pending.append(s)
                    continue
                target.routine = head
                members.append(s)
                work.append(s)
        routines.append((head, sorted(members)))
    return routines


def emit_routine(out, head, members, blocks, mem):
    def label(addr):
        return "L_%04X" % addr

    def inside(addr):
        return addr in blocks and blocks[addr].routine == head

    def go(addr):
        return "ZGOTO(%s, 0x%04X);" % (label(addr), addr) if inside(addr) else "ZLEAVE(0x%04X);" % addr

    def call(target, nxt):
        block = blocks.get(target)
        if block is None or not inside(nxt):
            return "ZLEAVE(0x%04X);" % target
        return "ZCALL(zfn_%04X, 0x%04X, %s, 0x%04X);" % (block.routine, target, label(nxt), nxt)

    out.append("void zfn_%04X(Z80 &c) {" % head)
    out.append("    switch (c.pc) {")
    for addr in members:
        out.append("    case 0x%04X: goto %s;" % (addr, label(addr)))
    out.append("    default: cps2recomp::z80native::NotHere(c, 0x%04X);" % head)
    out.append("    }")
    for index, addr in enumerate(members):
        block = blocks[addr]
        out.append("    ZBLOCK(%s, 0x%04X)" % (label(addr), addr))
        ended = False
        for insn in block.insns:
            nxt = insn.addr + insn.length
            raw = " ".join("%02X" % mem[a] for a in range(insn.addr, nxt))
            out.append("    // %04X: %-20s %s" % (insn.addr, insn.text, raw))
            if insn.interp:
                out.append("    c.Interpret(0x%04X);" % insn.addr)
                if insn.flow == "stop":
                    out.append("    return;")
                    ended = True
                else:
                    out.append("    if (c.pc != 0x%04X) {" % nxt)
                    out.append("        return;")
                    out.append("    }")
                continue
            out.append("    {")
            out.append("        c.refresh += %d;" % insn.refresh)
            for line in insn.lines:
                out.append("        " + line)
            flow = insn.flow
            if flow == "next":
                out.append("        c.cycles += %d;" % insn.cycles)
            elif flow == "cond":
                out.append("        if (%s) {" % insn.cond)
                out.append("            c.cycles += %d;" % insn.taken)
                out.append("            %s" % go(insn.target))
                out.append("        }")
                out.append("        c.cycles += %d;" % insn.cycles)
            elif flow == "djnz":
                out.append("        if (--c.r[0]) {")
                out.append("            c.cycles += %d;" % insn.taken)
                out.append("            %s" % go(insn.target))
                out.append("        }")
                out.append("        c.cycles += %d;" % insn.cycles)
            elif flow == "repeat":
                out.append("        if (again) {")
                out.append("            c.cycles += %d;" % insn.taken)
                out.append("            %s" % go(insn.addr))
                out.append("        }")
                out.append("        c.cycles += %d;" % insn.cycles)
            elif flow == "cret":
                out.append("        if (%s) {" % insn.cond)
                out.append("            const uint16_t to = c.Pop();")
                out.append("            c.cycles += %d;" % insn.taken)
                out.append("            ZLEAVE(to);")
                out.append("        }")
                out.append("        c.cycles += %d;" % insn.cycles)
            elif flow == "ccall":
                out.append("        if (%s) {" % insn.cond)
                out.append("            c.Push(0x%04X);" % nxt)
                out.append("            c.cycles += %d;" % insn.taken)
                out.append("            %s" % call(insn.target, nxt))
                out.append("        }")
                out.append("        c.cycles += %d;" % insn.cycles)
            elif flow == "branch":
                out.append("        c.cycles += %d;" % insn.cycles)
                out.append("        %s" % go(insn.target))
                ended = True
            elif flow == "call":
                out.append("        c.Push(0x%04X);" % nxt)
                out.append("        c.cycles += %d;" % insn.cycles)
                out.append("        %s" % call(insn.target, nxt))
                ended = True
            elif flow in ("return", "jump"):
                out.append("        c.cycles += %d;" % insn.cycles)
                out.append("        ZLEAVE(to);")
                ended = True
            elif flow == "end":
                out.append("        c.cycles += %d;" % insn.cycles)
                out.append("        ZLEAVE(0x%04X);" % nxt)
                ended = True
            out.append("    }")
        if not ended:
            nxt = block.end
            following = members[index + 1] if index + 1 < len(members) else None
            if nxt == following and inside(nxt):
                out.append("    ZFALL(0x%04X);" % nxt)
            else:
                out.append("    %s" % go(nxt))
    out.append("}")
    out.append("")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--program", required=True)
    ap.add_argument("--entries", default=None)
    ap.add_argument("--out", required=True)
    ap.add_argument("--list", default=None)
    args = ap.parse_args()

    mem = open(args.program, "rb").read()
    entries = [0x0000, 0x0038]  # reset; the interrupt (mode 1)
    if args.entries and os.path.exists(args.entries):
        for line in open(args.entries):
            line = line.split("#")[0].strip()
            if line:
                entries.append(int(line, 16))
    entries = [e for e in dict.fromkeys(entries) if e < LIMIT]

    insns, starts, calls = discover(mem, entries)
    blocks = make_blocks(insns, starts)
    first = [0] + sorted(calls - {0})
    routines = make_routines(blocks, first, [e for e in entries if e not in calls] + sorted(starts))

    os.makedirs(args.out, exist_ok=True)
    written = set()

    def write(name, text):
        path = os.path.join(args.out, name)
        written.add(name)
        if not os.path.exists(path) or open(path).read() != text:
            with open(path, "w") as f:
                f.write(text)

    header = ["// Generated by cps2recomp/tools/recompz80.py from the game's sound program. Not for the repository.",
              '#include "native_z80.h"', '#include "z80_decl.h"', "", "using namespace cps2recomp;", ""]
    files, current = [], list(header)
    for head, members in routines:
        emit_routine(current, head, members, blocks, mem)
        if len(current) > FILE_LINES:
            files.append(current)
            current = list(header)
    if len(current) > len(header):
        files.append(current)
    for n, lines in enumerate(files):
        write("z80_%03d.cpp" % n, "\n".join(lines) + "\n")
    decl = ["// Generated by cps2recomp/tools/recompz80.py", "#pragma once", '#include "native_z80.h"', ""]
    decl += ["void zfn_%04X(cps2recomp::Z80 &c);" % head for head, _ in routines]
    write("z80_decl.h", "\n".join(decl) + "\n")
    table = ["// Generated by cps2recomp/tools/recompz80.py", '#include "native_z80.h"', '#include "z80_decl.h"', "",
             "namespace cps2recomp {", "", "static const Z80Entry kEntries[] = {"]
    table += ["    {0x%04X, zfn_%04X}," % (a, blocks[a].routine) for a in sorted(blocks)]
    table += ["};", "", "static const Z80Range kRanges[] = {"]
    table += ["    {0x%04X, 0x%04X}," % (a, blocks[a].end) for a in sorted(blocks)]
    table += ["};", "", "const Z80Code kZ80Code = {kEntries, sizeof(kEntries) / sizeof(kEntries[0]), kRanges,",
              "                            sizeof(kRanges) / sizeof(kRanges[0])};", "", "} // namespace cps2recomp"]
    write("z80_table.cpp", "\n".join(table) + "\n")
    for name in os.listdir(args.out):
        if name.startswith("z80_") and name not in written:
            os.remove(os.path.join(args.out, name))
    if args.list:
        with open(args.list, "w") as f:
            for addr in sorted(insns):
                insn = insns[addr]
                f.write("%s%04X  %s%s\n" % (">" if addr in starts else " ", addr, insn.text, "   ; interpreter" if insn.interp else ""))
    print("recompz80: %d entries -> %d instructions (%d bytes) in %d blocks, %d routines; %d left to the interpreter"
          % (len(entries), len(insns), sum(i.length for i in insns.values()), len(blocks), len(routines),
             sum(1 for i in insns.values() if i.interp)))


if __name__ == "__main__":
    main()
