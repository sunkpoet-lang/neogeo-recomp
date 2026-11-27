// SPDX-License-Identifier: GPL-3.0-only
#include "../../../runtime/neogeo_game.h"
#include "../../../machine/neogeo.h"
#include "replay_match.h"
#include "match_score.h"
namespace cps2recomp {
namespace {
// Every character open (ForkAde's base): with this byte at 1 the select screens offer Saisyu Kusanagi and Omega Rugal
// (Team Edit, the bottom row's middle). The original MVS program keeps it at 0 and has no code to set it (the home
// version's Start code is not in it); found by a write sweep from a state before team select, see
// openspec/changes/neo-kof95/validation.md. Written before every frame, never while the system ROM has the board
// (its power-on RAM test), so offline, online and replays all see the same value.
constexpr uint32_t kBossesOpen = 0x10e79a;
void BeforeFrame(NeoGeo& board){
    if(board.GameRunning())board.Poke8(kBossesOpen,1);
}
}
void RegisterGame(){
    TheNeoGeoGame().beforeFrame=BeforeFrame;
    TheNeoGeoGame().replayVersus=kof95::ReplayVersus<NeoGeo>;
    TheNeoGeoGame().onlineMatchScore=kof95::ReadMatchScore<NeoGeo>;
    // Timer address independently documented by fbneo-training-mode's KOF95 facts.
    TheNeoGeoGame().replayClock=[](NeoGeo& b)->uint16_t{return static_cast<uint16_t>((b.Peek8(0x10a836)<<8)|b.Peek8(0x10a837));};
}
}
