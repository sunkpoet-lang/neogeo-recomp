// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
//
// What a port tells the shared launcher about itself. The launcher (the Game, Features and Controllers
// pages, Play) is the same program for every port; a port adds one file that defines launcher::describe()
// and, with it, the check of the player's game file. The names every part needs as text in the program
// (the game's name, its program, its app id, the variable naming a folder for tests, the disclaimer) are
// compile definitions set by launcher.cmake: LAUNCHER_GAME_NAME, LAUNCHER_GAME_EXE, LAUNCHER_APP_ID,
// LAUNCHER_HOME_ENV, LAUNCHER_DISCLAIMER.
#pragma once

#include <SDL3/SDL.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "bindings.h"
#include "settings.h"

namespace launcher {

// What the port found the player's game file to be. The file is only ever read.
struct FileCheck {
    enum class Result {
        Good,      // the file the port was made from
        Usable,    // another release, or a kind this check cannot look into: the game may run
        NotUsable, // another game, not a game file, not there
    };
    Result result = Result::NotUsable;
    std::string message; // for the player
    bool usable() const { return result != Result::NotUsable; }
};

// A switch of the Display card, kept in launcher.ini under [video]
struct Toggle {
    const char *key;
    const char *title;
    const char *detail;
    bool on_by_default = false;
    // Another switch this one needs: switching this on switches that on, switching that off switches this off
    const char *needs = nullptr;
};

// What Play starts the game with
struct Launch {
    std::string file;                        // the player's game file
    std::filesystem::path folder;            // the player's folder: settings.ini is in it, saves/ if asked for
    std::function<bool(const char *)> on;    // a Display switch's state, by its key
};

struct InfoRow {
    const char *label;
    const char *text;
};

// What a page of the port's own is drawn with (ui.h and layout.h are the launcher's look)
struct PageContext {
    float width;                                 // of the page's column
    SDL_Window *window;                          // for file dialogs
    recomp::Ini &prefs;                          // launcher.ini
    recomp::Ini &settings;                       // settings.ini, which the game reads too
    const std::filesystem::path &folder;         // the player's folder
    const std::filesystem::path &program_folder; // where the launcher (and the game program) is
    bool playing;                                // the game is running
    int exit_code;                               // of its last run
    const std::string &play_error;               // why it did not start, or ""
    std::function<void()> play;                  // starts the game with the port's `arguments`
};

// A page of the port's own, in the sidebar before the launcher's pages
struct CustomPage {
    const char *name;
    void (*draw)(PageContext &page) = nullptr;
    // Optional: a file dropped on the window (to the page shown if it takes files, else to the first that does)
    void (*drop)(PageContext &page, const char *path) = nullptr;
    // Optional: true while the page has work on another thread, so that the window keeps being drawn
    bool (*busy)() = nullptr;
};

struct Description {
    // Under the game's name in the sidebar
    const char *subtitle = "native port";

    // The port's own pages (none: the launcher's three)
    std::vector<CustomPage> pages;
    // false: no Game page (and no game file, check or Display card): the port's pages have Play
    bool game_page = true;

    // The Features page: its name, its first lines (nullptr: the usual ones about Pure and Enhanced), and
    // whether the player chooses a preset (without: the features alone; the launcher still makes
    // "enhanced" the preset of a new settings file, so kFeatureEnhanced features start on)
    const char *features_name = "Features";
    const char *features_intro = nullptr;
    bool presets = true;

    // Where the original ran, as the Features page says it: "in the arcade", "on the console"
    const char *original = "";

    // The game file
    const char *file_heading = "GAME FILE";   // over its card
    const char *file_intro = "";              // the Game page's first lines
    const char *file_empty = "";              // in the card while none is chosen
    const char *file_not_set_up = "";         // what --play says when none is chosen
    SDL_DialogFileFilter file_filter = {"Game file", "*"};
    const char *file_key = "file";            // its key in launcher.ini, under [game]
    std::function<FileCheck(const std::string &path)> check;
    bool check_on_thread = false;             // for a check that reads a large file
    // Optional: a game file near the program (a build inside the source tree), or ""
    std::function<std::string(const std::filesystem::path &program_folder)> find_nearby;
    // Optional, for a collection: where in a folder the player chose this game's file may be (the names a
    // collection of such files gives it), tried in turn with `check`
    std::function<std::vector<std::filesystem::path>(const std::filesystem::path &folder)> candidates;

    // The game's arguments
    std::function<std::vector<std::string>(const Launch &launch)> arguments;
    bool saves_folder = false;                // make <folder>/saves before starting

    // The Game page's Sound card has one switch, "Muted" (off unless the player switches it on; kept in
    // launcher.ini under [audio]). When it is on the game is started with this argument after the port's
    // own. nullptr: no card (a game with no sound to switch off).
    const char *mute_argument = "--mute";

    bool preset_summary = false;              // a line over Play saying which preset is chosen
    std::vector<Toggle> display;              // the Display card's switches (none: no card)

    // Controllers page. With actions, each player's keys and buttons can be chosen (bindings.h).
    recomp::BindTable bindings;
    const char *controls_heading = "WHILE PLAYING";
    std::vector<InfoRow> controls;            // what is fixed, as rows of text
};

// The port's own file defines this.
const Description &describe();

// ---- A collection: one launcher for several games -------------------------------------------------------
//
// A launcher built with LAUNCHER_COLLECTION defined starts any of several games, each a program of its own
// beside it (the ports of one toolkit, say); its file defines describe_collection() instead of describe(). The
// LAUNCHER_* definitions then name the collection (its name, program, folder, test variable, disclaimer).
//
// Its pages: Games (each game's file, which the launcher can find in a folder the player chooses, or which is
// dropped on the window; Play), Features (those that more than one of the games has, set for all of them at
// once, and the preset), Controllers (the same keys and buttons for all of them), and then a page for each game
// whose file has been chosen: Play, its Display switches and the features that are that game's alone.
//
// Each game keeps its settings and saves in its own folder (named by its name, as a launcher of its own keeps
// them), and its settings.ini has everything in it: the game started by itself, or from a Steam shortcut, plays
// as the collection set it. A feature changed in a game's own menu is set for the other games too when the game
// ends, if they have it.
struct CollectionGame {
    const char *id;          // short name: --play ID, --page ID, and its sections of the launcher.ini
    const char *name;        // as the player reads it; its folder too (as LAUNCHER_GAME_NAME is a port's)
    const char *exe;         // its program, beside the launcher (as LAUNCHER_GAME_EXE)
    const char *home_env;    // a variable naming a folder to use instead of its own, for tests (LAUNCHER_HOME_ENV)
    Description description; // what a launcher of its own is told (its pages, if any, are not shown)
};

struct Collection {
    const char *subtitle = "";
    const char *intro = "";       // the Games page's first lines
    const char *find_title = "";  // the row that looks in a folder for every game's file, and what it says
    const char *find_detail = "";
    std::vector<CollectionGame> games;
};

// A collection's own file defines this (and not describe()).
const Collection &describe_collection();

} // namespace launcher
