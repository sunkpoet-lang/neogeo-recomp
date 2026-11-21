// SPDX-License-Identifier: GPL-3.0-only
// (ForkAde's fork, replay-viewer) Watching a replay, the same in every port that plays them: play and pause,
// fast-forward (x2, x3, x4, x8), rewind seen as it goes (x1 to x8), slow motion (x0.75, x0.5, x0.25), a frame at a
// time either way, 5-second jumps, the start and the end, a timeline to go anywhere, round markers, an A-B loop and
// the aids the game can draw (hit boxes, the input display, a frame counter); and its HUD, a strip at the bottom of
// the picture (DrawHud), worked by keyboard, gamepad and mouse.
//
// The viewer decides; the port's board does. Each tick the port calls Tick with its Board, which runs the replay's
// frames (seen: drawn and heard; unseen: neither), keeps and puts back states, and goes back to a state it kept on
// the way (its keyframes). Going backwards: the viewer goes back to a keyframe before a window of frames, runs to
// the window unseen keeping a state after every frame of it, and then shows the window's frames last first, each by
// putting back the state before it and running it seen. So every frame shown is the replay's own.
#pragma once

#include "text.h"

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace recomp::replay {

// What the viewer asks of the port's board. Frame() is how many frames of the replay have been run (the frame shown
// is the last run); Length() how many it has.
class Board {
public:
    virtual ~Board() = default;
    virtual uint32_t Frame() const = 0;
    virtual uint32_t Length() const = 0;
    // One frame of the replay forward: seen (drawn, and heard when `sound`) or not
    virtual void RunFrame(bool seen, bool sound) = 0;
    // The board as it is now, kept in memory, and put back (Frame() becomes `frame`). `lasting`: kept for the rest
    // of the replay (the viewer's own marks every kWindow frames); else only for a little while (the rewind window's,
    // which a port may share pages between)
    virtual void Keep(std::vector<uint8_t> &state, bool lasting) = 0;
    virtual void PutBack(const std::vector<uint8_t> &state, uint32_t frame) = 0;
    // Back to the latest state the board kept at or before `frame` (its keyframes, or the replay's beginning);
    // Frame() is then that state's frame
    virtual void GoBack(uint32_t frame) = 0;
};

// The speeds, in order: rewind x8 ... x1, slow motion, x1, fast-forward x2 ... x8
enum class Speed { Back8, Back4, Back3, Back2, Back1, Quarter, Half, ThreeQuarters, Normal, Two, Three, Four, Eight };
const char *SpeedLabel(Speed speed); // "x2", "-x4", "x0.25"

// An aid the game can draw while a replay plays (a feature of its own), as the HUD offers it
struct Aid {
    std::string name, label; // ("hit_boxes", "Hit boxes")
    bool on = false;
};

// What the player asks for (keys, buttons, the HUD's buttons, Game > Replay)
enum class Command {
    PlayPause, Play, Pause,
    Faster,       // fast-forward, a step faster each time (x2 x3 x4 x8)
    Rewind,       // rewind, a step faster each time (x1 x2 x3 x4 x8)
    Slower,       // slow motion in turn (x0.75 x0.5 x0.25, then x1)
    Normal,       // x1
    StepBack, StepForward, // a frame (pausing)
    JumpBack, JumpForward, // 5 seconds
    ToStart, ToEnd,
    PreviousMarker, NextMarker,
    MarkA, MarkB, ClearLoop,
    PinHud,
    FrameCounter, // (the viewer's own aid: the frame's number over the strip)
    ToggleHud,    // (shown and kept, or hidden and kept hidden whatever is pressed, until asked again)
};

class Viewer {
public:
    static constexpr int kFramesPerSecond = 60;
    static constexpr uint32_t kJump = 5 * kFramesPerSecond;
    static constexpr uint32_t kWindow = 30;      // frames kept for going backwards at a time
    static constexpr uint32_t kSeekPerTick = 90; // frames run unseen in a tick on the way to a place
    static constexpr uint64_t kHudTicks = 4 * kFramesPerSecond;

    // A new replay to watch (from its first frame, playing at x1)
    void Begin(uint32_t length);
    void End() { m_active = false; }
    bool Active() const { return m_active; }

    // One tick of the screen: what the board must do now. True when a frame was shown (the picture changed)
    bool Tick(Board &board, uint64_t tick);
    void Do(Command command, uint64_t tick);
    // The timeline clicked: go there
    void SeekTo(uint32_t frame, uint64_t tick);
    // Any input: the HUD shows for a while
    void Touched(uint64_t tick) { m_hudUntil = m_hidden ? 0 : tick + kHudTicks; }

    // Round starts and the like, as the port finds them (kept for the replay)
    void AddMarker(uint32_t frame) { m_markers.insert(frame); }
    const std::set<uint32_t> &Markers() const { return m_markers; }
    std::vector<Aid> &Aids() { return m_aids; }

    Speed CurrentSpeed() const { return m_speed; }
    uint32_t Shown() const { return m_lastShown; }
    bool Paused() const { return m_paused; }
    bool Seeking() const { return m_seekTo != kNone; }
    bool Rewinding() const { return !m_paused && m_speed <= Speed::Back1; }
    // Sound: at x1 and in slow motion only
    bool Heard() const { return !m_paused && m_speed >= Speed::Quarter && m_speed <= Speed::Normal && !Seeking(); }
    // The frames a seen frame waits for in slow motion (1 at x1 and faster)
    uint32_t A() const { return m_a; }
    uint32_t B() const { return m_b; }
    static constexpr uint32_t kNone = UINT32_MAX;
    bool HudShown(uint64_t tick) const { return m_active && !m_hidden && (m_pinned || tick < m_hudUntil || m_paused); }
    bool Hidden() const { return m_hidden; }
    bool Pinned() const { return m_pinned; }
    bool Counter() const { return m_counter; }

    // The HUD over the picture (width x height), and what a click at (x, y) on it does: a command, the timeline (to
    // a frame) or an aid toggled; false when the click was not on it
    void DrawHud(const text::Canvas &canvas, uint32_t frame, uint32_t length, uint64_t tick) const;
    struct Hit {
        enum Kind { None, Do, Seek, Aid, Strip } kind = None; // (Strip: on the HUD, not a button: not the game's)
        Command command = Command::PlayPause;
        uint32_t frame = 0;
        size_t aid = 0;
    };
    Hit HitAt(float x, float y, int width, int height, uint32_t length) const;
    int HudHeight() const { return 26; }

private:
    void Seek(uint32_t frame);
    bool RunTo(Board &board, uint32_t frame); // (seeking: unseen frames, at most kSeekPerTick; true when there)
    bool Back(Board &board, uint32_t frames); // (the frame `frames` before the one shown, shown)

    bool m_active = false, m_paused = false, m_pinned = false, m_counter = false, m_hidden = false;
    Speed m_speed = Speed::Normal;
    uint32_t m_length = 0;
    uint32_t m_seekTo = kNone;
    int m_steps = 0;          // frames to step while paused (negative: back)
    uint32_t m_slowCount = 0; // (slow motion: ticks since the last frame)
    uint32_t m_lastShown = 0; // (the frame shown last)
    uint32_t m_a = kNone, m_b = kNone;
    uint64_t m_hudUntil = 0;
    std::set<uint32_t> m_markers;
    std::vector<Aid> m_aids;
    // (going backwards) the states after frames m_windowFrom .. m_windowFrom + size - 1
    std::vector<std::vector<uint8_t>> m_window;
    uint32_t m_windowFrom = 0;
    // (the viewer's own lasting states, every kWindow frames it has run past: going back starts from the nearest)
    static constexpr size_t kMostMarks = 64;
    std::map<uint32_t, std::vector<uint8_t>> m_marks;
    void From(Board &board, uint32_t frame); // (the board at the latest mark or keyframe at or before `frame`)
    void Ran(Board &board);                  // (a frame run: a mark kept if it is a kWindow one)
    void BuildWindow(Board &board, uint32_t last);
};

} // namespace recomp::replay
