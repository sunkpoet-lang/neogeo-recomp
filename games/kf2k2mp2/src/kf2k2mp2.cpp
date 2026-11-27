// SPDX-License-Identifier: GPL-3.0-only
#include "../../../runtime/neogeo_game.h"
#include "../../../machine/neogeo.h"
#include "match_score.h"
#include "../../../runtime/kof_replay.h"
#include "training.h"
namespace cps2recomp {
void RegisterGame() {
    kf2k2mp2::training::Register();
    TheNeoGeoGame().onlineMatchScore = kf2k2mp2::ReadMatchScore<NeoGeo>;
    TheNeoGeoGame().replayVersus = kof::ReplayVersus<NeoGeo>;
    TheNeoGeoGame().replayClock = kof::ReplayClock<0x10a7d2, NeoGeo>;
}
} // namespace cps2recomp
