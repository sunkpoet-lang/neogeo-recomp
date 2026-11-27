// SPDX-License-Identifier: GPL-3.0-only
#include "../../../runtime/neogeo_game.h"
#include "../../../machine/neogeo.h"
#include "match_score.h"
#include "../../../runtime/kof_replay.h"
#include "training.h"
namespace cps2recomp {
void RegisterGame() {
    TheNeoGeoGame().onlineMatchScore = kof98::ReadMatchScore<NeoGeo>;
    TheNeoGeoGame().replayVersus = kof::ReplayVersus<NeoGeo>;
    TheNeoGeoGame().replayClock = kof::ReplayClock<0x10a83a, NeoGeo>;
    kof98::training::Register();
}
} // namespace cps2recomp
