// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../../../runtime/state.h"
#include <cstdint>
namespace cps2recomp::kof2002::training {
// Conservative incident state only. Not wired to automatic inputs or presentation.
// Independent RAM-fact investigation: OpenSpec recovery-classifier receipts.
class RecoveryDetector {
public:
    enum Event : uint8_t {None,Hit,AirborneHit,Block};
    Event Pending()const{return pending;}
    bool Elevated()const{return elevated;}
    void ResetInteraction(){pending=None;elevated=false;}
    template<class Read> Event Next(Read read){
        const unsigned phase=read(0x10b056);
        if((phase!=1&&phase!=2)||read(0x10fdaf)!=2||read(0x10fdb6)!=1||read(0x10fdb7)!=1){ResetInteraction();return None;}
        const unsigned hit=read(0x108372),move=read(0x108373),height=read(0x108321),flags=read(0x1083e3);
        const bool blocking=flags==0x20||flags==0xa0;
        if(hit){if(pending!=Hit&&pending!=AirborneHit)elevated=false;pending=Hit;}
        else if(pending==None&&blocking){pending=Block;elevated=false;}
        if(pending==Hit||pending==AirborneHit){elevated=elevated||height!=0;if(elevated)pending=AirborneHit;}
        const bool clear=pending==Block?(!blocking&&!hit):(pending!=None&&hit==0&&move==0&&height==0&&(flags&0x60)==0);
        if(!clear)return None;
        const Event result=pending;ResetInteraction();return result;
    }
    void Serialize(Archive& ar){
        uint8_t incident=pending,raised=elevated;
        ar.Pod(incident);ar.Pod(raised);
        ar.Require(incident<=Block&&raised<=1&&((incident==AirborneHit)==(raised!=0)));
        if(ar.Ok()&&!ar.Saving()){pending=static_cast<Event>(incident);elevated=raised!=0;}
    }
private:
    Event pending=None;
    bool elevated=false;
};
}
