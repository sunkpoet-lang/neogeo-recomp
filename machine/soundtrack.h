// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// A custom soundtrack: the game's music played from the player's own files (an arranged soundtrack, say) in place
// of the board's. This is the machine's part of it; port/soundtrack.h plays the files.
//
// The game asks for a sound by leaving sixteen bytes at the start of the memory it shares with the sound side, the
// last written being the one that says a request is there (every CPS-2 game seen does it so, whatever its sound
// program's version). The first two bytes are a song's number, or 0xFF and a command: 00 everything stops, 05 the
// music stops, 06 and 07 its volume goes up or down to a level, 0C and 0D everything fades in or out. A song is
// music when its data in the sound program starts with a 0 (sound/driver.cpp, StartMusic); the others are sound
// effects, voices and single samples.
//
// When the game asks for a piece of music the player has a file for, the request is made "the music stops" before
// the sound side sees it, so the board's music stops and nothing else changes: the game's own state is the same as
// without a soundtrack (it never reads back what the sound side does). What is playing (the song, how long since
// it started, the game's fades) is counted in the game's frames and kept with save states, so a state loaded,
// run-ahead's frames taken back and a spare machine's frames all leave it where the game is.
#pragma once

#include <cstdint>
#include <vector>

namespace cps2recomp {

class Archive;

class Soundtrack {
public:
    // Which song numbers are music, read from the sound program's table of songs: `table` is where the entry of
    // song 0 is (four bytes a song: a three-byte address in the program, high byte first, then one more), `count`
    // how many entries to read. (Not kept with save states: the port sets them.)
    void FindMusic(const std::vector<uint8_t> &program, uint32_t table, uint32_t count);
    bool IsMusic(uint16_t song) const {
        return song < m_music.size() && m_music[song];
    }
    const std::vector<uint16_t> &MusicSongs() const {
        return m_songs;
    }
    // The songs the player's files are for (the port sets it; empty: the board plays all its music)
    void Replace(std::vector<bool> songs) {
        m_replaced = std::move(songs);
    }
    bool Replaced(uint16_t song) const {
        return song < m_replaced.size() && m_replaced[song];
    }

    static constexpr uint32_t kFull = 0x10000;
    struct State {
        int32_t song = -1;      // the song a file is playing for (-1: none; the board's music, if any, is heard)
        uint32_t serial = 0;    // counts the starts: the same song asked for again starts again
        uint32_t frames = 0;    // frames since it started
        uint32_t level = kFull; // its volume, with the game's music fades (kFull: as loud as it goes)
        int32_t step = 0;       // ... and how much that changes in a frame (0: it stays)
        uint32_t target = kFull;
        uint32_t allLevel = kFull; // everything's fade (commands 0C and 0D)
        int32_t allStep = 0;
    };
    State state;

    // The game has left a request (the shared memory's first sixteen bytes): looked at, and changed when it asks
    // for music the player has a file for
    // (CPS2RECOMP_SOUNDTRACK_LOG: each piece of music asked for, and each stop and fade, to stderr with the frame)
    void Request(uint8_t *shared, uint64_t frame);
    // A frame has passed
    void Frame();
    void Reset() {
        state = State{};
    }
    void Serialize(Archive &ar);

private:
    std::vector<bool> m_music, m_replaced;
    std::vector<uint16_t> m_songs;
};

} // namespace cps2recomp
