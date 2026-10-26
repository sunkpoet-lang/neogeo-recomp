// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The sound chips of the classic CPS-1 board (this fork: Street Fighter II Champion Edition and Hyper Fighting):
// a YM2151 (ymfm's, third_party/) for the music and an OKI MSM6295 (sound/okim6295.h) for the voices, both played
// by the board's Z80 at 3.579545 MHz. Everything here is timed in that Z80's cycles: the YM2151's timers (whose
// interrupt is the Z80's), its samples (a clock in 64), the OKI's (1 MHz in 132 or 165) and the game's output,
// mono, at QSound::kSampleRate (the rate the rest of the port takes).
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace cps2recomp {

class Archive;

class Cps1Sound {
public:
    static constexpr uint64_t kClock = 3579545;   // the Z80's and the YM2151's
    static constexpr uint64_t kOkiClock = 1000000; // the OKI's

    Cps1Sound(const uint8_t *samples, size_t size);
    ~Cps1Sound();
    void Reset();

    // The Z80's ports, at its cycle count `now`
    void WriteYm(unsigned port, uint8_t value, uint64_t now);
    uint8_t ReadYm(uint64_t now);
    void WriteOki(uint8_t value);
    uint8_t ReadOki() const;
    void SetPin7(bool high);

    // The YM2151's interrupt line (the Z80's)
    bool Interrupt() const;
    // The cycle count of the next thing due
    uint64_t NextEvent() const;
    // Everything due up to `now`, the samples it makes added to `audio` (stereo, both the same)
    void RunTo(uint64_t now, std::vector<int16_t> &audio);
    void Serialize(Archive &ar);
    // (for logs) the YM2151's timers run out since the chips were made
    uint64_t TimersRunOut() const;

private:
    struct Chips;
    std::unique_ptr<Chips> m;
};

} // namespace cps2recomp
