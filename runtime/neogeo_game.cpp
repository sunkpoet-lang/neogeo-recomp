// SPDX-License-Identifier: GPL-3.0-only
// Part of cps2recomp (the user's fork: the Neo Geo board).

#include "neogeo_game.h"

namespace cps2recomp {

NeoGeoHooks &TheNeoGeoGame() {
    static NeoGeoHooks hooks;
    return hooks;
}

} // namespace cps2recomp
