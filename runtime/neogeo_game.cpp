// SPDX-License-Identifier: GPL-3.0-only
// Part of neogeo-recomp.

#include "neogeo_game.h"

namespace cps2recomp {

NeoGeoHooks &TheNeoGeoGame() {
    static NeoGeoHooks hooks;
    return hooks;
}

} // namespace cps2recomp
