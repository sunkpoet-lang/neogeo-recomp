// SPDX-License-Identifier: GPL-3.0-only
#include "../games/kof98/src/hitboxes.h"
#include <array>
#include <cstdlib>
void check(bool value){if(!value)std::abort();}
int main(){
 using namespace cps2recomp::kof98::collision;std::array<uint8_t,65536> ram{};
 auto byte=[&](uint32_t at,uint8_t value){ram[at&65535]=value;};
 auto word=[&](uint32_t at,uint16_t value){byte(at,value>>8);byte(at+1,value);};
 auto read=[&](uint32_t at){check(at>=0x100000&&at<=0x10ffff);return ram[at&65535];};
 byte(0x10b094,1);byte(0x10fdaf,2);byte(0x10fdb6,1);byte(0x10fdb7,1);word(0x10b0cc,100);
 word(0x108118,200);word(0x108126,216);byte(0x10817c,1);
 byte(0x108190,1);byte(0x108191,uint8_t(-10));byte(0x108192,uint8_t(-30));byte(0x108193,20);byte(0x108194,10);
 auto frame=ReadPlayers(read);check(frame.valid&&frame.boxes.size()==1);check(frame.boxes[0]==Box{0,Kind::Hurt,70,160,109,179});
 byte(0x108131,1);frame=ReadPlayers(read);check(frame.boxes[0]==Box{0,Kind::Hurt,90,160,129,179});
 byte(0x108190,12);check(ReadPlayers(read).boxes[0].kind==Kind::Attack);
 byte(0x108190,56);check(ReadPlayers(read).boxes[0].kind==Kind::Guard);
 byte(0x108190,58);check(ReadPlayers(read).boxes.empty());
 byte(0x10817c,2);byte(0x108195,12);byte(0x108198,10);byte(0x108199,10);check(ReadPlayers(read).boxes.empty()); // transient/ghost attack slot
 byte(0x1081a4,0xff);byte(0x1081a7,20);byte(0x1081a8,30);check(ReadPlayers(read).boxes.empty());
 byte(0x1081a4,0);check(ReadPlayers(read).boxes.size()==1&&ReadPlayers(read).boxes[0].kind==Kind::Push);
 byte(0x10b094,0);check(!ReadPlayers(read).valid&&ReadPlayers(read).boxes.empty());
 byte(0x10b094,2);check(ReadPlayers(read).valid);byte(0x10fdb7,0);check(!ReadPlayers(read).valid);
    ram.fill(0);byte(0x10b094,1);byte(0x10fdaf,2);byte(0x10fdb6,1);byte(0x10fdb7,1);word(0x10b0cc,100);
    word(0x10bf24,0x0700);word(0x100718,300);word(0x100726,150);byte(0x10077c,1);
    byte(0x100790,12);byte(0x100791,3);byte(0x100792,uint8_t(-5));byte(0x100793,12);byte(0x100794,8);
    auto scene=ReadScene(read);check(scene.projectilesComplete&&scene.projectileObjects==1&&scene.boxes.size()==1);
    check(scene.boxes[0]==Box{-1,Kind::ProjectileAttack,191,121,214,136});
    byte(0x100790,58);check(ReadScene(read).boxes[0].kind==Kind::ProjectileHurt);
    word(0x10bf26,0x0700);check(ReadScene(read).projectilesComplete&&ReadScene(read).boxes.size()==1); // repeated pointer terminates
    word(0x10bf26,0xfffe);check(!ReadScene(read).projectilesComplete&&ReadScene(read).boxes.empty()); // discard partial result
    word(0x10bf26,0x0701);check(!ReadScene(read).projectilesComplete);
    word(0x10bf26,0);word(0x100706,0x8000);check(ReadScene(read).projectilesComplete&&ReadScene(read).boxes.empty());
    for(unsigned i=0;i<kProjectileLimit;++i)word(0x10bf24+i*2,uint16_t(0x100+i*2));
    check(!ReadScene(read).projectilesComplete&&ReadScene(read).boxes.empty()); // strict list cap
    check(!ReadThrow(read,0x108101)); // invalid actor must not read arbitrary RAM
    byte(0x108292,0);check(!ReadThrow(read,0x108100));
    byte(0x108292,1);byte(0x10828b,0);byte(0x10828c,8);check(!ReadThrow(read,0x108100)); // degenerate area
}
