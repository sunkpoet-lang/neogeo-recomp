// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../../../runtime/state.h"
#include <cstdint>
namespace cps2recomp::kof98::training {
class RandomGuard {
public:
    static constexpr uint32_t kSeed=0x6d2b79f5;
    static constexpr uint8_t kQuietFrames=30;
    bool Next(bool attacking){
        if(attacking){
            quiet=0;
            if(!episode){episode=true;seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;block=bool(seed&1);}
        }else if(episode&&++quiet>=kQuietFrames){episode=false;quiet=kQuietFrames;}
        return episode&&block;
    }
    void ResetInteraction(){episode=false;block=false;quiet=kQuietFrames;}
    void Serialize(Archive& ar){
        uint8_t active=episode,chosen=block;
        ar.Pod(seed);ar.Pod(quiet);ar.Pod(active);ar.Pod(chosen);
        ar.Require(seed!=0&&active<=1&&chosen<=1&&quiet<=kQuietFrames&&(active?quiet<kQuietFrames:quiet==kQuietFrames));
        if(ar.Ok()&&!ar.Saving()){episode=active!=0;block=chosen!=0;}
    }
    uint32_t Seed()const{return seed;}
    bool Episode()const{return episode;}
    bool Blocking()const{return episode&&block;}
private:
    uint32_t seed=kSeed;
    uint8_t quiet=kQuietFrames;
    bool episode=false,block=false;
};
}
