// SPDX-License-Identifier: GPL-3.0-only
// Part of neogeo-recomp.

#include "neogeo_sound.h"

#include "../runtime/state.h"

#include <algorithm>

namespace cps2recomp {

NeoGeoSound::NeoGeoSound(const std::vector<uint8_t> &samples)
    : m_samples(samples)
    , m_chip(*this) {
    // (the chip's own rate, one sample every 144 clocks, as the board makes it)
    m_chip.set_fidelity(ymfm::OPN_FIDELITY_MED);
    Reset();
}

void NeoGeoSound::Reset() {
    m_chip.reset();
    m_timerAt[0] = m_timerAt[1] = -1;
    m_interrupt = false;
    m_nextSample = m_clock;
}

uint8_t NeoGeoSound::Read(unsigned port) {
    return m_chip.read(port & 3);
}

void NeoGeoSound::Write(unsigned port, uint8_t value) {
    m_chip.write(port & 3, value);
}

void NeoGeoSound::ymfm_set_timer(uint32_t tnum, int32_t durationInClocks) {
    if (tnum < 2) {
        m_timerAt[tnum] = durationInClocks < 0 ? -1 : static_cast<int64_t>(m_clock) + durationInClocks;
    }
}

uint8_t NeoGeoSound::ymfm_external_read(ymfm::access_class type, uint32_t address) {
    // ADPCM-A and ADPCM-B both read the V samples (the cartridges with no second set for ADPCM-B, as MAME has them)
    if (type == ymfm::ACCESS_ADPCM_A || type == ymfm::ACCESS_ADPCM_B) {
        return address < m_samples.size() ? m_samples[address] : 0;
    }
    return 0;
}

void NeoGeoSound::RunTo(uint64_t clock) {
    while (m_clock < clock) {
        // the next thing that happens: a sample, a timer, or the clock asked for
        uint64_t next = std::min(clock, m_nextSample);
        for (int64_t at : m_timerAt) {
            if (at >= 0 && static_cast<uint64_t>(at) < next) {
                next = static_cast<uint64_t>(at);
            }
        }
        m_clock = std::max(next, m_clock);
        for (uint32_t t = 0; t < 2; t++) {
            if (m_timerAt[t] >= 0 && static_cast<uint64_t>(m_timerAt[t]) <= m_clock) {
                m_timerAt[t] = -1;
                m_engine->engine_timer_expired(t); // (which may set the timer going again)
            }
        }
        if (m_clock >= m_nextSample) {
            Sample();
            m_nextSample += kClocksPerSample;
        }
    }
}

void NeoGeoSound::Sample() {
    ymfm::ym2610::output_data out;
    m_chip.generate(&out);
    if (!keep) {
        return;
    }
    // FM and ADPCM in stereo (outputs 0 and 1) at 0.98, the SSG (output 2, mono) at 0.28 on both sides, as MAME's
    // Neo Geo routes them
    auto clamp = [](int32_t v) { return static_cast<int16_t>(std::clamp(v, -32768, 32767)); };
    const int32_t ssg = out.data[2] * 28 / 100;
    made.push_back(clamp(out.data[0] * 98 / 100 + ssg));
    made.push_back(clamp(out.data[1] * 98 / 100 + ssg));
}

void NeoGeoSound::Serialize(Archive &ar) {
    ar.Mark(0x594D3231); // "YM21"
    std::vector<uint8_t> chip;
    if (ar.Saving()) {
        ymfm::ymfm_saved_state state(chip, true);
        m_chip.save_restore(state);
    }
    ar.Vec(chip);
    if (!ar.Saving() && ar.Ok()) {
        ymfm::ymfm_saved_state state(chip, false);
        m_chip.save_restore(state);
    }
    ar.Pod(m_clock);
    ar.Pod(m_timerAt);
    ar.Pod(m_nextSample);
    ar.Pod(m_interrupt);
}

} // namespace cps2recomp
