// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy and ForkAde contributors. Part of recomp-common (ForkAde's fork).
// The in-game chat between the two players of an online match (ForkAde's docs/chat-protocol.md): Tommy's netplay
// chat (a line's number and text, how many have come) as a channel any online session can carry on its own socket,
// the text clean-up, the rate and the match's log. It knows nothing of the machine, the socket or the screen.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace recomp::chat {

constexpr size_t kLineMost = 60;      // characters in a line
constexpr size_t kNameMost = 16;      // characters in a name
constexpr uint64_t kResendMs = 250;   // lines not acknowledged yet, sent again this often
constexpr uint64_t kRateMs = 5000;    // at most kRateCount lines in this long, each way
constexpr int kRateCount = 3;

// A name or a line as it is sent, shown and logged: false when it is not valid UTF-8; otherwise the controls and
// invisible marks taken out, spaces at its ends too, and at most `most` characters
bool Clean(const std::string &text, size_t most, std::string &out);
// (the same, "" when not valid)
std::string Printable(const std::string &text, size_t most);

// The payloads after a packet's tag and kind: C (a line) and A (how many lines have come)
std::vector<uint8_t> EncodeLine(uint32_t number, const std::string &text);
bool DecodeLine(const uint8_t *data, size_t size, uint32_t &number, std::string &text);
std::vector<uint8_t> EncodeAck(uint32_t count);
bool DecodeAck(const uint8_t *data, size_t size, uint32_t &count);

// The hello with this side's name after a 0 byte; and a hello that came, split into the part both sides compare
// and the other side's name (cleaned; "" for none)
std::vector<uint8_t> HelloWithName(const std::vector<uint8_t> &hello, const std::string &name);
void SplitHello(const std::vector<uint8_t> &hello, std::vector<uint8_t> &compared, std::string &name);

// At most kRateCount in kRateMs: whether one more at `now` (ms) is within it, counted if so
class Rate {
public:
    bool Allow(uint64_t now);

private:
    std::deque<uint64_t> m_times;
};

// One side's chat in a match. The session gives it the packets that came ('C' or 'A', the payload after the kind)
// and calls Pump every frame; the channel sends with `send`
class Channel {
public:
    using Send = std::function<void(char kind, const std::vector<uint8_t> &payload)>;
    explicit Channel(Send send) : m_send(std::move(send)) {}

    enum class Said { Sent, Empty, Wait };
    // A line typed: cleaned, and sent unless empty or over the rate; `line` is what was sent
    Said Say(const std::string &text, uint64_t now, std::string *line = nullptr);
    void Receive(char kind, const uint8_t *data, size_t size, uint64_t now);
    void Pump(uint64_t now);
    // The lines that came, cleaned, in order, since the last call
    std::vector<std::string> TakeHeard();
    // The lines sent that the other side has not said it has (at the match's end: not delivered)
    std::vector<std::string> Unacknowledged() const;

private:
    Send m_send;
    std::deque<std::pair<uint32_t, std::string>> m_unacknowledged;
    uint32_t m_said = 0, m_heardUpTo = 0;
    uint64_t m_sentAt = 0;
    std::vector<std::string> m_heard;
    Rate m_sayRate, m_hearRate;
    void SendAll(uint64_t now);
};

// The match's chat log (docs/chat-protocol.md, design decision 7): <folder>/<date_time>_<game>.jsonl, opened at
// the first message, a line flushed for each. An empty folder: no log.
class Log {
public:
    Log() = default;
    Log(const Log &) = delete;
    Log &operator=(const Log &) = delete;
    ~Log();
    void Start(const std::string &folder, const std::string &game, const std::string &build, bool host);
    void Names(const std::string &me, const std::string &them);
    void Sent(const std::string &nick, const std::string &text) { Write("sent", nick, text); }
    void Received(const std::string &nick, const std::string &text) { Write("received", nick, text); }
    void Undelivered(const std::string &nick, const std::string &text) { Write("undelivered", nick, text); }
    const std::string &Path() const { return m_path; }

private:
    std::string m_folder, m_game, m_build, m_me, m_them, m_started, m_stamp, m_path;
    bool m_host = false;
    std::FILE *m_file = nullptr;
    void Write(const char *dir, const std::string &nick, const std::string &text);
};

// A path from UTF-8 text (std::filesystem::u8path, deprecated from C++20)
inline std::filesystem::path PathOf(const std::string &utf8) {
#if defined(__cpp_char8_t)
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
#else
    return std::filesystem::u8path(utf8);
#endif
}

// (the log's times: local, with the time zone, as 2026-11-03T21:41:05-06:00; and for a file name)
std::string LocalTime(bool forFileName = false);

} // namespace recomp::chat
