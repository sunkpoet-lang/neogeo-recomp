// SPDX-License-Identifier: GPL-3.0-only
#include "state.h"
#include <cstdlib>
#include <cstring>
using cps2recomp::Archive;
void check(bool value){if(!value)std::abort();}
int main(){
    std::vector<uint8_t> bytes,source={1,2,3,4};Archive save(bytes,true);save.BoundedVec(source,8);check(save.Ok());
    std::vector<uint8_t> current={9};Archive pre(bytes,false,true);pre.BoundedVec(current,8);check(pre.Complete()&&current==std::vector<uint8_t>{9});
    Archive load(bytes,false);load.BoundedVec(current,8);check(load.Complete()&&current==source);
    for(size_t n=0;n<bytes.size();++n){auto cut=std::vector<uint8_t>(bytes.begin(),bytes.begin()+n);Archive a(cut,false,true);a.BoundedVec(current,8);check(!a.Complete());}
    auto bad=bytes;uint64_t count=9000000;memcpy(bad.data(),&count,sizeof(count));Archive huge(bad,false);huge.BoundedVec(current,8);check(!huge.Ok()&&current==source);
    std::vector<uint8_t> tooMany(9),output;Archive limit(output,true);limit.BoundedVec(tooMany,8);check(!limit.Ok()&&output.empty());
}
