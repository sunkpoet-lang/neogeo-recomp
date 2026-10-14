// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// What a game's own sources (games/<id>/src) tell the port: optional features the player can switch (cheats
// and the like), what to do around every frame, and their own state for save states.
#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include "../recomp-common/settings.h"

namespace cps2recomp {

class Archive; // (state.h)
class Machine; // (machine/machine.h)

// An optional feature. The launcher lists them (--list-features) and writes the player's choices to
// settings.ini, section [features]; the in-game menu shows them all.
struct Feature {
    const char *name;        // as in settings.ini and --features
    const char *category;    // heading in the launcher
    const char *title;
    const char *description;
    unsigned flags = 0;      // kFeatureEnhanced, kFeatureExclusive
    // For a feature that is a choice of several things rather than on or off: their names, the first being
    // the one that means off (FeatureValue gives the number of the one chosen)
    std::vector<const char *> choices = {};
};
enum : unsigned {
    kFeatureEnhanced = 2,  // on in the launcher's "Enhanced" preset
    kFeatureExclusive = 4, // only one of its category at a time
    kFeatureSession = 8,   // for this run only: not kept in settings.ini, and off again when the board is reset
    // Its switch is on the launcher's first page, with "Full screen" (the game's game.cfg names it there:
    // `display`), and not on the Features page: the launcher says at every start whether it is on
    // (--features NAME or -NAME), which goes before what settings.ini has. The in-game menu still has it.
    kFeatureFirstPage = 16,
    // A training aid (hit boxes, frame data, the dummy...): it works only while a training session is on
    // (Game::inTraining); elsewhere it reads as off (FeatureValue), though the menu and settings.ini keep it
    kFeatureTraining = 32,
};

// Something the game's sources can do when the player asks (a row of the in-game menu, and a key if it has
// one): starting a training session, putting the players back where they began and the like.
struct Command {
    const char *name;
    const char *category;    // the menu page it is on ("": the first page)
    const char *title;
    const char *description;
    void (*run)(Machine &machine);
    unsigned key = 0;        // kKeyNone, or the key that does it while playing
};
enum : unsigned {
    kKeyNone,
    kKeyRestart, // Backspace; a gamepad's left shoulder
    kKeyExtra,   // Backslash; a gamepad's left trigger held with the left shoulder
};

// More about a page of the in-game menu than its name (a page is a category of features and commands), for
// the pages that need it: a page under another page, or at the head of the menu.
struct Page {
    const char *name;        // the category
    const char *parent;      // the page its row is on ("": the first page)
    const char *description; // what its row says
    bool top = false;        // on the first page: before everything else, not among the other pages
};

struct Game {
    std::vector<Feature> features;
    std::vector<Command> commands;
    std::vector<Page> pages;
    void (*beforeFrame)(Machine &machine) = nullptr; // with the players' input set, before the game runs a frame
    void (*frame)(Machine &machine) = nullptr;       // after every frame the game ran
    void (*state)(Archive &ar) = nullptr;            // the game sources' own state, for save states
    // For a game whose sources can ask for a wider picture (video.h, Video::wide): how many columns the
    // widest will have with the player's choices as they are, so that the window starts in its shape
    int (*widestPicture)() = nullptr;
    // Not to run ahead now (the port's run-ahead): a mode whose own workings a save state does not wholly hold,
    // so that frames run and taken back would change it. Not given: run-ahead whenever it is on.
    bool (*noRunAhead)() = nullptr;
    // A training session is on (the game's own idea of one): the training aids (kFeatureTraining) work only then.
    // Not given: they work everywhere.
    bool (*inTraining)() = nullptr;
};
Game &TheGame();
// The game's sources fill it in (games/<id>/src; runtime/no_game.cpp when a game has none)
void RegisterGame();

// The features as the launcher and the settings rules know them (recomp-common/settings.h): --list-features
// prints recomp::FeatureListText of this
std::vector<recomp::FeatureInfo> FeatureList();

// The player's choices: from settings.ini ([features]: preset = pure | enhanced | custom, then name = 0 | 1),
// then `list` (comma-separated names, each on; "-name": off). Call after RegisterGame.
void LoadFeatures(const std::string &settingsPath, const std::string &list);
// Where settings.ini is (empty: none): a game's sources keep files of their own beside it
const std::string &SettingsPath();
bool FeatureOn(const char *name);
// Changes one and writes settings.ini as the launcher would (recomp::SetFeature: preset = custom, the rest of
// the file as it was)
void SetFeature(const char *name, bool on);
// For a feature with choices: which is chosen (0: the first, which is off), and choosing
int FeatureValue(const char *name);
void SetFeatureValue(const char *name, int value);
// What the player has chosen for a feature, whether it works now or not (the menu shows this; FeatureValue gives
// 0 for a training aid outside a training session)
int FeatureSetting(const char *name);
// The game's sources mark features of theirs as training aids (kFeatureTraining), by name
void TrainingAids(std::initializer_list<const char *> names);
// The board is being reset from the menu: the features that last a session go off
void EndSessionFeatures();

// The game's sources can ask for the game to run as fast as it goes, unseen and unheard (through a start-up
// they are working the controls for, say); the window does that while this is set
void SetFastForward(bool on);
bool FastForward();
// Run-ahead (the port's "run_ahead" feature) is running frames that will be taken back: the game's sources skip
// what must happen once (writing a file, say). Also set while the frame before them is put back.
bool RunningAhead();
void SetRunningAhead(bool on);
// ... and for it to run slowly: a frame of the game in every `by` pictures of the screen (1: as it is). The
// slower of this and the speed the player chose in the menu is the speed the game runs at.
void SetSlowMotion(int by);
int SlowMotion();

// The readout: lines of text the game's sources want shown over the picture while it plays (a training
// display and the like). The port draws each as a small panel.
struct HudLine {
    float x, y;       // where, as parts of the picture's width and height (0-1)
    int align;        // -1: the text starts at x; 0: is centred on it; 1: ends at it
    std::string text;
    bool operator==(const HudLine &) const = default;
};
void SetHud(std::vector<HudLine> lines);
const std::vector<HudLine> &Hud();
uint64_t HudSerial(); // (goes up when the lines change)

// Rectangles the game's sources want drawn over the picture (hit boxes and the like): an outline and a fill
// (faint unless said: `fill` is how solid, 255 at most), in the picture's own pixels (they may reach past its
// edges)
struct HudBox {
    int x, y, width, height;
    uint8_t r, g, b;
    uint8_t fill = 56;
    bool operator==(const HudBox &) const = default;
};
void SetBoxes(std::vector<HudBox> boxes);
const std::vector<HudBox> &Boxes();

// The 68000's ROM as a data read sees it (RomSet::program), for game sources that change the data the game
// reads from it (its colours, say) before or while it runs. Never its code: the translated code is made from
// the deciphered program and does not see this.
void SetProgram(uint8_t *program, uint32_t size);
uint32_t ProgramSize();
uint8_t *Program(uint32_t address);

// Work RAM as the game's sources see it: by the 68000's address (FF0000-FFFFFF). Values are big-endian.
void SetRam(uint8_t *ram);
uint8_t *Ram(uint32_t address);
inline uint8_t Ram8(uint32_t address) {
    return *Ram(address);
}
inline uint16_t Ram16(uint32_t address) {
    const uint8_t *p = Ram(address);
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}
inline uint32_t Ram32(uint32_t address) {
    const uint8_t *p = Ram(address);
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | p[3];
}
inline void SetRam8(uint32_t address, uint8_t value) {
    *Ram(address) = value;
}
inline void SetRam16(uint32_t address, uint16_t value) {
    uint8_t *p = Ram(address);
    p[0] = static_cast<uint8_t>(value >> 8);
    p[1] = static_cast<uint8_t>(value);
}
inline void SetRam32(uint32_t address, uint32_t value) {
    uint8_t *p = Ram(address);
    p[0] = static_cast<uint8_t>(value >> 24);
    p[1] = static_cast<uint8_t>(value >> 16);
    p[2] = static_cast<uint8_t>(value >> 8);
    p[3] = static_cast<uint8_t>(value);
}

} // namespace cps2recomp
