# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.

"""Code the game was never seen to reach: found by looking at the program, for recomp.py.

Translating something that is not code does no harm: a block is only ever entered when the CPU's PC is at
its start, and what it then does is what those bytes say. So guesses can be generous. Two kinds:

- after():  what follows a routine's end (a return, a jump) is the next routine, if it decodes as one;
- tables(): runs of four or more long words that each point at something that decodes as a routine
            (the game's tables of handlers), in the program as data or as program.

A guess 'decodes as a routine' when everything fixed branches and calls lead to from it is a known
instruction, down to returns or code already known, without running into the middle of known instructions.
Against code found by running the game (held back, then compared), about 99% of after()'s guesses and 96%
of tables()'s that could be checked were right.
"""

import m68k

MIN_INSTRUCTIONS = 3
MAX_TRIAL = 400


class Guesser:
    def __init__(self, mem, data, insns, low, high):
        self.mem, self.data, self.low, self.high = mem, data, low, high
        self.known = insns
        self.inside = set()   # addresses in the middle of known instructions
        for a, i in insns.items():
            self.inside.update(range(a + 2, a + i.length, 2))

    def trial(self, start):
        """The instructions reached from start that are not known yet; None if it does not decode as code"""
        trial = {}
        stack = [start]
        while stack:
            a = stack.pop()
            while a not in trial and a not in self.known:
                if a in self.inside or a & 1 or not self.low <= a < self.high:
                    return None
                insn = m68k.decode(self.mem, a)
                if insn.text == "?":
                    return None
                trial[a] = insn
                if len(trial) > MAX_TRIAL:
                    return trial
                if insn.flow in ("next", "cond", "dbcc", "call", "calli", "end"):
                    if insn.flow in ("cond", "dbcc", "call"):
                        stack.append(insn.target)
                    a += insn.length
                    continue
                if insn.flow == "branch":
                    stack.append(insn.target)
                break
        if len(trial) < MIN_INSTRUCTIONS:
            return None
        middles = set()
        for a, i in trial.items():
            middles.update(range(a + 2, a + i.length, 2))
        if any(a in middles for a in trial):
            return None
        return trial

    def accept(self, trial):
        self.known.update(trial)
        for a, i in trial.items():
            self.inside.update(range(a + 2, a + i.length, 2))

    def after(self):
        found = []
        work = sorted(a + i.length for a, i in self.known.items() if i.flow in ("return", "branch", "jump", "stop"))
        tried = set()
        while work:
            s = work.pop()
            if s in tried or s in self.known:
                continue
            tried.add(s)
            trial = self.trial(s)
            if trial is None:
                continue
            self.accept(trial)
            found.append(s)
            work += [a + i.length for a, i in trial.items() if i.flow in ("return", "branch", "jump", "stop")]
        return found

    def tables(self, run_length=4):
        covered = bytearray(self.high + 4)
        for a, i in self.known.items():
            for b in range(a, min(a + i.length, self.high)):
                covered[b] = 1
        found = []
        ok = {}

        def plausible(v):
            if v not in ok:
                ok[v] = (self.low <= v < self.high and not v & 1 and v not in self.inside
                         and (v in self.known or self.trial(v) is not None))
            return ok[v]

        for view in (self.data, self.mem):
            a = self.low
            while a < len(view) - 4:
                if a < self.high and covered[a]:
                    a += 2
                    continue
                run = []
                b = a
                while b < len(view) - 4 and not (b < self.high and covered[b]):
                    v = int.from_bytes(view[b:b + 4], "big")
                    if not plausible(v):
                        break
                    run.append(v)
                    b += 4
                if len(run) >= run_length:
                    for v in run:
                        if v not in self.known:
                            trial = self.trial(v)
                            if trial is not None:
                                self.accept(trial)
                                found.append(v)
                    a = b
                else:
                    a += 2
        return found


def guess(mem, data, insns, low, high):
    """Entries worth translating from, besides those the game was seen to reach"""
    g = Guesser(mem, data, dict(insns), low, high)
    found = []
    while True:
        more = g.tables() + g.after()
        if not more:
            break
        found += more
    return sorted(set(found))
