// SPDX-License-Identifier: GPL-3.0-only
#include "../../../runtime/neogeo_game.h"
#include "../../../machine/neogeo.h"
#include "replay_match.h"
namespace cps2recomp {
void RegisterGame(){
    TheNeoGeoGame().replayVersus=kof95::ReplayVersus<NeoGeo>;
    // Timer address independently documented by fbneo-training-mode's KOF95 facts.
    TheNeoGeoGame().replayClock=[](NeoGeo& b)->uint16_t{return static_cast<uint16_t>((b.Peek8(0x10a836)<<8)|b.Peek8(0x10a837));};
}
}
