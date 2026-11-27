// SPDX-License-Identifier: GPL-3.0-only
#include "../games/kof98/src/random_guard.h"
#include <cstdlib>
#include <vector>
using namespace cps2recomp;
using kof98::training::RandomGuard;
void check(bool yes){if(!yes)std::abort();}
int main(){
 RandomGuard guard;auto seed=guard.Seed();for(int i=0;i<100;++i)check(!guard.Next(false));check(guard.Seed()==seed);
 bool first=guard.Next(true);auto chosen=guard.Seed();check(chosen!=seed);
 for(int i=0;i<20;++i)check(guard.Next(true)==first);check(guard.Seed()==chosen);
 for(int i=0;i<29;++i)check(guard.Next(false)==first);check(guard.Episode());check(!guard.Next(false));check(!guard.Episode());
 std::vector<uint8_t> saved;Archive save(saved,true);guard.Serialize(save);check(save.Ok());
 std::vector<bool> expected;unsigned blocks=0;
 for(int episode=0;episode<64;++episode){expected.push_back(guard.Next(true));blocks+=expected.back();for(int i=0;i<30;++i)guard.Next(false);}
 check(blocks>0&&blocks<64);
 RandomGuard restored;Archive load(saved,false);restored.Serialize(load);check(load.Complete());
 for(bool value:expected){check(restored.Next(true)==value);for(int i=0;i<30;++i)restored.Next(false);}
 check(restored.Seed()==guard.Seed());auto last=guard.Seed();guard.ResetInteraction();check(guard.Seed()==last&&!guard.Episode());
 for(int kind=0;kind<4;++kind){auto bad=saved;if(kind==0)for(int i=0;i<4;++i)bad[i]=0;if(kind==1)bad[4]=31;if(kind==2)bad[5]=2;if(kind==3)bad[6]=2;RandomGuard target;Archive invalid(bad,false);target.Serialize(invalid);check(!invalid.Ok());}
 auto truncated=saved;truncated.pop_back();RandomGuard target;Archive shortState(truncated,false);target.Serialize(shortState);check(!shortState.Ok());
}
