// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Running the 68000: the loop around translated code and the interpreter (M68K::Run).
//
// Time and interrupts are looked at only where a block starts (or at any instruction that has no
// translation). The interpreter can be run the same way (Mode::Blocks), and then does exactly what the
// translated code does, cycle for cycle: that is what translated code is checked against.
#include "native.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace cps2recomp {

namespace {
    // By address / 2: the routine a block starting there is in; whether a block starts there; whether the
    // instruction there is inside a translated block
    struct Tables {
        std::vector<NativeFn> fn;
        std::vector<uint8_t> covered;
        uint32_t limit = 0; // addresses below this are in the tables
        Tables() {
            for (size_t n = 0; n < kNativeCode.rangeCount; n++) {
                limit = std::max(limit, kNativeCode.ranges[n].end);
            }
            for (size_t n = 0; n < kNativeCode.entryCount; n++) {
                limit = std::max(limit, kNativeCode.entries[n].address + 2);
            }
            fn.assign(limit / 2 + 1, nullptr);
            covered.assign(limit / 2 + 1, 0);
            for (size_t n = 0; n < kNativeCode.entryCount; n++) {
                fn[kNativeCode.entries[n].address / 2] = kNativeCode.entries[n].fn;
            }
            for (size_t n = 0; n < kNativeCode.rangeCount; n++) {
                for (uint32_t at = kNativeCode.ranges[n].start; at < kNativeCode.ranges[n].end; at += 2) {
                    covered[at / 2] = 1;
                }
            }
        }
        NativeFn At(uint32_t pc) const {
            return (pc < limit && !(pc & 1)) ? fn[pc / 2] : nullptr;
        }
        // Inside a block, not at its start: time is not looked at here
        bool Inside(uint32_t pc) const {
            return pc < limit && !(pc & 1) && covered[pc / 2] && !fn[pc / 2];
        }
    };
    const Tables &GetTables() {
        static const Tables tables;
        return tables;
    }
} // namespace

namespace native {
    void NotHere(M68K &c, uint32_t routine) {
        fprintf(stderr, "translated routine %06X entered at %06X, where it has no block\n", routine, c.pc);
        abort();
    }
} // namespace native

// --- hooks (lift.h) ---

namespace {
    struct Hook {
        uint32_t address;
        const char *name;
        HookFn fn;
        RewriteFn rewrite = nullptr;
        RewriteWhenFn when = nullptr; // (a rewrite) asked before each call: false, and the game's own code runs
        uint32_t free = 0;
        uint64_t calls = 0, differed = 0;
        // Time: what a call is charged (the 68000's cycles), and how the original's time compared in the check
        uint32_t cycles = 0;
        int64_t lateMin = INT64_MAX, lateMax = INT64_MIN, lateSum = 0;
    };
    bool g_checkRewrites = false, g_noRewrites = false;
    constexpr uint32_t kRewriteCycles = 100; // what a rewrite that names no time of its own is charged
    std::vector<std::pair<uint32_t, uint32_t>> g_scratch; // work RAM the check does not compare (offsets)
    int g_inOriginal = 0; // the original of a rewrite is being run for the check: no rewrites, no hooks
    int g_inRewrite = 0;  // a rewrite is being run for the check: the rewrites it reaches are not checked again
    std::vector<Hook> &Hooks() {
        static std::vector<Hook> hooks;
        return hooks;
    }
    std::vector<uint8_t> g_hooked; // by address / 2: the hook's number + 1
    bool g_hooksChecked = false;

    // Translated code only stops by at the hooks the translator knew of
    void CheckHooks() {
        g_hooksChecked = true;
        for (const Hook &hook : Hooks()) {
            bool known = false;
            for (size_t n = 0; n < kNativeCode.hookCount; n++) {
                known = known || kNativeCode.hooks[n] == hook.address;
            }
            if (!known && GetTables().At(hook.address)) {
                fprintf(stderr, "the hook %s at %06X was added after the game's code was translated: run make again\n", hook.name,
                        hook.address);
                exit(1);
            }
            if (!known && GetTables().Inside(hook.address)) {
                fprintf(stderr, "the hook %s is at %06X, which is not where a block starts\n", hook.name, hook.address);
                exit(1);
            }
        }
    }
} // namespace

void AddHook(uint32_t address, const char *name, HookFn fn) {
    Hooks().push_back({address, name, fn});
    if (g_hooked.size() <= address / 2) {
        g_hooked.resize(address / 2 + 1, 0);
    }
    g_hooked[address / 2] = static_cast<uint8_t>(Hooks().size());
}

bool HasHook(uint32_t address) {
    return address / 2 < g_hooked.size() && !(address & 1) && g_hooked[address / 2];
}

namespace {
    // The check of a rewrite: the rewrite, then (everything put back) the original, and the two compared.
    // The game goes on from what the original did.
    void CheckRewrite(M68K &c, Hook &hook) {
        struct Regs {
            uint32_t d[8], a[8], otherSP, pc;
            uint16_t sr;
            uint64_t cycles;
        };
        auto save = [&]() {
            Regs s;
            memcpy(s.d, c.d, sizeof(s.d));
            memcpy(s.a, c.a, sizeof(s.a));
            s.otherSP = c.otherSP, s.pc = c.pc, s.sr = c.SR(), s.cycles = c.cycles;
            return s;
        };
        auto load = [&](const Regs &s) {
            c.SetSR(s.sr);
            memcpy(c.d, s.d, sizeof(s.d));
            memcpy(c.a, s.a, sizeof(s.a));
            c.otherSP = s.otherSP, c.pc = s.pc, c.cycles = s.cycles;
        };
        static std::vector<uint8_t> ramBefore(0x10000), ramRewrite(0x10000);
        // (translated code that arrives here by a jump has not brought the PC up to date: the original is to
        // be run from the routine's start, not from the start of the block that jumped)
        c.pc = hook.address;
        const Regs before = save();
        memcpy(ramBefore.data(), c.ram, 0x10000);
        static std::vector<uint8_t> videoBefore, videoRewrite;
        const bool video = (hook.free & kChecksVideo) && c.video;
        if (video) {
            videoBefore.assign(c.video, c.video + c.videoSize);
        }
        g_inRewrite++;
        hook.rewrite(c);
        c.cycles += hook.cycles ? hook.cycles : kRewriteCycles;
        Return(c);
        if (c.waitCycles) {
            c.cycles += c.waitCycles; // (for the comparison the wait is over: the original has done its waiting)
            c.FinishWait();
        }
        g_inRewrite--;
        const Regs mine = save();
        memcpy(ramRewrite.data(), c.ram, 0x10000);
        load(before);
        memcpy(c.ram, ramBefore.data(), 0x10000);
        if (video) {
            videoRewrite.assign(c.video, c.video + c.videoSize);
            memcpy(c.video, videoBefore.data(), c.videoSize);
        }
        g_inOriginal++;
        c.RunToReturn();
        g_inOriginal--;
        const Regs its = save();

        std::string report;
        char text[160];
        for (int n = 0; n < 8; n++) {
            if (mine.d[n] != its.d[n] && !(hook.free & (1u << n))) {
                snprintf(text, sizeof(text), " d%d %08X (the game's: %08X)", n, mine.d[n], its.d[n]);
                report += text;
            }
            if (mine.a[n] != its.a[n] && !(n < 7 && (hook.free & (0x100u << n)))) {
                snprintf(text, sizeof(text), " a%d %08X (the game's: %08X)", n, mine.a[n], its.a[n]);
                report += text;
            }
        }
        if (mine.pc != its.pc) {
            snprintf(text, sizeof(text), " pc %06X (the game's: %06X)", mine.pc, its.pc);
            report += text;
        }
        if (!(hook.free & kFreeFlags) && ((mine.sr ^ its.sr) & 0x1F)) {
            snprintf(text, sizeof(text), " flags %02X (the game's: %02X)", mine.sr & 0x1F, its.sr & 0x1F);
            report += text;
        }
        // (below the stack pointer is what the original's own calls left behind: not compared)
        const uint32_t sp = its.a[7] & 0xFFFF, scratch = sp >= 0x200 ? sp - 0x200 : 0;
        int listed = 0;
        for (uint32_t at = 0; at < 0x10000; at++) {
            if (c.ram[at] != ramRewrite[at] && !(at >= scratch && at < sp)) {
                bool spare = false;
                for (const auto &[low, high] : g_scratch) {
                    spare |= at >= low && at < high;
                }
                if (spare) {
                    continue;
                }
                if (listed++ < 8) {
                    snprintf(text, sizeof(text), " [%06X] %02X (the game's: %02X, was %02X)", 0xFF0000 + at, ramRewrite[at],
                             c.ram[at], ramBefore[at]);
                    report += text;
                }
            }
        }
        for (uint32_t at = 0; video && at < c.videoSize; at++) {
            if (c.video[at] != videoRewrite[at] && listed++ < 8) {
                snprintf(text, sizeof(text), " [%06X] %02X (the game's: %02X, was %02X)", 0x900000 + at, videoRewrite[at], c.video[at],
                         videoBefore[at]);
                report += text;
            }
        }
        hook.calls++;
        {
            const int64_t late = static_cast<int64_t>(its.cycles) - static_cast<int64_t>(mine.cycles);
            hook.lateMin = std::min(hook.lateMin, late);
            hook.lateMax = std::max(hook.lateMax, late);
            hook.lateSum += late;
        }
        if (!report.empty()) {
            if (hook.differed++ < 5) {
                fprintf(stderr, "rewrite %s (%06X), call %llu, differs:%s%s\n", hook.name, hook.address,
                        static_cast<unsigned long long>(hook.calls), report.c_str(), listed > 8 ? " ..." : "");
            }
        }
    }
} // namespace

void Wait(M68K &c, uint64_t cycles, uint32_t busy) {
    c.waitCycles = cycles;
    c.waitClears = busy;
    if (busy) {
        c.ram[busy & 0xFFFF] = 1;
    }
}

namespace {
    // A rewrite has asked for a wait and has returned: the wait is where the routine returned to. (Translated
    // code is made to leave for the run loop, which is where waiting is done.)
    void ArmWait(M68K &c) {
        if (!c.waitCycles || c.waitArmed) {
            return;
        }
#ifdef CPS2RECOMP_CHECK
        c.cycles += c.waitCycles; // (the build that checks blocks has no use for the wait: it is taken as over)
        c.FinishWait();
#else
        c.waitResume = c.pc;
        c.waitStack = c.a[7];
        c.waitArmed = true;
        c.budget = 0;
#endif
    }
} // namespace

void AddScratchMemory(uint32_t low, uint32_t high) {
    g_scratch.push_back({low & 0xFFFF, ((high - 1) & 0xFFFF) + 1});
}

void SetRewrites(bool check, bool off) {
    g_checkRewrites = check;
    g_noRewrites = off;
}

bool ReportRewrites() {
    bool ok = true;
    for (const Hook &hook : Hooks()) {
        if (hook.rewrite && g_checkRewrites) {
            fprintf(stderr, "rewrite %-24s %10llu calls, %llu differed", hook.name, static_cast<unsigned long long>(hook.calls),
                    static_cast<unsigned long long>(hook.differed));
            if (hook.calls) {
                // (how much longer the game's own code took than the rewrite was charged: all noughts when the
                // rewrite keeps the game's time exactly)
                fprintf(stderr, "; cycles short by %lld to %lld, %lld on average", static_cast<long long>(hook.lateMin),
                        static_cast<long long>(hook.lateMax), static_cast<long long>(hook.lateSum / static_cast<int64_t>(hook.calls)));
            }
            fprintf(stderr, "\n");
            ok = ok && hook.differed == 0;
        }
    }
    return ok;
}

void AddRewrite(uint32_t address, const char *name, RewriteFn fn, uint32_t free, uint32_t cycles) {
    AddHook(address, name, nullptr);
    Hooks().back().rewrite = fn;
    Hooks().back().free = free;
    Hooks().back().cycles = cycles;
}

void RewriteWhen(uint32_t address, RewriteWhenFn when) {
    for (Hook &hook : Hooks()) {
        if (hook.address == address && hook.rewrite) {
            hook.when = when;
            return;
        }
    }
    fprintf(stderr, "RewriteWhen: there is no rewrite at %06X\n", address);
    exit(1);
}

void Call(M68K &c, uint32_t address) {
    c.CallGuest(address);
}

bool RunHook(M68K &c, uint32_t address) {
    if (!HasHook(address) || g_inOriginal) {
        return false;
    }
#ifdef CPS2RECOMP_CHECK
    if (check::side) {
        return false; // (the run loop has asked the hook already)
    }
#endif
    Hook &hook = Hooks()[g_hooked[address / 2] - 1];
    if (!hook.rewrite) {
        return hook.fn(c);
    }
    if (g_noRewrites) {
        return false;
    }
    {
        // CPS2RECOMP_SKIP_REWRITES=Name,Name: those run as the game's own code, so that the rewrites they
        // would have used directly get called, and checked, on their own
        static const std::string skip = std::string(",") + (getenv("CPS2RECOMP_SKIP_REWRITES") ? getenv("CPS2RECOMP_SKIP_REWRITES") : "") + ",";
        if (skip.size() > 2 && skip.find(std::string(",") + hook.name + ",") != std::string::npos) {
            return false;
        }
    }
    if (hook.when && !hook.when(c)) {
        return false;
    }
    if (g_checkRewrites && !g_inRewrite) {
        CheckRewrite(c, hook);
        return true;
    }
    hook.rewrite(c);
    c.cycles += hook.cycles ? hook.cycles : kRewriteCycles;
    Return(c);
    ArmWait(c);
    return true;
}

// The game's code from the PC to the return of the routine it is in: the interpreter, nothing else
void M68K::RunToReturn() {
    const uint32_t back = Read(a[7], 2), stack = a[7] + 4;
    for (uint64_t n = 0; !(pc == back && a[7] == stack); n++) {
        if (n > 20000000) {
            fprintf(stderr, "the routine at %06X did not return (a rewrite's original has to, without waiting for anything)\n",
                    instruction);
            exit(1);
        }
        m_cycles = 0;
        Execute(Fetch());
        cycles += m_cycles;
    }
}

// The game's routine at `address`, called from C++: run until it returns to a place that is nowhere
void M68K::CallGuest(uint32_t address) {
    constexpr uint32_t kNowhere = 0x00FEDCBA & ~1u;
    const Tables &tables = GetTables();
    const uint64_t keptBudget = budget;
    const unsigned keptDepth = depth;
    budget = ~0ull;
    Push32(kNowhere);
    pc = address;
    for (uint64_t n = 0; pc != kNowhere; n++) {
        if (n > 50000000) {
            fprintf(stderr, "the game's routine at %06X, called from C++, did not come back (it must not wait for anything)\n", address);
            exit(1);
        }
        if (!g_hooked.empty() && RunHook(*this, pc)) {
            continue;
        }
#ifndef CPS2RECOMP_CHECK
        if (const NativeFn fn = g_inOriginal ? nullptr : tables.At(pc)) {
            depth = 0;
            fn(*this);
            continue;
        }
#else
        (void)tables;
#endif
        m_cycles = 0;
        Execute(Fetch());
        cycles += m_cycles;
    }
    budget = keptBudget;
    depth = keptDepth;
}

bool HasNativeCode() {
    return kNativeCode.entryCount != 0;
}

void M68K::Record(uint32_t address) {
    if (recording && address < romSize && !(address & 1) && !GetTables().At(address)) {
        newEntries.push_back(address);
        if (newEntries.size() > 100000) {
            // (kept small: the same places come again and again)
            std::sort(newEntries.begin(), newEntries.end());
            newEntries.erase(std::unique(newEntries.begin(), newEntries.end()), newEntries.end());
        }
    }
}

void M68K::Run(uint64_t until, Mode mode) {
    const Tables &tables = GetTables();
    budget = until;
    if (!g_hooksChecked) {
        CheckHooks();
    }
    if (mode == Mode::Check) {
        RunChecked(until);
        return;
    }
    for (;;) {
        // (an interrupt can only have become takable since the last look where the loop is come back to:
        // the lines change between runs, and instructions that change the mask end their block and leave
        // their routine)
        if (TakeInterrupt()) {
            cycles += 44;
            Record(pc);
        }
        if (waitArmed && pc == waitResume && a[7] == waitStack) {
            const uint64_t spent = std::min<uint64_t>(waitCycles, cycles < until ? until - cycles : 0);
            cycles += spent;
            waitCycles -= spent;
            if (waitCycles == 0) {
                FinishWait();
            }
        }
        budget = until;
        if (cycles >= until) {
            return;
        }
        if (stopped) {
            cycles = until;
            return;
        }
        if (mode == Mode::Native) {
            if (const NativeFn fn = tables.At(pc)) {
                depth = 0;
                fn(*this); // (which stops by at hooks itself)
                Record(pc); // (where translated code came out, if no block starts there)
                continue;
            }
        }
        if (!g_hooked.empty() && RunHook(*this, pc)) {
            continue;
        }
        // The interpreter: one instruction; on through the rest of a block when inside one
        do {
            m_cycles = 0;
            m_jumped = false;
            Execute(Fetch());
            cycles += m_cycles;
            interpreted++;
            if (m_jumped) {
                Record(pc); // (a computed jump, a return, an exception: a place the translator should start from)
            }
        } while (mode != Mode::Interpreter && !stopped && tables.Inside(pc));
    }
}

#ifndef CPS2RECOMP_CHECK
void M68K::RunChecked(uint64_t until) {
    fprintf(stderr, "this build cannot check translated code (build the check target)\n");
    exit(1);
}
#else

namespace {
    struct Access {
        uint32_t address;
        uint8_t size;
        uint32_t value;
        bool operator==(const Access &) const = default;
    };
    std::vector<Access> g_nativeWrites, g_interpreterWrites;
    uint64_t g_blocksChecked = 0;

    uint32_t RawRead(M68K &c, uint32_t address, unsigned size) {
        switch (size) {
        case 0: return c.bus.Read8(address & 0xFFFFFF);
        case 1: return c.bus.Read16(address & 0xFFFFFE);
        default: {
            const uint32_t high = c.bus.Read16(address & 0xFFFFFE);
            return (high << 16) | c.bus.Read16((address + 2) & 0xFFFFFE);
        }
        }
    }

    // The bytes of the side run's own writes that cover a read
    uint32_t Overlay(M68K &c, uint32_t address, unsigned size, uint32_t value) {
        const uint32_t bytes = M68K::kBytes[size];
        for (const Access &w : g_nativeWrites) {
            const uint32_t wBytes = M68K::kBytes[w.size];
            for (uint32_t i = 0; i < bytes; i++) {
                const uint32_t at = (address + i) & 0xFFFFFF;
                const uint32_t offset = (at - w.address) & 0xFFFFFF;
                if (offset < wBytes) {
                    const uint32_t byte = (w.value >> ((wBytes - 1 - offset) * 8)) & 0xFF;
                    const unsigned shift = (bytes - 1 - i) * 8;
                    value = (value & ~(0xFFu << shift)) | (byte << shift);
                }
            }
        }
        return value;
    }
} // namespace

namespace native {
    uint32_t CheckRead(M68K &c, uint32_t address, unsigned size) {
        address &= size == 0 ? 0xFFFFFF : 0xFFFFFE;
        const uint32_t value = RawRead(c, address, size);
        return g_nativeWrites.empty() ? value : Overlay(c, address, size, value);
    }
    void CheckWrite(M68K &c, uint32_t address, unsigned size, uint32_t value) {
        address &= size == 0 ? 0xFFFFFF : 0xFFFFFE;
        g_nativeWrites.push_back({address, static_cast<uint8_t>(size), value & M68K::kMask[size]});
    }
} // namespace native

// The check's two hooks in the interpreter's memory access (m68k.cpp): during the side run the interpreter
// (doing a rare instruction for translated code) reads and writes as translated code does; during the real
// run its writes are noted as well as done
namespace check {
    bool side = false, noting = false;
    void Note(uint32_t address, unsigned size, uint32_t value) {
        if (size == 2) {
            g_interpreterWrites.push_back({address & 0xFFFFFE, 1, value >> 16});
            g_interpreterWrites.push_back({(address + 2) & 0xFFFFFE, 1, value & 0xFFFF});
        } else {
            g_interpreterWrites.push_back({address & (size == 0 ? 0xFFFFFFu : 0xFFFFFEu), static_cast<uint8_t>(size),
                                           value & M68K::kMask[size]});
        }
    }
} // namespace check

uint64_t BlocksChecked() {
    return g_blocksChecked;
}

void M68K::RunChecked(uint64_t until) {
    const Tables &tables = GetTables();
    struct Regs {
        uint32_t d[8], a[8], otherSP, pc;
        bool c, v, z, n, x, supervisor, trace, stopped;
        uint8_t mask;
        uint64_t cycles;
        bool operator==(const Regs &) const = default;
    };
    auto save = [&]() {
        Regs r;
        memcpy(r.d, d, sizeof(d));
        memcpy(r.a, a, sizeof(a));
        r.otherSP = otherSP;
        r.pc = pc;
        r.c = flagC, r.v = flagV, r.z = flagZ, r.n = flagN, r.x = flagX;
        r.supervisor = supervisor, r.trace = trace, r.stopped = stopped;
        r.mask = interruptMask;
        r.cycles = cycles;
        return r;
    };
    auto load = [&](const Regs &r) {
        memcpy(d, r.d, sizeof(d));
        memcpy(a, r.a, sizeof(a));
        otherSP = r.otherSP;
        pc = r.pc;
        flagC = r.c, flagV = r.v, flagZ = r.z, flagN = r.n, flagX = r.x;
        supervisor = r.supervisor, trace = r.trace, stopped = r.stopped;
        interruptMask = r.mask;
        cycles = r.cycles;
    };
    for (;;) {
        if (TakeInterrupt()) {
            cycles += 44;
        }
        if (cycles >= until) {
            return;
        }
        if (stopped) {
            cycles = until;
            return;
        }
        if (!g_hooked.empty() && RunHook(*this, pc)) {
            continue;
        }
        const NativeFn fn = tables.At(pc);
        Regs before = save(), native;
        if (fn) {
            // The translated block on the side: its writes go to a list, nothing else changes
            g_nativeWrites.clear();
            depth = 0;
            check::side = true;
            fn(*this);
            check::side = false;
            native = save();
            load(before);
        }
        g_interpreterWrites.clear();
        check::noting = true;
        do {
            m_cycles = 0;
            Execute(Fetch());
            cycles += m_cycles;
        } while (!stopped && tables.Inside(pc));
        check::noting = false;
        if (!fn) {
            continue;
        }
        g_blocksChecked++;
        // (the interpreter writes a long word as two words; put the translated code's the same way)
        std::vector<Access> split;
        for (const Access &w : g_nativeWrites) {
            if (w.size == 2) {
                split.push_back({w.address, 1, w.value >> 16});
                split.push_back({(w.address + 2) & 0xFFFFFE, 1, w.value & 0xFFFF});
            } else {
                split.push_back(w);
            }
        }
        const Regs after = save();
        if (!(native == after) || split != g_interpreterWrites) {
            fprintf(stderr, "translated block at %06X differs from the interpreter (after %llu blocks checked)\n", before.pc,
                    static_cast<unsigned long long>(g_blocksChecked));
            auto show = [](const char *name, const Regs &r) {
                fprintf(stderr, "  %-12s pc %06X cycles +%llu  ccr %c%c%c%c%c mask %d%s\n", name, r.pc,
                        static_cast<unsigned long long>(r.cycles), r.x ? 'X' : '-', r.n ? 'N' : '-', r.z ? 'Z' : '-',
                        r.v ? 'V' : '-', r.c ? 'C' : '-', r.mask, r.stopped ? " stopped" : "");
                fprintf(stderr, "    d");
                for (uint32_t v : r.d) {
                    fprintf(stderr, " %08X", v);
                }
                fprintf(stderr, "\n    a");
                for (uint32_t v : r.a) {
                    fprintf(stderr, " %08X", v);
                }
                fprintf(stderr, "\n");
            };
            Regs b = before, n = native, i = after;
            n.cycles -= before.cycles;
            i.cycles -= before.cycles;
            b.cycles = 0;
            show("before", b);
            show("interpreter", i);
            show("translated", n);
            auto list = [](const char *name, const std::vector<Access> &writes) {
                fprintf(stderr, "  %s writes:", name);
                for (const Access &w : writes) {
                    fprintf(stderr, " %06X.%c=%X", w.address, "bwl"[w.size], w.value);
                }
                fprintf(stderr, "\n");
            };
            list("interpreter", g_interpreterWrites);
            list("translated", split);
            exit(3);
        }
    }
}
#endif

} // namespace cps2recomp
