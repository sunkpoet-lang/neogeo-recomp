// SPDX-License-Identifier: GPL-3.0-only
// Part of neogeo-recomp.
//
// The in-game chat of an online match (ForkAde's openspec change in-game-chat; the protocol: ForkAde's
// docs/chat-protocol.md, recomp-common's chat.h): what the window shows and the player types, modelled on Tommy
// McLeroy's chat in cps2recomp (port/main.cpp, issue #196) so that both families look and work alike. The lines are
// drawn at the bottom-left over the picture, five at most, each for eight seconds; the line being typed under them;
// how to chat said as a match begins. The chat's key is T, or Y where T is a game key (T again where both are: online
// it is then the chat's). Every line goes into the match's history (the menu's Chat history) and its log (--chat-log).
#pragma once

#include "../recomp-common/chat.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace port {

class NeoGeoOnline;

class NeoGeoChat {
public:
    std::string key = "T";  // (the window's choice for the match)
    bool keyShared = false; // (T and Y both game keys)
    bool typing = false;    // (the line being typed is open: the keyboard is the chat's)
    std::string typed;
    bool historyOpen = false;

    // Whether there is a match to chat in
    static bool Available(const NeoGeoOnline &online);
    // The typed line said: false when kept in the input, over the rate ("wait a moment")
    bool Say(NeoGeoOnline &online);
    // Text the keyboard typed (UTF-8), up to the line's 60 characters; and Backspace, a whole character
    void Type(const char *text);
    void Backspace();
    // Every tick of the session: a new match's log, the lines that came, the tests' lines (--net-say) at their
    // frames, and at the end the lines not delivered
    void Tick(NeoGeoOnline &online, uint64_t frame);
    // Over the picture (the board's 320 columns)
    void Draw(std::vector<uint32_t> &pixels, int width, int height) const;

private:
    struct Line {
        std::string text;
        uint64_t until;
        bool own;
    };
    struct Record {
        std::string time, who, text;
        bool own, delivered;
    };
    std::deque<Line> m_lines;
    std::vector<Record> m_history;
    std::unique_ptr<recomp::chat::Log> m_log;
    bool m_started = false, m_ended = false, m_available = false;
    uint64_t m_tick = 0, m_hintUntil = 0, m_waitUntil = 0;
    void Add(const std::string &who, const std::string &text, bool own, bool delivered);
};

} // namespace port
