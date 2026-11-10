// SPDX-License-Identifier: GPL-3.0-only
// Part of neogeo-recomp.
//
// The in-game chat (neogeo_chat.h)
#include "neogeo_chat.h"

#include "game_info.h"
#include "../runtime/game.h"

#include "../recomp-common/text.h"
#include "neogeo_online.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace port {

namespace {
    constexpr uint64_t kChatTicks = 8 * 60; // (each line said for eight seconds)
    constexpr size_t kChatLines = 5;

    std::string Shown(const NeoGeoOnline &online, int player) {
        const std::string name = online.ChatName(player);
        return name.empty() ? (player == 0 ? "Player 1" : "Player 2") : name;
    }
} // namespace

bool NeoGeoChat::Available(const NeoGeoOnline &online) {
    return online.ChatReady();
}

void NeoGeoChat::Add(const std::string &who, const std::string &text, bool own, bool delivered) {
    if (delivered) {
        m_lines.push_back({who + ": " + text, m_tick + kChatTicks, own});
    }
    m_history.push_back({recomp::chat::LocalTime().substr(11, 8), who, text, own, delivered});
}

bool NeoGeoChat::Say(NeoGeoOnline &online) {
    if (!online.ChatReady()) {
        return true;
    }
    std::string line;
    const recomp::chat::Channel::Said said = online.ChatSay(typed, line);
    if (said == recomp::chat::Channel::Said::Wait) {
        m_waitUntil = m_tick + 120;
        return false;
    }
    if (said == recomp::chat::Channel::Said::Sent) {
        const std::string who = Shown(online, online.ChatLocal());
        // (match-spectators: a spectator's own lines marked as they are on the others' screens)
        Add((online.Spectating() ? "(spec) " : "") + who, line, true, true);
        if (m_log) {
            m_log->Sent(who, line);
        }
    }
    return true;
}

void NeoGeoChat::Type(const char *text) {
    for (const char *c = text; *c;) {
        size_t used = 1;
        recomp::text::NextChar(c, strlen(c), used);
        if (static_cast<unsigned char>(*c) >= 32 && *c != 127 &&
            recomp::text::CharCount(typed.data(), typed.size()) < recomp::chat::kLineMost) {
            typed.append(c, used);
        }
        c += used;
    }
}

void NeoGeoChat::Backspace() {
    while (!typed.empty() && (static_cast<unsigned char>(typed.back()) & 0xC0) == 0x80) {
        typed.pop_back();
    }
    if (!typed.empty()) {
        typed.pop_back();
    }
}

void NeoGeoChat::Tick(NeoGeoOnline &online, uint64_t frame) {
    m_tick++;
    const bool ready = online.ChatReady();
    if (ready && !m_started) {
        m_started = true;
        m_hintUntil = m_tick + kChatTicks;
        m_log = std::make_unique<recomp::chat::Log>();
        m_log->Start(online.Options().chatLog, GAME_ID, NEOGEO_RECOMP_VERSION, online.ChatHost());
        m_log->Names(Shown(online, online.ChatLocal()), Shown(online, 1 - online.ChatLocal()));
        fprintf(stderr, "online: chat: with %s, as %s\n", Shown(online, 1 - online.ChatLocal()).c_str(),
                Shown(online, online.ChatLocal()).c_str());
    }
    if (ready) {
        // (match-spectators: each line with who said it; a spectator's marked "(spec)", and left off this player's
        // screen with Mute spectators, still logged)
        for (const NeoGeoOnline::Heard &heard : online.ChatHeardLines()) {
            const std::string who = (heard.spectator ? "(spec) " : "") + (heard.name.empty() ? Shown(online, 1 - online.ChatLocal()) : heard.name);
            const bool muted = heard.spectator && !online.Spectating() && cps2recomp::FeatureSetting("online_mute_spectators") != 0;
            fprintf(stderr, "online: chat: %s%s: %s\n", muted ? "(muted) " : "", who.c_str(), heard.text.c_str());
            if (!muted) {
                Add(who, heard.text, false, true);
            }
            if (m_log) {
                m_log->Received(who, heard.text);
            }
        }
        // (the tests' lines, at their frames)
        auto &says = online.Says();
        for (auto it = says.begin(); it != says.end();) {
            if (frame < it->first) {
                ++it;
                continue;
            }
            typed = it->second;
            Say(online);
            typed.clear();
            it = says.erase(it);
        }
    }
    if (m_started && !m_ended && online.ChatOver()) {
        m_ended = true;
        typing = false;
        const std::string who = Shown(online, online.ChatLocal());
        for (const std::string &line : online.ChatUndelivered()) {
            fprintf(stderr, "online: chat: not delivered: %s\n", line.c_str());
            Add(who, line, true, false);
            if (m_log) {
                m_log->Undelivered(who, line);
            }
        }
        m_log.reset();
    }
    m_available = ready;
    while (!m_lines.empty() && (m_lines.size() > kChatLines || m_lines.front().until <= m_tick)) {
        m_lines.pop_front();
    }
}

void NeoGeoChat::Draw(std::vector<uint32_t> &pixels, int width, int height) const {
    const recomp::text::Canvas canvas{pixels.data(), width, height};
    if (m_available) {
        float y = height - 30.0f;
        auto line = [&](const std::string &text, recomp::text::Color colour) {
            const float wide = recomp::text::TextWidth(text.c_str(), 1);
            canvas.Fill(3, y - 2, wide + 6, 12, {20, 22, 30, 200});
            canvas.Text(6, y, 1, 0, colour, text.c_str());
            y -= 12;
        };
        if (typing) {
            line("Enter: send   Esc: cancel", {170, 170, 170, 255});
            if (m_tick < m_waitUntil) {
                line("Wait a moment: 3 lines in 5 s at most", {255, 130, 120, 255});
            }
            line("Say: " + typed + ((m_tick / 20) % 2 ? "_" : " "), {255, 210, 80, 255});
        } else if (m_tick < m_hintUntil) {
            line("Chat: " + key + (keyShared ? " (online the chat's)" : ""), {170, 170, 170, 255});
        }
        for (auto it = m_lines.rbegin(); it != m_lines.rend(); ++it) {
            line(it->text, it->own ? recomp::text::Color{170, 200, 255, 255} : recomp::text::Color{255, 255, 255, 255});
        }
    }
    // (the menu's Chat history: the match's lines with their times, the newest lowest, as many as fit)
    if (historyOpen && !m_history.empty()) {
        const size_t fit = std::min<size_t>(m_history.size(), 13);
        const float top = 14.0f, tall = fit * 12.0f + 18.0f;
        canvas.Fill(3, top, width - 6.0f, tall, {20, 22, 30, 225});
        canvas.Text(8, top + 4, 1, 0, {255, 210, 80, 255}, "CHAT HISTORY (menu: Chat history closes it)");
        float y = top + 16;
        for (size_t i = m_history.size() - fit; i < m_history.size(); i++, y += 12) {
            const Record &one = m_history[i];
            const std::string text = one.time + " " + one.who + ": " + one.text + (one.delivered ? "" : " (not delivered)");
            canvas.Text(8, y, 1, 0, one.own ? recomp::text::Color{170, 200, 255, 255} : recomp::text::Color{255, 255, 255, 255},
                        text.c_str());
        }
    }
}

} // namespace port
