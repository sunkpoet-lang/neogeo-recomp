#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# The collection: one launcher for every game (recomp-common's launcher with LAUNCHER_COLLECTION; launcher/src/
# description.cpp), in build/collection/ with each game's program beside it. The games are built first (make
# games; each from its own ROM set); one that is not built is left out, and the launcher does not show it.
# Run from the repository's top (make collection).
#   B=folder           where (build/collection)
#   NO_GAME_CODE=1     take each game's program from build/<id>-nocode/ (the build with no translated code)
set -eu
B=${B:-build/collection}
games=""
mkdir -p "$B"
for cfg in games/*/game.cfg; do
    id=$(basename "$(dirname "$cfg")")
    games="$games${games:+;}$id"
    from=build/$id/$id
    [ -n "${NO_GAME_CODE:-}" ] && from=build/$id-nocode/$id
    for exe in "$from" "$from.exe"; do
        if [ -f "$exe" ]; then
            cp -p "$exe" "$B/"
            echo "$id: $exe"
        fi
    done
done
CFG="python3 tools/gamecfg.py"
cmake -S launcher -B "$B/launcher" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$PWD/$B" \
    -DCOLLECTION=ON -DGAMES="$games" \
    -DGAME_NAME="CPS-2 Recomp" -DGAME_EXE=cps2recomp -DAPP_ID=io.github.tmcleroy.cps2recomp -DHOME_ENV=CPS2RECOMP_HOME \
    -DDISCLAIMER="Not affiliated with Capcom." ${CMAKE_ARGS:-} > /dev/null
cmake --build "$B/launcher"
echo "built $B/cps2recomp-launcher"
