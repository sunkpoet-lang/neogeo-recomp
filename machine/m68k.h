// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The main CPU: a Motorola 68000, which runs the game's program from ROM.
//
// An interpreter: Step runs one instruction (or takes an interrupt) and says how many clock cycles it took.
// Bus and address errors are not modelled. The board deciphers what the CPU reads as program (instruction
// words, and data addressed relative to the PC); everything else it reads is the ROM as it is. So the bus
// has two kinds of read.
#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace cps2recomp {

class Archive;

// What the CPU is connected to: 24-bit addresses, big-endian words
struct M68KBus {
    virtual ~M68KBus() = default;
    virtual uint8_t Read8(uint32_t address) = 0;
    virtual uint16_t Read16(uint32_t address) = 0;
    virtual void Write8(uint32_t address, uint8_t value) = 0;
    virtual void Write16(uint32_t address, uint16_t value) = 0;
    // A word read as program (deciphered)
    virtual uint16_t Fetch16(uint32_t address) = 0;
    // The CPU takes the interrupt of this level
    virtual void InterruptAck(unsigned level) {}
};

class M68K;
// A translated routine (runtime/native.h): runs from the block at cpu.pc until it leaves the routine
using NativeFn = void (*)(M68K &);

class M68K {
public:
    explicit M68K(M68KBus &bus)
        : bus(bus) {}

    // How Run goes about it
    enum class Mode {
        Interpreter, // an instruction at a time
        Blocks,      // the interpreter, stopping only where translated code would (the reference for Native)
        Native,      // translated code; the interpreter for what has no translation
        Check,       // both, a block at a time, compared (the check build only)
    };

    // The reset line: the stack pointer and PC from the first two vectors
    void Reset(bool hard);
    // The level on the interrupt lines (0: none); taken before an instruction when above the mask in SR
    void SetInterruptLevel(uint8_t level) {
        m_interruptLevel = level;
    }
    uint8_t InterruptLevel() const {
        return m_interruptLevel;
    }
    // One instruction or interrupt; the clock cycles it took (not added to `cycles`)
    uint32_t Step();
    // Runs until `cycles` reaches `until` (a little past it: to the end of the instruction or block)
    void Run(uint64_t until, Mode mode);
    // For translated code: the instruction at `address` done by the interpreter (the rare ones)
    void Interpret(uint32_t address);

    uint32_t d[8] = {}, a[8] = {}; // a[7] is the stack pointer in use
    uint32_t otherSP = 0;          // the user or supervisor stack pointer not in use
    uint32_t pc = 0;               // of the next instruction
    bool flagC = false, flagV = false, flagZ = false, flagN = false, flagX = false;
    bool supervisor = true, trace = false;
    uint8_t interruptMask = 7;
    bool stopped = false; // (after STOP: until an interrupt)

    uint64_t cycles = 0; // clock cycles since reset
    uint64_t budget = 0; // translated code stops at the first block that starts at or after this
    // A wait (runtime/lift.h, Wait): the 68000 does nothing for so many cycles when it is next at `waitResume`
    // with the stack at `waitStack`; interrupts are taken meanwhile as they would be. When the wait is over
    // the byte of work RAM at `waitClears` (if not 0) is cleared.
    uint64_t waitCycles = 0;
    uint32_t waitResume = 0, waitStack = 0, waitClears = 0;
    bool waitArmed = false;
    void FinishWait() {
        if (waitClears) {
            ram[waitClears & 0xFFFF] = 0;
        }
        waitCycles = 0;
        waitClears = 0;
        waitArmed = false;
    }
    unsigned depth = 0;  // of translated routines calling one another on the host's stack

    // What translated code reaches without going through the bus (set by the machine)
    M68KBus &bus;
    uint8_t *ram = nullptr;          // FF0000-FFFFFF
    uint8_t *video = nullptr;        // 900000 and up: video RAM (for the check of rewrites that write it)
    uint32_t videoSize = 0;
    const uint8_t *rom = nullptr;    // the program as data
    const uint8_t *opcodes = nullptr; // the program as program
    uint32_t romSize = 0;

    // Addresses reached in ways the translator cannot follow (computed jumps, returns, exceptions) that are
    // not yet the start of a translated block; kept when recording is on
    bool recording = false;
    std::vector<uint32_t> newEntries;
    // (for --stats: instructions the interpreter ran while translated code was meant to, and all it ran)
    uint64_t interpreted = 0;
    // For finding out who writes somewhere (--watch): writes by the interpreter to addresses from watchLow to
    // watchHigh are reported with the instruction's address. Translated code is not watched (run with
    // --cpu blocks, which does the same things).
    uint32_t watchLow = 1, watchHigh = 0;
    uint32_t instruction = 0; // the address of the instruction the interpreter is doing

    uint16_t SR() const {
        return static_cast<uint16_t>(flagC | (flagV << 1) | (flagZ << 2) | (flagN << 3) | (flagX << 4) | (interruptMask << 8) |
                                     (supervisor << 13) | (trace << 15));
    }
    void SetSR(uint16_t value) {
        flagC = value & 1;
        flagV = value & 2;
        flagZ = value & 4;
        flagN = value & 8;
        flagX = value & 16;
        interruptMask = (value >> 8) & 7;
        trace = value & 0x8000;
        const bool super = value & 0x2000;
        if (super != supervisor) {
            std::swap(a[7], otherSP);
            supervisor = super;
        }
    }
    void Serialize(Archive &ar); // save states (runtime/state.h)

    // --- arithmetic and flags, shared by the interpreter and translated code ---
    // Sizes: 0 a byte, 1 a word, 2 a long word

    bool Condition(unsigned cc) const {
        switch (cc) {
        case 0: return true;
        case 1: return false;
        case 2: return !flagC && !flagZ;
        case 3: return flagC || flagZ;
        case 4: return !flagC;
        case 5: return flagC;
        case 6: return !flagZ;
        case 7: return flagZ;
        case 8: return !flagV;
        case 9: return flagV;
        case 10: return !flagN;
        case 11: return flagN;
        case 12: return flagN == flagV;
        case 13: return flagN != flagV;
        case 14: return !flagZ && flagN == flagV;
        default: return flagZ || flagN != flagV;
        }
    }
    uint32_t Add(uint32_t x, uint32_t y, unsigned size, bool withX, bool keepZ) {
        const uint64_t sum = static_cast<uint64_t>(x & kMask[size]) + (y & kMask[size]) + (withX && flagX);
        const uint32_t result = static_cast<uint32_t>(sum) & kMask[size];
        flagC = flagX = (sum >> (kBytes[size] * 8)) & 1;
        flagV = (~(x ^ y) & (x ^ result) & kSign[size]) != 0;
        flagN = result & kSign[size];
        flagZ = keepZ ? (flagZ && result == 0) : result == 0;
        return result;
    }
    // x - y
    uint32_t Sub(uint32_t x, uint32_t y, unsigned size, bool withX, bool keepZ, bool setX = true) {
        const uint64_t difference = static_cast<uint64_t>(x & kMask[size]) - (y & kMask[size]) - (withX && flagX);
        const uint32_t result = static_cast<uint32_t>(difference) & kMask[size];
        flagC = (difference >> (kBytes[size] * 8)) & 1;
        if (setX) {
            flagX = flagC;
        }
        flagV = ((x ^ y) & (x ^ result) & kSign[size]) != 0;
        flagN = result & kSign[size];
        flagZ = keepZ ? (flagZ && result == 0) : result == 0;
        return result;
    }
    void Logic(uint32_t result, unsigned size) {
        flagN = result & kSign[size];
        flagZ = (result & kMask[size]) == 0;
        flagV = flagC = false;
    }
    // kind: 0 arithmetic shift, 1 logical shift, 2 rotate through X, 3 rotate
    uint32_t Shift(unsigned kind, bool left, uint32_t value, unsigned count, unsigned size) {
        const uint32_t sign = kSign[size];
        value &= kMask[size];
        flagV = false;
        flagC = kind == 2 ? flagX : false;
        for (unsigned i = 0; i < count; i++) {
            const bool out = left ? (value & sign) : (value & 1);
            uint32_t in = 0;
            switch (kind) {
            case 0: in = left ? 0 : (value & sign); break;
            case 1: in = 0; break;
            case 2: in = flagX ? (left ? 1 : sign) : 0; break;
            default: in = out ? (left ? 1 : sign) : 0; break;
            }
            const uint32_t before = value;
            value = left ? ((value << 1) & kMask[size]) | in : (value >> 1) | in;
            flagC = out;
            if (kind != 3) {
                flagX = out;
            }
            if (kind == 0 && left && ((before ^ value) & sign)) {
                flagV = true; // (the sign changed at some point)
            }
        }
        flagN = value & sign;
        flagZ = value == 0;
        return value;
    }
    // Decimal arithmetic on two digits a byte: y + x or y - x, with X
    uint32_t Bcd(bool add, uint32_t x32, uint32_t y32) {
        const int x = static_cast<int>(x32 & 0xFF), y = static_cast<int>(y32 & 0xFF);
        int result;
        if (add) {
            const int low = (x & 15) + (y & 15) + flagX;
            result = x + y + flagX;
            if (low > 9) {
                result += 6;
            }
            flagC = flagX = result > 0x99;
            if (flagC) {
                result += 0x60;
            }
        } else {
            const int low = (y & 15) - (x & 15) - flagX;
            result = y - x - flagX;
            if (low < 0) {
                result -= 6;
            }
            flagC = flagX = result < 0;
            if (flagC) {
                result -= 0x60;
            }
        }
        flagZ = flagZ && (result & 0xFF) == 0;
        flagN = result & 0x80;
        return static_cast<uint32_t>(result) & 0xFF;
    }
    void Push32(uint32_t value) {
        a[7] -= 4;
        Write(a[7], 2, value);
    }
    uint32_t Pop32() {
        const uint32_t value = Read(a[7], 2);
        a[7] += 4;
        return value;
    }
    uint32_t Read(uint32_t address, unsigned size);
    void Write(uint32_t address, unsigned size, uint32_t value);
    uint32_t ReadProgram(uint32_t address, unsigned size);

    static constexpr uint32_t kMask[3] = {0xFF, 0xFFFF, 0xFFFFFFFF};
    static constexpr uint32_t kSign[3] = {0x80, 0x8000, 0x80000000};
    static constexpr uint32_t kBytes[3] = {1, 2, 4};

private:
    uint8_t m_interruptLevel = 0;
    uint32_t m_cycles = 0;  // of the instruction being run
    bool m_jumped = false;  // the instruction went somewhere the translator cannot follow

    struct Operand {
        enum class Kind : uint8_t { Data, Address, Memory, Program, Immediate } kind; // Program: memory, read as program
        uint32_t at; // register number, memory address or immediate value
    };
    uint16_t Fetch();
    uint32_t Fetch32();
    Operand Locate(unsigned mode, unsigned reg, unsigned size);
    uint32_t Load(const Operand &operand, unsigned size);
    void Store(const Operand &operand, unsigned size, uint32_t value);
    void Push16(uint16_t value);
    uint16_t Pop16();
    void Exception(unsigned vector, uint32_t returnAddress, int level);
    void Execute(uint16_t op);
    void Illegal(uint16_t op, uint32_t address);
    bool TakeInterrupt();

public:
    // For translated code: the instructions that have to do with exceptions and the status register, done as
    // the interpreter does them (the time they take is added by the caller)
    void Trap(unsigned number, uint32_t returnAddress) {
        pc = returnAddress;
        Exception(32 + number, returnAddress, -1);
    }
    void ReturnFromException() {
        const uint16_t sr = Pop16();
        pc = Pop32();
        SetSR(sr & 0xA71F);
    }
    void MoveToSR(uint16_t value) {
        SetSR(value & 0xA71F);
    }
    // For rewrites (runtime/lift.h): the game's code run on the spot, to the return of the routine the PC is
    // at (by the interpreter alone, hooks left out), or of the routine at `address` (as the game is run)
    // The registers kept and put back (for C++ that borrows the CPU to run a routine of the game's)
    struct Kept {
        uint32_t d[8], a[8];
        uint16_t sr;
    };
    Kept Keep() const {
        Kept kept;
        for (int n = 0; n < 8; n++) {
            kept.d[n] = d[n];
            kept.a[n] = a[n];
        }
        kept.sr = SR();
        return kept;
    }
    void PutBack(const Kept &kept) {
        SetSR(kept.sr);
        for (int n = 0; n < 8; n++) {
            d[n] = kept.d[n];
            a[n] = kept.a[n];
        }
    }
    void RunToReturn();
    void CallGuest(uint32_t address);

private:
    void Record(uint32_t address);
    void RunChecked(uint64_t until);
};

} // namespace cps2recomp
