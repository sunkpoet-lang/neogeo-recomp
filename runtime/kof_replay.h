// SPDX-License-Identifier: GPL-3.0-only
// Part of neogeo-recomp.
//
// The KOF games' automatic versus replays and round markers (TheNeoGeoGame().replayVersus/replayClock): a two-human
// versus is the MVS BIOS in game mode (0x10fdaf at 2) with both players' modes at 1 (playing), as KOF95's
// games/kof95/src/replay_match.h; the bout clock is two bytes, seconds in BCD and a frame count that starts at 0x6032,
// observed natively for KOF97/98 at 0x10a83a and KOF2002/Magic Plus II at 0x10a7d2 (the addresses fbneo-training-mode's
// scripts hold for infinite time). port/replay_round_clock.h turns its first decrement after a reset into a marker.
#pragma once
#include <cstdint>

namespace cps2recomp::kof {
template <class Board> bool ReplayVersus(Board &board) {
    return board.GameRunning() && board.Peek8(0x10fdaf) == 2 && board.Peek8(0x10fdb6) == 1 && board.Peek8(0x10fdb7) == 1;
}
template <uint32_t At, class Board> uint16_t ReplayClock(Board &board) {
    return static_cast<uint16_t>((board.Peek8(At) << 8) | board.Peek8(At + 1));
}
} // namespace cps2recomp::kof
