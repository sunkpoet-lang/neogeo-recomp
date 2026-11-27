// SPDX-License-Identifier: GPL-3.0-only
#include "../games/kof2002/src/hitboxes.h"
#include "../games/kf2k2mp2/src/hitboxes.h"
#include <array>
#include <cstdlib>
void check(bool yes){if(!yes)std::abort();}
int main(){
 std::array<uint8_t,65536> ram{};auto word=[&](unsigned at,unsigned value){ram[at]=value>>8;ram[at+1]=value;};
 ram[0xb056]=1;ram[0xfdaf]=2;ram[0xfdb6]=ram[0xfdb7]=1;
 word(0xb08e,64);word(0xb0cc,512);word(0x8118,200);word(0x8126,220);
 ram[0x81a4]=ram[0x83a4]=0xff;ram[0x817c]=1;ram[0x8190]=23;ram[0x8192]=uint8_t(-20);ram[0x8193]=ram[0x8194]=8;
 const auto before=ram;auto read=[&](uint32_t at){check(at>=0x100000&&at<=0x10ffff);return ram[at-0x100000];};
 auto original=cps2recomp::kof2002::collision::ReadPlayers(read);auto variant=cps2recomp::kf2k2mp2::collision::ReadPlayers(read);
 check(ram==before&&original.valid&&variant.valid&&original.boxes.size()==1&&variant.boxes.size()==1);
 check(original.axes[0].x==136&&variant.axes[0].x==136);
 check(original.boxes[0].kind==cps2recomp::kof2002::collision::Kind::Hurt&&variant.boxes[0].kind==cps2recomp::kf2k2mp2::collision::Kind::Hurt);
 check(original.boxes[0].left==128&&original.boxes[0].top==176&&original.boxes[0].right==143&&original.boxes[0].bottom==191);
 ram[0x8292]=1;ram[0x8289]=uint8_t(-12);ram[0x828a]=uint8_t(-20);ram[0x828b]=10;ram[0x828c]=8;
 auto attempt=cps2recomp::kof2002::collision::ReadThrow(read,0x108100);auto variantAttempt=cps2recomp::kf2k2mp2::collision::ReadThrow(read,0x108100);
 check(attempt&&variantAttempt&&attempt->left==114&&attempt->right==133&&attempt->top==176&&attempt->bottom==191);
 check(!cps2recomp::kof2002::collision::ReadThrow(read,0x108200));
 ram[0x8292]=0;check(!cps2recomp::kof2002::collision::ReadThrow(read,0x108100));ram[0x8292]=1;
 ram[0x828b]=0;check(!cps2recomp::kf2k2mp2::collision::ReadThrow(read,0x108100));
 ram[0x828d]=0;ram[0x828e]=0;ram[0x828f]=uint8_t(-20);ram[0x8290]=9;ram[0x8291]=15;
 auto vulnerable=cps2recomp::kof2002::collision::ReadThrowable(read,0x108100);check(vulnerable&&vulnerable->left==127&&vulnerable->top==169);
 check(cps2recomp::kf2k2mp2::collision::ReadThrowable(read,0x108100).has_value());
 ram[0x81e3]=0x20;check(!cps2recomp::kof2002::collision::ReadThrowable(read,0x108100));ram[0x81e3]=0;
 ram[0x82d4]=1;check(!cps2recomp::kof2002::collision::ReadThrowable(read,0x108100));ram[0x82d4]=0;
 ram[0x828d]=0x80;check(!cps2recomp::kf2k2mp2::collision::ReadThrowable(read,0x108100));ram[0x828d]=0;
 ram[0x817e]=1;check(!cps2recomp::kof2002::collision::ReadThrowable(read,0x108100));ram[0x817e]=3;check(cps2recomp::kof2002::collision::ReadThrowable(read,0x108100).has_value());
 ram[0x8290]=0;check(!cps2recomp::kf2k2mp2::collision::ReadThrowable(read,0x108100));
 ram[0x8193]=0;check(cps2recomp::kof2002::collision::ReadPlayers(read).boxes.empty());

 // Bounded traversal: ordinary list, duplicate termination and fail-closed failures.
 word(0xbee6,0x1000);word(0xbee8,0);word(0x1006,0);
 ram[0x107c]=1;ram[0x1090]=12;ram[0x1093]=8;ram[0x1094]=9;
 auto scene=cps2recomp::kof2002::collision::ReadScene(read);
 check(scene.projectilesComplete&&scene.projectileObjects==1&&scene.boxes.back().player==-1);
 check(scene.boxes.back().kind==cps2recomp::kof2002::collision::Kind::ProjectileAttack);
 check(cps2recomp::kf2k2mp2::collision::ReadScene(read).projectileObjects==1);
 word(0xbee8,0x1000);check(cps2recomp::kof2002::collision::ReadScene(read).projectilesComplete);
 for(unsigned bad:{0x1001u,0xff00u}){word(0xbee8,bad);auto x=cps2recomp::kof2002::collision::ReadScene(read);check(!x.projectilesComplete&&x.projectileObjects==0&&x.boxes==cps2recomp::kof2002::collision::ReadPlayers(read).boxes);}
 word(0xbee6,0x1000);word(0x1006,0xffff);check(cps2recomp::kf2k2mp2::collision::ReadScene(read).projectileObjects==0);word(0x1006,0);
 for(unsigned i=0;i<64;++i){word(0xbee6+2*i,0x1000+i*0x200);word(0x1006+i*0x200,0);}
 // Skip player objects while building a unique synthetic capped list.
 for(unsigned i=0;i<64;++i)word(0xbee6+2*i,0x1000+i*0x100);
 auto capped=cps2recomp::kof2002::collision::ReadScene(read);check(!capped.projectilesComplete&&capped.projectileObjects==0);
 ram[0xb056]=0;ram[0xb094]=1;check(!cps2recomp::kof2002::collision::ReadPlayers(read).valid&&!cps2recomp::kf2k2mp2::collision::ReadPlayers(read).valid);
}
