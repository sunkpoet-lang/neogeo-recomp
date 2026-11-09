// SPDX-License-Identifier: GPL-3.0-only
// Native-bus observations; see openspec/changes/neo-kof97-match-flow.
#pragma once
#include "../../../runtime/neogeo_game.h"

namespace cps2recomp::kof97 {
template<class Board> NeoGeoMatchScore ReadMatchScore(Board& board) {
    NeoGeoMatchScore result;
    result.valid=board.GameRunning() && board.Peek8(0x10fdaf)==2;
    if(!result.valid)return result;
    const auto phase=board.Peek8(0x10b092);
    result.humanFight=board.Peek8(0x10fdb6)==1 && board.Peek8(0x10fdb7)==1 && (phase==1 || phase==2);
    result.wins={board.Peek8(0x10daf5),board.Peek8(0x10daf7)};
    return result;
}
} // namespace cps2recomp::kof97
