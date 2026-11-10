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
#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "game_process.h"
#include "launcher.h"
#include "online.h"
#include "settings.h"
#ifdef LAUNCHER_UPDATE_REPO
#include "update.h"
#endif

// The launcher's own pages. A port's: Game, Features, Controllers. A collection's: Games (Library), Features and
// Controllers (for all the games), Multi game (if it has modes that play several games: launcher.h, MultiMode), then
// one page for each game whose file is chosen (Title).
enum class Page { Game, Features, Controllers, Library, Title, Multi, Online, Series, Count };

// A run that stopped with an error: its log, kept (logs/stopped-<when>.log), and what the runtime said stopped it
struct StoppedLog {
    std::filesystem::path path;
    std::string reason;
};
StoppedLog keep_stopped_log(const std::filesystem::path& folder);

// --play: straight into the game set up earlier (a collection's: --play ID), with `extra` after the launcher's
// own arguments for it; the exit code
int play_directly(const std::string& id, const std::vector<std::string>& extra = {});

// Portable mode (folders.h), before anything is read: its folder made, or the player told that it cannot be, and
// false (the launcher then stops: their things never go elsewhere without their knowing). The first time, the
// player is asked (`ask`) whether what the system's folders have is copied there; nothing is moved or written over.
bool prepare_folders(bool ask);

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
    // A collection's: the page of the game with this id (--page ID), if it has one; "multi": the Multi game page
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
    // A finger dragged on a touch screen (main.cpp): over the sidebar (x, in the window's points, is in it) its list is
    // scrolled by `by` points, as the mouse wheel scrolls it; false where it is not the sidebar's
    bool scroll_sidebar(float x, float by);

private:
    // One game the launcher starts: a port's launcher has one, a collection's one for each of its games.
    struct Slot {
        const launcher::Description* about = nullptr;
        std::string id;                  // a collection's game's (a port's: "")
        std::string name;                // its name; its player folder is named by it
        std::string series;              // a collection's game's series (launcher::CollectionGame::series), or ""
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
        std::filesystem::path stopped_log;  // its last run stopped with an error: that run's log, kept, or ""
        std::string stopped_reason;      // and what the log says stopped it ("recomp stop: <this>"), or ""
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

    // A game's feature on the pages: the port's feature_shown, given the game's file if it was found good
    bool feature_shown(const Slot& one, size_t feature);
    Slot& slot() { return *slots_[slot_]; }
    const Slot& slot() const { return *slots_[slot_]; }
    // The collection's games that have a page of their own (their program here, their file chosen and usable), in
    // the player's order
    std::vector<size_t> titled() const;
    // With "Only the games you have" on (launcher.ini [games] installed_only) and at least one game set up, the
    // Games, Features and Controllers pages leave out the collection's games whose file has not been chosen
    bool installed_only() const;
    // A collection's game on those pages: in this build and, with installed_only(), its file chosen (being
    // checked, or usable)
    bool shown(size_t which) const;
    // The collection's pages before its games': Games, Features, Controllers and, with modes for it, Multi game
    int fixed_pages() const;
    // A collection's games in the player's order: launcher.ini, [games] order (their ids; a game not in it comes
    // after those that are, in the collection's order). The player changes it by dragging a game's row in the
    // sidebar, or with Move up and Move down on the game's page.
    void load_order();
    // order_ with each series' games together, where its first game with a page is (else its first game)
    void group_order();
    // The games with a page as the places of the sidebar's list, in the player's order: a game, or a series' games
    // (two or more of them with a page: launcher::CollectionGame::series), which move together
    std::vector<std::vector<size_t>> units() const;
    bool grouped(const std::vector<size_t>& unit) const;
    // A row of the sidebar after the collection's own pages: a game's, or a series' (its games', shown under it
    // while it is open). A series' row is its page: its games, each to play or go to.
    struct Row {
        size_t game = 0;       // the game's; a series': its first game's
        bool series = false;   // the series' row
        bool inside = false;   // a game's row under its series'
    };
    std::vector<Row> rows() const;
    // launcher.ini, [games] open: the series whose rows are open (their names)
    bool series_open(const std::string& series) const;
    void open_series(const std::string& series, bool open);
    // A change to the sidebar's rows (a series opened or closed, a game moved), the page shown staying the one it was
    // (a game's in a series that closes: the series')
    void keep_page(const std::function<void()>& change);
    void reorder(const std::vector<size_t>& list);  // titled() in a new order, kept in launcher.ini
    void move_game(size_t which, int place);  // to that place among its series' games (or as move_unit, one of none)
    void move_unit(size_t unit, int place);   // to that place among units()

    // Game file.
    void choose_game_file(size_t which);
    void use_game_file(size_t which, const std::string& path);
    void play(size_t which, const std::vector<std::string>& extra = {});
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
    void after_game(Slot& slot);
    void keep_stopped_log(Slot& slot);           // (it stopped with an error: its log kept before the next run)
    void stopped_note(Slot& slot, float width);  // (and what to send, and where)  // (a collection's game has ended: what its menu changed is the others' too)

    // Controls (bindings.h): what each player's keys and gamepad buttons do, kept in settings.ini. A
    // collection's are every game's, but for a game given a layout of its own (launcher.ini, [controls]: its id
    // = own). The Controllers page shows every game's layout or one game's (bind_slot_), and these act on it.
    const recomp::BindTable& shared_table() const;
    bool has_own_layout(size_t which) const;  // (its description has a table to lay out)
    bool own_layout(size_t which) const;
    void set_own_layout(size_t which, bool own);
    const recomp::BindTable& bind_table() const;
    recomp::Bindings& bind_layout();
    void bind(int player, bool gamepad, int action, SDL_Scancode key, recomp::PadInput input);
    void save_bindings();
    void write_bindings(Slot& slot, const recomp::BindTable& table, const recomp::Bindings& bindings);
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
    void notice_card(float width);
    void library_page(float width);
    void title_page(size_t which, float width);
    void series_page(size_t which, float width);  // a series' page (`which`: one of its games)
    void features_page(float width);
    void controllers_page(float width);
    // The rows of a page's features under their categories, with the search box over them; set(row, value)
    // sets one
    // (`offered` and `set_offered`: a kFeatureMenuChoices feature's choices in the game's menu, settings.h MenuChoices)
    template <typename SetOffered>
    void menu_choices(float width, const FeatureRow& feature, std::vector<bool> offered, SetOffered set_offered);
    void set_menu_choices(Slot& one, size_t index, const std::vector<bool>& offered);
    template <typename Set, typename Offered, typename SetOffered>
    void feature_list(float width, const std::vector<FeatureRow>& rows, const char* none, Set set, Offered offered, SetOffered set_offered,
                      bool whole = true);
    void file_card(size_t which, float width);
    void display_cards(size_t which, float width);
    // Play, under the page's heading, and why the game didn't start; whether it can be played now (online_card's `ready`)
    bool play_button(size_t which, float width);
    void online_card(size_t which, float width, bool ready);
    // Under the game's other cards: its Online card, and Steam's (Linux)
    void play_cards(size_t which, float width, bool ready);
    // Host's arguments for a game's Online: listed under the player's name if they have one, and arcade while waiting
    // if they switched it on; and the row with that switch
    std::vector<std::string> host_arguments(const launcher::Online& online) const;
    // Join's: the match (a code or an address), and the player's name, which the game shows the other player
    std::vector<std::string> join_arguments(const launcher::Online& online, const std::string& match) const;
    void arcade_row(float width, const launcher::Online& online);
    void lobby_card(size_t which, float width, bool ready);
    // The Online page (a collection with a lobby: Collection::lobby): host in the lobby in any game, and every game's
    // open matches
    void online_page(float width);
    void load_online();
    // How often the lobby is fetched and the waiting room checked in at (ms): every 10 seconds, or 20 while the launcher
    // isn't the window in front (a player in the room is gone 30 seconds after their last check-in: online/)
    Uint64 online_every() const;
    // The lobby's list from `url`, fetched again every online_every() while it is shown; the Online page's comes with the
    // waiting room's check-in instead (one request), from a service that gives it
    void follow_lobby(const std::string& url);
    // The waiting room (Collection::room): checked in at every online_every() while the Online page is open and no game
    // is running, and left when not; its card, and the invitations' card
    // The waiting room's URL and protocol: the collection's, or the one game's Online card's (nullptr: none)
    const char* room_url() const;
    int room_protocol() const;
    // A game's id in the waiting room: a collection's game's, or the one game's Online::game
    std::string room_game(size_t which) const;
    // A launcher of one game: its sidebar's pages in order, each a port's own page (its place in Description::pages) or
    // -1 - a Page; and the port's own page at a place (-1: none)
    std::vector<int> single_layout() const;
    int port_page(int index) const;
    int place_of(Page kind) const;
    void single_online_page(float width);
    void follow_room(bool here);
    // A card's small capitals, with "Updates every 10 seconds" at the right, and a spinner beside it while `fetching`
    void live_label(const char* text, float width, bool fetching);
    // A card's only row while there is nothing in it: `text`, with a spinner while `looking`
    void empty_row(float width, const char* text, bool looking);
    void room_card(float width, const std::vector<size_t>& playable);
    void invitations_card(float width);
    std::vector<std::string> invite_arguments(const launcher::Online& online, const launcher::RoomPlayer& player) const;
    void block(const std::string& id);
    // A game that can be started now (its file chosen and good, its program there, not running)
    bool can_play(size_t which) const;
    // This launcher's version (LAUNCHER_VERSION, or ""), and whether a match's is the same (online play's must be)
    static const char* own_version();
    static bool version_matches(const std::string& version);
    void files_card(float width);
    void language_card(float width);

    // Online play (launcher::Description::Online): the ONLINE card, and one room at a time, its tunnel (cps-tunel
    // beside the launcher) and its game started together
    void tunnel_card(size_t which, float width);
    // (this fork) ForkAde's in-game chat: the player's name, whether each match's chat is kept, and its folder; and
    // what a game played through the tunnel is given for it (--nick, --chat-log)
    void chat_card(float width);
    std::vector<std::string> chat_arguments() const;
    std::filesystem::path chat_folder() const;
    void open_room(size_t which, bool host);
    void close_room(const std::string& message);  // (the game and the tunnel stopped; `message` shown on the card)
    void watch_room();                             // every frame: the tunnel's new lines, the game or the tunnel ended
    void room_line(const std::string& line);
    struct TunnelRoom {
        size_t game = SIZE_MAX;    // the slot it is for (SIZE_MAX: no room)
        bool host = false;
        GameProcess tunnel;
        std::filesystem::path log; // the tunnel's lines (logs/tunel.log of the game's folder)
        uintmax_t read = 0;        // of it so far
        std::string partial;       // (a line not ended yet)
        std::string code;          // the host's room code
        bool started = false;      // the game started
        int rival = 0;             // the host's first guest (its number in the tunnel's lines), 0: none yet
        bool connected = false, relay = false, left = false, copied = false;
        int ping = -1;
        std::string error;
    };
    TunnelRoom tunnel_room_;
    char room_code_[512] = {};       // what is pasted or typed in the card
    std::string room_message_;       // why the last room ended, or that a code isn't complete
    size_t room_message_for_ = SIZE_MAX;

    // The Multi game page (a collection's MultiModes): a mode's choices and games, and the run of one
    void multi_page(float width);
    void multi_buttons(const launcher::MultiMode& m, float width, bool after);
    void multi_card(size_t mode, float width);
    int multi_value(size_t mode, size_t choice) const;  // (launcher.ini, [multi.<key>])
    // The mode's games the player can play (each with a page of its own), in the sidebar's order, and whether each is
    // chosen (launcher.ini, [multi.<key>] off: the ids of those that are not)
    std::vector<size_t> multi_games(size_t mode) const;
    bool multi_chosen(size_t mode, size_t which) const;
    void set_multi_chosen(size_t mode, size_t which, bool chosen);
    bool multi_shuffled(size_t mode) const;
    void start_multi(size_t mode);
    // The game whose turn it was has ended: the next one's turn (`done`: it ended with the mode's next_code), or the
    // run is over
    void multi_after(bool done);
    bool multi_play_next();  // (the next of the run's games that will start; false: none would)
    struct MultiRun {
        int mode = -1;               // the mode running (-1: none)
        std::vector<size_t> games;   // in the order they are played
        size_t at = 0;               // whose turn it is
        size_t played = 0;           // turns had
    };
    MultiRun multi_;
    std::string multi_message_;      // why the last run stopped
#ifdef LAUNCHER_UPDATE_REPO
    // Updates (update.h): the card on the first page, where it is drawn when there is a new version (`top`) or
    // at the page's end when there is not (`always`: at the top either way)
    void update_card(float width, bool top, bool always = false);
#endif
    // A newer version is out (or being installed): the Games row's dot, and the Online page's prompt
    bool update_news() const;

    const launcher::Collection* collection_ = nullptr;  // a collection's launcher's (a port's: none)
    SDL_Window* window_;
    int page_ = 0;
    std::filesystem::path folder_;          // the player's (a collection's: its own, with its launcher.ini)
    recomp::Ini prefs_;     // launcher.ini: where each game's file is, the Display switches
    std::filesystem::path program_folder_;  // the launcher's
    std::vector<std::unique_ptr<Slot>> slots_;
    std::vector<size_t> order_;  // slots_, in the player's order (load_order)
    int dragging_ = -1;          // the game whose sidebar row is being dragged, or -1
    bool dragging_series_ = false;  // (its series' row: the series is being dragged)
    std::string opened_;         // the series opened in the last frame (its games' rows come out from under its row)
    float drag_grab_ = 0.0f;     //   (where in its row it was taken hold of)
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
    std::vector<std::string> menu_choices_open_;  // the features whose choices for the game's menu are shown

    std::string steam_message_;  // what Add to Steam did
    std::string desktop_message_;     // what Add shortcut did,
    std::string desktop_message_id_;  // and for which game
    char online_address_[256] = {};  // the Online card's Join address (launcher.ini [online] address)
    bool online_loaded_ = false;
    std::vector<std::string> online_here_;  // this machine's addresses, as the Host row says them
    char online_name_[64] = {};
    // (the Online page's Join with a code: what is typed, the lookup of which game it is for, and what it said)
    char join_code_[32] = {};
    std::future<launcher::CodeLookup> join_lookup_;
    std::string join_said_;
    int join_needs_ = -1;  // (a game whose file must be chosen first: Add…)  // the player's name, as the lobby shows a match they host ([online] name)
    // (the lobby's list: fetched every few seconds while the card is shown, on another thread)
    std::future<std::pair<bool, std::vector<launcher::OpenMatch>>> lobby_fetch_;
    std::vector<launcher::OpenMatch> lobby_;
    std::string lobby_url_;
    bool lobby_failed_ = false;
    Uint64 lobby_at_ = 0;
    // (the waiting room's: the check-in on another thread, its last answer, and the invitations answered here, which
    // the service may still list until its next answer)
    std::future<launcher::Room> room_fetch_;
    launcher::Room room_;
    Uint64 room_at_ = 0;
    bool room_in_ = false;
    bool room_lists_ = true; // (the check-in brings the Online page's lobby list too: until a service answers without it)
    std::vector<std::string> room_answered_;
#ifdef LAUNCHER_UPDATE_REPO
    std::unique_ptr<launcher::Updater> updater_;
    bool relaunched_ = false;  // (the new version has been started: this one quits)
#endif
    int selected_pad_ = 0;
    recomp::Bindings bindings_{};  // (a collection's are every game's)
    std::vector<recomp::Bindings> own_bindings_;  // (a collection's: each game's own layout, for those that have one)
    int bind_slot_ = -1;          // whose layout the Controllers page shows: every game's (-1), or one game's
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
    int focus_row_ = -1;           // ... on this row of it
    float sidebar_scroll_ = 0.0f;  // how far its list is scrolled (a long list, or a small window)
    int scrolled_for_ = -1;        // the page shown when it was last scrolled to
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
