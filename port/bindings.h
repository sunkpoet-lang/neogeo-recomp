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
// (and [keyboard2], [gamepad2] for player 2, and so on for a game with more). What is not in the file is the usual
// layout (MakeBoundTable). The first gamepad is player 1's, the second player 2's, and so on; a gamepad's left stick
// always works as the directions.
#pragma once

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <string>
#include <vector>

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
inline constexpr int kFirstButton = 4, kButtons = 6; // (the six buttons' place among them)
inline constexpr int kBoundPlayersMost = recomp::kBindMaxPlayers;

using recomp::Bindings;
using recomp::PadInput;

// A table of what can be bound, and each action's bit (its order is the table's, which need not have them all)
struct BoundTable {
    recomp::BindAction actions[kBoundActionCount];
    uint16_t bits[kBoundActionCount];
    recomp::BindTable table;
};

// Which table: how many players the game has (game.cfg's `players`, two unless it says), and its own names for
// its buttons (game.cfg's `buttons`, "Attack|Jump|Shoot": a game with fewer than six names them; none: the six
// of a fighting game, punches and kicks).
//   A game's own table (`shared` false) has only the buttons it names, called so.
//   The table every game shares in the collection (`shared` true) has all six, each called as a fighting game
//   calls it and, after a dot, what the other games call it ("Light punch · Attack"), and as many players as the
//   game with the most. Its usual layout is a fighting game's, whatever the names.
// The usual layout. Keyboard, player 1: arrows; A S D punches (or the first three buttons), Z X C kicks; 1 Start;
// 5 a coin. Players 2 to 4 have no keys until some are chosen. A gamepad is laid out like a six-button panel:
// punches on the top row (West, North, right shoulder), kicks below (South, East, right trigger); Start; Back puts
// a coin in. A game that names its buttons has them (on its own) as a beat-'em-up's pad has them: the first on
// West, the second on South (jump, say), the third on East, then North and the shoulder and trigger.
inline const BoundTable &MakeBoundTable(int players, const std::string &buttons, bool shared) {
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
    static constexpr PadInput kNamedPad[kButtons] = {
        {SDL_GAMEPAD_BUTTON_WEST, -1}, {SDL_GAMEPAD_BUTTON_SOUTH, -1}, {SDL_GAMEPAD_BUTTON_EAST, -1},
        {SDL_GAMEPAD_BUTTON_NORTH, -1}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, -1}, {-1, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER},
    };
    // (made once for each kind asked for, and kept: the launcher and the game point into them)
    static std::deque<std::pair<std::string, BoundTable>> made;
    static std::deque<std::string> titles;
    players = std::max(1, std::min(players, kBoundPlayersMost));
    const std::string key = std::to_string(players) + (shared ? "s" : "o") + buttons;
    for (const auto &[which, table] : made) {
        if (which == key) {
            return table;
        }
    }
    std::vector<std::string> names;
    for (size_t from = 0; !buttons.empty() && names.size() < kButtons;) {
        const size_t bar = buttons.find('|', from);
        std::string name = buttons.substr(from, bar == std::string::npos ? std::string::npos : bar - from);
        name.erase(0, name.find_first_not_of(' '));
        name.erase(name.find_last_not_of(' ') + 1);
        names.push_back(name);
        if (bar == std::string::npos) {
            break;
        }
        from = bar + 1;
    }
    made.emplace_back(key, BoundTable{});
    BoundTable &t = made.back().second;
    int count = 0;
    for (int action = 0; action < kBoundActionCount; action++) {
        const int button = action - kFirstButton;
        const bool isButton = button >= 0 && button < kButtons;
        const bool named = isButton && button < static_cast<int>(names.size()) && !names[button].empty();
        if (isButton && !shared && !names.empty() && !named) {
            continue; // (a button the game has not got)
        }
        const char *title = kBoundActions[action].title;
        if (named) {
            titles.push_back(shared ? std::string(title) + " \u00b7 " + names[button] : names[button]);
            title = titles.back().c_str();
        }
        const PadInput pad = !shared && named ? kNamedPad[button] : kPad[action];
        recomp::BindAction &a = t.actions[count];
        a.name = kBoundActions[action].name;
        a.title = title;
        a.key[0] = kKeys[action];
        a.pad[0] = a.pad[1] = a.pad[2] = a.pad[3] = pad;
        t.bits[count++] = kBoundActions[action].bit;
    }
    t.table = {t.actions, count, players};
    return t;
}

// The game's own table (shared false), or the collection's (true): see MakeBoundTable
#ifdef GAME_PLAYERS
inline int GamePlayers() {
    return atoi(GAME_PLAYERS);
}
#else
inline int GamePlayers() {
    return 2;
}
#endif
#ifdef GAME_BUTTONS
inline const char *GameButtons() {
    return GAME_BUTTONS;
}
#else
inline const char *GameButtons() {
    return "";
}
#endif

// The player's choices from settings.ini (the usual layout where it says nothing, or there is no file)
inline Bindings LoadBindings(const std::string &settingsPath, const BoundTable &table) {
    return recomp::LoadBindings(settingsPath, table.table);
}

// Some player has this gamepad input bound (then it does not also do what it does otherwise while playing)
inline bool PadInputBound(const Bindings &b, const BoundTable &table, PadInput input) {
    return recomp::PadInputBound(b, table.table, input);
}

} // namespace port
