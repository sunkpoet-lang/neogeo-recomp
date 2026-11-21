// SPDX-License-Identifier: GPL-3.0-only
#include "../games/kof95/src/replay_match.h"
#include "../port/replay_round_clock.h"
#include <cstdlib>
struct Board {bool running=true;unsigned mode=2,p1=1,p2=1;bool GameRunning(){return running;}unsigned Peek8(unsigned a){return a==0x10fdaf?mode:a==0x10fdb6?p1:p2;}};
void check(bool b){if(!b)std::abort();}
int main(){Board b;check(cps2recomp::kof95::ReplayVersus(b));b.p2=0;check(!cps2recomp::kof95::ReplayVersus(b));b.p2=1;b.running=false;check(!cps2recomp::kof95::ReplayVersus(b));b.running=true;b.mode=0x55;check(!cps2recomp::kof95::ReplayVersus(b));port::ReplayRoundClock r;r.Reset(0x6032);check(!r.Observe(0x6032,true));check(r.Observe(0x6031,true));check(!r.Observe(0x6030,true));check(!r.Observe(0x6030,true));check(!r.Observe(0x6029,true));check(!r.Observe(0x6032,true));check(r.Observe(0x6031,true));check(!r.Observe(0x5531,false));}
