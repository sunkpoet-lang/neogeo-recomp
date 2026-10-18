// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The board's settings memory: a 93C46 serial EEPROM, 64 words of 16 bits (a CPS-2's; a CPS-1 with QSound wires
// it as 128 bytes: eightBit), which the game talks to one bit at a time over three lines (select, clock, data in)
// and one back (data out).
#pragma once

#include <cstdint>

namespace cps2recomp {

class Archive;

class Eeprom {
public:
    uint16_t words[64];   // (with eightBit, byte n is the nth byte of this as it is in memory)
    bool changed = false; // (something was written since this was last cleared)
    bool eightBit = false; // (set by the machine for its board: not part of a save state)

    Eeprom();
    void SetLines(bool select, bool clock, bool dataIn);
    bool DataOut() const {
        return m_out;
    }
    void Serialize(Archive &ar);

private:
    int Bits() const {
        return eightBit ? 8 : 16;
    }
    int AddressMask() const {
        return eightBit ? 127 : 63;
    }
    uint32_t Get(int at) const;
    void Set(int at, uint32_t value);
    bool m_select = false, m_clock = false;
    bool m_out = true;
    bool m_writable = false;
    bool m_started = false;   // the start bit has come
    uint32_t m_shift = 0;     // bits received since the start bit
    int m_count = 0;
    int m_reading = -1;       // bits left to send, or -1
    uint32_t m_sending = 0;
    int m_address = 0;
    int m_writing = 0; // 0: no; 1: one word; 2: every word
};

} // namespace cps2recomp
