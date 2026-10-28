// SPDX-License-Identifier: GPL-3.0-only
// Part of cps2recomp (the user's fork: the Neo Geo board).
//
// A Neo Geo game in a window (SDL3): the board's picture at 4:3, its sound, the keyboard and up to two gamepads, at the
// board's pace (59.19 frames a second, kept by the sound queue). No menu or HUD yet: F11 full screen, F12 a screenshot,
// Esc or the window's X to quit.
//
// Keyboard, player 1: arrows, A / S for buttons A and C, Z / X for B and D, 1 start, 5 coin. Player 2: I J K L, T / Y
// for A and C, G / H for B and D, 2 start, 6 coin. 9: service, F2: test. Gamepads (the first is player 1): stick or
// pad, West A, South B, North C, East D, Start, Back for a coin.
#pragma once

#include <string>

namespace cps2recomp {

class NeoGeo;

struct NeoGeoWindowOptions {
    const char *title = "";
    const char *id = "";
    int scale = 3; // the window's height in times the picture's 224 lines
    bool mute = false;
    std::string shotDir = "."; // (F12)
};

// Runs until the player quits; the program's exit code
int RunNeoGeoWindow(NeoGeo &board, const NeoGeoWindowOptions &options);

} // namespace cps2recomp
