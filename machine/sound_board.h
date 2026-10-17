// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// A second sound side: another set's Z80 sound program and QSound samples (game.cfg's extra_set), for a game that
// runs code of that set's which asks for that set's sounds (its characters' voices, numbered as that set numbers
// them). It is the board's sound side again, run beside the machine's own and on the same clock; its samples are
// added to the machine's. The 68000 does not write to it: the game's sources give it the requests (a sound's
// number and the rest, as the game's own queue holds them), and it hands them to its Z80 as the game hands its own
// to the board's (Vampire Savior: 003136): when the Z80 has taken the one before.
#pragma once

#include "../sound/qsound.h"
#include "../sound/z80.h"
#include "romset.h"

#include <array>
#include <cstdint>
#include <vector>

namespace cps2recomp {

class Archive;

class SoundBoard final : public Z80Bus {
public:
    explicit SoundBoard(const RomSet &roms);
    void Reset();
    // As the machine's own: let run (from a reset) or held
    void SetRunning(bool run);
    // A request: the three longs of the game's queue entry (the sound's number first)
    void Request(uint32_t number, uint32_t second, uint32_t third);
    // A byte the 68000 writes to the shared memory that is not a request (the game's settings, stereo or mono and
    // sound on, and its greeting at start-up): as it writes the machine's
    void Shared(uint16_t at, uint8_t value) {
        m_shared[at & 0xFFF] = value;
    }
    // Runs to `cycles` on the sound side's clock (the machine's Z80's); then one sample (left, right)
    void RunTo(uint64_t cycles);
    void Step(int16_t out[2]) {
        m_chip.Step(out);
    }
    void Serialize(Archive &ar);

    // Z80Bus
    uint8_t Read(uint16_t address) override;
    void Write(uint16_t address, uint8_t value) override;
    bool ChangesMapping(uint16_t address) override {
        return address == 0xD003;
    }
    uint32_t Mapping() override {
        return m_bank;
    }
    void SetMapping(uint32_t mapping) override {
        m_bank = static_cast<uint8_t>(mapping);
    }

private:
    const RomSet &m_roms;
    Z80 m_cpu;
    QSound m_chip;
    uint8_t m_shared[0x1000] = {}; // as the board's: the 68000's 618000-619FFF, bytes; the Z80's C000-CFFF
    uint8_t m_own[0x1000] = {};    // the Z80's F000-FFFF
    uint8_t m_bank = 0;
    bool m_running = false;
    uint64_t m_interruptAt = 0;
    std::vector<std::array<uint8_t, 12>> m_waiting;
    void Send();
};

} // namespace cps2recomp
