// SPDX-License-Identifier: GPL-3.0-only
// Part of cps2recomp (the user's fork: the Neo Geo board).
//
// A Neo Geo game's port: the board (machine/neogeo.h) with what every port of the collection has around it, from the
// pieces that do not depend on a board: the game's sources and features (runtime/game.h, runtime/neogeo_game.h) and
// its settings.ini, the in-game menu (menu.h), save states, the HUD (hud.h). The window is neogeo_window.cpp, which
// also takes the launcher's controls (bindings.h). Capcom's port (port.h, main.cpp) is not used: its loop is built
// around the CPS boards' Machine. (openspec change neogeo-frontend)
#pragma once

#include "../machine/neogeo.h"
#include "menu.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace port {

struct NeoGeoOptions {
    std::string rom;           // the game's set (a zip or a folder); none: roms/<set>.zip beside the program
    std::string savesPath;     // --eeprom: a file in the game's saves folder (the backup RAM and the states go beside it)
    std::string settingsPath;  // settings.ini with the player's choices (features, controls)
    std::string features;      // --features A,B,-C
    std::string menuTest;      // --menu FRAME:KEYS[:shot.png]
    std::string keys;          // --keys TICK:KEY,...
    std::string hashLog, script, audioDump, stateSave, stateLoad, shotDir = ".";
    uint64_t frames = 0, shotEvery = 0, saveAt = 0;
    bool headless = false, mute = false, fullscreen = false, stats = false;
    int scale = 0;             // --scale: the window this many times the board's lines (0: the Display page's)
    std::string renderer;      // --renderer sdl|gpu
};

struct NeoGeoPort {
    NeoGeoOptions opt;
    std::unique_ptr<cps2recomp::NeoGeo> board;
    uint64_t frame = 0; // frames run
    uint64_t tick = 0;  // pictures of the screen (the game runs a frame in one of so many: menu.SlowBy())
    bool paused = false;
    int steps = 0;      // frames to go on by while stopped
    bool quit = false;
    bool test = false;  // the operator's test switch (F2)
    Menu menu;
    int coinFrames = 0, start2Frames = 0, testFrames = 0; // (the menu's coin, player 2's Start, test switch)

    // Loads the set, the sources and the settings; false with a message
    bool Init(std::string &error);
    // One frame with these controls (NeoGeo::players bits)
    void RunFrame(const uint16_t players[2]);
    // One tick; true if the game ran
    bool Tick(const uint16_t players[2]);
    // 'p' stop and go, 'o' a frame on, '-' '=' speed
    void Key(char key);
    void MenuAct(Menu::Action action);
    void RunCommand(size_t index);
    // The picture with the HUD and the stopped / speed line over it, and the menu's background (4:3, three times)
    void Shown(std::vector<uint32_t> &pixels) const;
    void MenuPicture(std::vector<uint32_t> &pixels, uint32_t &width, uint32_t &height) const;
    std::string StatePath(int slot) const;
    bool SaveState(const std::string &path, std::string &error);
    bool LoadState(const std::string &path, std::string &error);
    // Keeps the backup RAM for the next run
    void Finish();

private:
    std::string BackupPath() const;
};

// The features every Neo Geo game has from its port (the Display page's), after its own (RegisterGame)
void RegisterNeoGeoPortFeatures();

int RunNeoGeoWindow(NeoGeoPort &port);

} // namespace port
