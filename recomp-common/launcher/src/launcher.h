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
    // (a collection) the file chosen for another of its games, by the game's id: empty if none (a game that reads
    // another's files too, say)
    std::function<std::string(const char *)> file_of;
};

// A folder the player may choose for the game (a custom soundtrack's music files, say): a row of the Sound card with
// Choose… and Clear, kept in launcher.ini beside the game's file, given to the game as `argument FOLDER` while one
// is chosen (after the port's own arguments). The folder is only ever read by the launcher.
struct FolderChoice {
    const char *key;      // in launcher.ini, under the game's section
    const char *title;
    const char *detail;   // what it is for (shown while none is chosen)
    const char *argument; // "--soundtrack"
};

// Online play: two players, each on their own machine, one hosting and the other joining it. The game page gets an
// Online card under Play: Host starts the game with `host_argument` (it waits for the other player; the card says
// this machine's addresses to give them), Join with `join_argument ADDRESS` (the address typed, kept in launcher.ini
// under [online]). The game does the rest. nullptr: no card.
struct Online {
    const char *host_argument = nullptr; // "--host"
    const char *join_argument = nullptr; // "--join"
    int port = 0;                        // the UDP port the host waits on, as the card says it (0: none said)
    bool codes = false;                  // the game gives a host a code to hand out, which Join takes as well as an address
    const char *detail = "";             // the card's first lines: what a match is
    // A lobby: the matches hosts have listed, shown on the card to join with a press. `lobby` is the URL of the
    // game's list (JSON: {"matches": [{"code", "name", "waiting"}]}); a host lists its match when the player has
    // switched that on, with `list_argument`, and its name with `name_argument NAME` (given to Host and Join whenever the
    // player has a name, lobby or not: the game shows it to the other player). nullptr: no lobby.
    const char *lobby = nullptr;
    const char *list_argument = nullptr; // "--net-list"
    const char *name_argument = nullptr; // "--net-name"
    // Arcade while waiting: when the player switches it on ([online] arcade), Host adds `arcade_argument`, and the
    // host plays the game alone until the other player comes, who joins the game as it is. nullptr: no such switch.
    const char *arcade_argument = nullptr; // "--net-arcade"
    // Inviting a player from the waiting room (Collection::room): Host with `invite_argument THEIRS:MINE` (the two
    // players' ids) and `invite_name_argument NAME` (theirs), and the player's own name with `name_argument`; the match
    // isn't listed. nullptr: no Invite.
    const char *invite_argument = nullptr;      // "--net-invite"
    const char *invite_name_argument = nullptr; // "--net-invite-name"
    // The waiting room for a launcher of one game (a collection's is Collection::room, on its Online page): the URL a
    // launcher checks in at every few seconds while the Game page, with this card, is open, and the protocol said with
    // it. The card then has the invitations waiting for this player and the room's players to invite (with
    // `invite_argument`). nullptr: none.
    const char *room = nullptr;
    int room_protocol = 0;
    // The game's id as the online service knows it (the invitations a game hosts say it), for a launcher of one game:
    // its game in the waiting room. A collection's games have theirs (CollectionGame::id).
    const char *game = nullptr;
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
    // true: the launcher opens on the Game page (not on the first of the port's own, which stay above it in the
    // sidebar), and a dropped file goes to a page of the port's own only while that page is shown (else it is the
    // game file): for pages that are extras, not the way in
    bool open_on_game_page = false;
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
    // ... and below it, folders the player may choose (none: only the switch)
    std::vector<FolderChoice> folders;

    Online online;                            // the Online card (none unless its arguments are given)

    bool preset_summary = false;              // a line over Play saying which preset is chosen
    std::vector<Toggle> display;              // the Display card's switches (none: no card)

    // Controllers page. With actions, each player's keys and buttons can be chosen (bindings.h).
    recomp::BindTable bindings;
    const char *controls_heading = "WHILE PLAYING";
    std::vector<InfoRow> controls;            // what is fixed, as rows of text

    // Optional (this fork): online play through the online tunnel beside the launcher (cps-tunel), for a game with
    // netplay of its own over UDP. The game's page then has a card of its own (App::tunnel_card) besides `online`'s.
    // `port`: the game's netplay port (0: none); `arguments`: what goes after the game's own to host (address "") or to
    // join `address` ("127.0.0.1:PORT", the tunnel's).
    struct Tunnel {
        int port = 0;
        std::function<std::vector<std::string>(const std::string &address)> arguments;
    } tunnel;
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
// once, and the preset), Controllers (the same keys and buttons for all of them, or a game's own for a game that is
// given its own layout), Multi game (with modes that play several of the games one after another: MultiMode), and then
// a page for each game whose file has been chosen: Play, its Display switches and the features that are that game's
// alone.
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
    // The series it is one of, as the player reads it ("Street Fighter Alpha"), or nullptr. Two or more games of a series
    // with a page of their own are under one row of the sidebar, which opens and closes and has a page of the series'
    // games, and are kept together in the player's order and on the Games page.
    const char *series = nullptr;
};

// A choice of a multi-game mode's: a row of its card on the Multi game page, kept in launcher.ini under
// [multi.<the mode's key>]. One of `choices` (the first unless `initial` says), shown only while the choice
// `shown_with` (another of the mode's, by key; nullptr: always) has the value `shown_value`.
struct MultiChoice {
    const char *key;
    const char *title;
    const char *detail;
    std::vector<std::string> choices;
    int initial = 0;
    const char *shown_with = nullptr;
    int shown_value = 0;
};

// A key or button that does something while a multi-game mode's games play (the next game, say): a row on its card,
// with keycaps for `keys` (a key, a gamepad's button: either does it).
struct MultiButton {
    const char *title;
    const char *detail;
    std::vector<const char *> keys;
};

// A mode that plays several of a collection's games one after another, on its own page (Multi game). The player
// chooses which of the games that have it are played, in the sidebar's order or shuffled, and the mode's choices;
// Start starts the first with what its Play starts it with and the mode's arguments after that. A game that ends with
// `next_code` has had its turn, and the next is started (after the last, the first again: shuffled anew); ended any
// other way (the player quit it), the mode stops.
struct MultiMode {
    const char *key;                 // its section of launcher.ini: [multi.<key>]
    const char *title;               // its card's heading, and Start's row
    const char *detail;              // what it is
    std::vector<const char *> games; // the ids of the games that have it
    std::vector<MultiChoice> choices;
    std::vector<MultiButton> buttons; // what works while it plays (shown under its choices, and while it plays)
    // The arguments for it, by the choices' values (in the order of `choices`)
    std::function<std::vector<std::string>(const std::vector<int> &values)> arguments;
    int next_code = 3;
};

struct Collection {
    const char *subtitle = "";
    const char *intro = "";       // the Games page's first lines
    const char *find_title = "";  // the row that looks in a folder for every game's file, and what it says
    const char *find_detail = "";
    std::vector<CollectionGame> games;
    // The keys and buttons every game shares (no actions: the first game's description's). A game whose
    // description has a table of its own can be given a layout of its own on the Controllers page, with that
    // table's buttons and names; the shared table's actions its own has not got are "none" in its settings.ini.
    recomp::BindTable bindings;
    // Modes that play several of the games one after another (none: no Multi game page)
    std::vector<MultiMode> multi;
    const char *multi_intro = "";  // the Multi game page's first lines
    // Online play's lobby for every game (a game's own is its Online card's: Online::lobby): the URL of every game's
    // open matches ({"matches": [{"game", "code", "name", "waiting"}]}, `game` a CollectionGame's id). With it, an
    // Online page after Multi game: every game's open matches to join, and a match to host in the lobby in any of
    // the games (with its Online's list and name arguments). nullptr: no page.
    const char *lobby = nullptr;
    const char *online_intro = ""; // the Online page's first lines
    // Join with a code, on the Online page: the URL a match's code is put at the end of, which answers which game it is
    // for ({"game", "title"}; 404: none), so that the right game is started with its Online's join argument and the
    // code. nullptr: no such section.
    const char *code_lookup = nullptr;
    // The waiting room, on the Online page: the URL a launcher checks in at every few seconds while the page is open
    // (online.h: room_check_in; `/leave` and `/decline` after it), which lists the other players there to invite to
    // a match (with their game's Online::invite_argument), and gives the invitations waiting for this one, each
    // accepted by joining its code. `protocol` is said with it: only players with the same are listed. nullptr: none.
    const char *room = nullptr;
    int room_protocol = 0;
    const char *file_name = "game file"; // what a game's file is called, as the Online page says it ("ROM set")
    // The category of the features the Online page shows too, as the Features page does (what a match shows: the
    // ping...), or nullptr
    const char *online_features = nullptr;
};

// A collection's own file defines this (and not describe()).
const Collection &describe_collection();

} // namespace launcher
