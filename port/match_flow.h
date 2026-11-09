// SPDX-License-Identifier: GPL-3.0-only
// Adapted from the ForkAde CPS controller; GPL-3.0-only, same contributors.
#pragma once
#include <array>
#include <cstdint>
#include <map>

namespace port {
// Presentation only: never part of a board archive, checksum or network packet.
class MatchFlow {
public:
    struct State {
        std::array<bool, 2> joined{};
        std::array<int, 2> streak{}, rounds{};
        bool counted = false;
        int quiet = 0;
        std::array<int,2> score{};
        bool scoreKnown=false, scoreArmed=false;
    };
    void Reset() { state = {}; checkpoints.clear(); }
    void Save(int frame) {
        checkpoints[frame] = state;
        while (checkpoints.size() > 128) checkpoints.erase(checkpoints.begin());
    }
    bool Load(int frame) {
        const auto found = checkpoints.find(frame);
        if (found == checkpoints.end()) return false;
        state = found->second;
        checkpoints.erase(checkpoints.upper_bound(frame), checkpoints.end());
        return true;
    }
    void Advance(const uint16_t players[2], uint16_t start, int result, int roundsToWin) {
        for (int side = 0; side < 2; ++side) state.joined[side] = state.joined[side] || (players[side] & start);
        if (result < 0 || result > 3 || roundsToWin < 1) return;
        if (result == 0) {
            if (state.quiet < 60) ++state.quiet;
            if (state.quiet >= 60) state.counted = false;
            return;
        }
        state.quiet = 0;
        if (state.counted) return;
        state.counted = true;
        if (result == 3 || !state.joined[0] || !state.joined[1]) return;
        const int winner = result - 1;
        if (++state.rounds[winner] >= roundsToWin) {
            ++state.streak[winner];
            state.streak[1 - winner] = 0;
            state.rounds = {};
        }
    }
    // Cumulative match scores can stay on screen through selection and the next
    // match. Award an increment, never a held positive counter or a character KO.
    void ObserveScores(bool valid,bool humanFight,const std::array<int,2>& scores) {
        if(!valid || scores[0]<0 || scores[1]<0 || scores[0]>127 || scores[1]>127) {
            state.scoreKnown=state.scoreArmed=false;return;
        }
        if(!state.scoreKnown || scores[0]<state.score[0] || scores[1]<state.score[1]) {
            state.score=scores;state.scoreKnown=true;state.scoreArmed=false;
        }
        if(humanFight && state.joined[0] && state.joined[1])state.scoreArmed=true;
        const int one=scores[0]-state.score[0],two=scores[1]-state.score[1];
        if(state.scoreArmed && ((one==1&&two==0)||(one==0&&two==1))) {
            const int winner=one?0:1;
            ++state.streak[winner];state.streak[1-winner]=0;state.scoreArmed=false;
        }
        else if(one || two)state.scoreArmed=false;
        state.score=scores;
    }
    State state;
private:
    std::map<int, State> checkpoints;
};
} // namespace port
