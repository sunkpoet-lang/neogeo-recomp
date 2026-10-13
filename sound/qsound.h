// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The QSound chip (Capcom DL-1425): sixteen voices playing 8-bit samples from ROM at any rate, each with a
// volume, a place between left and right and a share sent to an echo.
//
// On the board this is a DSP running a program from its own internal ROM. That ROM is not part of a game's
// ROM set, so this is not a model of the DSP: it does what the program does with the registers the sound
// driver writes, after MAME's high-level QSound (src/devices/sound/qsoundhle.cpp, BSD-3-Clause, by superctr
// and Valley Bell). Left out because they need the DSP ROM's tables: the "Q1" spatial filter (so the
// stereo picture is plain panning) and the three ADPCM voices (which Capcom's games do not use).
#pragma once

#include <cstdint>
#include <vector>

namespace cps2recomp {

class Archive;

class QSound {
public:
    static constexpr int kSampleRate = 24038; // 60 MHz / 2 / 1248

    QSound(const uint8_t *samples, size_t size)
        : m_rom(samples)
        , m_romSize(size) {
        Reset();
    }
    void Reset();
    // The three write ports (data high, data low, register) and the read port (bit 7: ready)
    void Write(unsigned port, uint8_t value);
    uint8_t Read() const {
        return m_ready;
    }
    // One output sample (left, right)
    void Step(int16_t out[2]);
    void Serialize(Archive &ar);

private:
    struct Voice {
        uint16_t bank = 0x8000;
        int16_t addr = 0;
        uint16_t phase = 0;
        uint16_t rate = 0;
        int16_t loopLen = 0;
        int16_t endAddr = 0;
        int16_t volume = 0;
        int16_t echo = 0;
    };
    struct Echo {
        uint16_t endPos = 0;
        int16_t feedback = 0;
        int16_t length = 0;
        int16_t lastSample = 0;
        int16_t delayLine[1024] = {};
        int16_t delayPos = 0;
    };
    const uint8_t *m_rom;
    size_t m_romSize;
    Voice m_voice[16];
    uint16_t m_pan[16] = {};
    Echo m_echo;
    uint16_t m_dataLatch = 0;
    uint16_t m_nextState = 0;
    uint8_t m_ready = 0;
    int m_busy = 0;        // samples left of a (re)start, during which the chip is not ready
    int m_counter = 0;     // the program looks at the next-state register every six samples
    bool m_secondMode = false;

    void WriteRegister(uint8_t address, uint16_t data);
    void Init(bool secondMode);
};

} // namespace cps2recomp
