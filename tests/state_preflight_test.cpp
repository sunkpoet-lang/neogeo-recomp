// SPDX-License-Identifier: GPL-3.0-only
#include "state.h"
#include <cstdlib>
using cps2recomp::Archive;
void check(bool ok){if(!ok)std::abort();}
int main(){
    std::vector<uint8_t> data;uint32_t number=12;bool flag=true;std::vector<uint16_t> words={1,2,3};
    Archive write(data,true);write.Mark(123);write.Pod(number);write.Pod(flag);write.Vec(words);
    auto test=[&](std::vector<uint8_t> bytes){uint32_t live=99;bool b=false;std::vector<uint16_t> v={8,8,8};Archive ar(bytes,false,true);ar.Mark(123);ar.Pod(live);ar.Pod(b);ar.Vec(v);check(live==99&&!b&&v==std::vector<uint16_t>({8,8,8}));return ar.Complete();};
    check(test(data));auto bad=data;bad[0]^=1;check(!test(bad));bad=data;bad[8]=3;check(!test(bad));
    bad=data;bad[9]=255;check(!test(bad));bad=data;bad.pop_back();check(!test(bad));bad=data;bad.push_back(0);check(!test(bad));
}
