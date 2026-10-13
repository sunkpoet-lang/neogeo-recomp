// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// What cps2recomp tells the launcher the recomp ports share (recomp-common/launcher/src/launcher.h): the
// words for a ROM set, the check of the player's, what the game is started with, the buttons that can be
// chosen. The game identifies itself through LAUNCHER_* definitions (from games/<id>/game.cfg).

#include "launcher.h"

#include "../../machine/romset.h"
#include "../../port/bindings.h"

#include <filesystem>
#include <system_error>

namespace {

// A ROM set: the set's zip (with its parent set's zip beside it, if the collection keeps shared files
// there) or a folder with the set's files. Every file is checked against the checksums the port was made
// from, and the program is deciphered with the set's key, as the game itself does when it starts. The
// launcher only ever reads it: never copies, moves or changes it.
launcher::FileCheck check_rom_set(const std::string& path) {
    launcher::FileCheck check;
    std::error_code error;
    if (path.empty() || !std::filesystem::exists(path, error)) {
        check.message = "It isn't there any more.";
        return check;
    }
    // (the same reading the game does: its files by checksum, from the zip, its parent's or the folder)
    const cps2recomp::RomList list = cps2recomp::ParseRomList(LAUNCHER_ROM_PROGRAM, LAUNCHER_ROM_GRAPHICS, LAUNCHER_ROM_SOUND_PROGRAM,
                                                              LAUNCHER_ROM_SAMPLES, LAUNCHER_ROM_KEY, LAUNCHER_ROM_SET, LAUNCHER_ROM_PARENT);
    cps2recomp::RomSet set;
    std::string message;
    if (cps2recomp::LoadRomSet(path, list, set, message)) {
        check.result = launcher::FileCheck::Result::Good;
        check.message = LAUNCHER_ROM_NAME;
    } else {
        check.message = "Not usable: " + message + ". This port needs the set " LAUNCHER_ROM_SET " (" LAUNCHER_ROM_NAME "), with its key file. Other versions and regions of the game will not work.";
    }
    return check;
}

// A set in a "rom" folder beside the launcher or above it (a build inside the source tree finds the tree's
// own, from the build folder or from the app bundle)
std::string rom_set_nearby(const std::filesystem::path& program_folder) {
    std::filesystem::path folder = program_folder;
    std::error_code error;
    for (int up = 0; up < 8 && !folder.empty(); up++, folder = folder.parent_path()) {
        const std::filesystem::path rom = folder / "rom";
        if (std::filesystem::is_directory(rom, error) && check_rom_set(rom.string()).usable()) {
            return rom.string();
        }
        if (folder == folder.parent_path()) {
            break;
        }
    }
    return {};
}

// What the game is started with: the player's ROM set (read only) and their saves folder.
std::vector<std::string> arguments(const launcher::Launch& launch) {
    std::vector<std::string> args = {"--rom", launch.file, "--eeprom", (launch.folder / "saves" / "eeprom.bin").string(),
                                     "--settings", (launch.folder / "settings.ini").string()};
    if (launch.on("fullscreen")) {
        args.push_back("--fullscreen");
    }
    return args;
}

}  // namespace

const launcher::Description& launcher::describe() {
    static const Description about = [] {
        Description d;
        d.original = "in the arcade";
        d.file_heading = "ROM SET";
        d.file_intro = "Choose your own copy of the game: the arcade ROM set " LAUNCHER_ROM_SET " (" LAUNCHER_ROM_NAME "), with its key file. Only this version works. It stays where it is: the launcher only reads it.";
        d.file_empty = "None chosen. Drop " LAUNCHER_ROM_SET ".zip or its folder here, or choose the zip.";
        d.file_not_set_up = "The game isn't set up yet. Open the launcher (without --play) once to choose your ROM set.";
        d.file_filter = {"ROM set (zip)", "zip"};
        d.file_key = "rom";
        d.check = check_rom_set;
        d.find_nearby = rom_set_nearby;
        d.arguments = arguments;
        d.saves_folder = true;
        d.display = {{"fullscreen", "Full screen", "F11 switches while playing.", false, nullptr}};  // (a window unless asked)
        d.bindings = port::BindingTable();
        d.controls_heading = "WHILE PLAYING";
        d.controls = {{"Keys", "Esc, F1, the left stick's click, or Start and Select together: menu (training mode, save states, coin, cheats, quit)   P / O: stop, one frame on   - / =: speed   Backspace: start over (training)   F5 / F9: save and load a state   F6 / F7: slot   F2: operator's test switch   F11: full screen. On a gamepad: right stick's click stops the game and starts it again, left trigger goes on a frame while it is stopped, left shoulder starts over. (A key or button given to the game above does only that.)"}};
        return d;
    }();
    return about;
}
