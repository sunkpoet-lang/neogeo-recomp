// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Which key and which gamepad button does what, for each player: the launcher's Controllers page writes
// the player's choices to settings.ini and the game reads them. The format and its reading are the recomp
// ports' own (recomp-common/launcher/src/bindings.h); here are this toolkit's actions and its usual layout.
//
//   [keyboard1]         a key by SDL's name for it ("Up", "A", "Return"), or "none"
//   up = Up
//   [gamepad1]          a button or a trigger by SDL's name ("a", "leftshoulder", "righttrigger"), or "none"
//   lp = x
//
// (and [keyboard2], [gamepad2] for player 2). What is not in the file is as Defaults() has it. The first
// gamepad is player 1's and the second player 2's; a gamepad's left stick always works as the directions.
#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>

#include "../recomp-common/launcher/src/bindings.h"

namespace port {

// What can be bound: the name in settings.ini, what the launcher calls it, and its bit of a player's
// controls (cps2recomp::Button in machine/machine.h; frontend_sdl.cpp checks they agree)
struct BoundAction {
    const char *name;
    const char *title;
    uint16_t bit;
};
inline constexpr BoundAction kBoundActions[] = {
    {"up", "Up", 1 << 3},          {"down", "Down", 1 << 2},        {"left", "Left", 1 << 1},
    {"right", "Right", 1 << 0},    {"lp", "Light punch", 1 << 4},   {"mp", "Medium punch", 1 << 5},
    {"hp", "Hard punch", 1 << 6},  {"lk", "Light kick", 1 << 7},    {"mk", "Medium kick", 1 << 8},
    {"hk", "Hard kick", 1 << 9},   {"start", "Start", 1 << 10},     {"coin", "Coin", 1 << 11},
};
inline constexpr int kBoundActionCount = static_cast<int>(sizeof(kBoundActions) / sizeof(kBoundActions[0]));
inline constexpr int kBoundPlayers = 2;

using recomp::Bindings;
using recomp::PadInput;

// The layout with nothing chosen. Keyboard, player 1: arrows; A S D punches, Z X C kicks (light, medium,
// hard); 1 Start; 5 a coin. Player 2 has no keys until some are chosen. A gamepad is laid out like a
// six-button panel: punches on the top row (West, North, right shoulder), kicks below (South, East, right
// trigger); Start; Back puts a coin in.
inline const recomp::BindTable &BindingTable() {
    static constexpr SDL_Scancode kKeys[kBoundActionCount] = {
        SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_A, SDL_SCANCODE_S,
        SDL_SCANCODE_D,  SDL_SCANCODE_Z,    SDL_SCANCODE_X,    SDL_SCANCODE_C,     SDL_SCANCODE_1, SDL_SCANCODE_5,
    };
    static constexpr PadInput kPad[kBoundActionCount] = {
        {SDL_GAMEPAD_BUTTON_DPAD_UP, -1},   {SDL_GAMEPAD_BUTTON_DPAD_DOWN, -1},      {SDL_GAMEPAD_BUTTON_DPAD_LEFT, -1},
        {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, -1}, {SDL_GAMEPAD_BUTTON_WEST, -1},          {SDL_GAMEPAD_BUTTON_NORTH, -1},
        {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, -1}, {SDL_GAMEPAD_BUTTON_SOUTH, -1},     {SDL_GAMEPAD_BUTTON_EAST, -1},
        {-1, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER}, {SDL_GAMEPAD_BUTTON_START, -1},        {SDL_GAMEPAD_BUTTON_BACK, -1},
    };
    static const struct Table {
        recomp::BindAction actions[kBoundActionCount];
        recomp::BindTable table;
        Table() {
            for (int action = 0; action < kBoundActionCount; action++) {
                actions[action] = {kBoundActions[action].name, kBoundActions[action].title, {kKeys[action], SDL_SCANCODE_UNKNOWN}, {kPad[action], kPad[action]}};
            }
            table = {actions, kBoundActionCount, kBoundPlayers};
        }
    } table;
    return table.table;
}
inline Bindings DefaultBindings() {
    return recomp::DefaultBindings(BindingTable());
}

// The player's choices from settings.ini (the usual layout where it says nothing, or there is no file)
inline Bindings LoadBindings(const std::string &settingsPath) {
    return recomp::LoadBindings(settingsPath, BindingTable());
}

// Some player has this gamepad input bound (then it does not also do what it does otherwise while playing)
inline bool PadInputBound(const Bindings &b, PadInput input) {
    return recomp::PadInputBound(b, BindingTable(), input);
}

} // namespace port
