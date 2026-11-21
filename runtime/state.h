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
    Archive(std::vector<uint8_t> &data, bool saving, bool checking = false)
        : m_data(data)
        , m_saving(saving), m_checking(checking) {}

    // The version of the save state being read, where the port knows it (0: as this build writes): a part added
    // since is read only from states that have it
    uint32_t version = 0;

    bool Saving() const {
        return m_saving || m_checking;
    }
    // False once something read did not fit what was written
    bool Ok() const {
        return m_ok;
    }
    bool Complete() const { return m_ok && m_at == m_data.size(); }
    void Require(bool value) { m_ok=m_ok&&value; }
    void Bytes(void *p, size_t size) {
        if (m_saving) {
            const uint8_t *bytes = static_cast<const uint8_t *>(p);
            m_data.insert(m_data.end(), bytes, bytes + size);
        } else if (m_at + size <= m_data.size()) {
            if (!m_checking) memcpy(p, &m_data[m_at], size);
            m_at += size;
        } else {
            m_ok = false;
        }
    }
    // A plain value or structure (no pointers into what is not saved)
    template <typename T>
    void Pod(T &value) {
        static_assert(std::is_trivially_copyable_v<T>);
        if constexpr (std::is_same_v<T, bool>) {
            if (m_checking && m_at < m_data.size() && m_data[m_at] > 1) m_ok = false;
        }
        Bytes(&value, sizeof(T));
    }
    template <typename T>
    void Vec(std::vector<T> &values) {
        static_assert(std::is_trivially_copyable_v<T>);
        uint64_t count = values.size();
        if (m_checking) {
            if (m_at > m_data.size() || sizeof(count) > m_data.size()-m_at) { m_ok=false; return; }
            memcpy(&count,m_data.data()+m_at,sizeof(count)); m_at+=sizeof(count);
            if (count != values.size() || count > (m_data.size()-m_at)/sizeof(T)) { m_ok=false; return; }
            m_at += static_cast<size_t>(count)*sizeof(T); return;
        }
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
    // Optional application-owned data with a deliberate bound (not a chip's fixed
    // RAM layout). Preflight accepts different lengths without allocating them.
    template <typename T> void BoundedVec(std::vector<T>& values,size_t maximum) {
        static_assert(std::is_trivially_copyable_v<T>);
        uint64_t count=values.size();
        if(m_saving){if(count>maximum){m_ok=false;return;}Pod(count);Bytes(values.data(),count*sizeof(T));return;}
        if(m_at>m_data.size()||sizeof(count)>m_data.size()-m_at){m_ok=false;return;}
        memcpy(&count,m_data.data()+m_at,sizeof(count));m_at+=sizeof(count);
        if(count>maximum||count>(m_data.size()-m_at)/sizeof(T)){m_ok=false;return;}
        if(m_checking){m_at+=count*sizeof(T);return;}
        values.resize(static_cast<size_t>(count));Bytes(values.data(),count*sizeof(T));
    }
    void Mark(uint32_t mark) {
        uint32_t value = mark;
        if (m_checking) {
            if (m_at > m_data.size() || sizeof(value)>m_data.size()-m_at) {m_ok=false; return;}
            memcpy(&value,m_data.data()+m_at,sizeof(value));m_at+=sizeof(value);
            m_ok = m_ok && value==mark; return;
        }
        Pod(value);
        m_ok = m_ok && value == mark;
    }

private:
    std::vector<uint8_t> &m_data;
    size_t m_at = 0;
    bool m_saving, m_checking = false, m_ok = true;
};

} // namespace cps2recomp
