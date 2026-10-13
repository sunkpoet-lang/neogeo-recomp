// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.

#include "z80.h"

#include "../runtime/native_z80.h"
#include "../runtime/state.h"

#include <utility>

namespace cps2recomp {

namespace {
    enum : uint8_t { kC = 0x01, kN = 0x02, kPV = 0x04, kX = 0x08, kH = 0x10, kY = 0x20, kZ = 0x40, kS = 0x80 };

    // Sign, zero, the two undocumented bits and parity of a byte
    struct FlagTable {
        uint8_t szp[256];
        constexpr FlagTable()
            : szp() {
            for (int v = 0; v < 256; v++) {
                int bits = 0;
                for (int b = 0; b < 8; b++) {
                    bits += (v >> b) & 1;
                }
                szp[v] = static_cast<uint8_t>((v & (kS | kX | kY)) | (v == 0 ? kZ : 0) | ((bits & 1) ? 0 : kPV));
            }
        }
    };
    constexpr FlagTable kFlags;

    // (by opcode, 16 to a row; conditional ones as when the condition fails)
    // clang-format off
    constexpr uint8_t kCycles[256] = {
         4, 10,  7,  6,  4,  4,  7,  4,  4, 11,  7,  6,  4,  4,  7,  4,
         8, 10,  7,  6,  4,  4,  7,  4, 12, 11,  7,  6,  4,  4,  7,  4,
         7, 10, 16,  6,  4,  4,  7,  4,  7, 11, 16,  6,  4,  4,  7,  4,
         7, 10, 13,  6, 11, 11, 10,  4,  7, 11, 13,  6,  4,  4,  7,  4,
         4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
         4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
         4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
         7,  7,  7,  7,  7,  7,  4,  7,  4,  4,  4,  4,  4,  4,  7,  4,
         4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
         4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
         4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
         4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
         5, 10, 10, 10, 10, 11,  7, 11,  5, 10, 10,  0, 10, 17,  7, 11,
         5, 10, 10, 11, 10, 11,  7, 11,  5,  4, 10, 11, 10,  0,  7, 11,
         5, 10, 10, 19, 10, 11,  7, 11,  5,  4, 10,  4, 10,  0,  7, 11,
         5, 10, 10,  4, 10, 11,  7, 11,  5,  6, 10,  4, 10,  0,  7, 11,
    };
    // clang-format on
} // namespace

void Z80::Reset() {
    pc = 0;
    sp = 0xFFFF;
    i = refresh = 0;
    iff1 = iff2 = halted = false;
    mode = 0;
    m_interrupt = false;
    m_afterEI = false;
    depth = 0;
    for (auto &v : r) {
        v = 0xFF;
    }
    f = 0xFF;
}

// --- memory ---

uint8_t Z80::Rd(uint16_t address) {
#ifdef CPS2RECOMP_CHECK
    if (z80check::side) {
        return z80check::Read(*this, address);
    }
#endif
    return bus.Read(address);
}
void Z80::Wr(uint16_t address, uint8_t value) {
#ifdef CPS2RECOMP_CHECK
    if (z80check::side) {
        z80check::Write(*this, address, value);
        return;
    }
    if (z80check::noting) {
        z80check::Note(address, value);
    }
#endif
    bus.Write(address, value);
}
uint8_t Z80::Fetch() {
    return bus.Read(pc++);
}
uint16_t Z80::Fetch16() {
    const uint8_t low = Fetch();
    return static_cast<uint16_t>(low | (Fetch() << 8));
}
uint16_t Z80::Read16(uint16_t address) {
    const uint8_t low = Rd(address);
    return static_cast<uint16_t>(low | (Rd(static_cast<uint16_t>(address + 1)) << 8));
}
void Z80::Write16(uint16_t address, uint16_t value) {
    Wr(address, static_cast<uint8_t>(value));
    Wr(static_cast<uint16_t>(address + 1), static_cast<uint8_t>(value >> 8));
}
void Z80::Push(uint16_t value) {
    sp -= 2;
    Write16(sp, value);
}
uint16_t Z80::Pop() {
    const uint16_t value = Read16(sp);
    sp += 2;
    return value;
}

// --- registers ---

uint8_t Z80::Reg(unsigned n, unsigned prefix) const {
    if (prefix && n == 4) {
        return static_cast<uint8_t>((prefix == 1 ? ix : iy) >> 8);
    }
    if (prefix && n == 5) {
        return static_cast<uint8_t>(prefix == 1 ? ix : iy);
    }
    return r[n];
}
void Z80::SetReg(unsigned n, unsigned prefix, uint8_t v) {
    if (prefix && (n == 4 || n == 5)) {
        uint16_t &index = prefix == 1 ? ix : iy;
        index = n == 4 ? static_cast<uint16_t>((index & 0x00FF) | (v << 8)) : static_cast<uint16_t>((index & 0xFF00) | v);
        return;
    }
    r[n] = v;
}
uint16_t Z80::Pair(unsigned n, unsigned prefix) const {
    switch (n) {
    case 0: return BC();
    case 1: return DE();
    case 2: return Index(prefix);
    default: return sp;
    }
}
void Z80::SetPair(unsigned n, unsigned prefix, uint16_t v) {
    switch (n) {
    case 0: SetBC(v); break;
    case 1: SetDE(v); break;
    case 2: SetIndex(prefix, v); break;
    default: sp = v; break;
    }
}
uint16_t Z80::Operand(unsigned prefix) {
    if (prefix == 0) {
        return HL();
    }
    m_cycles += 8;
    return static_cast<uint16_t>(Index(prefix) + static_cast<int8_t>(Fetch()));
}
bool Z80::Condition(unsigned cc) const {
    switch (cc) {
    case 0: return !(f & kZ);
    case 1: return f & kZ;
    case 2: return !(f & kC);
    case 3: return f & kC;
    case 4: return !(f & kPV);
    case 5: return f & kPV;
    case 6: return !(f & kS);
    default: return f & kS;
    }
}
void Z80::ExchangeAF() {
    std::swap(r[7], r2[7]);
    std::swap(f, f2);
}
void Z80::Exx() {
    for (int n = 0; n < 6; n++) {
        std::swap(r[n], r2[n]);
    }
}

// --- arithmetic ---

void Z80::Alu(unsigned op, uint8_t value) {
    const uint8_t a = r[7];
    switch (op) {
    case 0:
    case 1: {
        const unsigned carry = (op == 1) ? (f & kC) : 0;
        const unsigned sum = a + value + carry;
        const uint8_t result = static_cast<uint8_t>(sum);
        f = static_cast<uint8_t>((result & (kS | kX | kY)) | (result == 0 ? kZ : 0) | ((a ^ value ^ result) & kH) |
                                 (((~(a ^ value) & (a ^ result)) & 0x80) ? kPV : 0) | (sum > 0xFF ? kC : 0));
        r[7] = result;
        break;
    }
    case 2:
    case 3:
    case 7: {
        const unsigned carry = (op == 3) ? (f & kC) : 0;
        const unsigned difference = a - value - carry;
        const uint8_t result = static_cast<uint8_t>(difference);
        f = static_cast<uint8_t>((result & kS) | (result == 0 ? kZ : 0) | ((a ^ value ^ result) & kH) |
                                 ((((a ^ value) & (a ^ result)) & 0x80) ? kPV : 0) | kN | (difference > 0xFF ? kC : 0));
        if (op == 7) {
            f |= value & (kX | kY); // (compare: the undocumented bits come from the operand)
        } else {
            f |= result & (kX | kY);
            r[7] = result;
        }
        break;
    }
    case 4:
        r[7] = a & value;
        f = kFlags.szp[r[7]] | kH;
        break;
    case 5:
        r[7] = a ^ value;
        f = kFlags.szp[r[7]];
        break;
    default:
        r[7] = a | value;
        f = kFlags.szp[r[7]];
        break;
    }
}
uint8_t Z80::Inc(uint8_t v) {
    const uint8_t result = static_cast<uint8_t>(v + 1);
    f = static_cast<uint8_t>((f & kC) | (result & (kS | kX | kY)) | (result == 0 ? kZ : 0) | ((result & 0x0F) == 0 ? kH : 0) |
                             (result == 0x80 ? kPV : 0));
    return result;
}
uint8_t Z80::Dec(uint8_t v) {
    const uint8_t result = static_cast<uint8_t>(v - 1);
    f = static_cast<uint8_t>((f & kC) | kN | (result & (kS | kX | kY)) | (result == 0 ? kZ : 0) |
                             ((result & 0x0F) == 0x0F ? kH : 0) | (result == 0x7F ? kPV : 0));
    return result;
}
uint8_t Z80::Rotate(unsigned op, uint8_t v) {
    uint8_t result, carry;
    switch (op) {
    case 0:
        carry = v >> 7;
        result = static_cast<uint8_t>((v << 1) | carry);
        break;
    case 1:
        carry = v & 1;
        result = static_cast<uint8_t>((v >> 1) | (carry << 7));
        break;
    case 2:
        carry = v >> 7;
        result = static_cast<uint8_t>((v << 1) | (f & kC));
        break;
    case 3:
        carry = v & 1;
        result = static_cast<uint8_t>((v >> 1) | ((f & kC) << 7));
        break;
    case 4:
        carry = v >> 7;
        result = static_cast<uint8_t>(v << 1);
        break;
    case 5:
        carry = v & 1;
        result = static_cast<uint8_t>((v >> 1) | (v & 0x80));
        break;
    case 6:
        carry = v >> 7;
        result = static_cast<uint8_t>((v << 1) | 1);
        break;
    default:
        carry = v & 1;
        result = v >> 1;
        break;
    }
    f = kFlags.szp[result] | carry;
    return result;
}
void Z80::Add16(unsigned prefix, uint16_t value) {
    const uint16_t hl = Index(prefix);
    const unsigned sum = hl + value;
    f = static_cast<uint8_t>((f & (kS | kZ | kPV)) | (((hl ^ value ^ sum) >> 8) & kH) | ((sum >> 16) & kC) |
                             ((sum >> 8) & (kX | kY)));
    SetIndex(prefix, static_cast<uint16_t>(sum));
}
void Z80::AdcHL(uint16_t value) {
    const uint16_t hl = HL();
    const unsigned sum = hl + value + (f & kC);
    const uint16_t result = static_cast<uint16_t>(sum);
    f = static_cast<uint8_t>(((result >> 8) & (kS | kX | kY)) | (result == 0 ? kZ : 0) | (((hl ^ value ^ result) >> 8) & kH) |
                             (((~(hl ^ value) & (hl ^ result)) & 0x8000) ? kPV : 0) | ((sum >> 16) & kC));
    SetHL(result);
}
void Z80::SbcHL(uint16_t value) {
    const uint16_t hl = HL();
    const unsigned difference = hl - value - (f & kC);
    const uint16_t result = static_cast<uint16_t>(difference);
    f = static_cast<uint8_t>(((result >> 8) & (kS | kX | kY)) | (result == 0 ? kZ : 0) | (((hl ^ value ^ result) >> 8) & kH) |
                             ((((hl ^ value) & (hl ^ result)) & 0x8000) ? kPV : 0) | kN | ((difference >> 16) & kC));
    SetHL(result);
}
void Z80::RotateA(unsigned op) {
    uint8_t carry;
    switch (op) {
    case 0:
        carry = r[7] >> 7;
        r[7] = static_cast<uint8_t>((r[7] << 1) | carry);
        break;
    case 1:
        carry = r[7] & 1;
        r[7] = static_cast<uint8_t>((r[7] >> 1) | (carry << 7));
        break;
    case 2:
        carry = r[7] >> 7;
        r[7] = static_cast<uint8_t>((r[7] << 1) | (f & kC));
        break;
    default:
        carry = r[7] & 1;
        r[7] = static_cast<uint8_t>((r[7] >> 1) | ((f & kC) << 7));
        break;
    }
    f = static_cast<uint8_t>((f & (kS | kZ | kPV)) | carry | (r[7] & (kX | kY)));
}
void Z80::Daa() {
    const uint8_t a = r[7];
    uint8_t adjust = 0, carry = f & kC;
    if ((f & kH) || (a & 0x0F) > 9) {
        adjust |= 0x06;
    }
    if (carry || a > 0x99) {
        adjust |= 0x60;
        carry = kC;
    }
    const uint8_t result = (f & kN) ? static_cast<uint8_t>(a - adjust) : static_cast<uint8_t>(a + adjust);
    f = static_cast<uint8_t>(kFlags.szp[result] | carry | (f & kN) | ((a ^ result) & kH));
    r[7] = result;
}
void Z80::Cpl() {
    r[7] = ~r[7];
    f = static_cast<uint8_t>((f & (kS | kZ | kPV | kC)) | kH | kN | (r[7] & (kX | kY)));
}
void Z80::Scf() {
    f = static_cast<uint8_t>((f & (kS | kZ | kPV)) | kC | (r[7] & (kX | kY)));
}
void Z80::Ccf() {
    f = static_cast<uint8_t>(((f & (kS | kZ | kPV | kC)) | ((f & kC) ? kH : 0) | (r[7] & (kX | kY))) ^ kC);
}
void Z80::Bit(unsigned bit, uint8_t value, uint8_t undocumented) {
    f = static_cast<uint8_t>((f & kC) | kH | (kFlags.szp[value & (1u << bit)] & ~(kX | kY)) | (undocumented & (kX | kY)));
}
void Z80::Neg() {
    const uint8_t a = r[7];
    r[7] = 0;
    Alu(2, a);
}
void Z80::LoadAFromIR(uint8_t value) {
    r[7] = value;
    f = static_cast<uint8_t>((f & kC) | (kFlags.szp[r[7]] & ~kPV) | (iff2 ? kPV : 0));
}
void Z80::Rrd() {
    const uint8_t m = Rd(HL());
    Wr(HL(), static_cast<uint8_t>((r[7] << 4) | (m >> 4)));
    r[7] = static_cast<uint8_t>((r[7] & 0xF0) | (m & 0x0F));
    f = static_cast<uint8_t>((f & kC) | kFlags.szp[r[7]]);
}
void Z80::Rld() {
    const uint8_t m = Rd(HL());
    Wr(HL(), static_cast<uint8_t>((m << 4) | (r[7] & 0x0F)));
    r[7] = static_cast<uint8_t>((r[7] & 0xF0) | (m >> 4));
    f = static_cast<uint8_t>((f & kC) | kFlags.szp[r[7]]);
}
bool Z80::BlockLoad(int step, bool repeat) {
    const uint8_t value = Rd(HL());
    Wr(DE(), value);
    SetHL(static_cast<uint16_t>(HL() + step));
    SetDE(static_cast<uint16_t>(DE() + step));
    SetBC(static_cast<uint16_t>(BC() - 1));
    const uint8_t n = static_cast<uint8_t>(value + r[7]);
    f = static_cast<uint8_t>((f & (kS | kZ | kC)) | (BC() ? kPV : 0) | (n & kX) | ((n & 0x02) ? kY : 0));
    return repeat && BC();
}
bool Z80::BlockCompare(int step, bool repeat) {
    const uint8_t value = Rd(HL());
    const uint8_t result = static_cast<uint8_t>(r[7] - value);
    const uint8_t half = (r[7] ^ value ^ result) & kH;
    SetHL(static_cast<uint16_t>(HL() + step));
    SetBC(static_cast<uint16_t>(BC() - 1));
    const uint8_t n = static_cast<uint8_t>(result - (half ? 1 : 0));
    f = static_cast<uint8_t>((f & kC) | kN | (result & kS) | (result == 0 ? kZ : 0) | half | (BC() ? kPV : 0) | (n & kX) |
                             ((n & 0x02) ? kY : 0));
    return repeat && BC() && result != 0;
}

// --- running ---

// Takes the interrupt if the line is up and interrupts are on; the cycles that took (0: none taken)
unsigned Z80::TakeInterrupt() {
    if (!(m_interrupt && iff1 && !m_afterEI)) {
        return 0;
    }
    m_interrupt = false;
    halted = false;
    iff1 = iff2 = false;
    refresh++;
    Push(pc);
    if (mode == 2) {
        pc = Read16(static_cast<uint16_t>((i << 8) | 0xFF));
        return 19;
    }
    pc = 0x0038; // (modes 0 and 1: nothing drives the bus, which reads as RST 38h)
    return 13;
}

unsigned Z80::Step() {
    m_cycles = 0;
    if (const unsigned taken = TakeInterrupt()) {
        return taken;
    }
    m_afterEI = false;
    if (halted) {
        return 4;
    }
    refresh++;
    Execute(Fetch(), 0);
    return m_cycles;
}

void Z80::Interpret(uint16_t address) {
    pc = address;
    m_cycles = 0;
    refresh++;
    Execute(Fetch(), 0);
    cycles += m_cycles;
}

void Z80::Execute(uint8_t op, unsigned prefix) {
    m_cycles += kCycles[op];
    const unsigned x = op >> 6, y = (op >> 3) & 7, z = op & 7;

    if (x == 1) {
        if (op == 0x76) {
            halted = true;
            return;
        }
        // ld r,r'  (with (HL) on one side the other is never an index half)
        if (z == 6) {
            const uint16_t address = Operand(prefix);
            r[y] = Rd(address);
        } else if (y == 6) {
            const uint16_t address = Operand(prefix);
            Wr(address, r[z]);
        } else {
            SetReg(y, prefix, Reg(z, prefix));
        }
        return;
    }
    if (x == 2) {
        Alu(y, z == 6 ? Rd(Operand(prefix)) : Reg(z, prefix));
        return;
    }
    if (x == 0) {
        switch (z) {
        case 0:
            switch (y) {
            case 0: break; // nop
            case 1: ExchangeAF(); break;
            case 2: { // djnz
                const int8_t d = static_cast<int8_t>(Fetch());
                if (--r[0]) {
                    pc = static_cast<uint16_t>(pc + d);
                    m_cycles += 5;
                }
                break;
            }
            case 3: {
                const int8_t d = static_cast<int8_t>(Fetch());
                pc = static_cast<uint16_t>(pc + d);
                break;
            }
            default: {
                const int8_t d = static_cast<int8_t>(Fetch());
                if (Condition(y - 4)) {
                    pc = static_cast<uint16_t>(pc + d);
                    m_cycles += 5;
                }
                break;
            }
            }
            return;
        case 1:
            if (y & 1) {
                Add16(prefix, Pair(y >> 1, prefix));
            } else {
                SetPair(y >> 1, prefix, Fetch16());
            }
            return;
        case 2:
            switch (y) {
            case 0: Wr(BC(), r[7]); break;
            case 1: r[7] = Rd(BC()); break;
            case 2: Wr(DE(), r[7]); break;
            case 3: r[7] = Rd(DE()); break;
            case 4: Write16(Fetch16(), Index(prefix)); break;
            case 5: SetIndex(prefix, Read16(Fetch16())); break;
            case 6: Wr(Fetch16(), r[7]); break;
            default: r[7] = Rd(Fetch16()); break;
            }
            return;
        case 3: {
            const uint16_t value = Pair(y >> 1, prefix);
            SetPair(y >> 1, prefix, static_cast<uint16_t>((y & 1) ? value - 1 : value + 1));
            return;
        }
        case 4:
            if (y == 6) {
                const uint16_t address = Operand(prefix);
                Wr(address, Inc(Rd(address)));
            } else {
                SetReg(y, prefix, Inc(Reg(y, prefix)));
            }
            return;
        case 5:
            if (y == 6) {
                const uint16_t address = Operand(prefix);
                Wr(address, Dec(Rd(address)));
            } else {
                SetReg(y, prefix, Dec(Reg(y, prefix)));
            }
            return;
        case 6:
            if (y == 6) {
                const uint16_t address = Operand(prefix);
                Wr(address, Fetch());
                if (prefix) {
                    m_cycles -= 3; // (the displacement and the value are fetched together)
                }
            } else {
                SetReg(y, prefix, Fetch());
            }
            return;
        default:
            switch (y) {
            case 0:
            case 1:
            case 2:
            case 3: RotateA(y); break;
            case 4: Daa(); break;
            case 5: Cpl(); break;
            case 6: Scf(); break;
            default: Ccf(); break;
            }
            return;
        }
    }

    // x == 3
    switch (z) {
    case 0:
        if (Condition(y)) {
            pc = Pop();
            m_cycles += 6;
            m_jumped = true;
        }
        return;
    case 1:
        if (!(y & 1)) {
            const uint16_t value = Pop();
            if (y == 6) {
                SetAF(value);
            } else {
                SetPair(y >> 1, prefix, value);
            }
            return;
        }
        switch (y >> 1) {
        case 0:
            pc = Pop();
            m_jumped = true;
            break;
        case 1: Exx(); break;
        case 2:
            pc = Index(prefix);
            m_jumped = true;
            break;
        default: sp = Index(prefix); break;
        }
        return;
    case 2: {
        const uint16_t target = Fetch16();
        if (Condition(y)) {
            pc = target;
        }
        return;
    }
    case 3:
        switch (y) {
        case 0: pc = Fetch16(); break;
        case 1: ExecuteCB(prefix); break;
        case 2: bus.Out(static_cast<uint16_t>((r[7] << 8) | Fetch()), r[7]); break;
        case 3: r[7] = bus.In(static_cast<uint16_t>((r[7] << 8) | Fetch())); break;
        case 4: { // ex (sp),hl
            const uint16_t value = Read16(sp);
            Write16(sp, Index(prefix));
            SetIndex(prefix, value);
            break;
        }
        case 5: { // ex de,hl (never the index registers)
            const uint16_t de = DE();
            SetDE(HL());
            SetHL(de);
            break;
        }
        case 6: iff1 = iff2 = false; break;
        default: EnableInterrupts(); break;
        }
        return;
    case 4: {
        const uint16_t target = Fetch16();
        if (Condition(y)) {
            Push(pc);
            pc = target;
            m_cycles += 7;
        }
        return;
    }
    case 5:
        if (!(y & 1)) {
            Push(y == 6 ? AF() : Pair(y >> 1, prefix));
            return;
        }
        switch (y >> 1) {
        case 0: {
            const uint16_t target = Fetch16();
            Push(pc);
            pc = target;
            break;
        }
        case 1:
            refresh++;
            m_cycles += 4;
            Execute(Fetch(), 1);
            break;
        case 2: ExecuteED(); break;
        default:
            refresh++;
            m_cycles += 4;
            Execute(Fetch(), 2);
            break;
        }
        return;
    case 6: Alu(y, Fetch()); return;
    default:
        Push(pc);
        pc = static_cast<uint16_t>(y * 8);
        return;
    }
}

void Z80::ExecuteCB(unsigned prefix) {
    // With an index prefix the displacement comes before the operation, and the operand is always memory
    uint16_t address = 0;
    if (prefix) {
        address = static_cast<uint16_t>(Index(prefix) + static_cast<int8_t>(Fetch()));
    }
    const uint8_t op = Fetch();
    refresh++;
    const unsigned x = op >> 6, y = (op >> 3) & 7, z = op & 7;
    const bool memory = prefix || z == 6;
    if (!prefix && z == 6) {
        address = HL();
    }
    uint8_t value = memory ? Rd(address) : r[z];
    m_cycles += prefix ? (x == 1 ? 16 : 19) : (z == 6 ? (x == 1 ? 12 : 15) : 8);
    switch (x) {
    case 0: value = Rotate(y, value); break;
    case 1: Bit(y, value, memory ? static_cast<uint8_t>(address >> 8) : value); return;
    case 2: value &= static_cast<uint8_t>(~(1u << y)); break;
    default: value |= static_cast<uint8_t>(1u << y); break;
    }
    if (memory) {
        Wr(address, value);
        if (prefix && z != 6) {
            r[z] = value; // (the undocumented copy to a register)
        }
    } else {
        r[z] = value;
    }
}

void Z80::ExecuteED() {
    const uint8_t op = Fetch();
    refresh++;
    const unsigned x = op >> 6, y = (op >> 3) & 7, z = op & 7;
    m_cycles += 8;
    if (x == 1) {
        switch (z) {
        case 0: { // in r,(c)
            const uint8_t value = bus.In(BC());
            if (y != 6) {
                r[y] = value;
            }
            f = static_cast<uint8_t>((f & kC) | kFlags.szp[value]);
            m_cycles += 4;
            return;
        }
        case 1:
            bus.Out(BC(), y == 6 ? 0 : r[y]);
            m_cycles += 4;
            return;
        case 2:
            if (y & 1) {
                AdcHL(Pair(y >> 1, 0));
            } else {
                SbcHL(Pair(y >> 1, 0));
            }
            m_cycles += 7;
            return;
        case 3:
            if (y & 1) {
                SetPair(y >> 1, 0, Read16(Fetch16()));
            } else {
                Write16(Fetch16(), Pair(y >> 1, 0));
            }
            m_cycles += 12;
            return;
        case 4: Neg(); return;
        case 5: // retn, reti
            pc = Pop();
            iff1 = iff2;
            m_cycles += 6;
            m_jumped = true;
            return;
        case 6: {
            static constexpr uint8_t kModes[8] = {0, 0, 1, 2, 0, 0, 1, 2};
            mode = kModes[y];
            return;
        }
        default:
            switch (y) {
            case 0:
                i = r[7];
                m_cycles += 1;
                break;
            case 1:
                refresh = r[7];
                m_cycles += 1;
                break;
            case 2:
            case 3:
                LoadAFromIR(y == 2 ? i : refresh);
                m_cycles += 1;
                break;
            case 4:
                Rrd();
                m_cycles += 10;
                break;
            case 5:
                Rld();
                m_cycles += 10;
                break;
            default: break;
            }
            return;
        }
    }
    if (x == 2 && y >= 4 && z < 4) {
        // Block instructions: y 4 increasing, 5 decreasing, 6 and 7 the same repeated
        const int step = (y & 1) ? -1 : 1;
        const bool repeat = y >= 6;
        m_cycles += 8;
        switch (z) {
        case 0: // ldi ldd ldir lddr
            if (BlockLoad(step, repeat)) {
                pc -= 2;
                m_cycles += 5;
            }
            return;
        case 1: // cpi cpd cpir cpdr
            if (BlockCompare(step, repeat)) {
                pc -= 2;
                m_cycles += 5;
            }
            return;
        case 2: { // ini ind inir indr
            Wr(HL(), bus.In(BC()));
            SetHL(static_cast<uint16_t>(HL() + step));
            r[0]--;
            f = static_cast<uint8_t>((f & kC) | kN | (r[0] == 0 ? kZ : 0));
            if (repeat && r[0]) {
                pc -= 2;
                m_cycles += 5;
            }
            return;
        }
        default: { // outi outd otir otdr
            const uint8_t value = Rd(HL());
            r[0]--;
            bus.Out(BC(), value);
            SetHL(static_cast<uint16_t>(HL() + step));
            f = static_cast<uint8_t>((f & kC) | kN | (r[0] == 0 ? kZ : 0));
            if (repeat && r[0]) {
                pc -= 2;
                m_cycles += 5;
            }
            return;
        }
        }
    }
    // (anything else does nothing)
}

void Z80::Serialize(Archive &ar) {
    ar.Mark(0x5A383020);
    ar.Pod(r);
    ar.Pod(f);
    ar.Pod(r2);
    ar.Pod(f2);
    ar.Pod(ix);
    ar.Pod(iy);
    ar.Pod(sp);
    ar.Pod(pc);
    ar.Pod(i);
    ar.Pod(refresh);
    ar.Pod(iff1);
    ar.Pod(iff2);
    ar.Pod(halted);
    ar.Pod(mode);
    ar.Pod(m_interrupt);
    ar.Pod(m_afterEI);
    ar.Pod(cycles);
}

} // namespace cps2recomp
