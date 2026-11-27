// SPDX-License-Identifier: GPL-3.0-only
#include "../games/kof98/src/recovery.h"
#include <cstdlib>
#include <map>
using namespace cps2recomp;
using kof98::training::RecoveryDetector;
void check(bool yes){if(!yes)std::abort();}
int main(){
 std::map<uint32_t,uint8_t> ram{{0x10b094,1},{0x10fdaf,2},{0x10fdb6,1},{0x10fdb7,1}};
 auto read=[&](uint32_t at){return ram[at];};RecoveryDetector detector;
 ram[0x108372]=1;check(detector.Next(read)==RecoveryDetector::None);
 ram[0x108321]=5;detector.Next(read);check(detector.Pending()==RecoveryDetector::AirborneHit);
 std::vector<uint8_t> saved;Archive save(saved,true);detector.Serialize(save);check(save.Ok()&&saved.size()==2);
 RecoveryDetector restored;Archive load(saved,false);restored.Serialize(load);check(load.Complete());
 ram[0x108372]=0;ram[0x108321]=0;check(restored.Next(read)==RecoveryDetector::AirborneHit);check(restored.Next(read)==RecoveryDetector::None);
 for(auto bad:std::vector<std::vector<uint8_t>>{{4,0},{2,2},{0,1},{1,1},{3,1},{2,0},{2},{}}){
  Archive invalid(bad,false);detector.Serialize(invalid);check(!invalid.Ok());check(detector.Pending()==RecoveryDetector::AirborneHit&&detector.Elevated());
 }
 ram[0x10b094]=0;detector.Next(read);check(detector.Pending()==RecoveryDetector::None&&!detector.Elevated());
}
