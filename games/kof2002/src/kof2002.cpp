// SPDX-License-Identifier: GPL-3.0-only
#include "../../../runtime/neogeo_game.h"
#include "../../../machine/neogeo.h"
#include "match_score.h"
#include "training.h"
namespace cps2recomp {
void RegisterGame() {
    kof2002::training::Register();
    TheNeoGeoGame().onlineMatchScore = kof2002::ReadMatchScore<NeoGeo>;
}
} // namespace cps2recomp
