// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
//
// Where a port keeps the player's things (settings, save states, the board's memory, logs). Usually the system's
// folder for them, a folder for each game (SDL_GetPrefPath: ~/Library/Application Support/<name>/ on macOS,
// ~/.local/share/<name>/ on Linux and Steam Deck, %APPDATA%\<name>\ on Windows). Portable mode: with a file named
// portable.txt in the folder the download was unpacked to (beside the launcher; on macOS beside the app, which
// is never written to), they are in user/<name>/ there instead, so the whole thing can go on a drive and be
// carried to another computer. Taking portable.txt away goes back to the system's folders; nothing is moved
// either way. The launcher and the game ask the same question, and so find the same folders. Nothing here knows
// of a game or SDL.
#pragma once

#include <cstdlib>
#include <filesystem>

namespace recomp {

constexpr const char *kPortableMarker = "portable.txt";
constexpr const char *kPortableFolder = "user";

// The folder the download was unpacked to, from the folder the program is in: that folder; on macOS the folder
// the app is in (the program is in <name>.app/Contents/MacOS or Resources); the AppImage's folder for a program
// run from one (APPIMAGE, which the AppImage sets: the program itself is in a folder that is gone when it ends).
inline std::filesystem::path InstallFolder(const std::filesystem::path &program_folder) {
    if (const char *appimage = std::getenv("APPIMAGE"); appimage && *appimage) {
        return std::filesystem::path(appimage).parent_path();
    }
    std::filesystem::path folder = program_folder.lexically_normal();
    if (!folder.has_filename() && folder.has_relative_path()) {
        folder = folder.parent_path();  // ("a/b/" is "a/b")
    }
    for (std::filesystem::path at = folder; at.has_relative_path(); at = at.parent_path()) {
        if (at.filename() == "Contents" && at.parent_path().extension() == ".app") {
            return at.parent_path().parent_path();
        }
    }
    return folder;
}

// The folder portable mode keeps the player's folders in (user/ beside portable.txt), or none when it is not on.
inline std::filesystem::path PortableFolder(const std::filesystem::path &program_folder) {
    const std::filesystem::path install = InstallFolder(program_folder);
    std::error_code error;
    return std::filesystem::is_regular_file(install / kPortableMarker, error) ? install / kPortableFolder : std::filesystem::path();
}

}  // namespace recomp
