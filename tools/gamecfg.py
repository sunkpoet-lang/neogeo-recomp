#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
"""Reads games/<id>/game.cfg (key = value lines, # comments).

  gamecfg.py KEY [GAME]            print one value
  gamecfg.py --header OUT [GAME]   write a C header with every value as a GAME_<KEY> string macro
"""

import os
import sys

# The game's repository: where this is run from (make runs it from there), or else the one the toolkit is in
ROOT = os.getcwd() if os.path.isdir("games") else os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def load(game=None):
    game = game or os.environ.get("GAME", "vampire_savior")
    cfg = {}
    with open(os.path.join(ROOT, "games", game, "game.cfg")) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, value = line.split("=", 1)
            cfg[key.strip()] = value.strip()
    return cfg


def main():
    args = sys.argv[1:]
    if len(args) >= 2 and args[0] == "--header":
        cfg = load(args[2] if len(args) > 2 else None)
        lines = ["// Generated from game.cfg by cps2recomp/tools/gamecfg.py", "#pragma once", ""]
        for key, value in cfg.items():
            lines.append('#define GAME_%s "%s"' % (key.upper(), value.replace("\\", "\\\\").replace('"', '\\"')))
        content = "\n".join(lines) + "\n"
        if not os.path.exists(args[1]) or open(args[1]).read() != content:
            os.makedirs(os.path.dirname(args[1]) or ".", exist_ok=True)
            with open(args[1], "w") as f:
                f.write(content)
        return
    if not args:
        sys.exit(__doc__)
    print(load(args[1] if len(args) > 1 else None).get(args[0], ""))


if __name__ == "__main__":
    main()
