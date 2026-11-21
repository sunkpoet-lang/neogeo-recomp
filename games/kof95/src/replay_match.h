// SPDX-License-Identifier: GPL-3.0-only
// MVS BIOS mode/participant observations from native KOF95 scripted runs.
#pragma once
namespace cps2recomp::kof95 {
template<class Board> bool ReplayVersus(Board& board) {
    return board.GameRunning() && board.Peek8(0x10fdaf)==2 &&
           board.Peek8(0x10fdb6)==1 && board.Peek8(0x10fdb7)==1;
}
}
