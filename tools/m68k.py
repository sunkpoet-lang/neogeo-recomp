# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.

"""The 68000's instructions, decoded for the translator (recomp.py) and the disassembler (m68kdis.py).

decode(mem, addr) gives an Insn: its length, where control goes after it, which flags it reads and sets,
its text, and (emit) the C++ statements that do what the port's interpreter (machine/m68k.cpp) does for it,
in the same order and with the same cycle counts. Instructions the game hardly uses are left to the
interpreter (Insn.interp): the translated code calls it for that one instruction.
"""

X, N, Z, V, C = 16, 8, 4, 2, 1
NZVC = N | Z | V | C
ALL = X | NZVC

MASK = (0xFF, 0xFFFF, 0xFFFFFFFF)
BYTES = (1, 2, 4)
SUFFIX = "bwl"
# What working out an operand's address and reading it costs: [long word][mode index]
OPERAND_CYCLES = ((0, 0, 4, 4, 6, 8, 10, 8, 12, 8, 10, 4), (0, 0, 8, 8, 10, 12, 14, 12, 16, 12, 14, 8))
JUMP_CYCLES = (0, 0, 8, 0, 0, 10, 14, 10, 12, 10, 14, 0)
LEA_CYCLES = (0, 0, 4, 0, 0, 8, 12, 8, 12, 8, 12, 0)
PEA_CYCLES = (0, 0, 12, 0, 0, 16, 20, 16, 20, 16, 20, 0)
MOVEM_TO_MEMORY = (0, 0, 8, 0, 8, 12, 14, 12, 16, 0, 0, 0)
MOVEM_TO_REGISTERS = (0, 0, 16, 12, 0, 20, 22, 20, 24, 20, 22, 0)
CC_NAMES = ("t", "f", "hi", "ls", "cc", "cs", "ne", "eq", "vc", "vs", "pl", "mi", "ge", "lt", "gt", "le")
CC_USES = (0, 0, C | Z, C | Z, C, C, Z, Z, V, V, N, N, N | V, N | V, N | V | Z, N | V | Z)
CC_EXPR = (
    "true", "false", "(!c.flagC && !c.flagZ)", "(c.flagC || c.flagZ)", "!c.flagC", "c.flagC", "!c.flagZ", "c.flagZ",
    "!c.flagV", "c.flagV", "!c.flagN", "c.flagN", "(c.flagN == c.flagV)", "(c.flagN != c.flagV)",
    "(!c.flagZ && c.flagN == c.flagV)", "(c.flagZ || c.flagN != c.flagV)",
)


class NotCode(Exception):
    pass


def sext8(v):
    return v - 0x100 if v & 0x80 else v


def sext16(v):
    return v - 0x10000 if v & 0x8000 else v


def hexs(v):
    """A signed number as C++ and assembler write it"""
    return "-0x%X" % -v if v < 0 else "0x%X" % v


def mode_index(mode, reg):
    return mode if mode < 7 else 7 + reg


class Insn:
    def __init__(self, addr):
        self.addr = addr
        self.length = 2
        self.text = "?"
        # next: runs on. cond / branch / call: `target`. jump / return: somewhere worked out at run time
        # (emit ends with the address in `to`). calli: a call the same way. dbcc: see recomp.py.
        # end: runs on, but the run loop must look at interrupts first. stop: goes somewhere unknown, by the
        # interpreter.
        self.flow = "next"
        self.target = None
        self.cond = None        # the C++ condition of a conditional branch
        self.uses = 0           # flags read
        self.defs = 0           # flags written (if any of these is wanted, emit(True))
        self.kills = 0          # flags whose old value is gone afterwards
        self.interp = False     # left to the interpreter
        self.emit = None        # emit(flags_wanted) -> list of C++ lines
        self.taken = 0          # cycles of a branch when taken / not (the others are in the emitted lines)
        self.not_taken = 0
        self.reg = None         # dbcc's counter


class Ea:
    """An operand: (mode, reg) with its extension words read. kind: d a m p i (data register, address register,
    memory, memory read as program, immediate)."""

    def __init__(self, dec, mode, reg, size):
        self.mode, self.reg, self.size = mode, reg, size
        self.index = mode_index(mode, reg)
        self.pre = []     # statements before the operand can be used (the address in `var`)
        self.var = None
        self.const = None  # a fixed address
        n = dec.temp()
        step = 2 if (size == 0 and reg == 7) else BYTES[size]
        if mode == 0:
            self.kind, self.text = "d", "d%d" % reg
        elif mode == 1:
            self.kind, self.text = "a", "a%d" % reg
        elif mode == 2:
            self.kind, self.text = "m", "(a%d)" % reg
            self.addr_expr = "c.a[%d]" % reg
        elif mode == 3:
            self.kind, self.text = "m", "(a%d)+" % reg
            self.var = "ea%d" % n
            self.pre = ["const uint32_t %s = c.a[%d];" % (self.var, reg), "c.a[%d] += %d;" % (reg, step)]
        elif mode == 4:
            self.kind, self.text = "m", "-(a%d)" % reg
            self.var = "ea%d" % n
            self.pre = ["c.a[%d] -= %d;" % (reg, step), "const uint32_t %s = c.a[%d];" % (self.var, reg)]
        elif mode == 5:
            d = sext16(dec.word())
            self.kind, self.text = "m", "%s(a%d)" % (hexs(d), reg)
            self.addr_expr = "c.a[%d] + %s" % (reg, hexs(d)) if d >= 0 else "c.a[%d] - 0x%X" % (reg, -d)
        elif mode == 6:
            self.kind = "m"
            idx, text = self.indexed(dec.word())
            self.text = "%s(a%d,%s)" % (text[0], reg, text[1])
            self.addr_expr = "c.a[%d] + %s" % (reg, idx)
        elif reg == 0:
            a = sext16(dec.word()) & 0xFFFFFFFF
            self.kind, self.text, self.const = "m", "$%X.w" % (a & 0xFFFFFF), a
            self.addr_expr = "0x%Xu" % a
        elif reg == 1:
            a = dec.long()
            self.kind, self.text, self.const = "m", "$%X" % a, a
            self.addr_expr = "0x%Xu" % a
        elif reg == 2:
            base = dec.pos
            a = (base + sext16(dec.word())) & 0xFFFFFFFF
            self.kind, self.text, self.const = "p", "$%X(pc)" % a, a
            self.addr_expr = "0x%Xu" % a
        elif reg == 3:
            base = dec.pos
            idx, text = self.indexed(dec.word())
            self.kind = "p"
            self.text = "$%X(pc,%s)" % ((base + int(text[0], 16)) & 0xFFFFFF, text[1])
            self.addr_expr = "0x%Xu + %s" % (base, idx)
        elif reg == 4:
            v = dec.long() if size == 2 else dec.word() & MASK[size]
            self.kind, self.text, self.value = "i", "#$%X" % v, v
        else:
            raise NotCode()
        if self.kind in "mp" and self.var is None:
            self.var = "ea%d" % n
            self.pre = ["const uint32_t %s = %s;" % (self.var, self.addr_expr)]

    @staticmethod
    def indexed(ext):
        r = (ext >> 12) & 7
        reg = ("c.a[%d]" if ext & 0x8000 else "c.d[%d]") % r
        d = sext8(ext & 0xFF)
        idx = reg if ext & 0x0800 else "S16(%s)" % reg
        expr = "%s + %s" % (idx, hexs(d)) if d >= 0 else "%s - 0x%X" % (idx, -d)
        return expr, (hexs(d), "%s%d.%s" % ("a" if ext & 0x8000 else "d", r, "l" if ext & 0x0800 else "w"))

    def load(self, size=None):
        size = self.size if size is None else size
        if self.kind == "d":
            return "c.d[%d]" % self.reg if size == 2 else "(c.d[%d] & 0x%X)" % (self.reg, MASK[size])
        if self.kind == "a":
            return "c.a[%d]" % self.reg if size == 2 else "(c.a[%d] & 0x%X)" % (self.reg, MASK[size])
        if self.kind == "m":
            return "R%d(c, %s)" % (BYTES[size] * 8, self.var)
        if self.kind == "p":
            return "P%d(c, %s)" % (BYTES[size] * 8, self.var)
        return "0x%Xu" % self.value

    def store(self, expr, size=None):
        size = self.size if size is None else size
        if self.kind == "d":
            if size == 2:
                return ["c.d[%d] = %s;" % (self.reg, expr)]
            return ["Set%d(c.d[%d], %s);" % (BYTES[size] * 8, self.reg, expr)]
        if self.kind == "a":
            return ["c.a[%d] = %s;" % (self.reg, expr)]
        if self.kind == "m":
            return ["W%d(c, %s, %s);" % (BYTES[size] * 8, self.var, expr)]
        return []  # (the interpreter stores nothing there either)


class Decoder:
    def __init__(self, mem, addr):
        self.mem, self.addr, self.pos = mem, addr, addr + 2
        self.temps = 0

    def temp(self):
        self.temps += 1
        return self.temps

    def word(self):
        if self.pos + 2 > len(self.mem):
            raise NotCode()
        v = (self.mem[self.pos] << 8) | self.mem[self.pos + 1]
        self.pos += 2
        return v

    def long(self):
        high = self.word()
        return (high << 16) | self.word()

    def ea(self, mode, reg, size):
        return Ea(self, mode, reg, size)


def cyc(n):
    return "c.cycles += %s;" % n


def decode(mem, addr):
    """The instruction at addr; an Insn with flow 'stop' and interp set if it is not one the translator knows"""
    insn = Insn(addr)
    dec = Decoder(mem, addr)
    try:
        if addr & 1 or addr + 2 > len(mem):
            raise NotCode()
        op = (mem[addr] << 8) | mem[addr + 1]
        _decode(insn, dec, op)
    except NotCode:
        insn.interp = True
        insn.flow = "stop"
        insn.text = "?"
        insn.uses = ALL
        insn.defs = insn.kills = 0
        insn.length = 2
        insn.emit = None
        return insn
    insn.length = dec.pos - addr
    return insn


def by_interpreter(insn, dec, text, flow="next", length=2):
    insn.interp = True
    insn.text = text
    insn.flow = flow
    insn.uses = ALL
    dec.pos = insn.addr + length


def _decode(insn, dec, op):
    mode, reg, reg2 = (op >> 3) & 7, op & 7, (op >> 9) & 7
    size = (op >> 6) & 3
    mi = mode_index(mode, reg)
    if mi > 11:
        mi = 11
    group = op >> 12

    def opc(sz, index=None):
        return OPERAND_CYCLES[1 if sz == 2 else 0][mi if index is None else index]

    def logic(expr, sz):
        return "c.Logic(%s, %d);" % (expr, sz)

    if group == 0:
        if op & 0x0100 and mode == 1:
            # movep: every other byte of memory to or from a data register
            d = sext16(dec.word())
            count = 4 if op & 0x0040 else 2
            to_memory = bool(op & 0x0080)
            where = "%s(a%d)" % (hexs(d), reg)
            insn.text = "movep.%s %s" % ("l" if count == 4 else "w", "d%d,%s" % (reg2, where) if to_memory else "%s,d%d" % (where, reg2))

            def emit(live, d=d, count=count, to_memory=to_memory):
                lines = ["const uint32_t address = c.a[%d] %s 0x%X;" % (reg, "+" if d >= 0 else "-", abs(d))]
                if to_memory:
                    for i in range(count):
                        lines.append("W8(c, address + %d, c.d[%d] >> %d);" % (i * 2, reg2, (count - 1 - i) * 8))
                else:
                    lines.append("uint32_t value = 0;")
                    for i in range(count):
                        lines.append("value = (value << 8) | R8(c, address + %d);" % (i * 2))
                    lines.append("c.d[%d] = value;" % reg2 if count == 4 else "Set16(c.d[%d], value);" % reg2)
                return lines + [cyc(24 if count == 4 else 16)]

            insn.emit = emit
            return
        if not (op & 0x0100) and reg2 not in (4, 7) and (op & 0x3F) == 0x3C and size < 2:
            if reg2 not in (0, 1, 5):
                raise NotCode()
            name = {0: "ori", 1: "andi", 5: "eori"}[reg2]
            value = dec.word()
            if size == 0:
                insn.text = "%s #$%X,ccr" % (name, value & 0xFF)
                insn.uses = insn.defs = ALL
                change = {0: "sr |= 0x%Xu;", 1: "sr &= 0x%Xu;", 5: "sr ^= 0x%Xu;"}[reg2] % (
                    (value & 0xFF) | (0xFF00 if reg2 == 1 else 0))
                insn.emit = lambda live: ["uint16_t sr = c.SR();", change,
                                          "c.SetSR(static_cast<uint16_t>((c.SR() & 0xFF00) | (sr & 0x1F)));", cyc(20)]
                return
            return by_interpreter(insn, dec, "%s #$%X,sr" % (name, value), flow="end", length=4)
        if op & 0x0100 or reg2 == 4:
            dynamic = bool(op & 0x0100)
            kind = (op >> 6) & 3
            name = ("btst", "bchg", "bclr", "bset")[kind]
            number = "c.d[%d]" % reg2 if dynamic else "0x%Xu" % dec.word()
            number_text = "d%d" % reg2 if dynamic else "#" + number[2:-1]
            change = ("", "^= bit", "&= ~bit", "|= bit")[kind]
            if mode == 0:
                insn.text = "%s %s,d%d" % (name, number_text, reg)
                cycles = (6, 8, 10, 8)[kind] + (0 if dynamic else 4)

                def emit(live, number=number, change=change, cycles=cycles):
                    lines = ["const uint32_t bit = 1u << (%s & 31);" % number]
                    if live:
                        lines.append("c.flagZ = !(c.d[%d] & bit);" % reg)
                    if change:
                        lines.append("c.d[%d] %s;" % (reg, change))
                    return lines + [cyc(cycles)]
            else:
                if mode == 1:
                    raise NotCode()
                ea = dec.ea(mode, reg, 0)
                if kind != 0 and ea.kind != "m":
                    raise NotCode()
                insn.text = "%s %s,%s" % (name, number_text, ea.text)
                cycles = opc(0) + (4 if kind == 0 else 8) + (0 if dynamic else 4)

                def emit(live, number=number, change=change, cycles=cycles, ea=ea, kind=kind):
                    lines = ea.pre + ["uint32_t value = %s;" % ea.load(), "const uint32_t bit = 1u << (%s & 7);" % number]
                    if live:
                        lines.append("c.flagZ = !(value & bit);")
                    if kind:
                        lines.append("value %s;" % change)
                        lines += ea.store("value")
                    return lines + [cyc(cycles)]
            insn.defs = insn.kills = Z
            insn.emit = emit
            return
        if size == 3 or reg2 == 7 or mode == 1:
            raise NotCode()
        name = ("ori", "andi", "subi", "addi", None, "eori", "cmpi")[reg2]
        value = dec.long() if size == 2 else dec.word() & MASK[size]
        ea = dec.ea(mode, reg, size)
        if ea.kind == "i" or (ea.kind == "p" and reg2 != 6):
            raise NotCode()
        insn.text = "%s.%s #$%X,%s" % (name, SUFFIX[size], value, ea.text)
        if reg2 != 6:
            cycles = (16 if size == 2 else 8) if mode == 0 else opc(size) + (20 if size == 2 else 12)
        else:
            cycles = (14 if size == 2 else 8) if mode == 0 else opc(size) + (12 if size == 2 else 8)
        insn.defs = insn.kills = ALL if reg2 in (2, 3) else NZVC

        def emit(live, value=value, ea=ea, cycles=cycles):
            v = "0x%Xu" % value
            lines = ea.pre + ["const uint32_t target = %s;" % ea.load()]
            if reg2 in (0, 1, 5):
                lines.append("const uint32_t result = target %s %s;" % ({0: "|", 1: "&", 5: "^"}[reg2], v))
                if live:
                    lines.append(logic("result", size))
                lines += ea.store("result")
            elif reg2 in (2, 3):
                fn, sign = ("Sub", "-") if reg2 == 2 else ("Add", "+")
                if live:
                    lines += ea.store("c.%s(target, %s, %d, false, false)" % (fn, v, size))
                else:
                    lines += ea.store("target %s %s" % (sign, v))
            elif live:
                lines.append("c.Sub(target, %s, %d, false, false, false);" % (v, size))
            else:
                lines.append("(void)target;")
            return lines + [cyc(cycles)]

        insn.emit = emit
        return

    if group in (1, 2, 3):
        sz = {1: 0, 3: 1, 2: 2}[group]
        src = dec.ea(mode, reg, sz)
        dst_mode = (op >> 6) & 7
        cycles = 4 + opc(sz)
        if dst_mode == 1:
            if sz == 0:
                raise NotCode()
            insn.text = "movea.%s %s,a%d" % (SUFFIX[sz], src.text, reg2)

            def emit(live, src=src, cycles=cycles):
                value = src.load() if sz == 2 else "S16(%s)" % src.load()
                return src.pre + ["c.a[%d] = %s;" % (reg2, value), cyc(cycles)]

            insn.emit = emit
            return
        dst_index = mode_index(dst_mode, reg2)
        if dst_index > 8 or (src.kind == "a" and sz == 0):
            raise NotCode()
        dst = dec.ea(dst_mode, reg2, sz)
        cycles += OPERAND_CYCLES[1 if sz == 2 else 0][dst_index]
        insn.text = "move.%s %s,%s" % (SUFFIX[sz], src.text, dst.text)
        insn.defs = insn.kills = NZVC

        def emit(live, src=src, dst=dst, cycles=cycles):
            lines = src.pre + ["const uint32_t value = %s;" % src.load()] + dst.pre + dst.store("value")
            if live:
                lines.append(logic("value", sz))
            return lines + [cyc(cycles)]

        insn.emit = emit
        return

    if group == 4:
        if op == 0x4E71:
            insn.text = "nop"
            insn.emit = lambda live: [cyc(4)]
            return
        if op == 0x4E75:
            insn.text, insn.flow = "rts", "return"
            insn.emit = lambda live: ["const uint32_t to = R32(c, c.a[7]);", "c.a[7] += 4;", cyc(16)]
            return
        if op == 0x4E73:
            return by_interpreter(insn, dec, "rte", flow="stop")
        if op == 0x4E77:
            return by_interpreter(insn, dec, "rtr", flow="stop")
        if op == 0x4E70:
            return by_interpreter(insn, dec, "reset")
        if op == 0x4E72:
            return by_interpreter(insn, dec, "stop #$%X" % dec.word(), flow="end", length=4)
        if op == 0x4E76:
            return by_interpreter(insn, dec, "trapv")
        if (op & 0xFFF0) == 0x4E40:
            return by_interpreter(insn, dec, "trap #%d" % (op & 15), flow="end")
        if (op & 0xFFF8) == 0x4E50:
            d = sext16(dec.word())
            insn.text = "link a%d,#%s" % (reg, hexs(d))
            insn.emit = lambda live: [
                "c.a[7] -= 4;", "W32(c, c.a[7], c.a[%d]);" % reg, "c.a[%d] = c.a[7];" % reg,
                "c.a[7] += %s;" % hexs(d) if d >= 0 else "c.a[7] -= 0x%X;" % -d, cyc(16)]
            return
        if (op & 0xFFF8) == 0x4E58:
            insn.text = "unlk a%d" % reg
            insn.emit = lambda live: [
                "c.a[7] = c.a[%d];" % reg, "c.a[%d] = R32(c, c.a[7]);" % reg, "c.a[7] += 4;", cyc(12)]
            return
        if (op & 0xFFF0) == 0x4E60:
            if op & 8:
                insn.text = "move usp,a%d" % reg
                insn.emit = lambda live: ["c.a[%d] = c.otherSP;" % reg, cyc(4)]
            else:
                insn.text = "move a%d,usp" % reg
                insn.emit = lambda live: ["c.otherSP = c.a[%d];" % reg, cyc(4)]
            return
        if (op & 0xFF80) == 0x4E80:
            if JUMP_CYCLES[mi] == 0:
                raise NotCode()
            ea = dec.ea(mode, reg, 2)
            call = not (op & 0x0040)
            cycles = JUMP_CYCLES[mi] + (8 if call else 0)
            insn.text = "%s %s" % ("jsr" if call else "jmp", ea.text)
            back = dec.pos
            push = ["c.a[7] -= 4;", "W32(c, c.a[7], 0x%Xu);" % back] if call else []
            if ea.const is not None:
                insn.flow, insn.target = ("call" if call else "branch"), ea.const & 0xFFFFFF
                insn.emit = lambda live: push + [cyc(cycles)]
            else:
                insn.flow = "calli" if call else "jump"
                insn.emit = lambda live: ea.pre + ["const uint32_t to = %s;" % ea.var] + push + [cyc(cycles)]
            return
        if (op & 0xF1C0) == 0x41C0:
            if LEA_CYCLES[mi] == 0:
                raise NotCode()
            ea = dec.ea(mode, reg, 2)
            insn.text = "lea %s,a%d" % (ea.text, reg2)
            insn.emit = lambda live: ea.pre + ["c.a[%d] = %s;" % (reg2, ea.var), cyc(LEA_CYCLES[mi])]
            return
        if (op & 0xF1C0) == 0x4180:
            ea = dec.ea(mode, reg, 1)
            return by_interpreter(insn, dec, "chk %s,d%d" % (ea.text, reg2), length=dec.pos - insn.addr)
        top = op & 0xFF00
        if top in (0x4000, 0x4200, 0x4400, 0x4600):
            if size == 3:
                if top == 0x4200:
                    raise NotCode()
                ea = dec.ea(mode, reg, 1)
                length = dec.pos - insn.addr
                if top == 0x4000:
                    return by_interpreter(insn, dec, "move sr,%s" % ea.text, length=length)
                if top == 0x4400:
                    return by_interpreter(insn, dec, "move %s,ccr" % ea.text, length=length)
                return by_interpreter(insn, dec, "move %s,sr" % ea.text, flow="end", length=length)
            if mode == 1:
                raise NotCode()
            ea = dec.ea(mode, reg, size)
            if ea.kind not in "dm":
                raise NotCode()
            name = {0x4000: "negx", 0x4200: "clr", 0x4400: "neg", 0x4600: "not"}[top]
            insn.text = "%s.%s %s" % (name, SUFFIX[size], ea.text)
            cycles = (6 if size == 2 else 4) if mode < 2 else opc(size) + (12 if size == 2 else 8)
            if top == 0x4000:
                insn.uses, insn.defs, insn.kills = X | Z, ALL, X | N | V | C
            elif top == 0x4400:
                insn.defs = insn.kills = ALL
            else:
                insn.defs = insn.kills = NZVC

            def emit(live, ea=ea, cycles=cycles):
                lines = list(ea.pre)
                if top == 0x4200:
                    if live:
                        lines.append(logic("0", size))
                    lines += ea.store("0")
                elif top == 0x4600:
                    lines.append("const uint32_t result = ~%s & 0x%Xu;" % (ea.load(), MASK[size]))
                    if live:
                        lines.append(logic("result", size))
                    lines += ea.store("result")
                elif top == 0x4000:
                    lines.append("const uint32_t value = %s;" % ea.load())
                    lines += ea.store("c.Sub(0, value, %d, true, true)" % size)
                else:
                    lines.append("const uint32_t value = %s;" % ea.load())
                    lines += ea.store("c.Sub(0, value, %d, false, false)" % size if live else "0u - value")
                return lines + [cyc(cycles)]

            insn.emit = emit
            return
        if top == 0x4800 or top == 0x4C00:
            if top == 0x4800 and size == 0:
                ea = dec.ea(mode, reg, 0)
                return by_interpreter(insn, dec, "nbcd %s" % ea.text, length=dec.pos - insn.addr)
            if top == 0x4800 and size == 1:
                if mode == 0:
                    insn.text = "swap d%d" % reg
                    insn.defs = insn.kills = NZVC
                    insn.emit = lambda live: ["c.d[%d] = (c.d[%d] << 16) | (c.d[%d] >> 16);" % (reg, reg, reg)] + (
                        [logic("c.d[%d]" % reg, 2)] if live else []) + [cyc(4)]
                    return
                if PEA_CYCLES[mi] == 0:
                    raise NotCode()
                ea = dec.ea(mode, reg, 2)
                insn.text = "pea %s" % ea.text
                insn.emit = lambda live: ea.pre + ["c.a[7] -= 4;", "W32(c, c.a[7], %s);" % ea.var, cyc(PEA_CYCLES[mi])]
                return
            if top == 0x4800 and mode == 0:
                insn.defs = insn.kills = NZVC
                if size == 2:
                    insn.text = "ext.w d%d" % reg
                    insn.emit = lambda live: ["Set16(c.d[%d], S8(c.d[%d]));" % (reg, reg)] + (
                        [logic("c.d[%d]" % reg, 1)] if live else []) + [cyc(4)]
                else:
                    insn.text = "ext.l d%d" % reg
                    insn.emit = lambda live: ["c.d[%d] = S16(c.d[%d]);" % (reg, reg)] + (
                        [logic("c.d[%d]" % reg, 2)] if live else []) + [cyc(4)]
                return
            # movem
            if size < 2:
                raise NotCode()
            sz = 2 if size == 3 else 1
            bytes_ = BYTES[sz]
            mask = dec.word()
            to_registers = top == 0x4C00
            count = bin(mask).count("1")
            names = ["d%d" % n for n in range(8)] + ["a%d" % n for n in range(8)]
            regs_c = ["c.d[%d]" % n for n in range(8)] + ["c.a[%d]" % n for n in range(8)]
            if not to_registers and mode == 4:
                listed = [names[15 - i] for i in range(16) if mask & (1 << i)]
                insn.text = "movem.%s %s,-(a%d)" % (SUFFIX[sz], "/".join(reversed(listed)), reg)

                def emit(live):
                    lines = ["uint32_t address = c.a[%d];" % reg]
                    for i in range(16):
                        if mask & (1 << i):
                            lines.append("address -= %d;" % bytes_)
                            lines.append("W%d(c, address, %s);" % (bytes_ * 8, regs_c[15 - i]))
                    lines.append("c.a[%d] = address;" % reg)
                    return lines + [cyc(8 + count * (8 if sz == 2 else 4))]

                insn.emit = emit
                return
            table = MOVEM_TO_REGISTERS if to_registers else MOVEM_TO_MEMORY
            if table[mi] == 0:
                raise NotCode()
            listed = "/".join(names[i] for i in range(16) if mask & (1 << i))
            if mode == 3:
                ea, ea_text, start, program = None, "(a%d)+" % reg, "c.a[%d]" % reg, False
            else:
                ea = dec.ea(mode, reg, sz)
                ea_text, start, program = ea.text, ea.var, ea.kind == "p"
            insn.text = ("movem.%s %s,%s" % (SUFFIX[sz], ea_text, listed) if to_registers
                         else "movem.%s %s,%s" % (SUFFIX[sz], listed, ea_text))
            cycles = table[mi] + count * (8 if sz == 2 else 4)

            def emit(live):
                lines = (ea.pre if ea else []) + ["uint32_t address = %s;" % start]
                read = "%s%d(c, address)" % ("P" if program else "R", bytes_ * 8)
                for i in range(16):
                    if not mask & (1 << i):
                        continue
                    if to_registers:
                        lines.append("%s = %s;" % (regs_c[i], read if sz == 2 else "S16(%s)" % read))
                    else:
                        lines.append("W%d(c, address, %s);" % (bytes_ * 8, regs_c[i]))
                    lines.append("address += %d;" % bytes_)
                if mode == 3:
                    lines.append("c.a[%d] = address;" % reg)
                return lines + [cyc(cycles)]

            insn.emit = emit
            return
        if top == 0x4A00:
            if size == 3:
                ea = dec.ea(mode, reg, 0)
                return by_interpreter(insn, dec, "tas %s" % ea.text, length=dec.pos - insn.addr)
            ea = dec.ea(mode, reg, size)
            if ea.kind == "i" or (ea.kind == "a" and size == 0):
                raise NotCode()
            insn.text = "tst.%s %s" % (SUFFIX[size], ea.text)
            insn.defs = insn.kills = NZVC
            cycles = 4 + opc(size)
            insn.emit = lambda live: ea.pre + [
                logic(ea.load(), size) if live else "(void)%s;" % ea.load(), cyc(cycles)]
            return
        raise NotCode()

    if group == 5:
        if size == 3:
            cc = (op >> 8) & 15
            if mode == 1:
                base = dec.pos
                insn.target = (base + sext16(dec.word())) & 0xFFFFFF
                insn.text = "db%s d%d,$%X" % (CC_NAMES[cc] if cc != 1 else "ra", reg, insn.target)
                insn.flow, insn.cond, insn.reg, insn.uses = "dbcc", CC_EXPR[cc], reg, CC_USES[cc]
                insn.emit = lambda live: []
                return
            ea = dec.ea(mode, reg, 0)
            if ea.kind not in "dm":
                raise NotCode()
            insn.text = "s%s %s" % (CC_NAMES[cc], ea.text)
            insn.uses = CC_USES[cc]

            def emit(live, ea=ea, cc=cc):
                lines = ea.pre + ["const bool set = %s;" % CC_EXPR[cc]] + ea.store("(set ? 0xFFu : 0u)")
                return lines + [cyc("(set ? 6 : 4)" if mode == 0 else 8 + opc(0))]

            insn.emit = emit
            return
        value = reg2 or 8
        sub = bool(op & 0x0100)
        name = "subq" if sub else "addq"
        if mode == 1:
            if size == 0:
                raise NotCode()
            insn.text = "%s.%s #%d,a%d" % (name, SUFFIX[size], value, reg)
            insn.emit = lambda live: ["c.a[%d] %s %d;" % (reg, "-=" if sub else "+=", value), cyc(8)]
            return
        ea = dec.ea(mode, reg, size)
        if ea.kind not in "dm":
            raise NotCode()
        insn.text = "%s.%s #%d,%s" % (name, SUFFIX[size], value, ea.text)
        insn.defs = insn.kills = ALL
        cycles = (8 if size == 2 else 4) if mode == 0 else opc(size) + (12 if (size == 2 or not sub) else 8)

        def emit(live, ea=ea, cycles=cycles):
            lines = ea.pre + ["const uint32_t before = %s;" % ea.load()]
            if live:
                lines += ea.store("c.%s(before, %d, %d, false, false)" % ("Sub" if sub else "Add", value, size))
            else:
                lines += ea.store("before %s %d" % ("-" if sub else "+", value))
            return lines + [cyc(cycles)]

        insn.emit = emit
        return

    if group == 6:
        base = dec.pos
        cc = (op >> 8) & 15
        d = sext8(op & 0xFF)
        wide = d == 0
        if wide:
            d = sext16(dec.word())
        insn.target = (base + d) & 0xFFFFFF
        if insn.target & 1:
            raise NotCode()
        back = dec.pos
        if cc == 1:
            insn.text, insn.flow = "bsr $%X" % insn.target, "call"
            insn.emit = lambda live: ["c.a[7] -= 4;", "W32(c, c.a[7], 0x%Xu);" % back, cyc(18)]
        elif cc == 0:
            insn.text, insn.flow = "bra $%X" % insn.target, "branch"
            insn.emit = lambda live: [cyc(10)]
        else:
            insn.text, insn.flow = "b%s $%X" % (CC_NAMES[cc], insn.target), "cond"
            insn.cond, insn.uses = CC_EXPR[cc], CC_USES[cc]
            insn.taken, insn.not_taken = 10, 12 if wide else 8
            insn.emit = lambda live: []
        return

    if group == 7:
        if op & 0x0100:
            raise NotCode()
        v = sext8(op & 0xFF) & 0xFFFFFFFF
        insn.text = "moveq #%s,d%d" % (hexs(sext8(op & 0xFF)), reg2)
        insn.defs = insn.kills = NZVC
        insn.emit = lambda live: ["c.d[%d] = 0x%Xu;" % (reg2, v)] + ([logic("c.d[%d]" % reg2, 2)] if live else []) + [cyc(4)]
        return

    if group in (8, 0xC):
        is_and = group == 0xC
        if size == 3:
            if mode == 1:
                raise NotCode()
            ea = dec.ea(mode, reg, 1)
            signed = bool(op & 0x0100)
            if not is_and:
                return by_interpreter(insn, dec, "div%s %s,d%d" % ("s" if signed else "u", ea.text, reg2),
                                      length=dec.pos - insn.addr)
            insn.text = "mul%s %s,d%d" % ("s" if signed else "u", ea.text, reg2)
            insn.defs = insn.kills = NZVC

            def emit(live, ea=ea):
                lines = ea.pre + ["const uint32_t source = %s;" % ea.load()]
                if signed:
                    lines.append("c.d[%d] = static_cast<uint32_t>(static_cast<int16_t>(c.d[%d]) * static_cast<int16_t>(source));"
                                 % (reg2, reg2))
                    extra = "std::popcount(static_cast<uint16_t>(source ^ (source << 1)))"
                else:
                    lines.append("c.d[%d] = (c.d[%d] & 0xFFFF) * source;" % (reg2, reg2))
                    extra = "std::popcount(source)"
                if live:
                    lines.append(logic("c.d[%d]" % reg2, 2))
                return lines + [cyc("%d + 2 * %s" % (38 + opc(1), extra))]

            insn.emit = emit
            return
        if (op & 0x01F0) == 0x0100:
            memory = bool(op & 8)
            if memory:
                src, dst = dec.ea(4, reg, 0), dec.ea(4, reg2, 0)
            else:
                src, dst = dec.ea(0, reg, 0), dec.ea(0, reg2, 0)
            insn.text = "%s %s,%s" % ("abcd" if is_and else "sbcd", src.text, dst.text)
            insn.uses, insn.defs, insn.kills = X | Z, X | N | Z | C, X | N | C
            insn.emit = lambda live: src.pre + dst.pre + [
                "const uint32_t x = %s, y = %s;" % (src.load(), dst.load())] + dst.store(
                    "c.Bcd(%s, x, y)" % ("true" if is_and else "false")) + [cyc(18 if memory else 6)]
            return
        if is_and and op & 0x0100 and mode < 2 and size != 0:
            if size == 1:
                x, y = (("c.d[%d]" % reg2, "c.d[%d]" % reg) if mode == 0 else ("c.a[%d]" % reg2, "c.a[%d]" % reg))
            else:
                x, y = "c.d[%d]" % reg2, "c.a[%d]" % reg
            insn.text = "exg %s,%s" % (x[2] + x[4], y[2] + y[4])
            insn.emit = lambda live: ["std::swap(%s, %s);" % (x, y), cyc(6)]
            return
        to_memory = bool(op & 0x0100)
        ea = dec.ea(mode, reg, size)
        if (to_memory and ea.kind != "m") or ea.kind == "a":
            raise NotCode()
        name = "and" if is_and else "or"
        insn.text = ("%s.%s d%d,%s" % (name, SUFFIX[size], reg2, ea.text) if to_memory
                     else "%s.%s %s,d%d" % (name, SUFFIX[size], ea.text, reg2))
        insn.defs = insn.kills = NZVC
        cycles = opc(size) + ((12 if size == 2 else 8) if to_memory else (6 if size == 2 else 4))

        def emit(live, ea=ea, cycles=cycles):
            lines = ea.pre + ["const uint32_t result = %s %s c.d[%d];" % (ea.load(), "&" if is_and else "|", reg2)]
            if live:
                lines.append(logic("result", size))
            if to_memory:
                lines += ea.store("result")
            elif size == 2:
                lines.append("c.d[%d] = result;" % reg2)
            else:
                lines.append("Set%d(c.d[%d], result);" % (BYTES[size] * 8, reg2))
            return lines + [cyc(cycles)]

        insn.emit = emit
        return

    if group in (9, 0xD):
        is_add = group == 0xD
        name, fn, sign = ("add", "Add", "+") if is_add else ("sub", "Sub", "-")
        if size == 3:
            sz = 2 if op & 0x0100 else 1
            ea = dec.ea(mode, reg, sz)
            insn.text = "%sa.%s %s,a%d" % (name, SUFFIX[sz], ea.text, reg2)
            cycles = opc(sz) + (6 if sz == 2 else 8)
            insn.emit = lambda live: ea.pre + [
                "c.a[%d] %s= %s;" % (reg2, sign, ea.load() if sz == 2 else "S16(%s)" % ea.load()), cyc(cycles)]
            return
        if (op & 0x0130) == 0x0100:
            memory = bool(op & 8)
            if memory:
                src, dst = dec.ea(4, reg, size), dec.ea(4, reg2, size)
            else:
                src, dst = dec.ea(0, reg, size), dec.ea(0, reg2, size)
            insn.text = "%sx.%s %s,%s" % (name, SUFFIX[size], src.text, dst.text)
            insn.uses, insn.defs, insn.kills = X | Z, ALL, X | N | V | C
            cycles = (30 if size == 2 else 18) if memory else (8 if size == 2 else (4 if is_add else 6))
            insn.emit = lambda live: src.pre + dst.pre + [
                "const uint32_t x = %s, y = %s;" % (src.load(), dst.load())] + dst.store(
                    "c.%s(y, x, %d, true, true)" % (fn, size)) + [cyc(cycles)]
            return
        to_memory = bool(op & 0x0100)
        ea = dec.ea(mode, reg, size)
        if (to_memory and ea.kind != "m") or (ea.kind == "a" and size == 0):
            raise NotCode()
        insn.text = ("%s.%s d%d,%s" % (name, SUFFIX[size], reg2, ea.text) if to_memory
                     else "%s.%s %s,d%d" % (name, SUFFIX[size], ea.text, reg2))
        insn.defs = insn.kills = ALL
        cycles = opc(size) + ((12 if size == 2 else 8) if to_memory else (6 if size == 2 else 4))

        def emit(live, ea=ea, cycles=cycles):
            lines = ea.pre + ["const uint32_t value = %s;" % ea.load()]
            if to_memory:
                lines += ea.store("c.%s(value, c.d[%d], %d, false, false)" % (fn, reg2, size) if live
                                  else "value %s c.d[%d]" % (sign, reg2))
            else:
                result = ("c.%s(c.d[%d], value, %d, false, false)" % (fn, reg2, size) if live
                          else "c.d[%d] %s value" % (reg2, sign))
                lines.append("c.d[%d] = %s;" % (reg2, result) if size == 2
                             else "Set%d(c.d[%d], %s);" % (BYTES[size] * 8, reg2, result))
            return lines + [cyc(cycles)]

        insn.emit = emit
        return

    if group == 0xB:
        if size == 3:
            sz = 2 if op & 0x0100 else 1
            ea = dec.ea(mode, reg, sz)
            insn.text = "cmpa.%s %s,a%d" % (SUFFIX[sz], ea.text, reg2)
            insn.defs = insn.kills = NZVC
            cycles = 6 + opc(sz)
            insn.emit = lambda live: ea.pre + [
                "const uint32_t value = %s;" % (ea.load() if sz == 2 else "S16(%s)" % ea.load()),
                "c.Sub(c.a[%d], value, 2, false, false, false);" % reg2 if live else "(void)value;", cyc(cycles)]
            return
        if not op & 0x0100:
            ea = dec.ea(mode, reg, size)
            if ea.kind == "a" and size == 0:
                raise NotCode()
            insn.text = "cmp.%s %s,d%d" % (SUFFIX[size], ea.text, reg2)
            insn.defs = insn.kills = NZVC
            cycles = opc(size) + (6 if size == 2 else 4)
            insn.emit = lambda live: ea.pre + [
                "const uint32_t value = %s;" % ea.load(),
                "c.Sub(c.d[%d], value, %d, false, false, false);" % (reg2, size) if live else "(void)value;", cyc(cycles)]
            return
        if mode == 1:
            src, dst = dec.ea(3, reg, size), dec.ea(3, reg2, size)
            insn.text = "cmpm.%s %s,%s" % (SUFFIX[size], src.text, dst.text)
            insn.defs = insn.kills = NZVC
            # (as the interpreter: the first operand is read before the second's register steps)
            insn.emit = lambda live: src.pre + ["const uint32_t x = %s;" % src.load()] + dst.pre + [
                "const uint32_t y = %s;" % dst.load(),
                "c.Sub(y, x, %d, false, false, false);" % size if live else "(void)x, (void)y;",
                cyc(20 if size == 2 else 12)]
            return
        ea = dec.ea(mode, reg, size)
        if ea.kind not in "dm":
            raise NotCode()
        insn.text = "eor.%s d%d,%s" % (SUFFIX[size], reg2, ea.text)
        insn.defs = insn.kills = NZVC
        cycles = opc(size) + (12 if size == 2 else 8)
        insn.emit = lambda live: ea.pre + [
            "const uint32_t result = %s ^ (c.d[%d] & 0x%Xu);" % (ea.load(), reg2, MASK[size])] + (
                [logic("result", size)] if live else []) + ea.store("result") + [cyc(cycles)]
        return

    if group == 0xE:
        left = bool(op & 0x0100)
        direction = "l" if left else "r"
        if size == 3:
            if reg2 > 3:
                raise NotCode()
            kind = reg2
            ea = dec.ea(mode, reg, 1)
            if ea.kind != "m":
                raise NotCode()
            insn.text = "%s%s.w %s" % (("as", "ls", "rox", "ro")[kind], direction, ea.text)
            insn.uses = X if kind == 2 else 0
            insn.defs = insn.kills = NZVC if kind == 3 else ALL
            cycles = 8 + opc(1)
            insn.emit = lambda live: ea.pre + ea.store(
                "c.Shift(%d, %s, %s, 1, 1)" % (kind, "true" if left else "false", ea.load())) + [cyc(cycles)]
            return
        kind = (op >> 3) & 3
        by_register = bool(op & 0x0020)
        count = "(c.d[%d] & 63)" % reg2 if by_register else str(reg2 or 8)
        insn.text = "%s%s.%s %s,d%d" % (("as", "ls", "rox", "ro")[kind], direction, SUFFIX[size],
                                        "d%d" % reg2 if by_register else "#%d" % (reg2 or 8), reg)
        insn.uses = X if kind == 2 else 0
        insn.defs = NZVC if kind == 3 else ALL
        # (a count of zero from a register leaves X as it was)
        insn.kills = NZVC if (kind == 3 or by_register) else ALL
        base = 8 if size == 2 else 6

        def emit(live, count=count, kind=kind):
            n = reg2 or 8
            d = "c.d[%d]" % reg
            if live or by_register or kind == 2:
                lines = ["const uint32_t count = %s;" % count,
                         "const uint32_t result = c.Shift(%d, %s, %s, count, %d);" % (kind, "true" if left else "false", d, size)]
                total = "%d + 2 * count" % base
            else:
                bits = BYTES[size] * 8
                value = d if size == 2 else "(%s & 0x%X)" % (d, MASK[size])
                if kind == 3:
                    n %= bits
                    expr = value if n == 0 else ("((%s << %d) | (%s >> %d))" % (value, n, value, bits - n) if left
                                                 else "((%s >> %d) | (%s << %d))" % (value, n, value, bits - n))
                elif left:
                    expr = "(%s << %d)" % (value, n)
                elif kind == 1:
                    expr = "(%s >> %d)" % (value, n)
                else:
                    signed = {0: "int8_t", 1: "int16_t", 2: "int32_t"}[size]
                    expr = "static_cast<uint32_t>(static_cast<%s>(%s) >> %d)" % (signed, d, n)
                lines = ["const uint32_t result = %s;" % expr]
                total = str(base + 2 * (reg2 or 8))
            lines.append("%s = result;" % d if size == 2 else "Set%d(%s, result);" % (BYTES[size] * 8, d))
            return lines + [cyc(total)]

        insn.emit = emit
        return

    raise NotCode()
