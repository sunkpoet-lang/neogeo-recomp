// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "match_flow.h"
#include "../machine/neogeo.h"
#include "../runtime/neogeo_game.h"
namespace port {
inline void AdvanceNeoGeoFlow(MatchFlow& flow,cps2recomp::NeoGeo& board,const uint16_t players[2]) {
    flow.Advance(players,cps2recomp::NeoGeo::kStart,0,1);
    const auto reader=cps2recomp::TheNeoGeoGame().onlineMatchScore;
    if(reader){const auto result=reader(board);flow.ObserveScores(result.valid,result.humanFight,result.wins);}
}
} // namespace port
