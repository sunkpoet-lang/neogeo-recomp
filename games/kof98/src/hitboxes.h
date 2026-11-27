// SPDX-License-Identifier: GPL-3.0-only
// Independent read-only adapter; RAM-fact provenance in the OpenSpec audit.
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>
#include <optional>
namespace cps2recomp::kof98::collision {
enum class Kind {Hurt,Attack,Guard,Push,ProjectileHurt,ProjectileAttack,Throw};
// player=-1 for projectile objects: ownership is not inferred from list order.
struct Box {int player;Kind kind;int left,top,right,bottom;bool operator==(const Box&)const=default;};
struct Point {int x=0,y=0;};
struct Frame {bool valid=false,projectilesComplete=false;unsigned projectileObjects=0;std::array<Point,2> axes{};std::vector<Box> boxes;};
inline constexpr unsigned kProjectileLimit=64;
namespace detail {
template<class Read> int Word(Read read,uint32_t at){return int(int16_t((unsigned(read(at))<<8)|read(at+1)));}
template<class Read> void Object(Read read,uint32_t base,int player,int camera,Frame& out){
    const bool projectile=player<0;const Point origin{Word(read,base+0x18)-camera,Word(read,base+0x26)-16};
    if(!projectile)out.axes[player]=origin;
    const int flip=read(base+0x31)&1?-1:1,status=read(base+0x7c);
    constexpr int offsets[]={0xa4,0x9f,0x9a,0x95,0x90};
    for(int slot=0;slot<5;++slot){
        const uint32_t at=base+offsets[slot];const int id=read(at);Kind kind=Kind::Push;
        if(slot==0){if(projectile||id==0xff)continue;}
        else{
            const int activeBit=4-slot;if(!(status&(1<<activeBit)))continue;
            if(id>=1&&id<=9)kind=Kind::Hurt;
            else if(id==10||id==11||id==56||id==57)kind=Kind::Guard;
            else if(id>=12&&id<=55){if(activeBit==1)continue;kind=projectile?Kind::ProjectileAttack:Kind::Attack;}
            else if(projectile&&id>=58&&id<=63)kind=Kind::ProjectileHurt;
            else continue;
        }
        const int rx=read(at+3),ry=read(at+4);if(!rx||!ry)continue;
        const int cx=origin.x+int(int8_t(read(at+1)))*flip,cy=origin.y+int(int8_t(read(at+2)));
        out.boxes.push_back({player,kind,cx-rx,cy-ry,cx+rx-1,cy+ry-1});
    }
}
}
template<class Read> std::optional<Box> ReadThrow(Read read,uint32_t base){
    if(base!=0x108100&&base!=0x108300)return {};
    if(!read(base+0x192))return {};
    const uint32_t at=base+0x188;
    const int dx=int(int8_t(read(at+1))),dy=int(int8_t(read(at+2))),rx=read(at+3),ry=read(at+4);
    if(!rx||!ry)return {}; // Do not invent an area for a degenerate reference box.
    const int flip=read(base+0x31)&1?-1:1;
    const int cx=detail::Word(read,base+0x18)-detail::Word(read,0x10b0cc)+dx*flip;
    const int cy=detail::Word(read,base+0x26)-16+dy;
    return Box{base==0x108100?0:1,Kind::Throw,cx-rx,cy-ry,cx+rx-1,cy+ry-1};
}
template<class Read> Frame ReadPlayers(Read read){
    Frame out;const auto phase=read(0x10b094);
    if((phase!=1&&phase!=2)||read(0x10fdaf)!=2||read(0x10fdb6)!=1||read(0x10fdb7)!=1)return out;
    out.valid=true;const int camera=detail::Word(read,0x10b0cc);
    for(int player=0;player<2;++player)detail::Object(read,0x108100+player*0x200,player,camera,out);
    return out;
}
template<class Read> Frame ReadScene(Read read){
    Frame out=ReadPlayers(read);if(!out.valid)return out;
    const auto bodyCount=out.boxes.size();const int camera=detail::Word(read,0x10b0cc);
    std::array<uint16_t,kProjectileLimit+2> seen{0x8100,0x8300};unsigned count=2;
    for(unsigned index=0;index<kProjectileLimit;++index){
        const uint32_t entry=0x10bf24+index*2;const auto low=uint16_t((unsigned(read(entry))<<8)|read(entry+1));
        if(!low||std::find(seen.begin(),seen.begin()+count,low)!=seen.begin()+count){out.projectilesComplete=true;return out;}
        if((low&1)||low>0xfe00)break; // Every referenced field must remain in work RAM.
        const uint32_t base=0x100000u|low;
        if(detail::Word(read,base+6)<0){out.projectilesComplete=true;return out;}
        seen[count++]=low;++out.projectileObjects;detail::Object(read,base,-1,camera,out);
    }
    out.boxes.resize(bodyCount);out.projectileObjects=0; // Fail closed for a partial/malformed list.
    return out;
}
}
