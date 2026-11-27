// SPDX-License-Identifier: GPL-3.0-only
#include "../games/kof98/src/combo.h"
#include <cassert>
#include <vector>

struct Ar { // the Archive's calls the combo uses, in memory
    std::vector<unsigned char> data; size_t at = 0; bool saving = true, ok = true;
    bool Saving() const { return saving; } bool Ok() const { return ok; } void Require(bool c) { ok = ok && c; }
    template <class T> void Pod(T& v) {
        auto* p = reinterpret_cast<unsigned char*>(&v);
        if (saving) data.insert(data.end(), p, p + sizeof v);
        else { for (size_t i = 0; i < sizeof v; ++i) p[i] = data[at++]; }
    }
};
int main() {
    using cps2recomp::kof98::training::Combo;
    Combo c;
    c.Next(0x67, false); assert(c.hits == 0 && c.total == 0);              // first frame: health only
    c.Next(0x60, true); assert(c.hits == 1 && c.lastDamage == 7 && c.total == 7);
    c.Next(0x60, true); assert(c.hits == 1);                               // hitstun, no new damage
    c.Next(0x58, true); assert(c.hits == 2 && c.lastDamage == 8 && c.total == 15 && c.shown == 2);
    c.Next(0x58, false); assert(c.ended && c.hits == 0 && c.shown == 2 && c.total == 15); // the combo ends, kept
    c.Next(0x58, false); assert(!c.ended);
    c.Next(0x67, false); assert(c.total == 15);                            // a refill is not damage
    c.Next(0x62, true); assert(c.hits == 1 && c.total == 5);               // a new combo starts its own total
    c.Next(0x60, false); assert(c.lastDamage == 2 && c.ended);             // chip as it leaves hitstun
    Ar save; c.Serialize(save); Ar load{save.data, 0, false}; Combo d; d.Serialize(load);
    assert(load.ok && d.health == c.health && d.shown == c.shown && d.total == c.total && d.ended == c.ended);
    c.Reset(); assert(c.hits == 0 && c.health == 0);
}
