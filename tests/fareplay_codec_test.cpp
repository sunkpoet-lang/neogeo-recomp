// SPDX-License-Identifier: GPL-3.0-only
#include "fareplay.h"
#include <cstdlib>
#include <zlib.h>
using port::Replay;
void check(bool b){if(!b)std::abort();}
void u32(std::vector<uint8_t>& v,uint32_t n){for(int i=0;i<4;i++)v.push_back(static_cast<uint8_t>(n>>(i*8)));}
void part(std::vector<uint8_t>& v,const std::vector<uint8_t>& raw){uLongf n=compressBound(raw.size());std::vector<uint8_t> packed(n);check(compress2(packed.data(),&n,raw.data(),raw.size(),1)==Z_OK);u32(v,raw.size());u32(v,n);v.insert(v.end(),packed.begin(),packed.begin()+n);}
int main(){
    Replay r;r.backend=0;r.info.backend="neogeo";r.info.game="kof95";r.info.frames=60;r.state={1,2,3};r.inputs[0]={1,2,0,0};r.checks={{60,123}};
    auto valid=r.Bytes();std::string error;Replay read;check(read.Read(valid,error));check(read.state==r.state&&read.inputs==r.inputs);
    for(size_t n=0;n<valid.size();n++)check(!read.Read(std::vector<uint8_t>(valid.begin(),valid.begin()+n),error));
    auto tail=valid;tail.push_back(0);check(!read.Read(tail,error));
    Replay::Info info;
    check(!Replay::ParseHeader("{\"frames\":1e999}",info));
    std::string nested="{\"unknown\":";for(int i=0;i<40;i++)nested+='[';nested+='0';for(int i=0;i<40;i++)nested+=']';nested+='}';check(!Replay::ParseHeader(nested,info));
    auto wire=[&](const std::vector<uint8_t>& inputs){std::vector<uint8_t> out={'F','A','R','E','P','L','A','Y',2,0,0,0};const auto h=Replay::HeaderJson(r.info);u32(out,h.size());out.insert(out.end(),h.begin(),h.end());part(out,r.state);part(out,inputs);u32(out,1);u32(out,60);u32(out,0);u32(out,123);return out;};
    check(!read.Read(wire({2,0,1,0,2,0,0,1,0,2,0}),error)); // duplicate frame
    check(!read.Read(wire({2,60,1,0,2,0}),error)); // beyond duration
    std::vector<uint8_t> overflow(11,0x80);overflow[0]=2;overflow[10]=0x82;overflow.insert(overflow.end(),4,0);check(!read.Read(wire(overflow),error));
    auto tooLarge=valid;const auto offset=16+Replay::HeaderJson(r.info).size();tooLarge[offset+3]=0x7f;check(!read.Read(tooLarge,error));
}
