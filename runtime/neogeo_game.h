// SPDX-License-Identifier: GPL-3.0-only
// Part of neogeo-recomp.
//
// What a Neo Geo game's own sources (games/<id>/src) do with the board. Everything that does not touch it is the same
// as for a Capcom game, in the same registry (runtime/game.h, TheGame()): the menu's pages, the features and their
// settings, the commands' names, the HUD's lines and boxes, the sources' own state for save states (Game::state). Their
// RegisterGame() fills that registry, and this, for the board:
//
//   beforeFrame  with the players' input set, before the board runs a frame (NeoGeo::players, Poke...)
//   frame        after every frame the board ran (Peek..., SetHud, SetBoxes)
//   commands     what each of TheGame().commands does, by the same index (their `run` stays empty)
#pragma once

#include "game.h"

#include <vector>

namespace cps2recomp {

class NeoGeo; // (machine/neogeo.h)

struct NeoGeoHooks {
    void (*beforeFrame)(NeoGeo &board) = nullptr;
    void (*frame)(NeoGeo &board) = nullptr;
    std::vector<void (*)(NeoGeo &board)> commands;
};

NeoGeoHooks &TheNeoGeoGame();

} // namespace cps2recomp
