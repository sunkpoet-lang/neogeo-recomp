// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// A custom soundtrack: the player's own files (an arranged soundtrack, say) played for the game's music, in place
// of the board's. machine/soundtrack.h finds the music the game asks for and keeps what is playing in the game's
// time; this plays it.
//
// The files are in a folder of the player's (--soundtrack DIR; the launcher's game page chooses it), one a song,
// named by the song's name (ryu_stage.ogg; --list-songs and the folder's songs.txt list them) or its number in
// hexadecimal (2A.ogg), as Ogg Vorbis, FLAC, MP3 or WAV. soundtrack.ini in the folder can name another file for a
// song, its loop and its volume:
//
//   volume = 80                 ; every song, in percent of the file's own loudness
//   [ryu_stage]                 ; or [2A]
//   file = 05 Ryu's Theme.flac
//   loop = 0:12.5 - 2:47.25     ; the part that repeats: start - end (minutes:seconds, or seconds); "no": played once
//   volume = 90
//
// Without a loop, a file repeats from its LOOPSTART tag (and LOOPEND or LOOPLENGTH, in samples) where it has one,
// or whole; a song the board plays once (a fanfare, a game over) is played once.
#pragma once

#include "../machine/soundtrack.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace port {

struct Decoder;

// A path as UTF-8 text (as the rest of the port keeps paths), and back
inline std::string PathText(const std::filesystem::path &path) {
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
}
inline std::filesystem::path TextPath(const std::string &text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

class SoundtrackPlayer {
public:
    SoundtrackPlayer();
    ~SoundtrackPlayer();

    // Looks in the folder for the songs' files (the game's music: `tap`'s, with the names the game's sources give)
    // and tells the machine which songs it has. Returns how many.
    int Open(const std::string &folder, cps2recomp::Soundtrack &tap);
    bool Any() const {
        return !m_files.empty();
    }
    // The file the player has for a song ("": none)
    std::string FileFor(uint16_t song) const;

    // The samples for the game's time up to now: what the machine says is playing, from where the samples given
    // before ended (left and right, at Rate()). Empty when nothing is playing. `heard`: the sound is played (not
    // fast-forwarded or held); when it is not, nothing is made and the next call starts where the game is.
    void Make(const cps2recomp::Soundtrack::State &state, bool heard, std::vector<float> &out);
    int Rate() const {
        return m_rate;
    }
    int volume = 100; // (percent) every song

private:
    struct File {
        std::string path;
        double loopStart = -1, loopEnd = -1; // seconds (-1: from the file's tags, or the whole of it)
        bool once = false;                   // played once, not repeated
        int volume = 100;
    };
    std::vector<File> m_files; // by song number (path "": none)
    std::unique_ptr<Decoder> m_decoder;
    int32_t m_song = -1;
    uint32_t m_serial = 0;
    uint64_t m_made = 0;           // samples made since the song started (counting each repeat)
    uint64_t m_at = 0;             // the decoder's place in the file
    uint64_t m_loopStart = 0, m_loopEnd = 0; // in samples (end 0: the file's end, when it is found)
    bool m_once = false, m_ended = false;
    float m_gain = 0;              // (the last sample's: the next frame's starts there)
    int m_rate = 0;
    void Start(int32_t song);
    void SeekTo(uint64_t made);
    uint64_t Place(uint64_t made) const; // where in the file a count of samples made since the start is
};

// The game's songs as the player's folder knows them: each piece of music's name and number, and what it is
std::string SongList(const cps2recomp::Soundtrack &tap);

} // namespace port
