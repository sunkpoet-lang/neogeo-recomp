// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Running the Z80: the loop around the sound driver's translated code and the interpreter (Z80::Run).
// The same rules as for the 68000 (recomp.cpp): time and the interrupt are looked at only where a block
// starts, and the interpreter can be run that way too (Mode::Blocks), which is what translated code is
// checked against.
#include "native_z80.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cps2recomp {

namespace {
    struct Tables {
        Z80Fn fn[0x10000] = {};
        uint8_t covered[0x10000] = {};
        Tables() {
            for (size_t n = 0; n < kZ80Code.entryCount; n++) {
                fn[kZ80Code.entries[n].address] = kZ80Code.entries[n].fn;
            }
            for (size_t n = 0; n < kZ80Code.rangeCount; n++) {
                for (unsigned at = kZ80Code.ranges[n].start; at < kZ80Code.ranges[n].end; at++) {
                    covered[at] = 1;
                }
            }
        }
        bool Inside(uint16_t pc) const {
            return covered[pc] && !fn[pc];
        }
    };
    const Tables &GetTables() {
        static const Tables *tables = new Tables;
        return *tables;
    }
} // namespace

bool HasZ80Code() {
    return kZ80Code.entryCount != 0;
}

namespace z80native {
    void NotHere(Z80 &c, uint16_t routine) {
        fprintf(stderr, "translated sound routine %04X entered at %04X, where it has no block\n", routine, c.pc);
        abort();
    }
} // namespace z80native

void Z80::Record(uint16_t address) {
    // (only the fixed part of the ROM is translated: what is banked in above it is data)
    if (recording && address < 0x8000 && !GetTables().fn[address]) {
        newEntries.push_back(address);
        if (newEntries.size() > 100000) {
            std::sort(newEntries.begin(), newEntries.end());
            newEntries.erase(std::unique(newEntries.begin(), newEntries.end()), newEntries.end());
        }
    }
}

void Z80::Run(uint64_t until, Mode runMode) {
    const Tables &tables = GetTables();
    budget = until;
    if (runMode == Mode::Check) {
        RunChecked(until);
        return;
    }
    for (;;) {
        if (cycles >= until) {
            return;
        }
        if (m_afterEI) {
            // The instruction after EI is done before an interrupt can be taken (always by the interpreter,
            // so that this is the same however the rest is run)
            m_afterEI = false;
            m_cycles = 0;
            m_jumped = false;
            refresh++;
            Execute(Fetch(), 0);
            cycles += m_cycles;
            if (m_jumped) {
                Record(pc);
            }
            continue;
        }
        if (const unsigned taken = TakeInterrupt()) {
            cycles += taken;
            Record(pc);
            continue;
        }
        if (halted) {
            cycles = until; // (nothing happens until the interrupt)
            return;
        }
        if (runMode == Mode::Native) {
            if (const Z80Fn fn = tables.fn[pc]) {
                depth = 0;
                fn(*this);
                Record(pc);
                continue;
            }
        }
        do {
            m_cycles = 0;
            m_jumped = false;
            refresh++;
            Execute(Fetch(), 0);
            cycles += m_cycles;
            interpreted++;
            if (m_jumped) {
                Record(pc);
            }
        } while (runMode != Mode::Interpreter && !halted && tables.Inside(pc));
    }
}

#ifndef CPS2RECOMP_CHECK
void Z80::RunChecked(uint64_t until) {
    fprintf(stderr, "this build cannot check translated code (build the check target)\n");
    exit(1);
}
#else

namespace {
    struct Access {
        uint16_t address;
        uint8_t value;
        bool operator==(const Access &) const = default;
    };
    std::vector<Access> g_sideWrites, g_realWrites;
    uint64_t g_checked = 0;
} // namespace

namespace z80check {
    bool side = false, noting = false;
    uint8_t Read(Z80 &c, uint16_t address) {
        for (auto it = g_sideWrites.rbegin(); it != g_sideWrites.rend(); ++it) {
            if (it->address == address && !c.bus.ChangesMapping(address)) {
                return it->value;
            }
        }
        return c.bus.Read(address);
    }
    void Write(Z80 &c, uint16_t address, uint8_t value) {
        g_sideWrites.push_back({address, value});
        if (c.bus.ChangesMapping(address)) {
            c.bus.Write(address, value);
        }
    }
    void Note(uint16_t address, uint8_t value) {
        g_realWrites.push_back({address, value});
    }
} // namespace z80check

uint64_t Z80BlocksChecked() {
    return g_checked;
}

void Z80::RunChecked(uint64_t until) {
    const Tables &tables = GetTables();
    struct Regs {
        uint8_t r[8], f, r2[8], f2;
        uint16_t ix, iy, sp, pc;
        uint8_t i, refresh, mode;
        bool iff1, iff2, halted, afterEI;
        uint64_t cycles;
        bool operator==(const Regs &) const = default;
    };
    auto save = [&]() {
        Regs s = {};
        memcpy(s.r, r, 8);
        memcpy(s.r2, r2, 8);
        s.f = f, s.f2 = f2, s.ix = ix, s.iy = iy, s.sp = sp, s.pc = pc, s.i = i, s.refresh = refresh, s.mode = mode;
        s.iff1 = iff1, s.iff2 = iff2, s.halted = halted, s.afterEI = m_afterEI, s.cycles = cycles;
        return s;
    };
    auto load = [&](const Regs &s) {
        memcpy(r, s.r, 8);
        memcpy(r2, s.r2, 8);
        f = s.f, f2 = s.f2, ix = s.ix, iy = s.iy, sp = s.sp, pc = s.pc, i = s.i, refresh = s.refresh, mode = s.mode;
        iff1 = s.iff1, iff2 = s.iff2, halted = s.halted, m_afterEI = s.afterEI, cycles = s.cycles;
    };
    for (;;) {
        if (cycles >= until) {
            return;
        }
        if (m_afterEI) {
            m_afterEI = false;
            m_cycles = 0;
            refresh++;
            Execute(Fetch(), 0);
            cycles += m_cycles;
            continue;
        }
        if (const unsigned taken = TakeInterrupt()) {
            cycles += taken;
            continue;
        }
        if (halted) {
            cycles = until;
            return;
        }
        const Z80Fn fn = tables.fn[pc];
        const Regs before = save();
        Regs native = {};
        if (fn) {
            g_sideWrites.clear();
            depth = 0;
            const uint32_t mapping = bus.Mapping();
            z80check::side = true;
            fn(*this);
            z80check::side = false;
            bus.SetMapping(mapping);
            native = save();
            load(before);
        }
        g_realWrites.clear();
        z80check::noting = true;
        do {
            m_cycles = 0;
            refresh++;
            Execute(Fetch(), 0);
            cycles += m_cycles;
        } while (!halted && tables.Inside(pc));
        z80check::noting = false;
        if (!fn) {
            continue;
        }
        g_checked++;
        const Regs after = save();
        if (!(native == after) || g_sideWrites != g_realWrites) {
            fprintf(stderr, "translated sound block at %04X differs from the interpreter (after %llu blocks checked)\n", before.pc,
                    static_cast<unsigned long long>(g_checked));
            auto show = [&](const char *name, const Regs &s) {
                fprintf(stderr, "  %-12s pc %04X cycles +%llu af %02X%02X bc %02X%02X de %02X%02X hl %02X%02X ix %04X iy %04X sp %04X r %02X%s%s\n",
                        name, s.pc, static_cast<unsigned long long>(s.cycles - before.cycles), s.r[7], s.f, s.r[0], s.r[1], s.r[2],
                        s.r[3], s.r[4], s.r[5], s.ix, s.iy, s.sp, s.refresh, s.iff1 ? " ei" : " di", s.halted ? " halted" : "");
            };
            show("before", before);
            show("interpreter", after);
            show("translated", native);
            auto list = [](const char *name, const std::vector<Access> &writes) {
                fprintf(stderr, "  %s writes:", name);
                for (const Access &w : writes) {
                    fprintf(stderr, " %04X=%02X", w.address, w.value);
                }
                fprintf(stderr, "\n");
            };
            list("interpreter", g_realWrites);
            list("translated", g_sideWrites);
            exit(3);
        }
    }
}
#endif

} // namespace cps2recomp
