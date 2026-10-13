# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.

"""The Z80's instructions, decoded for the sound driver's translator (recompz80.py).

decode(mem, addr) gives an Insn: its length, where control goes after it, its text, and the C++ statements
that do what the port's interpreter (sound/z80.cpp) does for it: the same calls of the same routines, in
the same order, with the same cycle counts. Port I/O (which the board does not have) and oddities are left
to the interpreter.
"""

CYCLES = [
    4, 10, 7, 6, 4, 4, 7, 4, 4, 11, 7, 6, 4, 4, 7, 4,
    8, 10, 7, 6, 4, 4, 7, 4, 12, 11, 7, 6, 4, 4, 7, 4,
    7, 10, 16, 6, 4, 4, 7, 4, 7, 11, 16, 6, 4, 4, 7, 4,
    7, 10, 13, 6, 11, 11, 10, 4, 7, 11, 13, 6, 4, 4, 7, 4,
] + [4, 4, 4, 4, 4, 4, 7, 4] * 6 + [7, 7, 7, 7, 7, 7, 4, 7] + [4, 4, 4, 4, 4, 4, 7, 4] * 9 + [
    5, 10, 10, 10, 10, 11, 7, 11, 5, 10, 10, 0, 10, 17, 7, 11,
    5, 10, 10, 11, 10, 11, 7, 11, 5, 4, 10, 11, 10, 0, 7, 11,
    5, 10, 10, 19, 10, 11, 7, 11, 5, 4, 10, 4, 10, 0, 7, 11,
    5, 10, 10, 4, 10, 11, 7, 11, 5, 6, 10, 4, 10, 0, 7, 11,
]
assert len(CYCLES) == 256

R = ("b", "c", "d", "e", "h", "l", "(hl)", "a")
PAIRS = ("bc", "de", "hl", "sp")
ALU = ("add a,", "adc a,", "sub ", "sbc a,", "and ", "xor ", "or ", "cp ")
ROT = ("rlc", "rrc", "rl", "rr", "sla", "sra", "sll", "srl")
CC = ("nz", "z", "nc", "c", "po", "pe", "p", "m")
CC_EXPR = ("!(c.f & 0x40)", "(c.f & 0x40)", "!(c.f & 0x01)", "(c.f & 0x01)", "!(c.f & 0x04)", "(c.f & 0x04)",
           "!(c.f & 0x80)", "(c.f & 0x80)")
INDEX = ("hl", "ix", "iy")


class NotCode(Exception):
    pass


class Insn:
    def __init__(self, addr):
        self.addr = addr
        self.length = 1
        self.text = "?"
        # next; cond / djnz / branch / call / ccall: `target`; return / jump: the lines leave the address in
        # `to`; cret: a conditional return; end: runs on, but the run loop comes first; repeat: the lines
        # leave `again`; stop: somewhere unknown, by the interpreter
        self.flow = "next"
        self.target = None
        self.cond = None
        self.lines = []
        self.interp = False
        self.cycles = 0      # when it runs on (or the condition fails)
        self.taken = 0       # when a conditional one goes
        self.refresh = 1


def s8(v):
    return v - 256 if v & 0x80 else v


def decode(mem, addr):
    insn = Insn(addr)
    try:
        _decode(insn, mem)
    except (NotCode, IndexError):
        insn.interp = True
        insn.flow = "stop"
        insn.text = "?"
        insn.length = 1
        insn.lines = []
    return insn


def _decode(insn, mem):
    pos = insn.addr

    def byte():
        nonlocal pos
        if pos >= 0x8000:
            raise NotCode()
        v = mem[pos]
        pos += 1
        return v

    def word():
        low = byte()
        return low | (byte() << 8)

    def interp(text, length):
        insn.interp = True
        insn.text = text
        insn.length = length

    op = byte()
    p = 0
    if op in (0xDD, 0xFD):
        p = 1 if op == 0xDD else 2
        op = byte()
        if op in (0xDD, 0xFD, 0xED):
            raise NotCode()
        insn.refresh = 2
    hl = INDEX[p]
    x, y, z = op >> 6, (op >> 3) & 7, op & 7
    cycles = CYCLES[op] + (4 if p else 0)
    lines = insn.lines

    def reg(n):
        return "c.Reg(%d, %d)" % (n, p) if p and n in (4, 5) else "c.r[%d]" % n

    def setreg(n, v):
        return "c.SetReg(%d, %d, %s);" % (n, p, v) if p and n in (4, 5) else "c.r[%d] = %s;" % (n, v)

    def regname(n):
        return (hl + "hl"[n - 4]) if p and n in (4, 5) else R[n]

    def operand():
        """The address of (hl) or (ix+d): (C++ expression, text)"""
        nonlocal cycles
        if not p:
            return "c.HL()", "(hl)"
        d = s8(byte())
        cycles += 8
        return ("static_cast<uint16_t>(c.%s %s %d)" % (hl, "+" if d >= 0 else "-", abs(d)),
                "(%s%s%d)" % (hl, "+" if d >= 0 else "-", abs(d)))

    def finish():
        insn.length = pos - insn.addr
        insn.cycles = cycles

    if op == 0xCB:
        address = None
        if p:
            d = s8(byte())
            address = "static_cast<uint16_t>(c.%s %s %d)" % (hl, "+" if d >= 0 else "-", abs(d))
            where = "(%s%s%d)" % (hl, "+" if d >= 0 else "-", abs(d))
        op2 = byte()
        insn.refresh += 1
        x, y, z = op2 >> 6, (op2 >> 3) & 7, op2 & 7
        memory = bool(p) or z == 6
        if not p and z == 6:
            address, where = "c.HL()", "(hl)"
        cycles = (4 + (16 if x == 1 else 19)) if p else ((12 if x == 1 else 15) if z == 6 else 8)
        name = where if memory else R[z]
        if memory:
            lines.append("const uint16_t address = %s;" % address)
            lines.append("uint8_t value = c.Rd(address);")
        else:
            lines.append("uint8_t value = c.r[%d];" % z)
        if x == 0:
            insn.text = "%s %s" % (ROT[y], name)
            lines.append("value = c.Rotate(%d, value);" % y)
        elif x == 1:
            insn.text = "bit %d,%s" % (y, name)
            lines.append("c.Bit(%d, value, %s);" % (y, "static_cast<uint8_t>(address >> 8)" if memory else "value"))
            return finish()
        elif x == 2:
            insn.text = "res %d,%s" % (y, name)
            lines.append("value &= 0x%02X;" % (~(1 << y) & 0xFF))
        else:
            insn.text = "set %d,%s" % (y, name)
            lines.append("value |= 0x%02X;" % (1 << y))
        if memory:
            lines.append("c.Wr(address, value);")
            if p and z != 6:
                lines.append("c.r[%d] = value;" % z)
        else:
            lines.append("c.r[%d] = value;" % z)
        return finish()

    if op == 0xED:
        if p:
            raise NotCode()
        op2 = byte()
        insn.refresh += 1
        x, y, z = op2 >> 6, (op2 >> 3) & 7, op2 & 7
        cycles = 8
        if x == 1:
            if z in (0, 1):
                return interp("in/out (c)", 2)
            if z == 2:
                insn.text = "%s hl,%s" % ("adc" if y & 1 else "sbc", PAIRS[y >> 1])
                lines.append("c.%s(c.Pair(%d, 0));" % ("AdcHL" if y & 1 else "SbcHL", y >> 1))
                cycles += 7
            elif z == 3:
                nn = word()
                if y & 1:
                    insn.text = "ld %s,($%04X)" % (PAIRS[y >> 1], nn)
                    lines.append("c.SetPair(%d, 0, c.Read16(0x%04X));" % (y >> 1, nn))
                else:
                    insn.text = "ld ($%04X),%s" % (nn, PAIRS[y >> 1])
                    lines.append("c.Write16(0x%04X, c.Pair(%d, 0));" % (nn, y >> 1))
                cycles += 12
            elif z == 4:
                insn.text = "neg"
                lines.append("c.Neg();")
            elif z == 5:
                insn.text = "reti" if y == 1 else "retn"
                insn.flow = "return"
                lines.append("const uint16_t to = c.Pop();")
                lines.append("c.iff1 = c.iff2;")
                cycles += 6
            elif z == 6:
                m = (0, 0, 1, 2, 0, 0, 1, 2)[y]
                insn.text = "im %d" % m
                lines.append("c.mode = %d;" % m)
            else:
                insn.text = ("ld i,a", "ld r,a", "ld a,i", "ld a,r", "rrd", "rld", "nop", "nop")[y]
                lines += (["c.i = c.r[7];"], ["c.refresh = c.r[7];"], ["c.LoadAFromIR(c.i);"], ["c.LoadAFromIR(c.refresh);"],
                          ["c.Rrd();"], ["c.Rld();"], [], [])[y]
                cycles += (1, 1, 1, 1, 10, 10, 0, 0)[y]
            return finish()
        if x == 2 and y >= 4 and z < 4:
            step = -1 if y & 1 else 1
            repeat = y >= 6
            if z >= 2:
                return interp("block i/o", 2)
            name = ("ld", "cp")[z] + ("d" if y & 1 else "i") + ("r" if repeat else "")
            fn = ("BlockLoad", "BlockCompare")[z]
            insn.text = name
            cycles += 8
            if repeat:
                insn.flow = "repeat"
                insn.taken = cycles + 5
                lines.append("const bool again = c.%s(%d, true);" % (fn, step))
            else:
                lines.append("c.%s(%d, false);" % (fn, step))
            return finish()
        insn.text = "nop (ed %02x)" % op2
        return finish()

    if x == 1:
        if op == 0x76:
            insn.text, insn.flow = "halt", "end"
            lines.append("c.halted = true;")
        elif z == 6:
            a, t = operand()
            insn.text = "ld %s,%s" % (R[y], t)
            lines.append("c.r[%d] = c.Rd(%s);" % (y, a))
        elif y == 6:
            a, t = operand()
            insn.text = "ld %s,%s" % (t, R[z])
            lines.append("c.Wr(%s, c.r[%d]);" % (a, z))
        else:
            insn.text = "ld %s,%s" % (regname(y), regname(z))
            lines.append(setreg(y, reg(z)))
        return finish()
    if x == 2:
        if z == 6:
            a, t = operand()
            insn.text = ALU[y] + t
            lines.append("c.Alu(%d, c.Rd(%s));" % (y, a))
        else:
            insn.text = ALU[y] + regname(z)
            lines.append("c.Alu(%d, %s);" % (y, reg(z)))
        return finish()
    if x == 0:
        if z == 0:
            if y == 0:
                insn.text = "nop"
            elif y == 1:
                insn.text = "ex af,af'"
                lines.append("c.ExchangeAF();")
            else:
                d = s8(byte())
                insn.target = (pos + d) & 0xFFFF
                if y == 2:
                    insn.text, insn.flow = "djnz $%04X" % insn.target, "djnz"
                    insn.taken = cycles + 5
                elif y == 3:
                    insn.text, insn.flow = "jr $%04X" % insn.target, "branch"
                else:
                    insn.text, insn.flow = "jr %s,$%04X" % (CC[y - 4], insn.target), "cond"
                    insn.cond = CC_EXPR[y - 4]
                    insn.taken = cycles + 5
        elif z == 1:
            if y & 1:
                insn.text = "add %s,%s" % (hl, hl if y >> 1 == 2 else PAIRS[y >> 1])
                lines.append("c.Add16(%d, c.Pair(%d, %d));" % (p, y >> 1, p))
            else:
                nn = word()
                insn.text = "ld %s,$%04X" % (hl if y >> 1 == 2 else PAIRS[y >> 1], nn)
                lines.append("c.SetPair(%d, %d, 0x%04X);" % (y >> 1, p, nn))
        elif z == 2:
            if y < 4:
                pair = ("BC", "DE")[y >> 1]
                if y & 1:
                    insn.text = "ld a,(%s)" % pair.lower()
                    lines.append("c.r[7] = c.Rd(c.%s());" % pair)
                else:
                    insn.text = "ld (%s),a" % pair.lower()
                    lines.append("c.Wr(c.%s(), c.r[7]);" % pair)
            else:
                nn = word()
                insn.text = ("ld ($%04X),%s" % (nn, hl), "ld %s,($%04X)" % (hl, nn), "ld ($%04X),a" % nn, "ld a,($%04X)" % nn)[y - 4]
                lines.append(("c.Write16(0x%04X, c.Index(%d));" % (nn, p), "c.SetIndex(%d, c.Read16(0x%04X));" % (p, nn),
                              "c.Wr(0x%04X, c.r[7]);" % nn, "c.r[7] = c.Rd(0x%04X);" % nn)[y - 4])
        elif z == 3:
            insn.text = "%s %s" % ("dec" if y & 1 else "inc", hl if y >> 1 == 2 else PAIRS[y >> 1])
            lines.append("c.SetPair(%d, %d, static_cast<uint16_t>(c.Pair(%d, %d) %s 1));" % (y >> 1, p, y >> 1, p, "-" if y & 1 else "+"))
        elif z in (4, 5):
            fn = "Inc" if z == 4 else "Dec"
            if y == 6:
                a, t = operand()
                insn.text = "%s %s" % (fn.lower(), t)
                lines.append("const uint16_t address = %s;" % a)
                lines.append("c.Wr(address, c.%s(c.Rd(address)));" % fn)
            else:
                insn.text = "%s %s" % (fn.lower(), regname(y))
                lines.append(setreg(y, "c.%s(%s)" % (fn, reg(y))))
        elif z == 6:
            if y == 6:
                a, t = operand()
                n = byte()
                if p:
                    cycles -= 3
                insn.text = "ld %s,$%02X" % (t, n)
                lines.append("c.Wr(%s, 0x%02X);" % (a, n))
            else:
                n = byte()
                insn.text = "ld %s,$%02X" % (regname(y), n)
                lines.append(setreg(y, "0x%02X" % n))
        else:
            insn.text = ("rlca", "rrca", "rla", "rra", "daa", "cpl", "scf", "ccf")[y]
            lines.append("c.RotateA(%d);" % y if y < 4 else ("c.Daa();", "c.Cpl();", "c.Scf();", "c.Ccf();")[y - 4])
        return finish()

    # x == 3
    if z == 0:
        insn.text, insn.flow, insn.cond = "ret %s" % CC[y], "cret", CC_EXPR[y]
        insn.taken = cycles + 6
    elif z == 1:
        if not y & 1:
            if y == 6:
                insn.text = "pop af"
                lines.append("c.SetAF(c.Pop());")
            else:
                insn.text = "pop %s" % (hl if y >> 1 == 2 else PAIRS[y >> 1])
                lines.append("c.SetPair(%d, %d, c.Pop());" % (y >> 1, p))
        elif y >> 1 == 0:
            insn.text, insn.flow = "ret", "return"
            lines.append("const uint16_t to = c.Pop();")
        elif y >> 1 == 1:
            insn.text = "exx"
            lines.append("c.Exx();")
        elif y >> 1 == 2:
            insn.text, insn.flow = "jp (%s)" % hl, "jump"
            lines.append("const uint16_t to = c.Index(%d);" % p)
        else:
            insn.text = "ld sp,%s" % hl
            lines.append("c.sp = c.Index(%d);" % p)
    elif z == 2:
        insn.target = word()
        insn.text, insn.flow, insn.cond = "jp %s,$%04X" % (CC[y], insn.target), "cond", CC_EXPR[y]
        insn.taken = cycles
    elif z == 3:
        if y == 0:
            insn.target = word()
            insn.text, insn.flow = "jp $%04X" % insn.target, "branch"
        elif y in (2, 3):
            return interp("in/out (n)", 2 + (1 if p else 0))
        elif y == 4:
            insn.text = "ex (sp),%s" % hl
            lines.append("const uint16_t value = c.Read16(c.sp);")
            lines.append("c.Write16(c.sp, c.Index(%d));" % p)
            lines.append("c.SetIndex(%d, value);" % p)
        elif y == 5:
            insn.text = "ex de,hl"
            lines.append("const uint16_t de = c.DE();")
            lines.append("c.SetDE(c.HL());")
            lines.append("c.SetHL(de);")
        elif y == 6:
            insn.text = "di"
            lines.append("c.iff1 = c.iff2 = false;")
        else:
            insn.text, insn.flow = "ei", "end"
            lines.append("c.EnableInterrupts();")
    elif z == 4:
        insn.target = word()
        insn.text, insn.flow, insn.cond = "call %s,$%04X" % (CC[y], insn.target), "ccall", CC_EXPR[y]
        insn.taken = cycles + 7
    elif z == 5:
        if not y & 1:
            insn.text = "push %s" % ("af" if y == 6 else hl if y >> 1 == 2 else PAIRS[y >> 1])
            lines.append("c.Push(%s);" % ("c.AF()" if y == 6 else "c.Pair(%d, %d)" % (y >> 1, p)))
        elif y == 1:
            insn.target = word()
            insn.text, insn.flow = "call $%04X" % insn.target, "call"
        else:
            raise NotCode()
    elif z == 6:
        n = byte()
        insn.text = "%s$%02X" % (ALU[y], n)
        lines.append("c.Alu(%d, 0x%02X);" % (y, n))
    else:
        insn.target = y * 8
        insn.text, insn.flow = "rst $%02X" % insn.target, "call"
    return finish()
