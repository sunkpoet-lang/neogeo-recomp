// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The in-game menu: save states (save, load, slot), a coin, the speed the game runs at, the game's optional
// features and commands (a page for each of their categories), and leaving the game, while playing.
// The frontend opens it, stops the game while it is open and gives it the player's presses; it draws itself
// over a copy of the game's picture.
//
// The menu itself is the one the recomp ports share (recomp-common/menu.h, a copy of recomp-common's: see
// tools/sync-common.sh). This is what is CPS-2's and this toolkit's about it: the rows of the first page,
// the buttons, the speeds, and the games' features and commands handed over.
#pragma once

#include "../recomp-common/menu.h"

#include <cstdint>
#include <string>

namespace port {

class Menu {
public:
    enum class Action { None, Save, Load, Coin, Start2, Test, Reset, Command, Quit };

    // withStates: show the save state rows
    void Open(bool withStates);
    void Close() {
        m_menu.Close();
    }
    bool IsOpen() const {
        return m_menu.IsOpen();
    }
    // Buttons newly pressed (cps2recomp::Button bits): Up/Down choose, any punch, kick or Start changes or
    // goes into a page, Left/Right change a row with several values.
    // Returns what the frontend is to do (with Slot()); it closes the menu if that went well.
    // (The tests' way in. A player's presses come through Press.)
    Action Buttons(uint16_t pressed);
    // The menu's own buttons newly pressed (recomp::kMenu... bits), which the frontend makes from keys and
    // gamepad buttons that are the same whatever the game's are set to: the arrows, the d-pad and the left
    // stick; Space, Enter and A choose; Backspace and B go back.
    Action Press(unsigned buttons);
    // The "Sound" row: the frontend plays nothing while this is set (--mute starts with it set)
    bool muted = false;
    int Slot() const {
        return m_menu.Slot();
    }
    void SetSlot(int slot) {
        m_menu.SetSlot(slot);
    }
    // After Action::Command: which of the game's commands (TheGame().commands)
    size_t Command() const {
        return m_command;
    }
    // The speed the player chose: the game runs one frame in this many of the screen's
    int SlowBy() const;
    void SetSlowBy(int by);
    // A line to tell the player (why not, say), until the next press
    void Note(const std::string &text) {
        m_menu.Note(text);
    }
    // Over a picture of 32-bit pixels (red in the low byte)
    void Draw(uint32_t *pixels, int width, int height) const {
        m_menu.Draw(pixels, width, height);
    }

private:
    recomp::Menu m_menu;
    size_t m_command = 0;
};

} // namespace port
