// SPDX-License-Identifier: GPL-3.0-only
#include "kof95_layout.h"
#include <cstdio>
#include <cstdlib>
using namespace cps2recomp;
void check(bool value) { if(!value) std::abort(); }
int main() {
    NeoGeoRoms r;
    r.program.resize(0x200000); r.samples.resize(0x700000);
    r.sprites.resize(0x1600000);r.fix.resize(0x20000);r.soundProgram.resize(0x20000);
    for(size_t i=0;i<r.program.size();i++)r.program[i]=uint8_t((i>>20)+1);
    for(size_t i=0;i<r.samples.size();i++)r.samples[i]=uint8_t((i>>20)+10);
    for(size_t i=0;i<r.sprites.size();i++)r.sprites[i]=uint8_t((i>>20)+40);
    std::string error;
    for(int region=0;region<5;region++) {
        auto malformed=r;
        auto *v=region==0?&malformed.program:region==1?&malformed.samples:region==2?&malformed.sprites:region==3?&malformed.fix:&malformed.soundProgram;
        v->pop_back(); const auto before=malformed;
        check(!AssembleKof95(malformed,error));
        check(malformed.program==before.program && malformed.samples==before.samples && malformed.sprites==before.sprites && malformed.fix==before.fix && malformed.soundProgram==before.soundProgram);
    }
    check(AssembleKof95(r,error));
    check(r.program.front()==2 && r.program[0xfffff]==2 && r.program[0x100000]==1 && r.program.back()==1);
    check(r.samples.size()==0x900000 && r.sprites.size()==0x1a00000);
    for(size_t i=0;i<r.samples.size();i++)check(r.samples[i]==(i<0x600000?uint8_t((i>>20)+10):i<0x800000?0:16));
    for(size_t i=0;i<r.sprites.size();i++)check(r.sprites[i]==(i<0x1400000?uint8_t((i>>20)+40):i<0x1800000?0:uint8_t(((i-0x400000)>>20)+40)));
    auto assembled=r;check(!AssembleKof95(r,error));check(r.program==assembled.program);
    puts("kof95 sparse region/continuation and fail-closed layout checks PASS");
}
