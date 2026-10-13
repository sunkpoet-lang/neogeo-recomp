// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Capcom's QSound sound driver, version 2.01b (April 1996), as ordinary code: what the board's Z80 program
// does with the commands the game leaves in the shared memory, without the Z80. It plays sequences from the
// sound ROM (music on sixteen tracks, sound effects on sixteen more that take the voices over while they
// last, single samples, three ADPCM voices) and writes the QSound chip's registers.
//
// Written from the program in the ROM (the addresses in the comments are its routines), and checked against
// it: with the Z80 running the original beside it, every register write must be the same, in the same
// order (--sound check). A game whose sound ROM holds another version of the driver keeps the Z80.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace cps2recomp {

class Archive;

class SoundDriver {
public:
    // Whether the sound program is the version this was written from
    static bool Knows(const std::vector<uint8_t> &program);

    // `shared`: the 0x1000 bytes both processors see (the Z80's C000-CFFF); `chip`: a register write, and the
    // Z80 cycles the program would have taken to come to it since the write before (or since it started on
    // the piece of work): the chip takes one write a sample, and the program's sums take time, so voices
    // that start together start a few samples apart, which is heard where they play the same thing
    SoundDriver(const std::vector<uint8_t> &program, uint8_t *shared, std::function<void(uint8_t, uint16_t, uint32_t)> chip);

    // Beside the Z80, for checking: reads the shared memory but leaves it alone, and does each piece of work
    // when it is told the Z80 has started on it (the Z80 takes time over its work, and what order it does
    // things in depends on when; the driver by itself is never behind)
    bool shadow = false;
    enum class Work { Commands = 1, Sequences = 2, Adpcm = 3 };
    void Do(Work work);
    void TakeCommand(); // (... and when the Z80's interrupt takes a command)
    void SetStereo(uint8_t stereo) {
        m.stereo = stereo;
    }

    void Reset();
    // The driver's interrupt, 250 times a second, and the work that has come due
    void Interrupt();
    void Serialize(Archive &ar);
    void Describe(unsigned voice) const; // (for looking into a difference: a voice's tracks, to stderr)

private:
    // A sound's place between left (0) and right (0x20), and how it moves
    struct Pan {
        uint8_t mode;   // 0xFF: stays; 0: to and fro; 1: to the target; 2: to the target, then on to the end
        uint16_t at;    // the place, in 256ths
        uint8_t step;   // (to and fro) the way to the turning point after the next
        uint8_t target;
        uint16_t speed, speed2;
        uint8_t delay;  // sequence ticks before the sound starts
    };
    struct Track {
        uint8_t state;  // 0: off; 1: a single sample; 0xFF: a sequence; 0x80: a sequence past its repeat mark
        uint8_t priority;
        uint32_t time;  // until the next event (sequences: 16.16 ticks; single samples: ticks)
        uint32_t gate;  // until the note is let go
        uint32_t place, start;
        uint8_t loopCount[4];
        uint32_t loopPlace[4];
        uint16_t pitchOffset;
        uint8_t transpose, transposeAdd;
        uint16_t fineTune;
        uint8_t sampleKey, instrumentTune;
        uint8_t flags;
        uint8_t slide;       // portamento: 0xFF under way, 1 arrived
        uint16_t slideRate;
        uint16_t pitch, target; // semitones, in 256ths
        uint16_t trackVolume;
        uint8_t expression, volumeOffset, volumeAdd;
        uint16_t volume;
        uint16_t instruments; // where the track's instrument table is
        uint8_t envelope;     // 0 attack, 1 decay, 3 sustain, 4 release, 5 still
        uint16_t envelopeRate, level;
        uint8_t attack, decay, sustainRate, release;
        uint16_t sustain;
        uint16_t noteVolume;
        uint16_t sample, sampleStart, echo;
        uint16_t tempo;
        bool pending, hold; // (single samples) not started yet; plays until stopped
        Pan pan;
        uint16_t lfoRate, lfoPhase, vibrato, tremolo;
    };
    struct Adpcm {
        uint8_t starting, repeats, priority;
        uint16_t left, length, volume;
        uint8_t bank;
        uint16_t start, end;
        Pan pan;
    };
    struct State {
        uint8_t booting, deaf;
        bool started;
        uint8_t ticks, commandsDue, sequenceDue, adpcmDue;
        uint8_t queue[256], queueIn, queueOut;
        uint8_t stereo;
        uint8_t masterVolume, musicVolume;
        uint16_t songs;
        uint32_t songTable, song;
        uint16_t musicTempo, tempoAdjust, pitchShift;
        uint8_t fade;        // everything: 1 coming up, 0xFF going down
        uint16_t fadeRate, fadeLevel;
        uint8_t musicFade;   // the music alone
        uint16_t musicFadeRate, musicFadeLevel, musicFadeTarget;
        uint16_t echoLength;
        Track effects[16], music[16];
        Adpcm adpcm[3];
    };

    const uint8_t *m_rom;
    size_t m_size;
    uint8_t *m_shared;
    std::function<void(uint8_t, uint16_t, uint32_t)> m_chip;
    uint32_t m_spent = 0; // Z80 cycles since the last write
    State m = {};
    // While a voice is being worked on
    uint8_t m_voice = 0;
    bool m_music = false; // one of the music's tracks
    bool m_taken = false; // a sound effect has the voice: the music's track runs on unheard
    uint8_t m_priority = 0;

    uint8_t Rom(uint32_t at) const {
        return at < m_size ? m_rom[at] : 0xFF;
    }
    uint16_t Word(uint32_t at) const {
        return static_cast<uint16_t>(Rom(at) | (Rom(at + 1) << 8));
    }
    uint16_t Table(uint32_t table, uint8_t index) const {
        return Word(table + static_cast<uint8_t>(index * 2));
    }
    uint8_t Next(uint32_t &place) const {
        return Rom(place++);
    }
    uint16_t NextWord(uint32_t &place) const {
        const uint8_t high = Next(place);
        return static_cast<uint16_t>((high << 8) | Next(place));
    }
    void Chip(unsigned reg, uint16_t data) {
        m_chip(static_cast<uint8_t>(reg), data, m_spent + kWriteTime);
        m_spent = 0;
    }
    // The program's time, roughly: what it does between writes, in Z80 cycles
    static constexpr uint32_t kWriteTime = 100;
    void Spend(uint32_t cycles) {
        m_spent += cycles;
    }
    // Its multiplication goes through the first number bit by bit, and adds for each bit that is set
    uint32_t Product(uint16_t a, uint16_t b) {
        Spend(660 + 17 * static_cast<uint32_t>(__builtin_popcount(a)));
        return static_cast<uint32_t>(a) * b;
    }
    // The high word of a product: a level times a fraction of 65536
    uint16_t Scale(uint16_t a, uint16_t b) {
        return static_cast<uint16_t>(Product(a, b) >> 16);
    }
    // ... and its middle word
    uint16_t Middle(uint16_t a, uint16_t b) {
        return static_cast<uint16_t>(Product(a, b) >> 8);
    }
    void Share(unsigned at, uint8_t value) {
        if (!shadow) {
            m_shared[at] = value;
        }
    }

    void StartUp();
    void Command();
    void Special(uint8_t *command);
    void StartMusic(uint32_t place);
    void StartEffect(uint32_t place, uint8_t priority, const uint8_t *command);
    void StartSample(uint32_t place, const uint8_t *command);
    void StartAdpcm(uint32_t place, const uint8_t *command);
    bool SetUpPan(Pan &pan, const uint8_t *command);
    bool MovePan(Pan &pan);
    void PlaceEffect(Track &track, const uint8_t *command);
    void PlacePan(Track &track);
    void WritePan(uint8_t place);
    void Silence(uint8_t voice);
    void SilenceAdpcm(uint8_t voice);
    void SilenceAll();
    void StopAdpcm();
    void SequenceTick();
    void Fades();
    void RunSample(Track &track);
    void RunSequence(Track &track, uint16_t tempo);
    void ReadSequence(Track &track);
    void EndTrack(Track &track);
    void EndEffect(Track &track);
    void SetSample(Track &track, uint16_t number);
    void KeyOn(Track &track);
    void SetPitchOffset(Track &track);
    void SetVolume(Track &track);
    void LetGo(Track &track);
    void Update(Track &track);
    void AdpcmTick();
    void RunAdpcm(Adpcm &voice);
};

} // namespace cps2recomp
