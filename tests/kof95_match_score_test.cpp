// SPDX-License-Identifier: GPL-3.0-only
#include "../games/kof95/src/match_score.h"
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
    auto sample=[&]{const auto score=cps2recomp::kof95::ReadMatchScore(board);flow.ObserveScores(score.valid,score.humanFight,score.wins);};
    // the BIOS's power-on and the title: no score
    board.ram={{0x10fdaf,1},{0x10fdb6,1},{0x10fdb7,1}};sample();assert(!flow.state.scoreKnown);
    // one player against the computer: player 1 alone, then lost to the computer (continue): no award
    board.ram={{0x10fdaf,2},{0x10fdb6,1},{0x10fdb7,0}};sample();assert(flow.state.streak[0]==0);
    board.ram[0x10fdb6]=2;sample();assert(flow.state.streak[0]==0&&flow.state.streak[1]==0);
    // a two-human versus: player 2 loses (continue, then game over): one award for player 1, held, not repeated
    board.ram[0x10fdb6]=1;board.ram[0x10fdb7]=1;sample();assert(flow.state.scoreArmed);
    flow.Save(100);board.ram[0x10fdb7]=2;sample();assert(flow.state.streak[0]==1&&flow.state.streak[1]==0);
    board.ram[0x10fdb7]=3;sample();board.ram[0x10fdb7]=0;for(int i=0;i<600;++i)sample();assert(flow.state.streak[0]==1);
    assert(flow.Load(100));board.ram[0x10fdb7]=2;sample();assert(flow.state.streak[0]==1); // rollback: no duplicate
    // the challenger comes back: a new baseline, no award; player 1 wins again
    board.ram[0x10fdb7]=1;sample();assert(flow.state.streak[0]==1&&flow.state.scoreArmed);
    board.ram[0x10fdb7]=2;sample();assert(flow.state.streak[0]==2);
    // a new versus that player 2 wins: player 1's streak ends
    board.ram[0x10fdb7]=1;sample();board.ram[0x10fdb6]=2;sample();assert(flow.state.streak[0]==0&&flow.state.streak[1]==1);
    // both out at once (a draw) awards nobody
    board.ram[0x10fdb6]=1;sample();board.ram[0x10fdb6]=2;board.ram[0x10fdb7]=2;sample();assert(flow.state.streak[1]==1&&flow.state.streak[0]==0);
    flow.Reset();assert(!flow.state.scoreKnown && !flow.state.joined[0]);
}
