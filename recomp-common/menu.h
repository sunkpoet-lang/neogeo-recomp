// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// The in-game menu shared by recomp ports (native ports of games made by static recompilation): save states
// (save, load, slot), the speed the game runs at, what the game itself offers (rows the host names, and its
// optional features and commands, a page for each of their categories), and leaving the game.
//
// It knows nothing of any game or system, and nothing of SDL. The host (a port's frontend) says what the
// rows are, opens it, stops the game while it is open, gives it the player's presses as the bits below and
// carries out what Buttons returns; the menu draws itself over a copy of the game's picture.
//
// C++17, no dependencies. C hosts use menu_c.h.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace recomp {

// The menu's own buttons. The host turns its system's into these (several of the system's may be one).
enum : unsigned {
    kMenuUp = 1,
    kMenuDown = 2,
    kMenuLeft = 4,
    kMenuRight = 8,
    kMenuConfirm = 16, // chooses a row, changes it, goes into a page (as Right does)
    kMenuBack = 32,    // leaves a page for the one it was reached from; on the first page, closes the menu
};

// A row of the first page.
struct MenuRow {
    enum Kind {
        Save,   // "Save state": Buttons returns Chosen::Save
        Load,   // "Load state": Chosen::Load
        Slot,   // the slot (1-9) those two use
        Speed,  // one of MenuConfig::speeds (left out when there are none)
        Pages,  // stands for several rows: the commands of no category, then a row for each category's page
        Page,   // a row for one page, named by `title`, wherever the host wants it; Pages then leaves it out
        Action, // the host's own: Chosen::Action with its id
        Quit,   // "Quit game": Chosen::Quit
        Resume, // closes the menu
        Mute,   // "Sound": on or muted, through MenuConfig::muted and setMuted (left out without them)
    };
    Kind kind;
    int id;                  // Action: handed back when chosen
    std::string title;       // Action; Page: the page's name
    std::string description; // Action
    MenuRow(Kind kind, int id = 0, std::string title = {}, std::string description = {})
        : kind(kind), id(id), title(std::move(title)), description(std::move(description)) {}
};

// An optional feature of the game: on or off, or a choice of several things.
struct MenuFeature {
    std::string category; // the page it is on
    std::string title;
    std::string description;
    // For a choice: the names, the first being the one that means off. Empty: on (1) or off (0).
    std::vector<std::string> choices = {};
};

// Something the game can do when asked: Chosen::Command with its place in MenuConfig::commands.
struct MenuCommand {
    std::string category; // the page it is on ("": the first page, where the Pages row is)
    std::string title;
    std::string description;
};

// More about a page than its name, for the pages that need it. A page is a category of features and
// commands; one with a parent is reached from the parent's page, not from the first.
struct MenuPage {
    std::string name;        // the category
    std::string parent;      // the page its row is on ("": the first page)
    std::string description; // what its row says ("": "The page of <name> choices.")
    bool counts = true;      // its row says how many of its features are on
};

struct MenuConfig {
    // The first page, in groups; a line is drawn between groups.
    std::vector<std::vector<MenuRow>> groups;
    std::vector<MenuFeature> features;
    std::vector<MenuCommand> commands;
    // Pages that are under another page, or whose rows say something of their own. A page under another
    // comes on its parent's page after the parent's commands and before its features, in this order.
    std::vector<MenuPage> pages;
    // A feature's value now (by its place in `features`), and changing it
    std::function<int(size_t feature)> value;
    std::function<void(size_t feature, int value)> setValue;
    // The Speed row: the speeds' names, the first being full speed, and what the row's description says
    std::vector<std::string> speeds;
    std::string speedDescription;
    // The Mute row: whether the game's sound is off now, and switching it off and on
    std::function<bool()> muted;
    std::function<void(bool muted)> setMuted;
    // What the menu's buttons are called on this system, for the hint line: kMenuConfirm ("a button",
    // "A, B or C") and what closes the menu ("Esc", "Esc or Select")
    std::string confirmName = "a button";
    std::string closeName = "Esc";
};

class Menu {
public:
    struct Chosen {
        enum Kind { None, Save, Load, Quit, Action, Command } kind = None;
        int id = 0; // Action: the row's id. Command: its place in MenuConfig::commands.
    };

    // What the menu shows. May be called again at any time (before each Open, say); the page and the row
    // the player was on are kept where they still exist.
    void Configure(MenuConfig config);
    void Open();
    void Close() {
        m_open = false;
    }
    bool IsOpen() const {
        return m_open;
    }
    // Buttons newly pressed (kMenu... bits): Up/Down choose, Confirm changes a row or goes into a page,
    // Left/Right change a row with several values, Right on a page's row goes into that page, Left and Back
    // leave a page (for the one it was reached from), Back on the first page closes.
    // Returns what the host is to do (with Slot() for Save and Load); the host closes the menu if that
    // went well. Resume and Back close it themselves.
    Chosen Buttons(unsigned pressed);
    int Slot() const {
        return m_slot;
    }
    void SetSlot(int slot) { // 1-9, around
        m_slot = (slot + 8) % 9 + 1;
    }
    // The speed chosen, by its place in MenuConfig::speeds (0: full speed)
    int Speed() const {
        return m_speed;
    }
    void SetSpeed(int speed);
    // A line to tell the player (why not, say), until the next press
    void Note(const std::string &text) {
        m_note = text;
    }
    // Over a picture of 32-bit pixels (red in the low byte)
    void Draw(uint32_t *pixels, int width, int height) const;

private:
    struct Row {
        int kind;
        size_t index;         // of the feature, the command or the first page's row
        std::string page;     // the page a row leads to
        bool divider = false; // a line above it: the first of a group of rows
    };
    void Fill();
    const MenuPage *PageOf(const std::string &name) const;
    std::string Parent(const std::string &page) const;
    bool Exists(const std::string &page) const;
    void Enter(const std::string &page);
    void Leave();
    MenuConfig m_config;
    std::vector<MenuRow> m_first; // the first page's rows as given, groups one after another
    bool m_open = false;
    std::vector<Row> m_rows;
    std::string m_page;     // the category shown ("": the first page)
    int m_selected = 0;
    std::vector<int> m_left; // (the rows the pages above this one were left on, the first page's first)
    int m_speed = 0;
    int m_slot = 1;
    std::string m_note;
};

} // namespace recomp
