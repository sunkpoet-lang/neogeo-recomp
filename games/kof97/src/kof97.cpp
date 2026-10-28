// SPDX-License-Identifier: GPL-3.0-only
// Part of cps2recomp (the user's fork: the Neo Geo board).
//
// The King of Fighters '97's own sources (runtime/neogeo_game.h). For now one cheat, which shows the way in for the
// training mode to come (openspec change neogeo-training).
//
// The players' health is a byte at 0x108239 and 0x108439 (0x68 full; 0 outside a fight), as fbneo-training-mode's
// kof97 script documents it (github.com/peon2/fbneo-training-mode: its facts about the game's memory, not its code).
#include "../../../runtime/neogeo_game.h"
#include "../../../machine/neogeo.h"

namespace cps2recomp {

namespace {
    constexpr uint32_t kHealth[2] = {0x108239, 0x108439};
    constexpr uint8_t kFullHealth = 0x67; // (the script writes one less than the 0x68 it reads as full)

    void Frame(NeoGeo &board) {
        // (never while the system ROM has the board: it tests the work RAM at power-on)
        if (!board.GameRunning()) {
            return;
        }
        for (int player = 0; player < 2; player++) {
            static const char *const kNames[2] = {"cheat_energy_p1", "cheat_energy_p2"};
            const uint8_t health = board.Peek8(kHealth[player]);
            if (FeatureOn(kNames[player]) && health != 0 && health <= 0x68) {
                board.Poke8(kHealth[player], kFullHealth);
            }
        }
    }
} // namespace

void RegisterGame() {
    Game &game = TheGame();
    game.pages.push_back({"Cheats", "", "Changes to the game's memory, for trying things out: each off unless chosen."});
    game.features.push_back({"cheat_energy_p1", "Cheats", "Infinite energy, player 1",
                             "Player 1's health is full again every frame of a fight.", kFeatureOwn});
    game.features.push_back({"cheat_energy_p2", "Cheats", "Infinite energy, player 2",
                             "Player 2's health (the computer's, in a one-player game) is full again every frame of a fight.",
                             kFeatureOwn});
    TheNeoGeoGame().frame = Frame;
}

} // namespace cps2recomp
