// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// What cps2recomp tells the launcher the recomp ports share (recomp-common/launcher/src/launcher.h): the
// words for a ROM set, the check of the player's, what the game is started with, the buttons that can be
// chosen. A game's own launcher is told about that game through LAUNCHER_* definitions (from
// games/<id>/game.cfg); the collection's (LAUNCHER_COLLECTION) about every game, from the table games.h
// that tools/gamecfg.py --collection writes from them all.

#include "launcher.h"

#include "../../machine/romset.h"
#include "../../port/bindings.h"

#include <algorithm>
#include <deque>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace {

// A game, as its game.cfg tells it
struct GameInfo {
    const char *id, *name, *rom_set, *rom_parent, *rom_name, *program, *graphics, *sound_program, *samples, *key;
    // Features whose switch is on the first page, under "Full screen" (game.cfg's `display`): name|Title|What it
    // says, with || between two of them (the game leaves these out of the list it gives for the Features page:
    // runtime/game.h, kFeatureFirstPage)
    const char* display;
    // How many players (game.cfg's `players`; "" is two), and the game's names for its buttons ("Attack|Jump",
    // port/bindings.h; "": a fighting game's six)
    const char *players, *buttons;
    // The board (game.cfg's `board` and `cps_b`: "" for a CPS-2; machine/romset.h)
    const char *board, *cps_b;
    // Another set the game reads too (game.cfg's extra_set; empty: none). In a collection, the set chosen for the
    // game that is that set's is given to it (--rom2); else the game looks for it beside its own
    const char* extra_set;
};
#ifdef LAUNCHER_COLLECTION
const GameInfo* game_of_set(const char* set);
#endif

#ifdef LAUNCHER_COLLECTION
// (the table every game shares in the collection: below)
const port::BoundTable& shared_table();
#endif

// (the words made for each game, kept for as long as the launcher runs: the descriptions point into them)
const char* keep(std::string text) {
    static std::deque<std::string> kept;
    kept.push_back(std::move(text));
    return kept.back().c_str();
}

// A ROM set: the set's zip (with its parent set's zip beside it, if the collection keeps shared files
// there) or a folder with the set's files. Every file is checked against the checksums the port was made
// from, and the program is deciphered with the set's key, as the game itself does when it starts. The
// launcher only ever reads it: never copies, moves or changes it.
launcher::FileCheck check_rom_set(const GameInfo& game, const std::string& path) {
    launcher::FileCheck check;
    std::error_code error;
    if (path.empty() || !std::filesystem::exists(path, error)) {
        check.message = "It isn't there any more.";
        return check;
    }
    // (the same reading the game does: its files by checksum, from the zip, its parent's or the folder)
    const cps2recomp::RomList list =
        cps2recomp::ParseRomList(game.program, game.graphics, game.sound_program, game.samples, game.key, game.rom_set, game.rom_parent,
                                 game.board, game.cps_b);
    cps2recomp::RomSet set;
    std::string message;
    if (cps2recomp::LoadRomSet(path, list, set, message)) {
        check.result = launcher::FileCheck::Result::Good;
        check.message = game.rom_name;
    } else {
        check.message = "Not usable: " + message + ". This port needs the set " + game.rom_set + " (" + game.rom_name + ")" +
                        (*game.key ? ", with its key file" : "") + ". Other versions and regions of the game will not work.";
    }
    return check;
}

// A set in a "rom" folder beside the launcher or above it (a build inside the source tree finds the tree's
// own, from the build folder or from the app bundle): rom/<id>/, or rom/ itself
std::string rom_set_nearby(const GameInfo& game, const std::filesystem::path& program_folder) {
    std::filesystem::path folder = program_folder;
    std::error_code error;
    for (int up = 0; up < 8 && !folder.empty(); up++, folder = folder.parent_path()) {
        for (const std::filesystem::path& rom : {folder / "rom" / game.id, folder / "rom"}) {
            if (std::filesystem::is_directory(rom, error) && check_rom_set(game, rom.string()).usable()) {
                return rom.string();
            }
        }
        if (folder == folder.parent_path()) {
            break;
        }
    }
    return {};
}

// Where in a folder of ROM sets this game's may be: its zip or a folder of its files, by the set's name (and
// then the parent set's, which a merged collection names it by)
std::vector<std::filesystem::path> rom_set_candidates(const GameInfo& game, const std::filesystem::path& folder) {
    std::vector<std::filesystem::path> list;
    for (const std::string& name : {std::string(game.rom_set), std::string(game.rom_parent)}) {
        if (!name.empty()) {
            list.push_back(folder / (name + ".zip"));
            list.push_back(folder / name);
        }
    }
    return list;
}

struct FeatureSwitch {
    std::string name, title, detail;
};
std::vector<FeatureSwitch> feature_switches(const std::string& all) {
    std::vector<FeatureSwitch> list;
    for (size_t at = 0; at < all.size();) {
        const size_t end = std::min(all.find("||", at), all.size());
        const std::string one = all.substr(at, end - at);
        const size_t first = one.find('|'), second = first == std::string::npos ? first : one.find('|', first + 1);
        auto trim = [](std::string text) {
            text.erase(0, text.find_first_not_of(' '));
            text.erase(text.find_last_not_of(' ') + 1);
            return text;
        };
        if (second != std::string::npos && !trim(one.substr(0, first)).empty()) {
            list.push_back({trim(one.substr(0, first)), trim(one.substr(first + 1, second - first - 1)), trim(one.substr(second + 1))});
        }
        at = end + 2;
    }
    return list;
}

// What a launcher is told about one game
launcher::Description describe_game(const GameInfo& game) {
    launcher::Description d;
    const std::string set = std::string(game.rom_set) + " (" + game.rom_name + ")";
    d.original = "in the arcade";
    d.file_heading = "ROM SET";
    // (a game that names its own buttons is not a fighting game, and has no training mode; the collection's controls,
    // which its first game's description gives, are for every game, the fighting games among them)
    const bool fighting = !*game.buttons;
#ifdef LAUNCHER_COLLECTION
    const bool forAll = true;
#else
    const bool forAll = fighting;
#endif
    d.file_intro = keep("Choose your own copy of the game: the arcade ROM set " + set + (*game.key ? ", with its key file" : "") +
                        ". Only this version works. It stays where it is: the launcher only reads it.\n\nWhile playing, Esc or F1 on "
                        "the keyboard, or the left stick's click (or Start and Select together) on a gamepad, opens the in-game menu: " +
                        (fighting ? "training mode and the other modes" : "the game's options") + ", save states, speed, and every feature.");
#ifdef LAUNCHER_COLLECTION
    if (const GameInfo* other = game.extra_set && *game.extra_set ? game_of_set(game.extra_set) : nullptr) {
        d.file_intro = keep(std::string(d.file_intro) + "\n\nIt takes things from " + other->name + " too: choose that game's ROM set (" +
                            other->rom_set + ") on its own page as well.");
    }
#endif
    d.file_empty = keep("None chosen. Drop " + std::string(game.rom_set) + ".zip or its folder here, or choose the zip.");
    d.file_not_set_up = "The game isn't set up yet. Open the launcher (without --play) once to choose your ROM set.";
    d.file_filter = {"ROM set (zip)", "zip"};
    d.file_key = "rom";
    d.check = [game](const std::string& path) { return check_rom_set(game, path); };
    d.find_nearby = [game](const std::filesystem::path& folder) { return rom_set_nearby(game, folder); };
    d.candidates = [game](const std::filesystem::path& folder) { return rom_set_candidates(game, folder); };
    const std::vector<FeatureSwitch> switches = feature_switches(game.display);
    // What the game is started with: the player's ROM set (read only) and their saves folder
    const std::string extra = game.extra_set ? game.extra_set : "";
    d.arguments = [switches, extra](const launcher::Launch& launch) {
        std::vector<std::string> args = {"--rom", launch.file, "--eeprom", (launch.folder / "saves" / "eeprom.bin").string(),
                                         "--settings", (launch.folder / "settings.ini").string()};
#ifdef LAUNCHER_COLLECTION
        if (const GameInfo* other = extra.empty() || !launch.file_of ? nullptr : game_of_set(extra.c_str())) {
            const std::string file = launch.file_of(other->id);
            if (!file.empty()) {
                args.push_back("--rom2");
                args.push_back(file);
            }
        }
#endif
        if (launch.on("fullscreen")) {
            args.push_back("--fullscreen");
        }
        // (each first-page feature on or off as its switch is: said every time, so that the switch decides)
        std::string features;
        for (const FeatureSwitch& one : switches) {
            features += (features.empty() ? "" : ",") + std::string(launch.on(one.name.c_str()) ? "" : "-") + one.name;
        }
        if (!features.empty()) {
            args.push_back("--features");
            args.push_back(features);
        }
        return args;
    };
    d.saves_folder = true;
    // (the custom soundtrack's files: port/soundtrack.h)
    d.folders = {{"soundtrack", "Custom soundtrack",
                  "Your own music, played in place of the board's music while the Custom soundtrack feature is on: a folder of music "
                  "plays as a playlist, or files named as the songs.txt the game writes there replace those songs. None chosen: the "
                  "soundtrack folder in the game's own folder.",
                  "--soundtrack"}};
    d.display = {{"fullscreen", "Full screen", "F11 switches while playing.", false, nullptr}};  // (a window unless asked)
    for (const FeatureSwitch& one : switches) {
        d.display.push_back({keep(one.name), keep(one.title), keep(one.detail), false, nullptr});
    }
#ifdef LAUNCHER_COLLECTION
    d.bindings = shared_table().table;
#else
    d.bindings = port::MakeBoundTable(*game.players ? atoi(game.players) : 2, game.buttons, false).table;
#endif
    d.controls_heading = "WHILE PLAYING";
    d.controls = {{"Keys", keep(std::string("Esc, F1, the left stick's click, or Start and Select together: the in-game menu (") + (forAll ? "training mode and the other modes" : "the game's options") + ", save states, coin, cheats, quit)   P / O: stop, one frame on   - / =: speed   " + (forAll ? "Backspace: start over (training)   " : "") + "F5 / F9: save and load a state   F6 / F7: slot   F2: operator's test switch   F11: full screen. On a gamepad: right stick's click stops the game and starts it again, left trigger goes on a frame while it is stopped, left shoulder starts over. (A key or button given to the game above does only that.)")}};
    return d;
}

}  // namespace

#ifdef LAUNCHER_COLLECTION

#include "games.h"

namespace {
// As many players as the game with the most, and each button called as a fighting game calls it and as the other
// games call it (port/bindings.h)
const port::BoundTable& shared_table() {
    static const port::BoundTable& table = [&]() -> const port::BoundTable& {
        int players = 2;
        std::vector<std::string> names(port::kButtons);
        for (const GameInfo& game : kGames) {
            players = std::max(players, *game.players ? atoi(game.players) : 2);
            std::string list = game.buttons;
            for (int button = 0; button < port::kButtons && !list.empty(); button++) {
                const size_t bar = list.find('|');
                std::string name = list.substr(0, bar);
                name.erase(0, name.find_first_not_of(' '));
                name.erase(name.find_last_not_of(' ') + 1);
                list = bar == std::string::npos ? "" : list.substr(bar + 1);
                if (!name.empty() && ("/" + names[button] + "/").find("/" + name + "/") == std::string::npos) {
                    names[button] += (names[button].empty() ? "" : "/") + name;
                }
            }
        }
        std::string buttons;
        for (int button = 0; button < port::kButtons; button++) {
            buttons += (button ? "|" : "") + names[button];
        }
        return port::MakeBoundTable(players, buttons, true);
    }();
    return table;
}
// The collection's game whose ROM set this is
const GameInfo* game_of_set(const char* set) {
    for (const GameInfo& game : kGames) {
        if (std::string(game.rom_set) == set) {
            return &game;
        }
    }
    return nullptr;
}
}  // namespace

const launcher::Collection& launcher::describe_collection() {
    static const Collection about = [] {
        Collection c;
        c.subtitle = "Capcom CPS-1 and CPS-2 arcade games, native";
        c.intro = "Each game needs your own copy of it: its arcade ROM set (a CPS-2 game's with its key file). Show the launcher the folder you keep them in, "
                  "or drop a set (or the folder) on this window. The sets stay where they are: the launcher only reads them.\n\n"
                  "While playing, Esc or F1 on the keyboard, or the left stick's click (or Start and Select together) on a gamepad, "
                  "opens the in-game menu.";
        c.find_title = "Look in a folder";
        c.find_detail = "Finds each game's ROM set in a folder of them, by the set's name (sfa2u.zip, say).";
        for (const GameInfo& game : kGames) {
            c.games.push_back({game.id, game.name, game.id, "", describe_game(game)});
        }
        return c;
    }();
    return about;
}

#else

#ifndef LAUNCHER_DISPLAY_SWITCHES
#define LAUNCHER_DISPLAY_SWITCHES ""
#endif
#ifndef LAUNCHER_PLAYERS
#define LAUNCHER_PLAYERS ""
#endif
#ifndef LAUNCHER_BUTTONS
#define LAUNCHER_BUTTONS ""
#endif
#ifndef LAUNCHER_BOARD
#define LAUNCHER_BOARD ""
#endif
#ifndef LAUNCHER_CPS_B
#define LAUNCHER_CPS_B ""
#endif

const launcher::Description& launcher::describe() {
    static const Description about = describe_game({LAUNCHER_GAME_EXE, LAUNCHER_GAME_NAME, LAUNCHER_ROM_SET, LAUNCHER_ROM_PARENT, LAUNCHER_ROM_NAME,
                                                    LAUNCHER_ROM_PROGRAM, LAUNCHER_ROM_GRAPHICS, LAUNCHER_ROM_SOUND_PROGRAM, LAUNCHER_ROM_SAMPLES,
                                                    LAUNCHER_ROM_KEY, LAUNCHER_DISPLAY_SWITCHES, LAUNCHER_PLAYERS, LAUNCHER_BUTTONS,
                                                    LAUNCHER_BOARD, LAUNCHER_CPS_B, ""});
    return about;
}

#endif
