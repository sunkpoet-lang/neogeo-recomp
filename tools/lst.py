#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
"""Prints part of the program's listing (work/<id>/program.lst).

  lst.py LOW HIGH            the lines from address LOW to HIGH (hex)
  lst.py -r PATTERN [MAX]    the lines matching a regular expression (say '0x76\\(a5\\)'), with the address
                             of the routine each is in ('routine': the nearest line above that is called
                             from somewhere, or follows an rts)
"""
import os
import re
import sys

# The game's repository: where this is run from (make runs it from there), or else the one the toolkit is in
ROOT = os.getcwd() if os.path.isdir("games") else os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def main():
    args = sys.argv[1:]
    if len(args) < 2:
        sys.exit(__doc__)
    game = os.environ.get("GAME", "vampire_savior")
    lines = open(os.path.join(ROOT, "work", game, "program.lst")).read().split("\n")
    if args[0] == "-r":
        pattern = re.compile(args[1])
        most = int(args[2]) if len(args) > 2 else 60
        called = set()
        for line in lines:
            m = re.search(r"\b(?:bsr|jsr) \$([0-9A-F]+)$", line)
            if m:
                called.add(int(m.group(1), 16))
        start = None
        after_rts = True
        shown = 0
        for line in lines:
            if len(line) < 8:
                continue
            at = int(line[1:7], 16)
            if at in called or after_rts:
                start = at
            after_rts = line.endswith(" rts") or " jmp " in line or line.endswith(" rte")
            if pattern.search(line):
                print("%s    [in %06X]" % (line, start))
                shown += 1
                if shown >= most:
                    break
        return
    low, high = int(args[0], 16), int(args[1], 16)
    for line in lines:
        if len(line) >= 8 and low <= int(line[1:7], 16) <= high:
            print(line)


if __name__ == "__main__":
    main()
