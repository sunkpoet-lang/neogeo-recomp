// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// What the sound driver's translated code is written in terms of (tools/recompz80.py writes the code).
// The same scheme as for the 68000 (native.h): routines over the Z80's state, entered at the block the PC
// names, left when control leaves them or time is up. The instructions themselves are calls of what the
// interpreter is made of (sound/z80.h), so the two cannot differ in what an instruction does.
#pragma once

#include "../sound/z80.h"

#include <cstddef>
#include <cstdint>

namespace cps2recomp {

struct Z80Entry {
    uint16_t address;
    Z80Fn fn;
};
struct Z80Range {
    uint16_t start, end;
};
struct Z80Code {
    const Z80Entry *entries;
    size_t entryCount;
    const Z80Range *ranges;
    size_t rangeCount;
};
// The sound driver's translated code (generated; an empty set when there is none)
extern const Z80Code kZ80Code;
bool HasZ80Code();

namespace z80native {
    constexpr unsigned kMaxDepth = 32;
    [[noreturn]] void NotHere(Z80 &c, uint16_t routine);
} // namespace z80native

#ifdef CPS2RECOMP_CHECK
// The check build's hooks in the Z80's memory access (see runtime/recomp_z80.cpp)
namespace z80check {
    extern bool side, noting;
    uint8_t Read(Z80 &c, uint16_t address);
    void Write(Z80 &c, uint16_t address, uint8_t value);
    void Note(uint16_t address, uint8_t value);
} // namespace z80check
uint64_t Z80BlocksChecked();
#endif

} // namespace cps2recomp

#define ZBLOCK(label, address)                                                                                         \
    label:                                                                                                             \
    if (c.cycles >= c.budget) {                                                                                        \
        c.pc = address;                                                                                                \
        return;                                                                                                        \
    }
#define ZLEAVE(address)                                                                                                \
    do {                                                                                                               \
        c.pc = address;                                                                                                \
        return;                                                                                                        \
    } while (0)

#ifdef CPS2RECOMP_CHECK
#define ZGOTO(label, address) ZLEAVE(address)
#define ZFALL(address) ZLEAVE(address)
#define ZCALL(fn, address, label, back) ZLEAVE(address)
#else
#define ZGOTO(label, address) goto label
#define ZFALL(address)
#define ZCALL(fn, address, label, back)                                                                                \
    do {                                                                                                               \
        c.pc = address;                                                                                                \
        if (c.depth >= cps2recomp::z80native::kMaxDepth) {                                                             \
            return;                                                                                                    \
        }                                                                                                              \
        c.depth++;                                                                                                     \
        fn(c);                                                                                                         \
        c.depth--;                                                                                                     \
        if (c.pc != back) {                                                                                            \
            return;                                                                                                    \
        }                                                                                                              \
        goto label;                                                                                                    \
    } while (0)
#endif
