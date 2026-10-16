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
};

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
        cps2recomp::ParseRomList(game.program, game.graphics, game.sound_program, game.samples, game.key, game.rom_set, game.rom_parent);
    cps2recomp::RomSet set;
    std::string message;
    if (cps2recomp::LoadRomSet(path, list, set, message)) {
        check.result = launcher::FileCheck::Result::Good;
        check.message = game.rom_name;
    } else {
        check.message = "Not usable: " + message + ". This port needs the set " + game.rom_set + " (" + game.rom_name +
                        "), with its key file. Other versions and regions of the game will not work.";
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
    d.file_intro = keep("Choose your own copy of the game: the arcade ROM set " + set +
                        ", with its key file. Only this version works. It stays where it is: the launcher only reads it.\n\nWhile playing, Esc or F1 on "
                        "the keyboard, or the left stick's click (or Start and Select together) on a gamepad, opens the in-game menu: training mode "
                        "and the other modes, save states, speed, and every feature.");
    d.file_empty = keep("None chosen. Drop " + std::string(game.rom_set) + ".zip or its folder here, or choose the zip.");
    d.file_not_set_up = "The game isn't set up yet. Open the launcher (without --play) once to choose your ROM set.";
    d.file_filter = {"ROM set (zip)", "zip"};
    d.file_key = "rom";
    d.check = [game](const std::string& path) { return check_rom_set(game, path); };
    d.find_nearby = [game](const std::filesystem::path& folder) { return rom_set_nearby(game, folder); };
    d.candidates = [game](const std::filesystem::path& folder) { return rom_set_candidates(game, folder); };
    const std::vector<FeatureSwitch> switches = feature_switches(game.display);
    // What the game is started with: the player's ROM set (read only) and their saves folder
    d.arguments = [switches](const launcher::Launch& launch) {
        std::vector<std::string> args = {"--rom", launch.file, "--eeprom", (launch.folder / "saves" / "eeprom.bin").string(),
                                         "--settings", (launch.folder / "settings.ini").string()};
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
                  "Your own music files, played in place of the board's music while the Custom soundtrack feature is on. None chosen: "
                  "the soundtrack folder in the game's own folder. The songs.txt the game writes there names the files.",
                  "--soundtrack"}};
    d.display = {{"fullscreen", "Full screen", "F11 switches while playing.", false, nullptr}};  // (a window unless asked)
    for (const FeatureSwitch& one : switches) {
        d.display.push_back({keep(one.name), keep(one.title), keep(one.detail), false, nullptr});
    }
    d.bindings = port::BindingTable();
    d.controls_heading = "WHILE PLAYING";
    d.controls = {{"Keys", "Esc, F1, the left stick's click, or Start and Select together: the in-game menu (training mode and the other modes, save states, coin, cheats, quit)   P / O: stop, one frame on   - / =: speed   Backspace: start over (training)   F5 / F9: save and load a state   F6 / F7: slot   F2: operator's test switch   F11: full screen. On a gamepad: right stick's click stops the game and starts it again, left trigger goes on a frame while it is stopped, left shoulder starts over. (A key or button given to the game above does only that.)"}};
    return d;
}

}  // namespace

#ifdef LAUNCHER_COLLECTION

#include "games.h"

const launcher::Collection& launcher::describe_collection() {
    static const Collection about = [] {
        Collection c;
        c.subtitle = "CPS-2 arcade games, native";
        c.intro = "Each game needs your own copy of it: its arcade ROM set, with its key file. Show the launcher the folder you keep them in, "
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

const launcher::Description& launcher::describe() {
    static const Description about = describe_game({LAUNCHER_GAME_EXE, LAUNCHER_GAME_NAME, LAUNCHER_ROM_SET, LAUNCHER_ROM_PARENT, LAUNCHER_ROM_NAME,
                                                    LAUNCHER_ROM_PROGRAM, LAUNCHER_ROM_GRAPHICS, LAUNCHER_ROM_SOUND_PROGRAM, LAUNCHER_ROM_SAMPLES,
                                                    LAUNCHER_ROM_KEY, LAUNCHER_DISPLAY_SWITCHES});
    return about;
}

#endif
