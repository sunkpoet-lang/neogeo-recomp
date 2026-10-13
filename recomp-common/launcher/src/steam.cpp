// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#include "steam.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

#ifdef __linux__
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

#ifdef __linux__

bool on_path(const char* name) {
    const char* path = std::getenv("PATH");
    std::string paths = path ? path : "/usr/bin:/bin";
    size_t start = 0;
    while (start <= paths.size()) {
        size_t end = paths.find(':', start);
        std::string dir = paths.substr(start, end == std::string::npos ? std::string::npos : end - start);
        std::error_code error;
        if (!dir.empty() && fs::exists(fs::path(dir) / name, error)) {
            return true;
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return false;
}

int run(const std::vector<std::string>& args) {
    pid_t pid = fork();
    if (pid == 0) {
        std::vector<char*> argv;
        for (const std::string& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) < 0) {
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

std::string url_encode(const std::string& text) {
    std::string out;
    for (unsigned char c : text) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            char hex[4];
            std::snprintf(hex, sizeof hex, "%%%02X", c);
            out += hex;
        }
    }
    return out;
}

// The program a shortcut should start: the AppImage itself when running from one.
fs::path program() {
    if (const char* appimage = std::getenv("APPIMAGE"); appimage && *appimage) {
        return appimage;
    }
    std::error_code error;
    return fs::read_symlink("/proc/self/exe", error);
}

bool write_entry(const fs::path& path, const std::string& name, const std::string& comment, const std::string& exec, const fs::path& icon) {
    std::ofstream file(path, std::ios::trunc);
    file << "[Desktop Entry]\nType=Application\nName=" << name << "\nComment=" << comment << "\nExec=" << exec << "\n";
    if (!icon.empty()) {
        file << "Icon=" << icon.string() << "\n";
    }
    file << "Terminal=false\nCategories=Game;\n";
    return static_cast<bool>(file);
}

bool add(const fs::path& entry) {
    if (on_path("steamos-add-to-steam")) {
        return run({"steamos-add-to-steam", entry.string()}) == 0;
    }
    // What steamos-add-to-steam does: Steam's own link for adding a non-Steam game.
    std::ofstream(fs::path("/tmp") / "addnonsteamgamefile");
    return run({"steam", "steam://addnonsteamgame/" + url_encode(entry.string())}) == 0;
}

#endif

}  // namespace

bool can_add_to_steam() {
#ifdef __linux__
    return on_path("steam") || on_path("steamos-add-to-steam");
#else
    return false;
#endif
}

bool add_to_steam(std::string& message) {
#ifdef __linux__
    const char* home = std::getenv("HOME");
    if (!home) {
        message = "No home folder.";
        return false;
    }
    fs::path launcher = program();
    if (launcher.empty()) {
        message = "Can't tell where the launcher is.";
        return false;
    }
    fs::path share = fs::path(home) / ".local" / "share";
    std::error_code error;
    fs::create_directories(share / "applications", error);
    fs::create_directories(share / "icons", error);
    fs::path icon;
    if (const char* appdir = std::getenv("APPDIR"); appdir && fs::exists(fs::path(appdir) / LAUNCHER_GAME_EXE ".png", error)) {
        icon = share / "icons" / LAUNCHER_GAME_EXE ".png";
        fs::copy_file(fs::path(appdir) / LAUNCHER_GAME_EXE ".png", icon, fs::copy_options::overwrite_existing, error);
    }
    std::string quoted = "\"" + launcher.string() + "\"";
    fs::path game_entry = share / "applications" / (LAUNCHER_GAME_EXE ".desktop");
    fs::path launcher_entry = share / "applications" / (LAUNCHER_GAME_EXE "-launcher.desktop");
    if (!write_entry(game_entry, LAUNCHER_GAME_NAME, "Play " LAUNCHER_GAME_NAME, quoted + " --play", icon) ||
        !write_entry(launcher_entry, LAUNCHER_GAME_NAME " Launcher", "Game file, features and controllers for " LAUNCHER_GAME_NAME, quoted, icon)) {
        message = "Can't write the shortcuts in " + (share / "applications").string() + ".";
        return false;
    }
    if (!add(game_entry) || !add(launcher_entry)) {
        message = "Steam didn't take the shortcuts. Is Steam running? You can add " + launcher.filename().string() +
                  " with Steam's \"Add a Non-Steam Game\" instead (launch option --play to skip the launcher).";
        return false;
    }
    message = "Added \"" LAUNCHER_GAME_NAME "\" and \"" LAUNCHER_GAME_NAME " Launcher\" to Steam (Library, Non-Steam). If you move this file, add them again.";
    return true;
#else
    message = "Adding to Steam from here works on Linux and Steam Deck.";
    return false;
#endif
}
