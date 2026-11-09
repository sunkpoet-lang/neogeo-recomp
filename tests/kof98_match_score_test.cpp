// SPDX-License-Identifier: GPL-3.0-only
#include "../games/kof98/src/match_score.h"
#include "../port/match_flow.h"
#include <cassert>
#include <map>

struct Board {
    bool running=true;
    std::map<unsigned,unsigned char> ram;
    bool GameRunning()const{return running;}
    unsigned char Peek8(unsigned address){return ram[address];}
};
int main(){
    Board board;port::MatchFlow flow;
    const uint16_t starts[2]={1,1};flow.Advance(starts,1,0,1);
    auto sample=[&]{const auto score=cps2recomp::kof98::ReadMatchScore(board);flow.ObserveScores(score.valid,score.humanFight,score.wins);};
    board.ram={{0x10fdaf,1},{0x10fdb6,1},{0x10fdb7,1},{0x10b094,1},{0x10daf9,50}};sample();assert(flow.state.streak[0]==0);
    board.ram[0x10fdaf]=2;board.ram[0x10daf9]=0;board.ram[0x10fdb7]=0;sample();
    board.ram[0x10daf9]=1;sample();assert(flow.state.streak[0]==0); // CPU match
    board.ram[0x10daf9]=0;board.ram[0x10fdb7]=1;sample();
    board.ram[0x10b094]=2;sample();assert(flow.state.scoreArmed);
    for(int character=0;character<3;++character){board.ram[0x108439]=0;sample();assert(flow.state.streak[0]==0);}
    flow.Save(100);board.ram[0x10b094]=0;board.ram[0x10daf9]=1;sample();assert(flow.state.streak[0]==1);
    for(int i=0;i<600;++i){sample();}
    assert(flow.state.streak[0]==1);
    assert(flow.Load(100));sample();assert(flow.state.streak[0]==1); // rollback award, no duplicate
    board.ram[0x10b094]=1;sample();board.ram[0x10b094]=0;board.ram[0x10daf9]=2;sample();assert(flow.state.streak[0]==2);
    board.ram[0x10b094]=1;sample();board.ram[0x10b094]=0;board.ram[0x10dafb]=1;sample();assert(flow.state.streak[0]==0&&flow.state.streak[1]==1);
    flow.Reset();assert(!flow.state.scoreKnown && !flow.state.joined[0] && flow.state.streak[1]==0);
}
