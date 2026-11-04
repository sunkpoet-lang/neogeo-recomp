// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
//
// Controllers SDL doesn't know as gamepads (cps2recomp issue #213: an arcade panel that isn't an XInput one).
// A port only opens gamepads, and a controller SDL has no mapping for is only a joystick to it, so it was never
// seen. A gamecontrollerdb.txt, in SDL_GameControllerDB's form (https://github.com/mdqinc/SDL_GameControllerDB: a
// line for each controller, "GUID,name,a:b0,b:b1,...,platform:Windows,"), gives SDL the mappings for more of them.
// AddGamepadMappings, called once SDL's gamepads are started, reads that file from each folder given (beside the
// program, the player's folder), a later one's mapping winning over an earlier one's for the same controller.
// SDL takes only the lines for the system it runs on. A port's launcher and game call it with the same folders.
#pragma once

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_log.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace recomp {

constexpr const char *kGamepadMappingsFile = "gamecontrollerdb.txt";

// The mappings added from the folders' files (a file named twice is read once; one SDL couldn't read is logged)
inline int AddGamepadMappings(const std::vector<std::filesystem::path> &folders) {
    int added = 0;
    std::vector<std::filesystem::path> read;
    for (const std::filesystem::path &folder : folders) {
        if (folder.empty()) {
            continue;
        }
        std::error_code error;
        const std::filesystem::path file = folder / kGamepadMappingsFile;
        if (!std::filesystem::is_regular_file(file, error)) {
            continue;
        }
        std::filesystem::path same = std::filesystem::weakly_canonical(file, error);
        if (error) {
            same = file;
        }
        bool again = false;
        for (const std::filesystem::path &before : read) {
            again = again || before == same;
        }
        if (again) {
            continue;
        }
        read.push_back(same);
        const auto name = file.u8string(); // (SDL takes UTF-8)
        const std::string utf8(name.begin(), name.end());
        const int count = SDL_AddGamepadMappingsFromFile(utf8.c_str());
        if (count < 0) {
            SDL_Log("%s: %s", utf8.c_str(), SDL_GetError());
        } else {
            SDL_Log("%s: %d controller mappings", utf8.c_str(), count);
            added += count;
        }
    }
    return added;
}

} // namespace recomp
