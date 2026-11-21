// SPDX-License-Identifier: GPL-3.0-only
// (ForkAde's fork, replay-viewer) replay_viewer.h
#include "replay_viewer.h"

#include <algorithm>
#include <cstdio>

namespace recomp::replay {

namespace {
    // Frames a tick runs at each forward speed, and for slow motion, ticks a frame takes (of every `of`, `run` run)
    uint32_t Forward(Speed speed) {
        switch (speed) {
        case Speed::Two: return 2;
        case Speed::Three: return 3;
        case Speed::Four: return 4;
        case Speed::Eight: return 8;
        default: return 1;
        }
    }
    uint32_t Backward(Speed speed) {
        switch (speed) {
        case Speed::Back8: return 8;
        case Speed::Back4: return 4;
        case Speed::Back3: return 3;
        case Speed::Back2: return 2;
        default: return 1;
        }
    }
    std::string Time(uint32_t frames) {
        const uint32_t seconds = frames / Viewer::kFramesPerSecond;
        char text[16];
        snprintf(text, sizeof(text), "%u:%02u", seconds / 60, seconds % 60);
        return text;
    }
} // namespace

const char *SpeedLabel(Speed speed) {
    static const char *const kLabels[] = {"-x8", "-x4", "-x3", "-x2", "-x1", "x0.25", "x0.5", "x0.75", "x1", "x2", "x3", "x4", "x8"};
    return kLabels[static_cast<int>(speed)];
}

void Viewer::Begin(uint32_t length) {
    std::vector<Aid> aids = std::move(m_aids); // (the aids as the player left them, from one replay to the next)
    const bool counter = m_counter, pinned = m_pinned, hidden = m_hidden;
    *this = Viewer();
    m_aids = std::move(aids);
    m_counter = counter;
    m_pinned = pinned;
    m_hidden = hidden;
    m_active = true;
    m_length = length;
}

void Viewer::Seek(uint32_t frame) {
    m_seekTo = std::clamp<uint32_t>(frame, 1, std::max<uint32_t>(1, m_length));
    m_steps = 0;
}

void Viewer::SeekTo(uint32_t frame, uint64_t tick) {
    Touched(tick);
    Seek(frame);
}

void Viewer::Do(Command command, uint64_t tick) {
    Touched(tick);
    const auto at = [](Speed s) { return static_cast<int>(s); };
    switch (command) {
    case Command::PlayPause:
        m_paused = !m_paused;
        if (!m_paused && m_speed <= Speed::Back1) {
            m_speed = Speed::Normal;
        }
        m_steps = 0;
        break;
    case Command::Play: m_paused = false; m_steps = 0; break;
    case Command::Pause: m_paused = true; m_steps = 0; break;
    case Command::Faster:
        // (x1 -> x2 -> x3 -> x4 -> x8, and from slow motion or rewinding: x2)
        m_paused = false;
        m_speed = m_speed < Speed::Normal ? Speed::Two : m_speed == Speed::Eight ? Speed::Eight : static_cast<Speed>(at(m_speed) + 1);
        break;
    case Command::Rewind:
        m_paused = false;
        m_speed = m_speed > Speed::Back1 ? Speed::Back1 : m_speed == Speed::Back8 ? Speed::Back8 : static_cast<Speed>(at(m_speed) - 1);
        break;
    case Command::Slower:
        m_paused = false;
        m_speed = m_speed == Speed::ThreeQuarters ? Speed::Half : m_speed == Speed::Half ? Speed::Quarter
                : m_speed == Speed::Quarter ? Speed::Normal : Speed::ThreeQuarters;
        m_slowCount = 0;
        break;
    case Command::Normal: m_paused = false; m_speed = Speed::Normal; break;
    case Command::StepBack: m_paused = true; m_steps--; break;
    case Command::StepForward: m_paused = true; m_steps++; break;
    case Command::JumpBack: Seek(m_lastShown > kJump ? m_lastShown - kJump : 1); break;
    case Command::JumpForward: Seek(m_lastShown + kJump); break;
    case Command::ToStart: Seek(1); break;
    case Command::ToEnd: Seek(m_length); m_paused = true; break;
    case Command::PreviousMarker: {
        auto it = m_markers.lower_bound(m_lastShown > kFramesPerSecond / 2 ? m_lastShown - kFramesPerSecond / 2 : 0);
        Seek(it == m_markers.begin() ? 1 : *--it);
        break;
    }
    case Command::NextMarker: {
        auto it = m_markers.upper_bound(m_lastShown);
        if (it != m_markers.end()) {
            Seek(*it);
        }
        break;
    }
    case Command::MarkA:
        m_a = m_lastShown;
        if (m_b != kNone && m_b <= m_a) {
            m_b = kNone;
        }
        break;
    case Command::MarkB:
        if (m_a != kNone && m_lastShown > m_a) {
            m_b = m_lastShown;
        }
        break;
    case Command::ClearLoop: m_a = m_b = kNone; break;
    case Command::PinHud: m_pinned = !m_pinned; break;
    case Command::FrameCounter: m_counter = !m_counter; break;
    case Command::ToggleHud:
        if (HudShown(tick)) {
            m_hidden = true;
            m_pinned = false;
            m_hudUntil = 0;
        } else {
            m_hidden = false;
            m_pinned = true;
        }
        break;
    }
}

void Viewer::Ran(Board &board) {
    const uint32_t frame = board.Frame();
    if (frame % kWindow == 0 && !m_marks.count(frame)) {
        if (m_marks.size() >= kMostMarks) {
            // (the one farthest from here goes)
            auto far = m_marks.begin();
            uint32_t most = 0;
            for (auto it = m_marks.begin(); it != m_marks.end(); ++it) {
                const uint32_t distance = it->first > frame ? it->first - frame : frame - it->first;
                if (distance > most) {
                    most = distance;
                    far = it;
                }
            }
            m_marks.erase(far);
        }
        board.Keep(m_marks[frame], true);
    }
}

void Viewer::From(Board &board, uint32_t frame) {
    auto it = m_marks.upper_bound(frame);
    const bool mark = it != m_marks.begin();
    if (mark) {
        --it;
    }
    board.GoBack(frame);
    // (the viewer's mark, if it is nearer than the board's keyframe)
    if (mark && it->first > board.Frame()) {
        board.PutBack(it->second, it->first);
    }
}

bool Viewer::RunTo(Board &board, uint32_t frame) {
    const uint32_t before = frame - 1; // (the board at the frame before, then that one run seen)
    if (board.Frame() > before || board.Frame() + kSeekPerTick * 20 < before) {
        From(board, before);
    }
    for (uint32_t n = 0; n < kSeekPerTick && board.Frame() < before; n++) {
        board.RunFrame(false, false);
        Ran(board);
    }
    if (board.Frame() < before) {
        return false;
    }
    board.RunFrame(true, false);
    Ran(board);
    m_lastShown = board.Frame();
    m_window.clear();
    return true;
}

void Viewer::BuildWindow(Board &board, uint32_t last) {
    // The states after frames from .. last (the window), run from the nearest mark or keyframe unseen
    const uint32_t from = last + 1 >= kWindow ? last + 1 - kWindow : 0;
    From(board, from);
    while (board.Frame() < from) {
        board.RunFrame(false, false);
        Ran(board);
    }
    m_window.assign(last - from + 1, {});
    m_windowFrom = from;
    board.Keep(m_window[0], false);
    for (uint32_t f = from + 1; f <= last; f++) {
        board.RunFrame(false, false);
        Ran(board);
        board.Keep(m_window[f - from], false);
    }
}

bool Viewer::Back(Board &board, uint32_t frames) {
    if (m_lastShown <= 1) {
        m_paused = true;
        return false;
    }
    const uint32_t target = m_lastShown > frames ? m_lastShown - frames : 1;
    const uint32_t need = target - 1; // (the state before it, then it run seen)
    if (m_window.empty() || need < m_windowFrom || need >= m_windowFrom + m_window.size()) {
        BuildWindow(board, need);
    }
    board.PutBack(m_window[need - m_windowFrom], need);
    board.RunFrame(true, false);
    m_lastShown = board.Frame();
    if (m_a != kNone && m_b != kNone && m_lastShown <= m_a) {
        Seek(m_b); // (a loop played backwards: from B again)
    }
    return true;
}

bool Viewer::Tick(Board &board, uint64_t tick) {
    if (!m_active) {
        return false;
    }
    m_length = board.Length();
    if (m_lastShown == 0) {
        m_lastShown = board.Frame();
    }
    if (m_seekTo != kNone) {
        if (!RunTo(board, m_seekTo)) {
            return false;
        }
        m_seekTo = kNone;
        return true;
    }
    if (m_paused) {
        if (m_steps > 0) {
            m_steps--;
            if (board.Frame() < m_length) {
                board.RunFrame(true, false);
                Ran(board);
                m_lastShown = board.Frame();
                m_window.clear();
                return true;
            }
        } else if (m_steps < 0) {
            m_steps++;
            return Back(board, 1);
        }
        return false;
    }
    if (m_speed <= Speed::Back1) {
        return Back(board, Backward(m_speed));
    }
    // Forward: slow motion runs a frame some ticks only; fast-forward several a tick, the last of them seen
    if (m_speed < Speed::Normal) {
        const uint32_t of = m_speed == Speed::Quarter ? 4 : m_speed == Speed::Half ? 2 : 4;
        const uint32_t run = m_speed == Speed::ThreeQuarters ? 3 : 1;
        const uint32_t now = m_slowCount++ % of;
        if (now >= run) {
            return false;
        }
    }
    (void)tick;
    m_window.clear();
    const uint32_t per = Forward(m_speed);
    bool shown = false;
    for (uint32_t n = 0; n < per; n++) {
        if (board.Frame() >= m_length) {
            m_paused = true; // (the end: it stops there)
            break;
        }
        const bool seen = n + 1 == per || board.Frame() + 1 >= m_length;
        board.RunFrame(seen, seen && Heard());
        Ran(board);
        shown = shown || seen;
        if (m_a != kNone && m_b != kNone && board.Frame() >= m_b) {
            Seek(m_a); // (the loop: from A again)
            break;
        }
    }
    m_lastShown = board.Frame();
    return shown;
}

// --- the HUD ---

namespace {
    struct Button {
        const char *label;
        Command command;
    };
    // The strip: the timeline on top (a bar the picture's width but for margins), the buttons, the speed and time and
    // the aids under it
    constexpr float kMargin = 6, kBarHeight = 5, kRow = 12;

    struct Layout {
        float top, barX, barW, barY, rowY;
        struct Item {
            float x, w;
            Viewer::Hit hit;
            std::string label;
            bool lit;
            bool small; // (the small font: the words; the arrows and the time in the ordinary one)
        };
        std::vector<Item> items;
    };

    Layout Place(const Viewer &viewer, int width, int height, uint32_t frame, uint32_t length, bool compact = false) {
        Layout l;
        l.top = static_cast<float>(height - viewer.HudHeight());
        l.barX = kMargin;
        l.barW = static_cast<float>(width) - 2 * kMargin;
        l.barY = l.top + 3;
        l.rowY = l.barY + kBarHeight + 4;
        static const Button kButtons[] = {
            {"|<", Command::ToStart}, {"<<", Command::Rewind}, {"<.", Command::StepBack}, {"", Command::PlayPause},
            {".>", Command::StepForward}, {">>", Command::Faster}, {">|", Command::ToEnd}, {"SLOW", Command::Slower},
            {"A", Command::MarkA}, {"B", Command::MarkB},
        };
        float x = kMargin;
        auto add = [&](const std::string &label, Viewer::Hit hit, bool lit, bool small) {
            const float w = (small ? text::SmallTextWidth(label.c_str()) : text::TextWidth(label.c_str(), 1)) + 6;
            l.items.push_back({x, w, hit, label, lit, small});
            x += w + 2;
        };
        for (const Button &b : kButtons) {
            Viewer::Hit hit;
            hit.kind = Viewer::Hit::Do;
            hit.command = b.command;
            const std::string label = b.command == Command::PlayPause ? (viewer.Paused() ? " >" : "II") : b.label;
            const bool lit = (b.command == Command::MarkA && viewer.A() != Viewer::kNone) ||
                             (b.command == Command::MarkB && viewer.B() != Viewer::kNone);
            const bool small = b.command == Command::Slower || b.command == Command::MarkA || b.command == Command::MarkB;
            add(label, hit, lit, small);
        }
        // (the speed and the time, not a button)
        add(std::string(SpeedLabel(viewer.CurrentSpeed())) + " " + Time(frame) + (compact ? "" : "/" + Time(length)), Viewer::Hit{}, false, false);
        for (size_t i = 0; i < const_cast<Viewer &>(viewer).Aids().size(); i++) {
            const Aid &aid = const_cast<Viewer &>(viewer).Aids()[i];
            Viewer::Hit hit;
            hit.kind = Viewer::Hit::Aid;
            hit.aid = i;
            add(compact ? aid.label.substr(0, 3) : aid.label, hit, aid.on, true);
        }
        Viewer::Hit counter;
        counter.kind = Viewer::Hit::Do;
        counter.command = Command::FrameCounter;
        add(compact ? "FRM" : "FRAME", counter, viewer.Counter(), true);
        Viewer::Hit pin;
        pin.kind = Viewer::Hit::Do;
        pin.command = Command::PinHud;
        add("PIN", pin, viewer.Pinned(), true);
        // (too wide for the picture: the short labels, and the time without the length)
        if (!compact && x > static_cast<float>(width) - kMargin) {
            return Place(viewer, width, height, frame, length, true);
        }
        return l;
    }
} // namespace

void Viewer::DrawHud(const text::Canvas &canvas, uint32_t frame, uint32_t length, uint64_t tick) const {
    if (!HudShown(tick)) {
        return;
    }
    const Layout l = Place(*this, canvas.width, canvas.height, frame, length);
    canvas.Fill(0, l.top, static_cast<float>(canvas.width), static_cast<float>(HudHeight()), {16, 18, 26, 210});
    // The timeline: played part, markers, the loop, where it is
    canvas.Fill(l.barX, l.barY, l.barW, kBarHeight, {70, 74, 90, 255});
    const auto xOf = [&](uint32_t f) { return l.barX + (length ? l.barW * static_cast<float>(std::min(f, length)) / length : 0); };
    if (m_a != kNone) {
        const float a = xOf(m_a), b = m_b != kNone ? xOf(m_b) : a + 1;
        canvas.Fill(a, l.barY - 1, std::max(1.0f, b - a), kBarHeight + 2, {90, 160, 255, 160});
    }
    canvas.Fill(l.barX, l.barY, xOf(frame) - l.barX, kBarHeight, {255, 130, 60, 255});
    for (uint32_t m : m_markers) {
        // (a round's start: a yellow tick taller than the bar, edged dark so it shows on the played part too)
        const float x = static_cast<float>(static_cast<int>(xOf(m)));
        canvas.Fill(x - 1, l.barY - 3, 3, kBarHeight + 6, {10, 10, 14, 255});
        canvas.Fill(x, l.barY - 2, 1, kBarHeight + 4, {255, 230, 60, 255});
    }
    if (m_seekTo != kNone) {
        canvas.Fill(xOf(m_seekTo) - 1, l.barY - 2, 2, kBarHeight + 4, {255, 255, 255, 180});
    }
    canvas.Fill(xOf(frame) - 1.5f, l.barY - 2, 3, kBarHeight + 4, {255, 255, 255, 255});
    for (const auto &item : l.items) {
        const bool button = item.hit.kind != Hit::None;
        if (button) {
            canvas.Fill(item.x, l.rowY - 2, item.w, kRow - 1, item.lit ? text::Color{255, 130, 60, 230} : text::Color{50, 54, 68, 230});
        }
        const text::Color colour = button ? text::Color{255, 255, 255, 255} : text::Palette::kReadout;
        if (item.small) {
            canvas.SmallText(item.x + 3, l.rowY + 1, colour, item.label.c_str());
        } else {
            canvas.Text(item.x + 3, l.rowY, 1, 0, colour, item.label.c_str());
        }
    }
    // (the frame counter: the frame's number over the strip's right end)
    if (m_counter) {
        const std::string number = "FRAME " + std::to_string(frame);
        const float w = text::SmallTextWidth(number.c_str()) + 6;
        const float x = static_cast<float>(canvas.width) - kMargin - w;
        canvas.Fill(x, l.top - 10, w, 9, {16, 18, 26, 210});
        canvas.SmallText(x + 3, l.top - 8, text::Palette::kReadout, number.c_str());
    }
}

Viewer::Hit Viewer::HitAt(float x, float y, int width, int height, uint32_t length) const {
    Hit none;
    if (!m_active) {
        return none;
    }
    const Layout l = Place(*this, width, height, m_lastShown, length);
    if (y < l.top) {
        return none;
    }
    if (y <= l.barY + kBarHeight + 2 && x >= l.barX && x <= l.barX + l.barW) {
        Hit seek;
        seek.kind = Hit::Seek;
        seek.frame = static_cast<uint32_t>((x - l.barX) / l.barW * length);
        return seek;
    }
    for (const auto &item : l.items) {
        if (item.hit.kind != Hit::None && x >= item.x && x < item.x + item.w && y >= l.rowY - 2 && y < l.rowY - 2 + kRow) {
            return item.hit;
        }
    }
    Hit strip; // (on the strip but not a button: nothing, but the game does not take it)
    strip.kind = Hit::Strip;
    return strip;
}

} // namespace recomp::replay
