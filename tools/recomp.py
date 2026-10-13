#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
"""Translates the game's 68000 program to C++.

  recomp.py --program work/<id>/program.bin --entries work/<id>/entries.txt --out work/<id>/gen

The program is the deciphered ROM (the port writes it: --dump-program). Starting from the reset vector and
the addresses in the entries file (places the game was seen to reach by computed jumps, interrupts and
returns; the port adds to it with --record-entries), every instruction that can be reached by fixed branches
and calls is decoded. The instructions are cut into blocks (a block starts wherever control can arrive) and
the blocks gathered into routines: a call's target starts one, and it takes in what its branches reach.
Each routine becomes a C++ function (runtime/native.h says how they are written).

Output: native_NNN.cpp (the routines), native_decl.h, native_table.cpp (which block is in which routine).
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import guess as guesser  # noqa: E402
import m68k  # noqa: E402

ROUTINE_BLOCKS = 600      # a routine takes in at most this many blocks
FILE_LINES = 12000        # about this many lines to a generated file


def find_tables(mem, insns, limit):
    """Jump tables the compiler's switch idiom leaves: 'move.w T(pc,dN.w),dM' followed by 'jmp T(pc,dM.w)',
    the table of 16-bit offsets from T. The table runs up to the first place it sends to."""
    found = []
    for addr, insn in insns.items():
        if insn.flow != "jump" or not insn.text.startswith("jmp $") or "(pc," not in insn.text:
            continue
        base = int(insn.text[5:insn.text.index("(")], 16)
        targets = []
        at = base
        end = limit
        while at + 2 <= end and len(targets) < 512:
            offset = (mem[at] << 8) | mem[at + 1]
            target = base + m68k.sext16(offset)
            if target & 1 or target < base or target >= limit or target - base > 0x4000:
                break
            targets.append(target)
            end = min(end, target)
            at += 2
        if len(targets) >= 2:
            found += targets
    return found


def discover(mem, entries, limit, tables=True):
    """Decodes everything reachable. Returns (insns by address, block starts, call targets)."""
    insns = {}
    starts = set()
    calls = set()
    work = []

    def start(addr):
        if addr < limit and not addr & 1 and addr not in starts:
            starts.add(addr)
            work.append(addr)

    for e in entries:
        start(e)
    seen_tables = set()
    while True:
        while work:
            addr = work.pop()
            while addr not in insns:  # (until it runs into code already decoded)
                insn = insns[addr] = m68k.decode(mem, addr)
                nxt = addr + insn.length
                if insn.flow in ("next", "cond", "dbcc"):
                    if insn.flow != "next":
                        start(insn.target)
                    if nxt >= limit:
                        start(nxt)
                        break
                    addr = nxt
                    continue
                if insn.flow == "branch":
                    start(insn.target)
                elif insn.flow == "call":
                    start(insn.target)
                    if insn.target < limit:
                        calls.add(insn.target)
                    start(nxt)
                elif insn.flow in ("calli", "end"):
                    start(nxt)
                break
        if not tables:
            break
        fresh = [t for t in find_tables(mem, insns, limit) if t not in seen_tables]
        if not fresh:
            break
        for t in fresh:
            seen_tables.add(t)
            start(t)
    return insns, starts, calls


class Block:
    def __init__(self, addr):
        self.addr = addr
        self.insns = []
        self.end = addr
        self.succ = []       # blocks control goes to within a routine (branches, running on, after a call)
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
            if insn.flow in ("cond", "dbcc"):
                block.succ.append(insn.target)
            if insn.flow in ("next", "cond", "dbcc"):
                if nxt in starts or nxt not in insns:
                    block.succ.append(nxt)
                    break
                addr = nxt
                continue
            if insn.flow == "branch":
                block.succ.append(insn.target)
            elif insn.flow in ("call", "calli", "end"):
                block.succ.append(nxt)
            break
        blocks[s] = block
    return blocks


def make_routines(blocks, first, rest):
    """Each block goes to one routine. first: the addresses that start routines whatever reaches them (call
    targets); rest: the other places control arrives, which start one only if no routine took them in."""
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
        members = [head]
        work = [head]
        while work:
            block = blocks[work.pop()]
            for s in block.succ:
                target = blocks.get(s)
                if target is None or target.routine is not None:
                    continue
                if len(members) >= ROUTINE_BLOCKS:
                    # (too big: what is left starts a routine of its own)
                    target.routine = s
                    pending.append(s)
                    continue
                target.routine = head
                members.append(s)
                work.append(s)
        routines.append((head, sorted(members)))
    return routines


def liveness(block):
    """For each instruction of the block, whether the flags it sets are wanted. At the block's end and at
    every way out of it all flags are (the state must be whole wherever the routine can be left)."""
    live = m68k.ALL
    wanted = [True] * len(block.insns)
    for n in range(len(block.insns) - 1, -1, -1):
        insn = block.insns[n]
        if insn.flow != "next" or insn.interp:
            live = m68k.ALL
        wanted[n] = bool(insn.defs & live)
        live = (live & ~insn.kills) | insn.uses
        if insn.interp:
            live = m68k.ALL
    return wanted



def directly(insn, mem, nxt):
    """C++ for the few instructions the translator has no emitter for but that need no interpreter: traps,
    the return from an exception, a constant moved to the status register. None for any other."""
    if not insn.interp or insn.text == "?":
        return None
    op = (mem[insn.addr] << 8) | mem[insn.addr + 1]
    if op == 0x4E73:  # rte
        return ["c.ReturnFromException();", "c.cycles += 20;"]
    if op & 0xFFF0 == 0x4E40:  # trap #n
        return ["c.Trap(%d, 0x%Xu);" % (op & 15, nxt), "c.cycles += 38;"]
    if op == 0x46FC:  # move #value,sr
        value = (mem[insn.addr + 2] << 8) | mem[insn.addr + 3]
        return ["c.pc = 0x%Xu;" % nxt, "c.MoveToSR(0x%04X);" % value, "c.cycles += 16;"]
    return None

def emit_routine(out, head, members, blocks, mem, hooks=()):
    def label(addr):
        return "L_%06X" % addr

    def inside(addr):
        return addr in blocks and blocks[addr].routine == head

    def go(addr):
        """Control to the block at addr"""
        if inside(addr):
            return "GOTO(%s, 0x%Xu);" % (label(addr), addr)
        return "LEAVE(0x%Xu);" % addr

    out.append("void fn_%06X(M68K &c) {" % head)
    out.append("    switch (c.pc) {")
    for addr in members:
        out.append("    case 0x%X: goto %s;" % (addr, label(addr)))
    out.append("    default: cps2recomp::native::NotHere(c, 0x%Xu);" % head)
    out.append("    }")
    for index, addr in enumerate(members):
        block = blocks[addr]
        out.append("    BLOCK(%s, 0x%Xu)" % (label(addr), addr))
        if addr in hooks:
            out.append("    HOOK(0x%Xu)" % addr)
        wanted = liveness(block)
        ended = False
        for n, insn in enumerate(block.insns):
            nxt = insn.addr + insn.length
            words = " ".join("%04X" % ((mem[a] << 8) | mem[a + 1]) for a in range(insn.addr, nxt, 2))
            out.append("    // %06X: %-22s %s" % (insn.addr, insn.text, words))
            if insn.interp:
                # (the block ends here either way, as it does for the interpreter)
                direct = directly(insn, mem, nxt)
                if direct:
                    out.extend("    " + line for line in direct)
                else:
                    out.append("    c.Interpret(0x%Xu);" % insn.addr)
                if insn.flow in ("stop", "end"):
                    out.append("    return;")
                    ended = True
                else:
                    out.append("    if (c.pc != 0x%Xu) {" % nxt)
                    out.append("        return;")
                    out.append("    }")
                continue
            lines = insn.emit(wanted[n])
            if insn.flow == "cond":
                out.append("    if (%s) {" % insn.cond)
                out.append("        c.cycles += %d;" % insn.taken)
                out.append("        %s" % go(insn.target))
                out.append("    }")
                out.append("    c.cycles += %d;" % insn.not_taken)
                continue
            if insn.flow == "dbcc":
                r = insn.reg
                out.append("    if (%s) {" % insn.cond)
                out.append("        c.cycles += 12;")
                out.append("    } else {")
                out.append("        const uint32_t count = (c.d[%d] - 1) & 0xFFFF;" % r)
                out.append("        Set16(c.d[%d], count);" % r)
                out.append("        if (count != 0xFFFF) {")
                out.append("            c.cycles += 10;")
                out.append("            %s" % go(insn.target))
                out.append("        }")
                out.append("        c.cycles += 14;")
                out.append("    }")
                continue
            out.append("    {")
            for line in lines:
                out.append("        " + line)
            if insn.flow == "branch":
                out.append("        %s" % go(insn.target))
                ended = True
            elif insn.flow == "call":
                target = blocks.get(insn.target)
                if target is None:
                    out.append("        LEAVE(0x%Xu);" % insn.target)
                elif inside(nxt):
                    out.append("        CALL(fn_%06X, 0x%Xu, %s, 0x%Xu);" % (target.routine, insn.target, label(nxt), nxt))
                else:
                    out.append("        CALL_AWAY(fn_%06X, 0x%Xu);" % (target.routine, insn.target))
                ended = True
            elif insn.flow in ("jump", "calli", "return"):
                out.append("        LEAVE(to);")
                ended = True
            out.append("    }")
        if not ended:
            nxt = block.end
            following = members[index + 1] if index + 1 < len(members) else None
            if nxt == following and inside(nxt):
                out.append("    FALL(0x%Xu);" % nxt)
            else:
                out.append("    %s" % go(nxt))
    out.append("}")
    out.append("")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--program", required=True)
    ap.add_argument("--entries", default=None)
    ap.add_argument("--out", required=True)
    ap.add_argument("--limit", default=None, help="the end of the code (hex); default: the whole program")
    ap.add_argument("--no-tables", action="store_true", help="do not look for jump tables")
    ap.add_argument("--guess", default=None,
                    help="also translate code found by looking at the program (guess.py); the guesses are kept in this file")
    ap.add_argument("--data", default=None, help="the program as data (the port writes it beside the program)")
    ap.add_argument("--list", default=None, help="write the disassembly of everything decoded here")
    ap.add_argument("--hook-sources", default=None,
                    help="the game's sources: translated code stops by for a hook wherever they say AddHook(0x...) or AddRewrite(0x...)")
    args = ap.parse_args()

    mem = open(args.program, "rb").read()
    limit = int(args.limit, 16) if args.limit else len(mem)
    reset = int.from_bytes(mem[4:8], "big")
    entries = [reset]
    if args.entries and os.path.exists(args.entries):
        for line in open(args.entries):
            line = line.split("#")[0].strip()
            if line:
                entries.append(int(line, 16))
    hooks = set()
    if args.hook_sources and os.path.isdir(args.hook_sources):
        # AddHook(0x1234, ...), AddRewrite(kName, ...) or HookAt(0x1234), with "kName = 0x1234" somewhere in the sources
        import re
        texts = [open(os.path.join(args.hook_sources, name)).read() for name in sorted(os.listdir(args.hook_sources))
                 if name.endswith((".cpp", ".h"))]
        names = {}
        for text in texts:
            names.update((n, int(v, 16)) for n, v in re.findall(r"\b(k\w+)\s*=\s*0x([0-9A-Fa-f]+)", text))
        for text in texts:
            for where in re.findall(r"(?:Add(?:Hook|Rewrite)|HookAt)\(\s*(0x[0-9A-Fa-f]+|k\w+)", text):
                if where.startswith("0x"):
                    hooks.add(int(where, 16))
                elif where in names:
                    hooks.add(names[where])
                else:
                    sys.exit("recomp: the sources hook %s, and nothing says where that is" % where)
    hooks = sorted(hooks)
    entries += hooks  # (a hook's place is a place to translate from)
    entries = [e for e in dict.fromkeys(entries) if e < limit and not e & 1]

    insns, starts, calls = discover(mem, entries, limit, not args.no_tables)
    seen = len(insns)
    guessed = []
    if args.guess:
        # (guessing takes a minute or two, so the guesses are kept, and made again when the entries change)
        import hashlib
        key = hashlib.sha1(("%d " % limit + " ".join("%X" % e for e in sorted(entries))).encode()).hexdigest()
        lines = open(args.guess).read().split() if os.path.exists(args.guess) else []
        if lines and lines[0] == key:
            guessed = [int(v, 16) for v in lines[1:]]
        else:
            data = open(args.data, "rb").read() if args.data and os.path.exists(args.data) else mem
            code_end = max(insns) + 0x100 if insns else limit
            guessed = guesser.guess(mem, data, insns, 0x400, min(limit, code_end))
            with open(args.guess, "w") as f:
                f.write(key + "\n" + "".join("%06X\n" % g for g in guessed))
        insns, starts, calls = discover(mem, entries + guessed, limit, not args.no_tables)
    blocks = make_blocks(insns, starts)
    # Routines start at the reset entry and at call targets; then at whatever else nothing took in
    first = [reset] + sorted(calls - {reset})
    routines = make_routines(blocks, first, [e for e in entries + guessed if e not in calls] + sorted(starts))
    leftover = [b.addr for b in blocks.values() if b.routine is None]
    assert not leftover, leftover[:8]

    os.makedirs(args.out, exist_ok=True)
    written = set()

    def write(name, text):
        path = os.path.join(args.out, name)
        written.add(name)
        if not os.path.exists(path) or open(path).read() != text:
            with open(path, "w") as f:
                f.write(text)

    header = ['// Generated by cps2recomp/tools/recomp.py from the game\'s program. Not for the repository.',
              '#include "native.h"', '#include "native_decl.h"', "",
              "using namespace cps2recomp;", "using namespace cps2recomp::native;", ""]
    files = []
    current = list(header)
    for head, members in routines:
        emit_routine(current, head, members, blocks, mem, set(hooks))
        if len(current) > FILE_LINES:
            files.append(current)
            current = list(header)
    if len(current) > len(header):
        files.append(current)
    for n, lines in enumerate(files):
        write("native_%03d.cpp" % n, "\n".join(lines) + "\n")

    decl = ["// Generated by cps2recomp/tools/recomp.py", "#pragma once", '#include "native.h"', ""]
    decl += ["void fn_%06X(cps2recomp::M68K &c);" % head for head, _ in routines]
    write("native_decl.h", "\n".join(decl) + "\n")

    table = ["// Generated by cps2recomp/tools/recomp.py", '#include "native.h"', '#include "native_decl.h"', "",
             "namespace cps2recomp {", "", "static const NativeEntry kEntries[] = {"]
    for addr in sorted(blocks):
        table.append("    {0x%X, fn_%06X}," % (addr, blocks[addr].routine))
    table += ["};", "", "static const NativeRange kRanges[] = {"]
    for addr in sorted(blocks):
        table.append("    {0x%X, 0x%X}," % (addr, blocks[addr].end))
    table += ["};", "", "static const uint32_t kHooks[] = {" + "".join("0x%X, " % h for h in hooks) + "0};", "",
              "const NativeCode kNativeCode = {kEntries, sizeof(kEntries) / sizeof(kEntries[0]), kRanges,",
              "                                sizeof(kRanges) / sizeof(kRanges[0]), kHooks, %d};" % len(hooks), "",
              "} // namespace cps2recomp"]
    write("native_table.cpp", "\n".join(table) + "\n")

    for name in os.listdir(args.out):
        if name.startswith("native_") and name not in written:
            os.remove(os.path.join(args.out, name))

    if args.list:
        with open(args.list, "w") as f:
            for addr in sorted(insns):
                insn = insns[addr]
                mark = ">" if addr in starts else " "
                f.write("%s%06X  %s%s\n" % (mark, addr, insn.text, "   ; interpreter" if insn.interp else ""))

    count = len(insns)
    by_interp = sum(1 for i in insns.values() if i.interp and not directly(i, mem, i.addr + i.length))
    not_code = sum(1 for i in insns.values() if i.interp and i.text == "?")
    size = sum(i.length for i in insns.values())
    print("recomp: %d entries -> %d instructions (%d bytes) in %d blocks, %d routines, %d files; %d left to the interpreter%s"
          % (len(entries), count, size, len(blocks), len(routines), len(files), by_interp,
             " (%d of them not instructions: data a guess took for code)" % not_code if not_code else ""))
    if args.guess:
        print("        of those, %d instructions from %d guessed entries" % (count - seen, len(guessed)))


if __name__ == "__main__":
    main()
