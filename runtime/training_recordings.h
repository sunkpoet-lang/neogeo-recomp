// SPDX-License-Identifier: GPL-3.0-only
// Bounded input-only practice slots. No board state or host-time RNG.
#pragma once
#include "state.h"
#include <array>
#include <optional>

namespace cps2recomp::training {
class Recordings {
public:
    static constexpr size_t Slots=3,Limit=1800;
    static constexpr uint32_t Seed=0x6d2b79f5;
    enum Mode {Once,Loop,Random};
    static uint16_t Relative(uint16_t pad,bool left) {
        pad&=0xff; // never operator, coin, Start or Select
        return left?uint16_t((pad&~0xcu)|((pad&4u)<<1)|((pad&8u)>>1)):pad;
    }
    bool Record(int slot) {
        Stop();if(slot<0||slot>=int(Slots))return false;
        data[slot].clear();recording=slot;return true;
    }
    void Stop(){recording=playing=-1;cursor=0;}
    void ResetInteraction(){Stop();random=Seed;}
    void Clear(int slot){Stop();if(slot>=0&&slot<int(Slots))data[slot].clear();}
    bool Play(int slot,int how) {
        Stop();mode=how>=Once&&how<=Random?how:Once;
        if(mode==Random)slot=Choose();
        if(slot<0||slot>=int(Slots)||data[slot].empty())return false;
        playing=slot;return true;
    }
    uint16_t Capture(uint16_t pad,bool left) {
        if(!Recording())return 0;
        auto& tape=data[recording];
        if(tape.size()>=Limit){Stop();return 0;}
        tape.push_back(Relative(pad,left));
        if(tape.size()==Limit)Stop();
        return pad&0xff;
    }
    std::optional<uint16_t> Next(bool left) {
        if(!Playing())return {};
        if(cursor>=data[playing].size()){Stop();return {};}
        const auto result=Relative(data[playing][cursor++],left);
        if(cursor==data[playing].size()) {
            if(mode==Once)Stop();
            else {cursor=0;if(mode==Random){playing=Choose();if(playing<0)Stop();}}
        }
        return result;
    }
    bool Recording()const{return recording>=0&&recording<int(Slots);}
    bool Playing()const{return playing>=0&&playing<int(Slots);}
    int RecordingSlot()const{return Recording()?recording:-1;}
    int PlayingSlot()const{return Playing()?playing:-1;}
    uint32_t Cursor()const{return cursor;}
    int PlaybackMode()const{return mode;}
    size_t Length(int slot)const{return slot>=0&&slot<int(Slots)?data[slot].size():0;}
    uint32_t RandomState()const{return random;}
    void Serialize(Archive& ar) {
        ar.Mark(0x44534c31);
        for(auto& tape:data)ar.BoundedVec(tape,Limit);
        ar.Pod(recording);ar.Pod(playing);ar.Pod(cursor);ar.Pod(mode);ar.Pod(random);
        if(!ar.Saving()) {
            // Semantic metadata is sanitised before any indexing/injection. The
            // archive validates payload bounds independently, without allocation
            // during preflight. Invalid operator bits can never reach a board.
            for(auto& tape:data)for(auto& pad:tape)pad&=0xff;
            if(!Recording())recording=-1;
            if(!Playing())playing=-1;
            if(mode<Once||mode>Random)mode=Once;
            if(!random)random=Seed;
            if((Recording()&&Playing())||(Playing()&&cursor>=data[playing].size())||
               (Recording()&&data[recording].size()>=Limit))Stop();
        }
    }
private:
    int Choose() {
        std::array<int,Slots> eligible{};unsigned count=0;
        for(int i=0;i<int(Slots);++i)if(!data[i].empty())eligible[count++]=i;
        if(!count)return -1;
        random^=random<<13;random^=random>>17;random^=random<<5;
        return eligible[random%count];
    }
    std::array<std::vector<uint16_t>,Slots> data;
    int recording=-1,playing=-1;
    uint32_t cursor=0;
    int mode=Once;
    uint32_t random=Seed;
};
}
