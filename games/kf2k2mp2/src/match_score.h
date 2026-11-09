// SPDX-License-Identifier: GPL-3.0-only
// Native-bus observations; see openspec/changes/neo-kof-family-match-flow.
#pragma once
#include "../../../runtime/neogeo_game.h"

namespace cps2recomp::kf2k2mp2 {
template<class Board> NeoGeoMatchScore ReadMatchScore(Board& board) {
    NeoGeoMatchScore result;
    result.valid=board.GameRunning() && board.Peek8(0x10fdaf)==2;
    if(!result.valid)return result;
    const auto phase=board.Peek8(0x10b056);
    result.humanFight=board.Peek8(0x10fdb6)==1 && board.Peek8(0x10fdb7)==1 && (phase==1 || phase==2);
    result.wins={board.Peek8(0x10dab7),board.Peek8(0x10dab9)};
    return result;
}
} // namespace cps2recomp::kf2k2mp2
