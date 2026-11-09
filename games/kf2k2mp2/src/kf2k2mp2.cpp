// SPDX-License-Identifier: GPL-3.0-only
#include "../../../runtime/neogeo_game.h"
#include "../../../machine/neogeo.h"
#include "match_score.h"
namespace cps2recomp {
void RegisterGame() {
    TheNeoGeoGame().onlineMatchScore = kf2k2mp2::ReadMatchScore<NeoGeo>;
}
} // namespace cps2recomp
