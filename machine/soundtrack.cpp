// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#include "soundtrack.h"

#include "../runtime/state.h"
#include "machine.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cps2recomp {

void Soundtrack::FindMusic(const std::vector<uint8_t> &program, uint32_t table, uint32_t count, uint32_t musicBelow) {
    m_music.assign(count, false);
    m_songs.clear();
    for (uint32_t song = 0; song < count; song++) {
        const uint32_t entry = table + song * 4;
        if (entry + 3 > program.size()) {
            break;
        }
        const uint32_t at = (program[entry] << 16) | (program[entry + 1] << 8) | program[entry + 2];
        // (below musicBelow, a song is music even with no entry here: driver 1.71 finds some of its music elsewhere)
        if (musicBelow ? song < musicBelow : at != 0 && at < program.size() && program[at] == 0) {
            m_music[song] = true;
            m_songs.push_back(static_cast<uint16_t>(song));
        }
    }
}

namespace {
    // A fade's rate is how much of the whole the level moves in each of the sound program's sequence ticks
    // (62.5 a second: every fourth of its 250 interrupts); here, in a frame
    int32_t FadeStep(uint16_t rate) {
        return static_cast<int32_t>(rate * (62.5 / Machine::kFramesPerSecond));
    }
} // namespace

void Soundtrack::Request(uint8_t *shared, uint64_t frame) {
    static const bool log = getenv("CPS2RECOMP_SOUNDTRACK_LOG") != nullptr;
    if (log && (shared[0] == 0xFF || IsMusic(static_cast<uint16_t>((shared[0] << 8) | shared[1])))) {
        fprintf(stderr, "soundtrack: frame %llu %s %02X%02X %02X%02X%02X\n", static_cast<unsigned long long>(frame),
                shared[0] == 0xFF ? "command" : Replaced(static_cast<uint16_t>((shared[0] << 8) | shared[1])) ? "music (file)" : "music",
                shared[0], shared[1], shared[2], shared[3], shared[4]);
    }
    if (shared[0] == 0xFF) {
        switch (shared[1]) {
        case 0x00: // everything stops
            state.song = -1;
            state.allLevel = kFull;
            state.allStep = 0;
            break;
        case 0x05: state.song = -1; break; // the music stops
        case 0x06:                         // the music's volume goes up to a level, or (07) down to one
        case 0x07: {
            const bool down = shared[1] == 0x07;
            if (state.step == 0) {
                state.level = down ? kFull : 0;
            }
            // (the level is one of 128, by the even number in the third byte; the highest is full again)
            const uint32_t target = static_cast<uint32_t>(shared[2] & 0xFE) << 8;
            state.target = target >= 0xFE00 ? kFull : target;
            const int32_t step = std::max(FadeStep(static_cast<uint16_t>((shared[3] << 8) | shared[4])), 1);
            state.step = down ? -step : step;
            break;
        }
        case 0x0C: // everything fades in, or (0D) out
        case 0x0D: {
            const bool out = shared[1] == 0x0D;
            if (state.allStep == 0) {
                state.allLevel = out ? kFull : 0;
            }
            const int32_t step = std::max(FadeStep(static_cast<uint16_t>((shared[3] << 8) | shared[4])), 1);
            state.allStep = out ? -step : step;
            break;
        }
        default: break;
        }
        return;
    }
    const uint16_t song = static_cast<uint16_t>((shared[0] << 8) | shared[1]);
    if (!IsMusic(song)) {
        return;
    }
    if (!Replaced(song)) {
        state.song = -1; // (the board's own)
        return;
    }
    state.song = song;
    state.serial++;
    state.frames = 0;
    state.level = state.target = kFull;
    state.step = 0;
    // The board's music stops instead, and the sound side never hears of the song
    memset(shared, 0, 15);
    shared[0] = 0xFF;
    shared[1] = 0x05;
}

void Soundtrack::Frame() {
    if (state.song >= 0) {
        state.frames++;
    }
    if (state.step) {
        const int64_t level = static_cast<int64_t>(state.level) + state.step;
        if ((state.step < 0 && level <= state.target) || (state.step > 0 && level >= state.target)) {
            state.level = state.target;
            state.step = 0;
        } else {
            state.level = static_cast<uint32_t>(level);
        }
    }
    if (state.allStep) {
        const int64_t level = static_cast<int64_t>(state.allLevel) + state.allStep;
        // (in: full, and the fade is over; out: silent, and it stays so)
        state.allLevel = static_cast<uint32_t>(std::clamp<int64_t>(level, 0, kFull));
        if (state.allLevel == kFull) {
            state.allStep = 0;
        }
    }
}

void Soundtrack::Serialize(Archive &ar) {
    ar.Mark(0x534F4E47);
    ar.Pod(state);
}

} // namespace cps2recomp
