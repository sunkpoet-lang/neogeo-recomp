// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// The launcher's pages: the player's game file, the optional features, controllers, Play. What is about one
// port comes from launcher::describe() (launcher.h) and the LAUNCHER_* definitions; a collection's launcher
// (LAUNCHER_COLLECTION) has several games, from launcher::describe_collection().
//
// All of it works with the arrow keys and Space or Enter, and with a gamepad (the d-pad or the left stick,
// A to press, B to go back to the sidebar or close a list, the shoulder buttons for the page before and
// after, the right stick to scroll), as well as with the mouse. A ring is round what they are on.
#pragma once

#include <SDL3/SDL.h>

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "game_process.h"
#include "launcher.h"
#include "settings.h"

// The launcher's own pages. A port's: Game, Features, Controllers. A collection's: Games (Library), Features and
// Controllers (for all the games), then one page for each game whose file is chosen (Title).
enum class Page { Game, Features, Controllers, Library, Title, Count };

// --play: straight into the game set up earlier (a collection's: --play ID), with `extra` after the launcher's
// own arguments for it; the exit code
int play_directly(const std::string& id, const std::vector<std::string>& extra = {});

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
    // A collection's: the page of the game with this id (--page ID), if it has one
    void set_page(const std::string& id);
    bool busy() const;
    bool any_running() const;
    // --press: the next key may be pressed (a port's launcher: nothing is busy; a collection's: nothing but its
    // games, so that a second can be started while one runs)
    bool ready_for_keys() const;
    // A window the player sees (not pictures for a test): with a gamepad connected the ring shows from the start
    void set_interactive();
    // While a key or a button is being chosen for an action (and until that button is let go), and while
    // the game runs (its gamepad is the game's), the arrow keys and the gamepad do not move the ring:
    // main.cpp switches them off for those frames. (A collection's launcher, whose games can run several at
    // once, is paused only while one runs and the launcher's window is not the one in front.)
    bool navigation_paused() const;

private:
    // One game the launcher starts: a port's launcher has one, a collection's one for each of its games.
    struct Slot {
        const launcher::Description* about = nullptr;
        std::string id;                  // a collection's game's (a port's: "")
        std::string name;                // its name; its player folder is named by it
        std::string exe;                 // its program, beside the launcher
        std::string file_section;        // launcher.ini: where its file is ("game", or "game.<id>")
        std::string video_section;       // its Display switches ("video", or "video.<id>")
        std::filesystem::path folder;    // the player's folder for it: settings.ini, saves
        recomp::Ini settings;            // its settings.ini: features, controls
        std::vector<recomp::FeatureInfo> features;
        std::string features_error;
        std::string file_path;
        launcher::FileCheck file;        // (under mutex_ when checked on a thread)
        std::atomic<bool> checking{false};
        std::thread checker;
        std::vector<int> values_before;  // its features' values when it was started (a collection's: what its menu changed)
        GameProcess game;                // its program, while it runs (a collection's games can run at once)
        bool was_running = false;
        std::string play_error;          // why it did not start, or ""
        // What another game's menu changed of a feature they share while this one was running: set when it
        // ends, unless its own menu changed the same feature
        std::vector<std::pair<std::string, int>> pending;

        explicit Slot(const std::filesystem::path& settings_path) : settings(settings_path) {}
        std::filesystem::path program() const;
        bool has_program() const;
    };
    // A feature more than one of a collection's games has, set for all of them at once
    struct Shared {
        recomp::FeatureInfo info;                      // as the first game that has it tells it
        std::vector<std::pair<size_t, size_t>> where;  // (game, feature) of each game that has it
    };
    // A row of a page's features: the feature, its value, a note under it
    struct FeatureRow {
        const recomp::FeatureInfo* info;
        int value;
        std::string note;  // ("": none)
        size_t index;      // in the list it came from
    };

    Slot& slot() { return *slots_[slot_]; }
    const Slot& slot() const { return *slots_[slot_]; }
    // The collection's games that have a page of their own (their program here, their file chosen and usable)
    std::vector<size_t> titled() const;

    // Game file.
    void choose_game_file(size_t which);
    void use_game_file(size_t which, const std::string& path);
    void play(size_t which);
    // A collection's: every game's file looked for in a folder (on a thread), or a file or folder given to
    // whichever game takes it
    void find_in(const std::string& path);
    void choose_folder();
    // A folder of the Sound card's (Description::folders): chosen with the system's dialog or the launcher's browser
    void choose_folder_for(size_t which, int choice);
    void set_folder_choice(size_t which, int choice, const std::string& path);

    // Display switches (launcher.ini, [video]).
    bool toggle_on(const Slot& slot, const launcher::Toggle& toggle) const;
    void set_toggle(Slot& slot, const launcher::Toggle& toggle, bool on);

    // Features (settings.h: the rules the game follows too).
    void load_features(Slot& slot);
    void find_shared();
    void set_preset(const std::string& preset);
    void set_feature(Slot& slot, size_t index, int value);
    void set_shared(size_t index, int value);
    void after_game(Slot& slot);  // (a collection's game has ended: what its menu changed is the others' too)

    // Controls (bindings.h): what each player's keys and gamepad buttons do, kept in settings.ini.
    void bind(int player, bool gamepad, int action, SDL_Scancode key, recomp::PadInput input);
    void save_bindings();
    bool capture(const SDL_Event& event);

    // The game file chosen with a gamepad: a list of folders and files in the launcher's own window.
    void open_browser(size_t which, bool folders);
    void list_folder(const std::filesystem::path& folder);
    void browser();

    void navigation();
    void sidebar(float height);
    void page(float width, float height);
    Page own_page(int index, size_t* which = nullptr) const;
    launcher::PageContext context(float width);
    void game_page(float width);
    void library_page(float width);
    void title_page(size_t which, float width);
    void features_page(float width);
    void controllers_page(float width);
    // The rows of a page's features under their categories, with the search box over them; set(row, value)
    // sets one
    template <typename Set> void feature_list(float width, const std::vector<FeatureRow>& rows, const char* none, Set set);
    void file_card(size_t which, float width);
    void display_cards(size_t which, float width);
    void play_button(size_t which, float width);

    const launcher::Collection* collection_ = nullptr;  // a collection's launcher's (a port's: none)
    SDL_Window* window_;
    int page_ = 0;
    std::filesystem::path folder_;          // the player's (a collection's: its own, with its launcher.ini)
    recomp::Ini prefs_;     // launcher.ini: where each game's file is, the Display switches
    std::filesystem::path program_folder_;  // the launcher's
    std::vector<std::unique_ptr<Slot>> slots_;
    size_t slot_ = 0;       // the game the page shown is about
    const launcher::Description& about_;  // a port's (a collection's: its first game's, for what they all share)
    std::vector<Shared> shared_;

    mutable std::mutex mutex_;
    std::string picked_;  // from the file dialog (another thread)
    size_t picked_for_ = 0;
    bool picked_folder_ = false;
    int picked_choice_ = -1;  // (under mutex_) the Sound card's folder the dialog was for (-1: none)
    std::atomic<bool> finding_{false};  // looking in a folder for the games' files
    std::thread finder_;
    std::vector<std::pair<size_t, std::string>> found_;  // (under mutex_) what it found: (game, file)
    bool found_ready_ = false;                          // (under mutex_)
    std::string find_message_;

    char feature_search_[128] = {};  // what the Features page is narrowed to ("": all of them)
    bool focus_search_ = false;      // Ctrl+F (or Cmd+F): into the search box

    std::string steam_message_;  // what Add to Steam did
    int selected_pad_ = 0;
    recomp::Bindings bindings_{};  // (a collection's are every game's)
    int bind_player_ = 0;         // whose buttons the Controllers page shows
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
    bool browse_open_ = false;     // open the list in this frame
    bool browse_moved_ = false;    // another folder is listed: back to its top
    bool browse_folders_ = false;  // a folder is being chosen (a collection's, to look in), not a file
    size_t browse_for_ = 0;        // the game a file is being chosen for
    int browse_choice_ = -1;       // ... or the Sound card's folder (Description::folders) being chosen
    std::filesystem::path browse_folder_;
    std::vector<BrowseEntry> browse_entries_;
};
