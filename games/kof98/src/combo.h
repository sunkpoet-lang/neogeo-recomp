// SPDX-License-Identifier: GPL-3.0-only
// Independent native implementation. KOF98 RAM facts (each side's combo counter on the other: 0x1084b0 for player 1's
// hits on player 2, 0x1082b0 for player 2's on player 1; health 0x108239/0x108439) as peon2's fbneo-training-mode
// kof98 script documents them; the logic is this port's own.
//
// The dummy's (player 2's) side of a combo: hits counted while the game's combo counter on it is not 0, the damage of
// the last hit and the combo's total, the last combo kept on screen after it ends; and the frame its combo ended, for
// the "after combo" health refill. Saved with the training state (rewind, save states).
#pragma once
#include <cstdint>

namespace cps2recomp::kof98::training {
struct Combo {
    uint8_t health = 0;     // the dummy's health the frame before
    uint16_t hits = 0;      // hits in the combo running now (0: none)
    uint16_t shown = 0;     // the last combo's hits, kept after it ends
    uint16_t lastDamage = 0, total = 0;
    bool ended = false;     // this frame the combo ended (the dummy left hitstun)

    // health: the dummy's health now; stunned: the game's combo counter on it is not 0
    void Next(uint8_t now, bool stunned) {
        ended = false;
        const int diff = int(health) - int(now);
        if (diff > 0 && health != 0) {
            lastDamage = uint16_t(diff);
            if (stunned) {
                if (hits == 0) total = 0;
                ++hits;
                total = uint16_t(total + diff);
            } else {
                hits = 0;
                total = uint16_t(diff); // a single hit that left no hitstun (a throw's last frame, chip)
            }
        }
        if (stunned && hits) shown = hits;
        if (!stunned && hits) { ended = true; hits = 0; }
        health = now;
    }
    void Reset() { *this = {}; }
    template <class Ar> void Serialize(Ar& ar) {
        ar.Pod(health); ar.Pod(hits); ar.Pod(shown); ar.Pod(lastDamage); ar.Pod(total);
        uint8_t e = ended; ar.Pod(e); ar.Require(e <= 1); if (ar.Ok() && !ar.Saving()) ended = e != 0;
    }
};
} // namespace cps2recomp::kof98::training
