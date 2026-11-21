// SPDX-License-Identifier: GPL-3.0-only
#include "confirmed_replay.h"
#include <cstdlib>
#include <cstdio>
using namespace port;
void check(bool b){if(!b)std::abort();}
int main() {
    ConfirmedReplayProof proof;proof.initial=100;
    std::vector<Replay::Pads> input(125);
    for(size_t i=0;i<input.size();++i){input[i]={uint16_t(i),uint16_t(2*i),0,0};proof.Observe(int(i),input[i],100+i,101+i);}
    Replay replay;check(proof.Finish(input,replay)==125);
    check(replay.checks.size()==3&&replay.checks.back()==std::make_pair(uint64_t(125),uint32_t(225)));
    // Same inputs in a stale post-rollback frame: input comparison alone would pass.
    proof.frames[70].before=999;check(proof.Finish(input,replay)==70);
    check(replay.checks.back()==std::make_pair(uint64_t(70),uint32_t(170)));
    proof.Observe(70,input[70],170,171);check(proof.Finish(input,replay)==125);
    input[61][0]^=1;check(proof.Finish(input,replay)==61);
    proof.Observe(-1,{},0,0);proof.Observe(72000,{},0,0);check(proof.frames.size()==125);
    std::vector<unsigned char> raw(50+8);
    raw[0]=0x50;raw[1]=0x52;raw[2]=0x4b;raw[3]=0x47;raw[4]=2;raw[9]=2;raw[16]=2;
    raw[30]=8;raw[34]=1;raw[36]=2;raw[38]=3;raw[40]=4;
    std::vector<Replay::Pads> decoded;
    check(ConfirmedInputs(raw.data(),raw.size(),50,decoded));
    check(decoded.size()==2&&decoded[1][0]==3&&decoded[1][1]==4);
    raw.back()=1;check(!ConfirmedInputs(raw.data(),raw.size(),50,decoded));raw.back()=0;
    raw[30]=7;check(!ConfirmedInputs(raw.data(),raw.size(),50,decoded));raw[30]=8;
    check(!ConfirmedInputs(raw.data(),raw.size(),51,decoded));
    raw[8]=1;check(!ConfirmedInputs(raw.data(),raw.size(),50,decoded));raw[8]=0;
    check(!ConfirmedInputs(nullptr,0,0,decoded));
    puts("confirmed replay: chain, stale predictions, final checksum and envelope PASS");
}
