// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// What a port tells the shared launcher (launcher.h), for a game that is not there: any file that can be
// read is "the game". A real port checks the file against what it was made from, and only ever reads it.
#include "launcher.h"

#include <cstdio>

namespace {

launcher::FileCheck check(const std::string &path) {
    launcher::FileCheck result;
    std::FILE *file = std::fopen(path.c_str(), "rb"); // read only, always
    if (!file) {
        result.message = "The file can't be opened.";
        return result;
    }
    const bool empty = std::fgetc(file) == EOF;
    std::fclose(file);
    result.result = empty ? launcher::FileCheck::Result::Usable : launcher::FileCheck::Result::Good;
    result.message = empty ? "An empty file: it may not work." : "Example Game, verified.";
    return result;
}

std::vector<std::string> arguments(const launcher::Launch &launch) {
    std::vector<std::string> args = {launch.file, "--settings", (launch.folder / "settings.ini").string()};
    if (launch.on("fullscreen")) {
        args.push_back("--fullscreen");
    }
    if (launch.on("widescreen")) {
        args.push_back("--widescreen");
    }
    return args;
}

// (the usual layout: player 1 has the keyboard, both a gamepad)
constexpr recomp::BindAction kActions[] = {
    {"up", "Up", {SDL_SCANCODE_UP, SDL_SCANCODE_UNKNOWN}, {{SDL_GAMEPAD_BUTTON_DPAD_UP, -1}, {SDL_GAMEPAD_BUTTON_DPAD_UP, -1}}},
    {"down", "Down", {SDL_SCANCODE_DOWN, SDL_SCANCODE_UNKNOWN}, {{SDL_GAMEPAD_BUTTON_DPAD_DOWN, -1}, {SDL_GAMEPAD_BUTTON_DPAD_DOWN, -1}}},
    {"left", "Left", {SDL_SCANCODE_LEFT, SDL_SCANCODE_UNKNOWN}, {{SDL_GAMEPAD_BUTTON_DPAD_LEFT, -1}, {SDL_GAMEPAD_BUTTON_DPAD_LEFT, -1}}},
    {"right", "Right", {SDL_SCANCODE_RIGHT, SDL_SCANCODE_UNKNOWN}, {{SDL_GAMEPAD_BUTTON_DPAD_RIGHT, -1}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, -1}}},
    {"jump", "Jump", {SDL_SCANCODE_Z, SDL_SCANCODE_UNKNOWN}, {{SDL_GAMEPAD_BUTTON_SOUTH, -1}, {SDL_GAMEPAD_BUTTON_SOUTH, -1}}},
    {"fire", "Fire", {SDL_SCANCODE_X, SDL_SCANCODE_UNKNOWN}, {{-1, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER}, {-1, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER}}},
    {"start", "Start", {SDL_SCANCODE_RETURN, SDL_SCANCODE_UNKNOWN}, {{SDL_GAMEPAD_BUTTON_START, -1}, {SDL_GAMEPAD_BUTTON_START, -1}}},
};

} // namespace

const launcher::Description &launcher::describe() {
    static const Description about = [] {
        Description d;
        d.original = "on the console";
        d.file_heading = "GAME FILE";
        d.file_intro = "Choose your own copy of the game. It stays where it is: the launcher only reads it.";
        d.file_empty = "No game file chosen. Drop it on this window or choose it.";
        d.file_not_set_up = "The game isn't set up yet. Open the launcher (without --play) once to choose your game file.";
        d.file_filter = {"Game file", "*"};
        d.file_key = "file";
        d.check = check;
        d.arguments = arguments;
        d.display = {
            {"fullscreen", "Full screen", "F11 switches while playing.", false, nullptr},
            {"high_resolution", "High resolution", "Drawn at the window's resolution. Off: the original's.", true, nullptr},
            {"widescreen", "Widescreen", "16:9. Needs high resolution.", false, "high_resolution"},
        };
        d.bindings = {kActions, static_cast<int>(sizeof kActions / sizeof kActions[0]), 2};
        d.controls = {{"Keys", "Esc or F1: menu (save states, cheats, quit)   F11: full screen"}};
        return d;
    }();
    return about;
}
