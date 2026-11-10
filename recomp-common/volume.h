// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy and ForkAde contributors. Part of recomp-common (ForkAde's fork).
// A game's sound level (ForkAde's openspec change game-volume-control): a percentage from 0 (silent) to 100 (the
// sound as the game makes it), in 5 percent steps, turned into the output's gain by a perceptual taper (the
// one ForkBoy's engine uses, retropp::perceptualGain: a power of 1.66, so half the slider sounds about half,
// some -10 dB). 100 percent is a gain of exactly 1, so a game at full level sounds as it always did; 0 is
// silence. Muting is separate, so unmuting returns to the level chosen.
#pragma once

#include <algorithm>
#include <cmath>

namespace recomp {

constexpr int kVolumeStep = 5;
constexpr int kVolumeFull = 100;

// A level kept to 0-100, on a 5 percent step
inline int ClampVolume(int percent) {
    percent = std::clamp(percent, 0, kVolumeFull);
    return (percent + kVolumeStep / 2) / kVolumeStep * kVolumeStep;
}

// The level `steps` steps louder (or quieter, below 0)
inline int StepVolume(int percent, int steps) {
    return ClampVolume(ClampVolume(percent) + steps * kVolumeStep);
}

// The gain for a level: 0 at 0, exactly 1 at 100, the perceptual taper between
inline float VolumeGain(int percent) {
    percent = std::clamp(percent, 0, kVolumeFull);
    if (percent == 0) {
        return 0.0f;
    }
    if (percent == kVolumeFull) {
        return 1.0f;
    }
    return static_cast<float>(std::pow(percent / 100.0, 1.66));
}

} // namespace recomp
