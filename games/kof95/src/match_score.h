// SPDX-License-Identifier: GPL-3.0-only
// Native-bus observations; see openspec/changes/neo-kof95 (native-results.md).
//
// KOF95 keeps no team-match win counter in RAM. A two-human versus ends when the losing side's MVS player mode
// (0x10fdb6/0x10fdb7) leaves 1 (playing) for the continue countdown while the winner's stays 1. The reader reports
// that as a one-match score for the winner, which MatchFlow awards once as an increment; a returning challenger
// sets both modes to 1 again, the score drops to zero and MatchFlow takes a new baseline without an award.
#pragma once
#include "../../../runtime/neogeo_game.h"

namespace cps2recomp::kof95 {
template<class Board> NeoGeoMatchScore ReadMatchScore(Board& board) {
    NeoGeoMatchScore result;
    result.valid=board.GameRunning() && board.Peek8(0x10fdaf)==2;
    if(!result.valid)return result;
    const bool one=board.Peek8(0x10fdb6)==1, two=board.Peek8(0x10fdb7)==1;
    result.humanFight=one && two;
    result.wins={one && !two ? 1 : 0, two && !one ? 1 : 0};
    return result;
}
} // namespace cps2recomp::kof95
