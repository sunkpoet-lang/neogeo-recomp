// SPDX-License-Identifier: GPL-3.0-only
// Cartridge layout facts: MAME 8c285468, src/mame/snk/neogeo.cpp, kof95.
#pragma once
#include "romset.h"
#include <algorithm>
namespace cps2recomp {
inline bool AssembleKof95(NeoGeoRoms &r, std::string &error) {
    if (r.program.size()!=0x200000 || r.samples.size()!=0x700000 ||
        r.sprites.size()!=0x1600000 || r.fix.size()!=0x20000 || r.soundProgram.size()!=0x20000) {
        error="kof95: unexpected original MVS cartridge region sizes";
        return false;
    }
    // Build the sparse regions before mutation. The ordinary loader already
    // word-swapped P1 and interleaved each C pair.
    std::vector<uint8_t> samples(0x900000,0), sprites(0x1a00000,0);
    std::copy_n(r.samples.begin(),0x600000,samples.begin());
    std::copy(r.samples.begin()+0x600000,r.samples.end(),samples.begin()+0x800000);
    std::copy_n(r.sprites.begin(),0x1400000,sprites.begin());
    std::copy(r.sprites.begin()+0x1400000,r.sprites.end(),sprites.begin()+0x1800000);
    std::rotate(r.program.begin(),r.program.begin()+0x100000,r.program.end());
    r.samples.swap(samples); r.sprites.swap(sprites);
    return true;
}
}
