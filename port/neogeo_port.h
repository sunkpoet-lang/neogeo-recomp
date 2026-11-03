// SPDX-License-Identifier: GPL-3.0-only
// Part of neogeo-recomp.
//
// A Neo Geo game's port: the board (machine/neogeo.h) with what every port of the collection has around it, from the
// pieces that do not depend on a board: the game's sources and features (runtime/game.h, runtime/neogeo_game.h) and
// its settings.ini, the in-game menu (menu.h), save states, the HUD (hud.h). The window is neogeo_window.cpp, which
// also takes the launcher's controls (bindings.h). Capcom's port (port.h, main.cpp) is not used: its loop is built
// around the CPS boards' Machine. (openspec change neogeo-frontend)
#pragma once

#include "../machine/neogeo.h"
#include "menu.h"
#include "neogeo_online.h"
#include "present.h"

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
    OnlineOptions online;      // --host, --join, --delay, --stress (neogeo_online.h)
    std::string presentTest;   // --present-test F.png: (headless) the last picture drawn by the GPU path, no window
    std::string presentBar;    // --present-bar [N[.K]]: ... with the menu bar's strip above it (menu N open; its entry K's)
    std::string windowEvents;  // --window-events: mouse and key events put to the window at times (neogeo_window.cpp)
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
    // Online (neogeo_online.h): no stopping, slowing, save states, reset or test switch, and the menu's coin goes
    // through the session as this player's
    bool online = false;
    std::string notice;       // said at the top of the picture ...
    uint64_t noticeUntil = 0; // ... until this tick

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
    // The small picture kept beside a save state (a quarter of the board's, for the menu bar's slots)
    static std::string StatePicture(const std::string &statePath);
    // The board's picture (no HUD, menu or notice) as a PNG in the screenshots folder beside the save states, named
    // for the game and the time; false with the reason
    bool Screenshot(std::string &file, std::string &error) const;
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

// Where in a window `width` x `height` (its pixels) the picture goes, as the Display page says
SDL_FRect NeoGeoWhere(int width, int height);
// The Display page's shape and scaling now
Shape DisplayShapeNow();
Scaling DisplayScalingNow();
// The window's menu bar (recomp-common/menubar.h), above the picture in a window: its menus (the in-game menu's rows
// and the window's own, made again whenever what they show may have changed), how many of the window's pixels make a
// dot of its letters in a window so many pixels tall, how tall its strip is then, and its picture as the presenter's
// overlay (the strip, and the menus open)
void BarMenus(NeoGeoPort &port, recomp::MenuBar &bar, bool fullscreen);
int BarDot(int windowHeight);
int BarStrip(int windowHeight);
void BarPicture(const recomp::MenuBar &bar, int width, int height, std::vector<uint32_t> &pixels, std::vector<SDL_Rect> &regions);

} // namespace port
