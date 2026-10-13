// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Save states: one description of a piece of state serves both to write it and to read it back.
//
//   void Thing::Serialize(Archive &ar) { ar.Pod(m_counter); ar.Vec(m_bytes); }
//
// A state is only read back by the build that wrote it (the header carries the game and a version; the
// sizes of everything written must be the same).
#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

namespace cps2recomp {

class Archive {
public:
    Archive(std::vector<uint8_t> &data, bool saving)
        : m_data(data)
        , m_saving(saving) {}

    bool Saving() const {
        return m_saving;
    }
    // False once something read did not fit what was written
    bool Ok() const {
        return m_ok;
    }
    void Bytes(void *p, size_t size) {
        if (m_saving) {
            const uint8_t *bytes = static_cast<const uint8_t *>(p);
            m_data.insert(m_data.end(), bytes, bytes + size);
        } else if (m_at + size <= m_data.size()) {
            memcpy(p, &m_data[m_at], size);
            m_at += size;
        } else {
            m_ok = false;
        }
    }
    // A plain value or structure (no pointers into what is not saved)
    template <typename T>
    void Pod(T &value) {
        static_assert(std::is_trivially_copyable_v<T>);
        Bytes(&value, sizeof(T));
    }
    template <typename T>
    void Vec(std::vector<T> &values) {
        static_assert(std::is_trivially_copyable_v<T>);
        uint64_t count = values.size();
        Pod(count);
        if (!m_saving) {
            if (!m_ok || count * sizeof(T) > m_data.size() - m_at) {
                m_ok = false;
                return;
            }
            values.resize(static_cast<size_t>(count));
        }
        Bytes(values.data(), static_cast<size_t>(count) * sizeof(T));
    }
    // A mark between parts: reading stops making sense if it is not where it was written
    void Mark(uint32_t mark) {
        uint32_t value = mark;
        Pod(value);
        m_ok = m_ok && value == mark;
    }

private:
    std::vector<uint8_t> &m_data;
    size_t m_at = 0;
    bool m_saving, m_ok = true;
};

} // namespace cps2recomp
