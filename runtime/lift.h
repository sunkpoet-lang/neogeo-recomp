// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Hooks: how a game's own sources (games/<id>/src) put C++ in the way of the game's code.
//
//   AddHook(0x012345, "TakeDamage", [](M68K &c) { ...; return false; });
//
// The function runs whenever the 68000 arrives at the address, which must be where a routine (or a block)
// of the game starts. It sees and can change the CPU's registers and the game's memory (game.h: Ram16...).
// Returning false lets the game's code go on from there as usual; returning true says the hook has done the
// routine's work itself and left the CPU where the routine would (Return(c) is the routine's 'rts').
//
// The translator reads the addresses out of the sources (it looks for "AddHook(0x"), so that translated
// code stops by at those places and nowhere else: after adding a hook, 'make' again.
#pragma once

#include "../machine/m68k.h"

namespace cps2recomp {

// An address given to something that will hook it (the board's services in board.h, say): written this way
// the translator finds it in the sources, as it finds AddHook's and AddRewrite's
constexpr uint32_t HookAt(uint32_t address) {
    return address;
}

// From a rewrite whose original spends a long time waiting on a part of the board (frames, not cycles): the
// rewrite does its work at once and then has the 68000 do nothing for `cycles`, once the routine has
// returned. Interrupts are taken during the wait as they were during the original's, so the game's count of
// frames stays what it was. `busy`, if not 0, is a byte of work RAM that is 1 for the wait and cleared after
// it (the flag the game itself keeps set while it is busy).
void Wait(M68K &c, uint64_t cycles, uint32_t busy = 0);

using HookFn = bool (*)(M68K &c);
void AddHook(uint32_t address, const char *name, HookFn fn);
// For the run loop and translated code: runs the hook at the address, if there is one
bool RunHook(M68K &c, uint32_t address);
bool HasHook(uint32_t address);

// A rewrite: a routine of the game written again as ordinary C++ with names, which runs in the original's
// place. It has to do exactly what the original does: leave the same values in the game's memory and in the
// registers (but for those named as free: ones the original changes too and no caller reads), and return as
// the original does with 'rts' (the port does that part). `--check-rewrites` runs both, every call, and
// reports any difference (docs/READABLE.md).
//
//   AddRewrite(0x019172, "BoxesOverlap", BoxesOverlap, kFreeD0 | kFreeD1);
using RewriteFn = void (*)(M68K &c);
enum : uint32_t {
    kFreeD0 = 1u << 0, kFreeD1 = 1u << 1, kFreeD2 = 1u << 2, kFreeD3 = 1u << 3,
    kFreeD4 = 1u << 4, kFreeD5 = 1u << 5, kFreeD6 = 1u << 6, kFreeD7 = 1u << 7,
    kFreeA0 = 1u << 8, kFreeA1 = 1u << 9, kFreeA2 = 1u << 10, kFreeA3 = 1u << 11,
    kFreeA4 = 1u << 12, kFreeA5 = 1u << 13, kFreeA6 = 1u << 14,
    kFreeFlags = 1u << 16,
    // Not a register: the rewrite writes video RAM (a scroll screen's map, the row scroll table), and the
    // check is to compare that too (it is slower: all of video RAM is kept three times a call)
    kChecksVideo = 1u << 17,
};
void AddRewrite(uint32_t address, const char *name, RewriteFn fn, uint32_t free = 0, uint32_t cycles = 0);
// A rewrite that stands aside in some cases: `when` is asked before each call of the rewrite at `address`,
// and when it says no, the game's own code runs that time. For a routine that in some state waits for an
// interrupt (a rewrite cannot, and neither can anything it calls with Call: a rewrite that would reach
// such a wait has to stand aside too).
using RewriteWhenFn = bool (*)(M68K &c);
void RewriteWhen(uint32_t address, RewriteWhenFn when);
// Time. A rewrite takes no time of the 68000's unless it says so, and a game that is short of time in a
// frame (and so slows down, as the board does) would then not be. `cycles` is charged for every call; a
// rewrite whose original takes longer for some calls than for others adds the rest itself (Spend). The check
// reports how the original's time compared.
inline void Spend(M68K &c, uint32_t cycles) {
    c.cycles += cycles;
}
// Work RAM from `low` up to `high` is scratch: the check does not compare it. For places where the game
// puts registers aside (a routine that saves the registers it is called with, whatever they hold, leaves
// there the values of registers a rewrite was allowed not to match).
void AddScratchMemory(uint32_t low, uint32_t high);
// From a rewrite: runs the game's routine at `address` to its return (its own code, or its rewrite)
void Call(M68K &c, uint32_t address);
// How rewrites are run: checked against the originals; or not at all (the originals run)
void SetRewrites(bool check, bool off);
// What the check found, for the end of a run: false if anything differed
bool ReportRewrites();

// What 'rts' does
inline void Return(M68K &c) {
    c.pc = c.Pop32();
    c.cycles += 16;
}

} // namespace cps2recomp
