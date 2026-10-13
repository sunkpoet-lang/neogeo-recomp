// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The sound CPU: a Zilog Z80, which runs the game's sound driver (the QSound program ROM).
//
// An interpreter: Step runs one instruction (or takes the interrupt) and says how many clock cycles it took.
// The undocumented halves of IX and IY and the undocumented flag bits are there; the undocumented block
// I/O flags are not (the board has nothing on the I/O ports).
#pragma once

#include <cstdint>
#include <vector>

namespace cps2recomp {

class Archive;

struct Z80Bus {
    virtual ~Z80Bus() = default;
    virtual uint8_t Read(uint16_t address) = 0;
    virtual void Write(uint16_t address, uint8_t value) = 0;
    virtual uint8_t In(uint16_t port) {
        return 0xFF;
    }
    virtual void Out(uint16_t port, uint8_t value) {}
    // For the check of translated code, which runs a block twice: a write that changes what reads give
    // afterwards (a ROM bank register) has to take effect in the trial run too, and be put back after it
    virtual bool ChangesMapping(uint16_t address) {
        return false;
    }
    virtual uint32_t Mapping() {
        return 0;
    }
    virtual void SetMapping(uint32_t mapping) {}
};

class Z80;
// A translated routine of the sound driver (runtime/native_z80.h)
using Z80Fn = void (*)(Z80 &);

class Z80 {
public:
    explicit Z80(Z80Bus &bus)
        : bus(bus) {}

    // How Run goes about it (as for the 68000: machine/m68k.h)
    enum class Mode { Interpreter, Blocks, Native, Check };

    void Reset();
    // The interrupt line; held until the CPU takes the interrupt
    void RaiseInterrupt() {
        m_interrupt = true;
    }
    // One instruction or interrupt; the clock cycles it took (not added to `cycles`)
    unsigned Step();
    // Runs until `cycles` reaches `until` (a little past it: to the end of the instruction or block)
    void Run(uint64_t until, Mode mode);
    // For translated code: the instruction at `address` done by the interpreter (the rare ones)
    void Interpret(uint16_t address);
    void Serialize(Archive &ar);

    // Registers: b c d e h l (unused) a, in the order of the instruction encoding
    uint8_t r[8] = {};
    uint8_t f = 0;
    uint8_t r2[8] = {}; // the alternate set
    uint8_t f2 = 0;
    uint16_t ix = 0, iy = 0, sp = 0, pc = 0;
    uint8_t i = 0, refresh = 0;
    bool iff1 = false, iff2 = false, halted = false;
    uint8_t mode = 0;

    uint64_t cycles = 0; // clock cycles since reset
    uint64_t budget = 0; // translated code stops at the first block that starts at or after this
    unsigned depth = 0;  // of translated routines calling one another on the host's stack
    Z80Bus &bus;

    // Addresses reached in ways the translator cannot follow, kept when recording is on
    bool recording = false;
    std::vector<uint16_t> newEntries;
    uint64_t interpreted = 0; // (for --stats)

    // --- what instructions are made of, shared by the interpreter and translated code ---

    uint8_t Rd(uint16_t address);
    void Wr(uint16_t address, uint8_t value);
    uint16_t Read16(uint16_t address);
    void Write16(uint16_t address, uint16_t value);
    void Push(uint16_t value);
    uint16_t Pop();
    uint16_t BC() const {
        return static_cast<uint16_t>((r[0] << 8) | r[1]);
    }
    uint16_t DE() const {
        return static_cast<uint16_t>((r[2] << 8) | r[3]);
    }
    uint16_t HL() const {
        return static_cast<uint16_t>((r[4] << 8) | r[5]);
    }
    uint16_t AF() const {
        return static_cast<uint16_t>((r[7] << 8) | f);
    }
    void SetBC(uint16_t v) {
        r[0] = v >> 8;
        r[1] = static_cast<uint8_t>(v);
    }
    void SetDE(uint16_t v) {
        r[2] = v >> 8;
        r[3] = static_cast<uint8_t>(v);
    }
    void SetHL(uint16_t v) {
        r[4] = v >> 8;
        r[5] = static_cast<uint8_t>(v);
    }
    void SetAF(uint16_t v) {
        r[7] = v >> 8;
        f = static_cast<uint8_t>(v);
    }
    // With an index prefix (1: IX, 2: IY) the HL of an instruction is that register
    uint16_t Index(unsigned prefix) const {
        return prefix == 0 ? HL() : prefix == 1 ? ix : iy;
    }
    void SetIndex(unsigned prefix, uint16_t v) {
        if (prefix == 0) {
            SetHL(v);
        } else if (prefix == 1) {
            ix = v;
        } else {
            iy = v;
        }
    }
    uint8_t Reg(unsigned n, unsigned prefix) const;
    void SetReg(unsigned n, unsigned prefix, uint8_t v);
    uint16_t Pair(unsigned n, unsigned prefix) const; // bc de hl sp
    void SetPair(unsigned n, unsigned prefix, uint16_t v);
    bool Condition(unsigned cc) const;
    void Alu(unsigned op, uint8_t value); // add adc sub sbc and xor or cp, on A
    uint8_t Inc(uint8_t v);
    uint8_t Dec(uint8_t v);
    uint8_t Rotate(unsigned op, uint8_t v); // rlc rrc rl rr sla sra sll srl
    void Add16(unsigned prefix, uint16_t value);
    void AdcHL(uint16_t value);
    void SbcHL(uint16_t value);
    void RotateA(unsigned op); // rlca rrca rla rra
    void Daa();
    void Cpl();
    void Scf();
    void Ccf();
    void Bit(unsigned bit, uint8_t value, uint8_t undocumented);
    void Neg();
    void LoadAFromIR(uint8_t value); // ld a,i / ld a,r
    void Rrd();
    void Rld();
    // One step of ldi/ldd (step 1 / -1), cpi/cpd; true if a repeating one (ldir, cpir...) goes round again
    bool BlockLoad(int step, bool repeat);
    bool BlockCompare(int step, bool repeat);
    void ExchangeAF();
    void Exx();
    void EnableInterrupts() {
        iff1 = iff2 = true;
        m_afterEI = true;
    }

private:
    bool m_interrupt = false;
    bool m_afterEI = false; // (interrupts are taken one instruction after EI)
    unsigned m_cycles = 0;
    bool m_jumped = false;  // the instruction went somewhere the translator cannot follow

    uint8_t Fetch();
    uint16_t Fetch16();
    uint16_t Operand(unsigned prefix); // the address of (HL), or of (IX+d) / (IY+d)
    unsigned TakeInterrupt();
    void Record(uint16_t address);
    void RunChecked(uint64_t until);
    void Execute(uint8_t op, unsigned prefix);
    void ExecuteCB(unsigned prefix);
    void ExecuteED();
};

} // namespace cps2recomp
