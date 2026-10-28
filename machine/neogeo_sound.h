// SPDX-License-Identifier: GPL-3.0-only
// Part of cps2recomp (the user's fork: the Neo Geo board).
//
// The Neo Geo's sound chip, a YM2610 at 8 MHz: FM, the SSG, ADPCM-A and ADPCM-B, played from the cartridge's V samples.
// The chip is ymfm's (third_party/ymfm_opn.*, Aaron Giles, BSD-3-Clause), unchanged; this is what it is wired to on the
// board: its samples, its two timers (which interrupt the Z80) and its output, mixed as MAME mixes it.
//
// It runs on the Z80's time: the board brings it up to the Z80's clock (twice the Z80's cycles) before each access and
// after each instruction, so writes land when they land on the board, and the timers and the interrupt are where the
// Z80 sees them. Its whole state is in the board's save states and hash.
#pragma once

#include "../third_party/ymfm_opn.h"

#include <cstdint>
#include <vector>

namespace cps2recomp {

class Archive;

class NeoGeoSound final : public ymfm::ymfm_interface {
public:
    static constexpr uint32_t kClock = 8000000;
    static constexpr uint32_t kClocksPerSample = 144; // (the chip's own rate: 55,555 samples a second)

    explicit NeoGeoSound(const std::vector<uint8_t> &samples);
    void Reset();
    static constexpr uint32_t SampleRate() {
        return kClock / kClocksPerSample;
    }

    // The Z80's ports 0x04-0x07: address and data, low and high halves
    uint8_t Read(unsigned port);
    void Write(unsigned port, uint8_t value);
    // Runs the chip to this clock (since power-on): its samples made, its timers run out
    void RunTo(uint64_t clock);
    bool Interrupt() const {
        return m_interrupt;
    }

    // What has been made since the frontend last took it: 16-bit stereo, left first
    std::vector<int16_t> made;
    bool keep = true; // (false: made is not filled; the chip runs the same)

    void Serialize(Archive &ar);

    // ymfm_interface
    void ymfm_set_timer(uint32_t tnum, int32_t durationInClocks) override;
    void ymfm_update_irq(bool asserted) override {
        m_interrupt = asserted;
    }
    uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override;

private:
    void Sample();

    const std::vector<uint8_t> &m_samples;
    ymfm::ym2610 m_chip;
    uint64_t m_clock = 0;
    int64_t m_timerAt[2] = {-1, -1}; // the clock each timer runs out at (-1: stopped)
    uint64_t m_nextSample = 0;
    bool m_interrupt = false;
};

} // namespace cps2recomp
