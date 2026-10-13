// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// The launcher's pages: the player's game file, the optional features, controllers, Play. What is about one
// port comes from launcher::describe() (launcher.h) and the LAUNCHER_* definitions.
#pragma once

#include <SDL3/SDL.h>

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "game_process.h"
#include "launcher.h"
#include "settings.h"

enum class Page { Game, Features, Controllers, Count };

// --play: straight into the game set up earlier; the exit code
int play_directly();

class App {
public:
    App(SDL_Window* window);
    ~App();
    // Window events the launcher handles itself (dropped files, a key or button being chosen); false: pass to ImGui.
    bool consume(const SDL_Event& event);
    void frame();
    void set_page(Page page) { page_ = page; }
    bool busy() const;

private:
    // Paths.
    std::filesystem::path game_program() const;
    std::filesystem::path data_folder() const;  // per player: settings, saves

    // Game file.
    void choose_game_file();
    void use_game_file(const std::string& path);
    void play();

    // Display switches (launcher.ini, [video]).
    bool toggle_on(const launcher::Toggle& toggle) const;
    void set_toggle(const launcher::Toggle& toggle, bool on);

    // Features (settings.h: the rules the game follows too).
    void load_features();
    void set_preset(const std::string& preset);
    void set_feature(size_t index, int value);

    // Controls (bindings.h): what each player's keys and gamepad buttons do, kept in settings.ini.
    void bind(int player, bool gamepad, int action, SDL_Scancode key, recomp::PadInput input);
    void save_bindings();
    bool capture(const SDL_Event& event);

    void sidebar(float height);
    void page(float width, float height);
    void heading(const char* title, const char* intro, float width);
    void game_page(float width);
    void features_page(float width);
    void controllers_page(float width);

    const launcher::Description& about_;
    SDL_Window* window_;
    Page page_ = Page::Game;
    recomp::Ini prefs_;     // launcher.ini: where the game file is, the Display switches
    recomp::Ini settings_;  // the game's settings.ini: features, controls

    mutable std::mutex mutex_;
    std::string picked_;  // from the file dialog (another thread)
    std::string file_path_;
    launcher::FileCheck file_;         // (under mutex_ when checked on a thread)
    std::atomic<bool> checking_{false};
    std::thread checker_;

    std::vector<recomp::FeatureInfo> features_;
    std::string features_error_;

    GameProcess game_;
    std::string play_error_;
    std::string steam_message_;  // what Add to Steam did
    int selected_pad_ = 0;
    recomp::Bindings bindings_{};
    int bind_player_ = 0;          // whose buttons the Controllers page shows
    int capture_action_ = -1;      // the action waiting for a key or a button to be pressed for it, or -1
    bool capture_gamepad_ = false; //   (a gamepad's, or the keyboard's)
    std::vector<SDL_JoystickID> opened_pads_;  // gamepads opened so that their buttons are heard
};
