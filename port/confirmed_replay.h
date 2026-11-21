// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "fareplay.h"
#include <algorithm>
#include <cstring>

namespace port {
// Local GekkoNet ReplayBlob format 2, uncompressed, no state/checksums/application data.
// Reject a changed SDK envelope rather than interpreting arbitrary serialized data.
inline bool ConfirmedInputs(const unsigned char* data,unsigned length,unsigned base,
                            std::vector<Replay::Pads>& inputs) {
    inputs.clear();
    if(!data||base!=50||length<base)return false;
    const unsigned bytes=length-base;
    if(bytes%4||bytes>72064u*4||bytes>length-20)return false;
    const unsigned char* at=data+length-16-bytes;
    const auto u32=[](const unsigned char* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);};
    if(u32(data)!=0x474b5250||u32(data+4)!=2||data[8]||data[9]!=2||u32(data+16)!=2||data[24]>1||data[25]>1)return false;
    if(u32(at-4)!=bytes)return false;
    for(unsigned i=length-16;i<length;++i)if(data[i])return false;
    inputs.resize(bytes/4);
    for(size_t i=0;i<inputs.size();++i) {
        inputs[i]={uint16_t(at[4*i]|unsigned(at[4*i+1])<<8),uint16_t(at[4*i+2]|unsigned(at[4*i+3])<<8),0,0};
    }
    return true;
}

class ConfirmedReplayProof {
public:
    static constexpr size_t Cap=72000;
    struct Frame {Replay::Pads pads{};uint32_t before=0,after=0;bool valid=false;};
    std::vector<Frame> frames;
    uint32_t initial=0;
    void Observe(int frame,const Replay::Pads& pads,uint32_t before,uint32_t after) {
        if(frame<0||size_t(frame)>=Cap)return;
        if(frames.size()<=size_t(frame))frames.resize(size_t(frame)+1);
        frames[frame]={pads,before,after,true};
    }
    // Inputs alone are insufficient: a later frame can retain the same input after
    // an earlier prediction was corrected. Require the entire state-hash chain too.
    size_t Finish(const std::vector<Replay::Pads>& confirmed,Replay& replay)const {
        replay.inputs.clear();replay.checks.clear();
        size_t n=0;uint32_t previous=initial;
        for(;n<std::min(confirmed.size(),frames.size());++n) {
            const auto& f=frames[n];
            if(!f.valid||f.pads!=confirmed[n]||f.before!=previous)break;
            if(n==0||confirmed[n]!=confirmed[n-1])replay.inputs[n]=confirmed[n];
            previous=f.after;
            if((n+1)%60==0)replay.checks.emplace_back(n+1,previous);
        }
        if(n&&(replay.checks.empty()||replay.checks.back().first!=n))replay.checks.emplace_back(n,previous);
        replay.info.frames=n;return n;
    }
};
}
