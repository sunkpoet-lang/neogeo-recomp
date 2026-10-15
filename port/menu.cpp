// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The in-game menu (menu.h): the shared menu told what a CPS-2 game's is made of.
#include "menu.h"

#include "../machine/machine.h"
#include "../runtime/game.h"

#include <algorithm>
#include <iterator>

namespace port {

namespace {
    // The first page's own rows
    enum { kCoin = 1, kStart2, kReset, kTest };

    // The speeds: the game runs one frame in this many of the screen's
    const int kSlowBy[] = {1, 2, 4, 8};
    const char *const kSpeedNames[] = {"Full", "1/2", "1/4", "1/8"};
} // namespace

void Menu::Open(bool withStates) {
    const cps2recomp::Game &game = cps2recomp::TheGame();
    recomp::MenuConfig config;
    // The game's pages that belong at the head; then three groups with a line between them: the save
    // states, what the game offers, and leaving
    std::vector<recomp::MenuRow> top;
    for (const cps2recomp::Page &page : game.pages) {
        // (such pages are ways of running the game: their rows do not count features that are on)
        config.pages.push_back({page.name, page.parent, page.description, false});
        if (page.top && !*page.parent) {
            top.push_back({recomp::MenuRow::Page, 0, page.name});
        }
    }
    if (!top.empty()) {
        config.groups.push_back(std::move(top));
    }
    if (withStates) {
        config.groups.push_back({{recomp::MenuRow::Save}, {recomp::MenuRow::Load}, {recomp::MenuRow::Slot}});
    }
    config.groups.push_back({
        {recomp::MenuRow::Pages},
        {recomp::MenuRow::Speed},
        {recomp::MenuRow::Mute},
        {recomp::MenuRow::Action, kCoin, "Insert coin", "Puts a coin in the first slot."},
        {recomp::MenuRow::Action, kStart2, "Player 2 joins",
         "Presses player 2's Start: with a coin in (or free play), a second player to practise against."},
        {recomp::MenuRow::Action, kTest, "Test menu",
         "The game's own operator menu (the board's test switch, F2): its settings, such as difficulty and rounds. "
         "Its Exit row goes back."},
    });
    config.groups.push_back({
        {recomp::MenuRow::Action, kReset, "Reset", "Starts the board again, as if switched off and on."},
        {recomp::MenuRow::Quit},
        {recomp::MenuRow::Resume},
    });
    for (const cps2recomp::Feature &feature : game.features) {
        config.features.push_back({feature.category, feature.title, feature.description, {feature.choices.begin(), feature.choices.end()}});
    }
    for (const cps2recomp::Command &command : game.commands) {
        config.commands.push_back({command.category, command.title, command.description});
    }
    config.value = [](size_t feature) { return cps2recomp::FeatureSetting(cps2recomp::TheGame().features[feature].name); };
    config.setValue = [](size_t feature, int value) { cps2recomp::SetFeatureValue(cps2recomp::TheGame().features[feature].name, value); };
    config.speeds.assign(std::begin(kSpeedNames), std::end(kSpeedNames));
    config.speedDescription = "Left/Right: slow motion. While playing, P (or a click of the right stick) stops the game and starts it "
                              "again; O (or, once stopped, the left trigger) goes on by one frame; - and = change the speed.";
    config.muted = [this] { return muted; };
    config.setMuted = [this](bool to) { muted = to; };
    config.confirmName = "Enter or A";
    config.closeName = "Esc";
    m_menu.Configure(std::move(config));
    m_menu.Open();
}

int Menu::SlowBy() const {
    // (the slower of the player's choice and what the game's sources ask for: runtime/game.h)
    return std::max(kSlowBy[m_menu.Speed() < static_cast<int>(std::size(kSlowBy)) ? m_menu.Speed() : 0], cps2recomp::SlowMotion());
}

void Menu::SetSlowBy(int by) {
    int speed = 0;
    while (speed < static_cast<int>(std::size(kSlowBy)) - 1 && kSlowBy[speed] < by) {
        speed++;
    }
    m_menu.SetSpeed(speed);
}

Menu::Action Menu::Buttons(uint16_t pressed) {
    unsigned buttons = 0;
    buttons |= (pressed & cps2recomp::kUp) ? recomp::kMenuUp : 0;
    buttons |= (pressed & cps2recomp::kDown) ? recomp::kMenuDown : 0;
    buttons |= (pressed & cps2recomp::kLeft) ? recomp::kMenuLeft : 0;
    buttons |= (pressed & cps2recomp::kRight) ? recomp::kMenuRight : 0;
    if (pressed & (cps2recomp::kPunch1 | cps2recomp::kPunch2 | cps2recomp::kPunch3 | cps2recomp::kKick1 | cps2recomp::kKick2 |
                   cps2recomp::kKick3 | cps2recomp::kStart)) {
        buttons |= recomp::kMenuConfirm;
    }
    // (a press of something else, a coin button say, still takes the note away, as it always did)
    if (pressed && !buttons && m_menu.IsOpen()) {
        m_menu.Note("");
    }
    return Press(buttons);
}

Menu::Action Menu::Press(unsigned buttons) {
    const recomp::Menu::Chosen chosen = m_menu.Buttons(buttons);
    switch (chosen.kind) {
    case recomp::Menu::Chosen::Save: return Action::Save;
    case recomp::Menu::Chosen::Load: return Action::Load;
    case recomp::Menu::Chosen::Quit: return Action::Quit;
    case recomp::Menu::Chosen::Command:
        m_command = static_cast<size_t>(chosen.id);
        return Action::Command;
    case recomp::Menu::Chosen::Action:
        return chosen.id == kCoin ? Action::Coin : chosen.id == kStart2 ? Action::Start2 : chosen.id == kTest ? Action::Test : Action::Reset;
    default: return Action::None;
    }
}

} // namespace port
