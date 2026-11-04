// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy and ForkAde contributors. Part of recomp-common (ForkAde's fork).
// The in-game chat (chat.h)
#include "chat.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <system_error>

namespace recomp::chat {

namespace {
    // The next character of strictly valid UTF-8, or false (a cut sequence, an overlong form, a surrogate, a value
    // above U+10FFFF)
    bool Next(const std::string &s, size_t &at, uint32_t &code) {
        const auto byte = [&](size_t i) { return static_cast<unsigned char>(s[i]); };
        const unsigned c = byte(at);
        size_t n;
        uint32_t least;
        if (c < 0x80) {
            code = c;
            at++;
            return true;
        } else if (c >= 0xC2 && c <= 0xDF) {
            n = 2, least = 0x80, code = c & 0x1F;
        } else if (c >= 0xE0 && c <= 0xEF) {
            n = 3, least = 0x800, code = c & 0x0F;
        } else if (c >= 0xF0 && c <= 0xF4) {
            n = 4, least = 0x10000, code = c & 0x07;
        } else {
            return false;
        }
        if (at + n > s.size()) {
            return false;
        }
        for (size_t i = 1; i < n; i++) {
            if ((byte(at + i) & 0xC0) != 0x80) {
                return false;
            }
            code = code << 6 | (byte(at + i) & 0x3F);
        }
        if (code < least || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
            return false;
        }
        at += n;
        return true;
    }

    bool Removed(uint32_t c) {
        return c < 0x20 || (c >= 0x7F && c <= 0x9F) || (c >= 0x200B && c <= 0x200F) || (c >= 0x202A && c <= 0x202E) ||
               (c >= 0x2060 && c <= 0x2064) || (c >= 0x2066 && c <= 0x2069) || c == 0xFEFF;
    }

    void Put16(std::vector<uint8_t> &out, uint32_t v) {
        out.push_back(static_cast<uint8_t>(v));
        out.push_back(static_cast<uint8_t>(v >> 8));
    }
    void Put32(std::vector<uint8_t> &out, uint32_t v) {
        Put16(out, v & 0xFFFF);
        Put16(out, v >> 16);
    }
    uint32_t Get32(const uint8_t *p) {
        return p[0] | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24;
    }

    std::string Json(const std::string &s) {
        std::string out = "\"";
        for (const char c : s) {
            const unsigned char u = static_cast<unsigned char>(c);
            if (c == '"' || c == '\\') {
                out += '\\';
                out += c;
            } else if (u < 0x20) {
                char hex[8];
                std::snprintf(hex, sizeof(hex), "\\u%04x", u);
                out += hex;
            } else {
                out += c;
            }
        }
        return out + "\"";
    }
} // namespace

bool Clean(const std::string &text, size_t most, std::string &out) {
    // (the characters kept, as their bytes)
    std::vector<std::string> kept;
    for (size_t at = 0; at < text.size();) {
        const size_t from = at;
        uint32_t code = 0;
        if (!Next(text, at, code)) {
            out.clear();
            return false;
        }
        if (!Removed(code)) {
            kept.push_back(text.substr(from, at - from));
        }
    }
    size_t first = 0, last = kept.size();
    while (first < last && kept[first] == " ") {
        first++;
    }
    while (last > first && kept[last - 1] == " ") {
        last--;
    }
    last = std::min(last, first + most);
    while (last > first && kept[last - 1] == " ") {
        last--;
    }
    out.clear();
    for (size_t i = first; i < last; i++) {
        out += kept[i];
    }
    return true;
}

std::string Printable(const std::string &text, size_t most) {
    std::string out;
    Clean(text, most, out);
    return out;
}

std::vector<uint8_t> EncodeLine(uint32_t number, const std::string &text) {
    std::vector<uint8_t> out;
    const size_t size = std::min<size_t>(text.size(), 0xFFFF);
    Put32(out, number);
    Put16(out, static_cast<uint32_t>(size));
    out.insert(out.end(), text.begin(), text.begin() + size);
    return out;
}

bool DecodeLine(const uint8_t *data, size_t size, uint32_t &number, std::string &text) {
    if (size < 6) {
        return false;
    }
    const size_t length = data[4] | data[5] << 8;
    if (size < 6 + length) {
        return false;
    }
    number = Get32(data);
    text.assign(reinterpret_cast<const char *>(data + 6), length);
    return true;
}

std::vector<uint8_t> EncodeAck(uint32_t count) {
    std::vector<uint8_t> out;
    Put32(out, count);
    return out;
}

bool DecodeAck(const uint8_t *data, size_t size, uint32_t &count) {
    if (size < 4) {
        return false;
    }
    count = Get32(data);
    return true;
}

std::vector<uint8_t> HelloWithName(const std::vector<uint8_t> &hello, const std::string &name) {
    std::vector<uint8_t> out = hello;
    const std::string clean = Printable(name, kNameMost);
    out.push_back(0);
    out.insert(out.end(), clean.begin(), clean.end());
    return out;
}

void SplitHello(const std::vector<uint8_t> &hello, std::vector<uint8_t> &compared, std::string &name) {
    size_t zero = 0;
    while (zero < hello.size() && hello[zero] != 0) {
        zero++;
    }
    compared.assign(hello.begin(), hello.begin() + zero);
    name = zero < hello.size() ? Printable(std::string(hello.begin() + zero + 1, hello.end()), kNameMost) : "";
}

bool Rate::Allow(uint64_t now) {
    while (!m_times.empty() && now - m_times.front() >= kRateMs) {
        m_times.pop_front();
    }
    if (static_cast<int>(m_times.size()) >= kRateCount) {
        return false;
    }
    m_times.push_back(now);
    return true;
}

Channel::Said Channel::Say(const std::string &text, uint64_t now, std::string *line) {
    std::string clean;
    if (!Clean(text, kLineMost, clean) || clean.empty()) {
        return Said::Empty;
    }
    if (!m_sayRate.Allow(now)) {
        return Said::Wait;
    }
    m_unacknowledged.push_back({m_said++, clean});
    SendAll(now);
    if (line) {
        *line = clean;
    }
    return Said::Sent;
}

void Channel::SendAll(uint64_t now) {
    for (const auto &[number, text] : m_unacknowledged) {
        m_send('C', EncodeLine(number, text));
    }
    m_sentAt = now;
}

void Channel::Pump(uint64_t now) {
    if (!m_unacknowledged.empty() && now - m_sentAt >= kResendMs) {
        SendAll(now);
    }
}

void Channel::Receive(char kind, const uint8_t *data, size_t size, uint64_t now) {
    if (kind == 'C') {
        uint32_t number = 0;
        std::string text, clean;
        if (!DecodeLine(data, size, number, text)) {
            return;
        }
        // (the next line, or one again or ahead: either way, how many have come is said back)
        if (number == m_heardUpTo) {
            m_heardUpTo++;
            if (Clean(text, kLineMost, clean) && !clean.empty() && m_hearRate.Allow(now)) {
                m_heard.push_back(clean);
            }
        }
        m_send('A', EncodeAck(m_heardUpTo));
    } else if (kind == 'A') {
        uint32_t count = 0;
        if (!DecodeAck(data, size, count)) {
            return;
        }
        while (!m_unacknowledged.empty() && m_unacknowledged.front().first < count) {
            m_unacknowledged.pop_front();
        }
    }
}

std::vector<std::string> Channel::TakeHeard() {
    std::vector<std::string> taken;
    taken.swap(m_heard);
    return taken;
}

std::vector<std::string> Channel::Unacknowledged() const {
    std::vector<std::string> out;
    for (const auto &line : m_unacknowledged) {
        out.push_back(line.second);
    }
    return out;
}

std::string LocalTime(bool forFileName) {
    const std::time_t now = std::time(nullptr);
    std::tm local{}, utc{};
#ifdef _WIN32
    localtime_s(&local, &now);
    gmtime_s(&utc, &now);
#else
    localtime_r(&now, &local);
    gmtime_r(&now, &utc);
#endif
    char text[40];
    if (forFileName) {
        std::strftime(text, sizeof(text), "%Y-%m-%d_%H-%M-%S", &local);
        return text;
    }
    utc.tm_isdst = -1;
    std::tm localCopy = local;
    const long offset = static_cast<long>(std::difftime(std::mktime(&localCopy), std::mktime(&utc)) / 60);
    std::strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%S", &local);
    char zone[8];
    const long minutes = offset < 0 ? -offset : offset;
    std::snprintf(zone, sizeof(zone), "%c%02ld:%02ld", offset < 0 ? '-' : '+', minutes / 60, minutes % 60);
    return std::string(text) + zone;
}

Log::~Log() {
    if (m_file) {
        std::fclose(m_file);
    }
}

void Log::Start(const std::string &folder, const std::string &game, const std::string &build, bool host) {
    m_folder = folder;
    m_game = game;
    m_build = build;
    m_host = host;
    m_started = LocalTime();
    m_stamp = LocalTime(true);
}

void Log::Names(const std::string &me, const std::string &them) {
    m_me = me;
    m_them = them;
}

void Log::Write(const char *dir, const std::string &nick, const std::string &text) {
    if (m_folder.empty()) {
        return;
    }
    if (!m_file) {
        std::error_code error;
        const std::filesystem::path folder = PathOf(m_folder);
        std::filesystem::create_directories(folder, error);
        std::string safe; // (the game's id, as a file name may have it)
        for (const char c : m_game) {
            safe += (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') ? c : '_';
        }
        const std::filesystem::path path = folder / PathOf(m_stamp + "_" + safe + ".jsonl");
#ifdef _WIN32
        m_file = _wfopen(path.wstring().c_str(), L"ab");
#else
        m_file = std::fopen(path.c_str(), "ab");
#endif
        if (!m_file) {
            m_folder.clear(); // (no log, and no more tries: the chat goes on)
            return;
        }
        const auto text = path.u8string(); // (std::string or, from C++20, std::u8string)
        m_path.assign(text.begin(), text.end());
        const std::string head = "{\"game\":" + Json(m_game) + ",\"build\":" + Json(m_build) + ",\"started\":" +
                                 Json(m_started) + ",\"host\":" + (m_host ? "true" : "false") + ",\"me\":" + Json(m_me) +
                                 ",\"them\":" + Json(m_them) + "}\n";
        std::fputs(head.c_str(), m_file);
    }
    const std::string line = "{\"t\":" + Json(LocalTime()) + ",\"dir\":\"" + dir + "\",\"nick\":" + Json(nick) +
                             ",\"text\":" + Json(text) + "}\n";
    std::fputs(line.c_str(), m_file);
    std::fflush(m_file);
}

} // namespace recomp::chat
