// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// The launcher's pages: the player's game file, the optional features, controllers, Play. What is about one
// port comes from launcher::describe() (launcher.h) and the LAUNCHER_* definitions.
//
// All of it works with the arrow keys and Space or Enter, and with a gamepad (the d-pad or the left stick,
// A to press, B to go back to the sidebar or close a list, the shoulder buttons for the page before and
// after, the right stick to scroll), as well as with the mouse. A ring is round what they are on.
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
    // The pages in the sidebar's order: the port's own, then the launcher's
    int page_count() const;
    void set_page(int page) { page_ = page; }
    bool busy() const;
    // A window the player sees (not pictures for a test): with a gamepad connected the ring shows from the start
    void set_interactive();
    // While a key or a button is being chosen for an action (and until that button is let go), and while
    // the game runs (its gamepad is the game's), the arrow keys and the gamepad do not move the ring:
    // main.cpp switches them off for those frames.
    bool navigation_paused() const { return capture_action_ >= 0 || nav_hold_ || game_.running(); }

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

    // The game file chosen with a gamepad: a list of folders and files in the launcher's own window.
    void open_browser();
    void list_folder(const std::filesystem::path& folder);
    void browser();

    void navigation();
    void sidebar(float height);
    void page(float width, float height);
    Page own_page(int index) const;
    launcher::PageContext context(float width);
    void game_page(float width);
    void features_page(float width);
    void controllers_page(float width);

    const launcher::Description& about_;
    SDL_Window* window_;
    int page_ = 0;
    recomp::Ini prefs_;     // launcher.ini: where the game file is, the Display switches
    recomp::Ini settings_;  // the game's settings.ini: features, controls
    std::filesystem::path folder_;          // the player's
    std::filesystem::path program_folder_;  // the launcher's

    mutable std::mutex mutex_;
    std::string picked_;  // from the file dialog (another thread)
    std::string file_path_;
    launcher::FileCheck file_;         // (under mutex_ when checked on a thread)
    std::atomic<bool> checking_{false};
    std::thread checker_;

    std::vector<recomp::FeatureInfo> features_;
    std::string features_error_;
    char feature_search_[128] = {};  // what the Features page is narrowed to ("": all of them)
    bool focus_search_ = false;      // Ctrl+F (or Cmd+F): into the search box

    GameProcess game_;
    bool was_running_ = false;
    std::string play_error_;
    std::string steam_message_;  // what Add to Steam did
    int selected_pad_ = 0;
    recomp::Bindings bindings_{};
    int bind_player_ = 0;          // whose buttons the Controllers page shows
    int capture_action_ = -1;      // the action waiting for a key or a button to be pressed for it, or -1
    bool capture_gamepad_ = false; //   (a gamepad's, or the keyboard's)
    std::vector<SDL_JoystickID> opened_pads_;  // gamepads opened so that their buttons are heard

    // The arrow keys and a gamepad.
    bool interactive_ = false;
    bool show_focus_ = false;      // show the ring as soon as there is something for it to be round
    bool starting_ = true;         // (the first time that is done)
    unsigned start_focus_ = 0;     // what the ring was first on, while it is still there: it goes to Play when Play can be pressed
    bool nav_hold_ = false;        // a button was just chosen for an action: nothing moves until it is let go
    bool focus_sidebar_ = false;   // put the ring on the sidebar's row for the page shown
    bool in_sidebar_ = false;      // the ring was in the sidebar in the last frame
    unsigned focus_seen_ = 0;      // what it was on in the last frame
    bool focus_page_ = false;      // put the ring on the first thing in the page
    bool right_from_sidebar_ = false;  // Right was pressed in the last frame with the ring in the sidebar
    unsigned left_from_ = 0;       // Left was pressed in the last frame with the ring on this (0: it was not)
    bool popup_was_open_ = false;  // a list was open in the last frame (Back closes that, and does no more)

    struct BrowseEntry {
        std::string name;
        bool folder;
    };
    bool browse_open_ = false;   // open the list in this frame
    bool browse_moved_ = false;  // another folder is listed: back to its top
    std::filesystem::path browse_folder_;
    std::vector<BrowseEntry> browse_entries_;
};
