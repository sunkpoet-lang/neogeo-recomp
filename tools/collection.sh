#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy and the neogeo-recomp contributors. Part of neogeo-recomp, built on cps2recomp.
#
# The collection: one launcher for every game (recomp-common's launcher with LAUNCHER_COLLECTION; launcher/src/
# description.cpp), in build/collection/ with each game's program beside it. The games are built first (make games);
# one that is not built is left out, and the launcher does not show it. Run from the repository's top (make collection).
#   B=folder           where (build/collection)
#   TUNNEL=program     the online tunnel (cps-tunel) to put beside the launcher, for the games' ONLINE card (none: no card)
set -eu
B=${B:-build/collection}
games=""
mkdir -p "$B"
for cfg in games/*/game.cfg; do
    id=$(basename "$(dirname "$cfg")")
    games="$games${games:+;}$id"
    for exe in "build/$id/$id" "build/$id/$id.exe"; do
        if [ -f "$exe" ]; then
            cp -p "$exe" "$B/"
            echo "$id: $exe"
        fi
    done
done
cmake -S launcher -B "$B/launcher" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$PWD/$B" \
    -DCOLLECTION=ON -DGAMES="$games" \
    -DGAME_NAME="Neo Geo Recomp" -DGAME_EXE=neogeo-recomp -DAPP_ID=io.github.sunkpoet-lang.neogeo-recomp \
    -DHOME_ENV=NEOGEORECOMP_HOME -DDISCLAIMER="Not affiliated with SNK." ${CMAKE_ARGS:-} > /dev/null
cmake --build "$B/launcher"
if [ -n "${TUNNEL:-}" ]; then
    case "$TUNNEL" in *.exe) cp -p "$TUNNEL" "$B/cps-tunel.exe" ;; *) cp -p "$TUNNEL" "$B/cps-tunel" ;; esac
    echo "tunnel: $TUNNEL"
fi
echo "built $B/neogeo-recomp-launcher"
