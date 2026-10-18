// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#include "app.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include "layout.h"
#include "steam.h"
#include "ui.h"

using launcher::FileCheck;
using layout::Card;
using layout::inset_separator;
using layout::row;
using ui::Font;
using ui::Kind;
using ui::px;

namespace {

constexpr float kSidebarWidth = 220.0f;
constexpr float kPagePaddingX = 44.0f;
constexpr float kPagePaddingTop = 40.0f;
constexpr float kContentMaxWidth = 720.0f;
constexpr size_t kAnyGame = static_cast<size_t>(-1);  // (a file or folder for whichever game takes it)

// The folder the launcher program is in (the game program is beside it).
std::filesystem::path base_path() {
#ifdef __APPLE__
    // Not SDL_GetBasePath: inside an app bundle that is Contents/Resources, not Contents/MacOS.
    char buffer[4096];
    uint32_t size = sizeof buffer;
    std::error_code error;
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        std::filesystem::path program = std::filesystem::canonical(buffer, error);
        if (!error) {
            return program.parent_path();
        }
    }
#endif
    const char* base = SDL_GetBasePath();
    return base ? std::filesystem::path(base) : std::filesystem::current_path();
}

void wake() {
    SDL_Event event{};
    event.type = SDL_EVENT_USER;
    SDL_PushEvent(&event);
}

const char* kPresetNames[] = {"pure", "enhanced", "custom"};

// A game's program beside the launcher. In a build folder the player build is <exe>_app (<exe> there is the
// strict test build); in an app bundle or AppImage it's <exe>.
std::filesystem::path program_path(const std::string& exe) {
    std::filesystem::path base = base_path();
    std::error_code error;
    if (std::filesystem::exists(base / (exe + "_app"), error)) {
        return base / (exe + "_app");
    }
#ifdef _WIN32
    return base / (exe + ".exe");
#else
    return base / exe;
#endif
}

// The player's folder for a game (or a collection): named by it, or the folder a variable names (tests)
std::filesystem::path player_folder(const char* name, const char* home_env) {
    if (const char* home = home_env && *home_env ? std::getenv(home_env) : nullptr; home && *home) {
        return home;
    }
    char* path = SDL_GetPrefPath("", name);  // where the game keeps settings.ini too
    std::filesystem::path folder = path ? std::filesystem::path(path) : base_path() / "user";
    SDL_free(path);
    return folder;
}

// A port's launcher's description, or a collection's (its first game's: what the games share, the controls)
const launcher::Collection* the_collection() {
#ifdef LAUNCHER_COLLECTION
    return &launcher::describe_collection();
#else
    return nullptr;
#endif
}

const launcher::Description& the_description() {
#ifdef LAUNCHER_COLLECTION
    return launcher::describe_collection().games.front().description;
#else
    return launcher::describe();
#endif
}

// Who a game is, as the launcher keeps it: a port's one game, or one of a collection's.
struct Target {
    const launcher::Description* about = nullptr;
    std::string id, name, exe, file_section = "game", video_section = "video";
    std::filesystem::path folder;
};

std::vector<Target> targets() {
    std::vector<Target> list;
    if (const launcher::Collection* collection = the_collection()) {
        // (a test's folder for the collection holds one for each game, unless the game's own variable says)
        const char* home = std::getenv(LAUNCHER_HOME_ENV);
        for (const launcher::CollectionGame& game : collection->games) {
            Target target;
            target.about = &game.description;
            target.id = game.id;
            target.name = game.name;
            target.exe = game.exe;
            target.file_section = std::string("game.") + game.id;
            target.video_section = std::string("video.") + game.id;
            const char* own = game.home_env && *game.home_env ? std::getenv(game.home_env) : nullptr;
            target.folder = own && *own ? std::filesystem::path(own)
                            : home && *home ? std::filesystem::path(home) / game.id
                                            : player_folder(game.name, nullptr);
            list.push_back(target);
        }
        return list;
    }
    Target target;
    target.about = &the_description();
    target.name = LAUNCHER_GAME_NAME;
    target.exe = LAUNCHER_GAME_EXE;
    target.folder = player_folder(LAUNCHER_GAME_NAME, LAUNCHER_HOME_ENV);
    list.push_back(target);
    return list;
}

bool toggle_state(const recomp::Ini& prefs, const std::string& section, const launcher::Toggle& toggle) {
    std::string value = prefs.get(section, toggle.key);
    return toggle.on_by_default ? value != "0" : value == "1";
}

// What the game is started with: the port says (the player's file, read only; their folder; the switches).
std::vector<std::string> arguments_for(const recomp::Ini& prefs, const Target& target) {
    const launcher::Description& about = *target.about;
    launcher::Launch launch;
    launch.file = prefs.get(target.file_section, about.file_key);
    launch.folder = target.folder;
    launch.on = [&](const char* key) {
        for (const launcher::Toggle& toggle : about.display) {
            if (std::strcmp(toggle.key, key) == 0) {
                // (a switch that needs another is only on with it)
                return toggle_state(prefs, target.video_section, toggle) && (!toggle.needs || launch.on(toggle.needs));
            }
        }
        return false;
    };
    launch.file_of = [&](const char* id) {
        for (const Target& other : targets()) {
            if (other.id == id) {
                return prefs.get(other.file_section, other.about->file_key);
            }
        }
        return std::string();
    };
    std::vector<std::string> arguments = about.arguments(launch);
    if (about.mute_argument && prefs.get("audio", "muted") == "1") {
        arguments.push_back(about.mute_argument);
    }
    for (const launcher::FolderChoice& folder : about.folders) {
        std::string chosen = prefs.get(target.file_section, folder.key);
        if (!chosen.empty()) {
            arguments.push_back(folder.argument);
            arguments.push_back(chosen);
        }
    }
    return arguments;
}

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

}  // namespace

// (--play is often typed: what goes wrong is said on the terminal too)
static void say(SDL_MessageBoxFlags kind, const std::string& title, const std::string& message) {
    std::fprintf(stderr, "%s: %s\n", title.c_str(), message.c_str());
    SDL_ShowSimpleMessageBox(kind, title.c_str(), message.c_str(), nullptr);
}

int play_directly(const std::string& id, const std::vector<std::string>& extra) {
    // Straight into the game with the file chosen earlier (e.g. a Steam shortcut in Game Mode).
    std::vector<Target> all = targets();
    const Target* target = &all.front();
    for (const Target& one : all) {
        if (one.id == id) {
            target = &one;
        }
    }
    if (the_collection() && target->id != id) {
        std::string message = "There is no game \"" + id + "\" here. Start it with --play and one of:";
        for (const Target& one : all) {
            message += " " + one.id;
        }
        say(SDL_MESSAGEBOX_INFORMATION, LAUNCHER_GAME_NAME, message);
        return 1;
    }
    const launcher::Description& about = *target->about;
    recomp::Ini prefs(player_folder(LAUNCHER_GAME_NAME, LAUNCHER_HOME_ENV) / "launcher.ini");
    prefs.load();
    if (about.game_page && !about.check(prefs.get(target->file_section, about.file_key)).usable()) {
        say(SDL_MESSAGEBOX_INFORMATION, target->name, about.file_not_set_up);
        return 1;
    }
    GameProcess game;
    std::string message;
    std::error_code error;
    if (about.saves_folder) {
        std::filesystem::create_directories(target->folder / "saves", error);
    }
    std::filesystem::create_directories(target->folder / "logs", error);
    std::vector<std::string> arguments = arguments_for(prefs, *target);
    arguments.insert(arguments.end(), extra.begin(), extra.end());
    if (!game.start(program_path(target->exe), arguments, target->folder, message, target->folder / "logs" / "game.log")) {
        say(SDL_MESSAGEBOX_ERROR, target->name, message);
        return 1;
    }
    while (game.running()) {
        SDL_Delay(100);
    }
    return game.exit_code();
}

std::filesystem::path App::Slot::program() const {
    return program_path(exe);
}

bool App::Slot::has_program() const {
    std::error_code error;
    return std::filesystem::exists(program(), error);
}

App::App(SDL_Window* window)
    : collection_(the_collection()),
      window_(window),
      folder_(player_folder(LAUNCHER_GAME_NAME, LAUNCHER_HOME_ENV)),
      prefs_(folder_ / "launcher.ini"),
      program_folder_(base_path()),
      about_(the_description()) {
    prefs_.load();
    for (const Target& target : targets()) {
        auto slot = std::make_unique<Slot>(target.folder / "settings.ini");
        slot->about = target.about;
        slot->id = target.id;
        slot->name = target.name;
        slot->exe = target.exe;
        slot->file_section = target.file_section;
        slot->video_section = target.video_section;
        slot->folder = target.folder;
        slots_.push_back(std::move(slot));
    }
    for (size_t which = 0; which < slots_.size(); which++) {
        Slot& one = *slots_[which];
        one.settings.load();
        // A collection's game that had a launcher of its own: where its file was and its Display switches, from
        // that launcher's launcher.ini (in the game's folder, which the collection keeps using)
        if (collection_ && prefs_.get(one.file_section, one.about->file_key).empty()) {
            recomp::Ini own(one.folder / "launcher.ini");
            own.load();
            if (!own.get("game", one.about->file_key).empty()) {
                prefs_.set(one.file_section, one.about->file_key, own.get("game", one.about->file_key));
                for (const launcher::Toggle& toggle : one.about->display) {
                    if (!own.get("video", toggle.key).empty()) {
                        prefs_.set(one.video_section, toggle.key, own.get("video", toggle.key));
                    }
                }
                prefs_.save();
            }
        }
        // The launcher's default is the Enhanced preset (the game alone, with no settings, runs Pure).
        std::string chosen = one.id.empty() ? std::string("preset_chosen") : "preset_chosen." + one.id;
        if (prefs_.get("launcher", chosen).empty()) {
            if (one.settings.get("features", "preset").empty()) {
                one.settings.set("features", "preset", "enhanced");
                one.settings.save();
            }
            prefs_.set("launcher", chosen, "1");
            prefs_.save();
        }
        load_features(one);
        std::string remembered = one.about->game_page ? prefs_.get(one.file_section, one.about->file_key) : std::string();
        if (!remembered.empty()) {
            use_game_file(which, remembered);
        }
        if (one.about->game_page && one.about->find_nearby && !one.checking && !one.file.usable()) {
            // Nothing usable remembered: the port may know of one near the program
            std::string nearby = one.about->find_nearby(base_path());
            if (!nearby.empty()) {
                use_game_file(which, nearby);
            }
        }
    }
    find_shared();
    // The keys and buttons: a port's game's, or a collection's first game's that is here (they are set for all)
    size_t first = 0;
    for (size_t which = 0; which < slots_.size(); which++) {
        if (slots_[which]->has_program()) {
            first = which;
            break;
        }
    }
    bindings_ = recomp::LoadBindings(slots_[first]->settings.path().string(), about_.bindings);
}

App::~App() {
    for (auto& one : slots_) {
        if (one->checker.joinable()) {
            one->checker.join();
        }
    }
    if (finder_.joinable()) {
        finder_.join();
    }
}

bool App::busy() const {
    for (const launcher::CustomPage& page : about_.pages) {
        if (page.busy && page.busy()) {
            return true;
        }
    }
    for (const auto& one : slots_) {
        if (one->checking) {
            return true;
        }
    }
    return finding_ || any_running();
}

bool App::ready_for_keys() const {
    if (!collection_) {
        return !busy();
    }
    return !finding_ && std::none_of(slots_.begin(), slots_.end(), [](const auto& one) { return one->checking.load(); });
}

bool App::any_running() const {
    return std::any_of(slots_.begin(), slots_.end(), [](const auto& one) { return one->game.running(); });
}

bool App::navigation_paused() const {
    if (capture_action_ >= 0 || nav_hold_) {
        return true;
    }
    if (collection_) {
        // (a window that is not shown, for pictures, is never in front)
        return any_running() && interactive_ && !(SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS);
    }
    return any_running();
}

std::vector<size_t> App::titled() const {
    std::vector<size_t> list;
    for (size_t which = 0; which < slots_.size(); which++) {
        const Slot& one = *slots_[which];
        std::lock_guard<std::mutex> lock(mutex_);
        if (one.has_program() && !one.file_path.empty() && !one.checking && one.file.usable()) {
            list.push_back(which);
        }
    }
    return list;
}

int App::page_count() const {
    if (collection_) {
        return 3 + static_cast<int>(titled().size());
    }
    return static_cast<int>(about_.pages.size()) + (about_.game_page ? 3 : 2);
}

void App::set_page(const std::string& id) {
    std::vector<size_t> list = titled();
    for (size_t i = 0; i < list.size(); i++) {
        if (slots_[list[i]]->id == id) {
            page_ = 3 + static_cast<int>(i);
        }
    }
}

// The launcher's own page at a place in the sidebar (a port's pages come first), or Page::Count; for a
// collection's game's page, which game
Page App::own_page(int index, size_t* which) const {
    if (collection_) {
        if (index < 3) {
            return index == 0 ? Page::Library : index == 1 ? Page::Features : Page::Controllers;
        }
        std::vector<size_t> list = titled();
        if (static_cast<size_t>(index - 3) >= list.size()) {
            return Page::Count;
        }
        if (which) {
            *which = list[static_cast<size_t>(index - 3)];
        }
        return Page::Title;
    }
    int own = index - static_cast<int>(about_.pages.size());
    if (own < 0) {
        return Page::Count;
    }
    return static_cast<Page>(about_.game_page ? own : own + 1);
}

launcher::PageContext App::context(float width) {
    Slot& one = slot();
    launcher::PageContext context{width, window_, prefs_, one.settings, one.folder, program_folder_, one.game.running(), one.game.exit_code(), one.play_error, {}};
    context.play = [this] { play(slot_); };
    return context;
}

// ---- game file ---------------------------------------------------------------------------

void App::choose_game_file(size_t which) {
    const Slot& one = *slots_[which];
    std::string start = one.file_path.empty() ? std::string() : std::filesystem::path(one.file_path).parent_path().string();
    picked_for_ = which;
    SDL_ShowOpenFileDialog(
        [](void* self, const char* const* files, int) {
            if (files && files[0]) {
                App* app = static_cast<App*>(self);
                std::lock_guard<std::mutex> lock(app->mutex_);
                app->picked_ = files[0];
                app->picked_folder_ = false;
            }
            wake();
        },
        this, window_, &one.about->file_filter, 1, start.empty() ? nullptr : start.c_str(), false);
}

void App::choose_folder() {
    picked_for_ = kAnyGame;
    SDL_ShowOpenFolderDialog(
        [](void* self, const char* const* files, int) {
            if (files && files[0]) {
                App* app = static_cast<App*>(self);
                std::lock_guard<std::mutex> lock(app->mutex_);
                app->picked_ = files[0];
                app->picked_folder_ = true;
            }
            wake();
        },
        this, window_, nullptr, false);
}

void App::choose_folder_for(size_t which, int choice) {
    const Slot& one = *slots_[which];
    std::string start = prefs_.get(one.file_section, one.about->folders[static_cast<size_t>(choice)].key);
    picked_for_ = which;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        picked_choice_ = choice;
    }
    SDL_ShowOpenFolderDialog(
        [](void* self, const char* const* files, int) {
            App* app = static_cast<App*>(self);
            std::lock_guard<std::mutex> lock(app->mutex_);
            if (files && files[0]) {
                app->picked_ = files[0];
                app->picked_folder_ = true;
            } else {
                app->picked_choice_ = -1;
            }
            wake();
        },
        this, window_, start.empty() ? nullptr : start.c_str(), false);
}

void App::set_folder_choice(size_t which, int choice, const std::string& path) {
    Slot& one = *slots_[which];
    prefs_.set(one.file_section, one.about->folders[static_cast<size_t>(choice)].key, path);
    prefs_.save();
}

void App::use_game_file(size_t which, const std::string& path) {
    Slot& one = *slots_[which];
    if (one.checking) {
        return;
    }
    if (one.checker.joinable()) {
        one.checker.join();
    }
    one.file_path = path;
    // Remember where it is; the file itself is only ever read.
    prefs_.set(one.file_section, one.about->file_key, path);
    prefs_.save();
    if (!one.about->check_on_thread) {
        one.file = one.about->check(path);
        return;
    }
    one.checking = true;
    one.checker = std::thread([this, &one, path] {
        FileCheck result = one.about->check(path);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            one.file = result;
        }
        one.checking = false;
        wake();
    });
}

// A collection's: the file or folder dropped or chosen, for whichever of the games it is. A folder is looked in
// for each game's file (its description's candidates); the folder itself may be one game's. On a thread (a
// check may read a large file); frame() takes what was found.
void App::find_in(const std::string& path) {
    if (finding_) {
        return;
    }
    if (finder_.joinable()) {
        finder_.join();
    }
    finding_ = true;
    find_message_.clear();
    std::vector<std::pair<size_t, const launcher::Description*>> games;
    for (size_t which = 0; which < slots_.size(); which++) {
        if (slots_[which]->has_program()) {
            games.push_back({which, slots_[which]->about});
        }
    }
    finder_ = std::thread([this, path, games] {
        std::vector<std::pair<size_t, std::string>> found;
        std::error_code error;
        std::filesystem::path given = std::filesystem::u8path(path);
        for (const auto& [which, about] : games) {
            std::vector<std::filesystem::path> tries = {given};
            if (about->candidates && std::filesystem::is_directory(given, error)) {
                std::vector<std::filesystem::path> more = about->candidates(given);
                tries.insert(tries.end(), more.begin(), more.end());
            }
            for (const std::filesystem::path& one : tries) {
                if (std::filesystem::exists(one, error) && about->check(one.u8string()).usable()) {
                    found.push_back({which, one.u8string()});
                    break;
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            found_ = found;
            found_ready_ = true;
        }
        finding_ = false;
        wake();
    });
}

void App::play(size_t which) {
    Slot& one = *slots_[which];
    if (one.game.running()) {
        return;  // (one of each game at a time: a second would share its settings and saves)
    }
    one.play_error.clear();
    std::error_code error;
    if (one.about->saves_folder) {
        std::filesystem::create_directories(one.folder / "saves", error);
    }
    std::filesystem::create_directories(one.folder / "logs", error);
    one.settings.save();
    one.values_before = recomp::FeatureValues(one.settings, one.features);
    Target target;
    target.about = one.about;
    target.file_section = one.file_section;
    target.video_section = one.video_section;
    target.folder = one.folder;
    one.game.start(one.program(), arguments_for(prefs_, target), one.folder, one.play_error, one.folder / "logs" / "game.log");
}

bool App::toggle_on(const Slot& one, const launcher::Toggle& toggle) const {
    if (toggle.needs) {
        for (const launcher::Toggle& other : one.about->display) {
            if (std::strcmp(other.key, toggle.needs) == 0 && !toggle_on(one, other)) {
                return false;
            }
        }
    }
    return toggle_state(prefs_, one.video_section, toggle);
}

void App::set_toggle(Slot& one, const launcher::Toggle& toggle, bool on) {
    prefs_.set(one.video_section, toggle.key, on ? "1" : "0");
    for (const launcher::Toggle& other : one.about->display) {
        if (on && toggle.needs && std::strcmp(other.key, toggle.needs) == 0) {
            prefs_.set(one.video_section, other.key, "1");  // (what it needs comes on with it)
        }
        if (!on && other.needs && std::strcmp(other.needs, toggle.key) == 0) {
            prefs_.set(one.video_section, other.key, "0");  // (what needs it goes off with it)
        }
    }
    prefs_.save();
}

// ---- features ----------------------------------------------------------------------------

void App::load_features(Slot& one) {
    one.features.clear();
    one.features_error.clear();
    if (!one.has_program()) {
        one.features_error = "The game program wasn't found next to the launcher.";
        return;
    }
    one.features = recomp::ParseFeatureList(GameProcess::output_of(one.program(), {"--list-features"}));
}

// A collection's features that more than one of its games (those here) has, by the same name and with the same
// choices: one row for all of them. (A choice whose choices are not the same in each, a stage of each game's say,
// is each game's own.)
void App::find_shared() {
    shared_.clear();
    if (!collection_) {
        return;
    }
    for (size_t which = 0; which < slots_.size(); which++) {
        const Slot& one = *slots_[which];
        for (size_t i = 0; i < one.features.size(); i++) {
            if (one.features[i].flags & recomp::kFeatureOwn) {
                continue;
            }
            auto known = std::find_if(shared_.begin(), shared_.end(), [&](const Shared& s) {
                return s.info.name == one.features[i].name && s.info.choices == one.features[i].choices;
            });
            if (known == shared_.end()) {
                shared_.push_back({one.features[i], {}});
                known = shared_.end() - 1;
            }
            known->where.push_back({which, i});
        }
    }
    shared_.erase(std::remove_if(shared_.begin(), shared_.end(), [](const Shared& s) { return s.where.size() < 2; }), shared_.end());
}

void App::set_preset(const std::string& name) {
    for (auto& one : slots_) {
        if (!collection_ && one.get() != slots_.front().get()) {
            continue;
        }
        recomp::SetPreset(one->settings, one->features, name);
        one->settings.save();
    }
}

void App::set_feature(Slot& one, size_t index, int value) {
    recomp::SetFeature(one.settings, one.features, index, value);
    one.settings.save();
}

void App::set_shared(size_t index, int value) {
    for (const auto& [which, feature] : shared_[index].where) {
        set_feature(*slots_[which], feature, value);
    }
}

// A collection's game has ended: what its own menu changed of a feature the others have is theirs too
void App::after_game(Slot& one) {
    if (!collection_ || one.values_before.size() != one.features.size()) {
        return;
    }
    std::vector<int> values = recomp::FeatureValues(one.settings, one.features);
    for (size_t i = 0; i < one.features.size(); i++) {
        if (values[i] == one.values_before[i]) {
            continue;
        }
        for (const Shared& shared : shared_) {
            if (std::none_of(shared.where.begin(), shared.where.end(), [&](const auto& at) { return slots_[at.first].get() == &one && at.second == i; })) {
                continue;
            }
            for (const auto& [which, feature] : shared.where) {
                Slot& other = *slots_[which];
                if (&other == &one) {
                    continue;
                }
                if (other.game.running()) {
                    // (it would write its settings.ini over this: set when it ends)
                    const std::string name = other.features[feature].name;
                    other.pending.erase(std::remove_if(other.pending.begin(), other.pending.end(), [&](const auto& p) { return p.first == name; }),
                                        other.pending.end());
                    other.pending.push_back({name, values[i]});
                } else if (recomp::FeatureValues(other.settings, other.features)[feature] != values[i]) {
                    set_feature(other, feature, values[i]);
                }
            }
        }
    }
    // What the others' menus changed while it ran, of what its own did not
    std::vector<int> now = recomp::FeatureValues(one.settings, one.features);
    for (const auto& [name, value] : one.pending) {
        for (size_t i = 0; i < one.features.size(); i++) {
            if (one.features[i].name == name && now[i] == one.values_before[i] && now[i] != value) {
                set_feature(one, i, value);
            }
        }
    }
    one.pending.clear();
    one.values_before.clear();
}

// ---- frame -------------------------------------------------------------------------------

bool App::consume(const SDL_Event& event) {
    if (capture_action_ >= 0 && capture(event)) {
        return true;
    }
    if (event.type == SDL_EVENT_GAMEPAD_ADDED && interactive_ && SDL_GetTicks() < 2000) {
        show_focus_ = true;  // (a gamepad that was there when the launcher started)
    }
    if (event.type == SDL_EVENT_DROP_FILE && event.drop.data) {
        if (collection_) {
            // (whichever game it is, or a folder with several)
            find_in(event.drop.data);
            page_ = 0;
            return true;
        }
        // To the port's page that is shown if it takes files, else to the first of its pages that does,
        // else it is the game file
        int count = static_cast<int>(about_.pages.size());
        for (int i = -1; i < count; i++) {
            int index = i < 0 ? page_ : i;
            if (index < count && about_.pages[index].drop) {
                launcher::PageContext dropped = context(0.0f);
                about_.pages[index].drop(dropped, event.drop.data);
                page_ = index;
                return true;
            }
        }
        if (about_.game_page) {
            use_game_file(0, event.drop.data);
            page_ = count;
        }
        return true;
    }
    return false;
}

void App::frame() {
    std::string picked;
    bool picked_folder = false;
    int picked_choice = -1;
    std::vector<std::pair<size_t, std::string>> found;
    bool found_ready = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        picked.swap(picked_);
        picked_folder = picked_folder_;
        if (!picked.empty()) {
            picked_choice = picked_choice_;
            picked_choice_ = -1;
        }
        found.swap(found_);
        found_ready = found_ready_;
        found_ready_ = false;
    }
    if (!picked.empty() && picked_choice >= 0) {
        set_folder_choice(picked_for_, picked_choice, picked);
    } else if (!picked.empty()) {
        if (picked_folder || picked_for_ == kAnyGame) {
            find_in(picked);
        } else {
            use_game_file(picked_for_, picked);
        }
    }
    if (found_ready) {
        for (const auto& [which, path] : found) {
            use_game_file(which, path);
        }
        int here = 0;
        for (const auto& one : slots_) {
            here += one->has_program();
        }
        if (found.empty()) {
            find_message_ = "None of the games was found there.";
        } else {
            std::string names;
            for (size_t i = 0; i < found.size(); i++) {
                names += (i == 0 ? "" : i + 1 == found.size() ? " and " : ", ") + slots_[found[i].first]->name;
            }
            find_message_ = "Found " + names + " (" + std::to_string(found.size()) + " of " + std::to_string(here) + ").";
        }
    }
    // A game that ended may have changed its settings.ini (its menu's features): what it left is what is shown
    // and kept
    for (auto& one : slots_) {
        if (one->was_running && !one->game.running()) {
            one->settings.load();
            after_game(*one);
            show_focus_ = interactive_ && SDL_HasGamepad();  // (back from the game with a gamepad in hand)
        }
        one->was_running = one->game.running();
    }
    navigation();
    const ui::Palette& p = ui::palette();
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::Begin("##root", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    float height = viewport->Size.y;
    ImGui::GetWindowDrawList()->AddRectFilled(viewport->Pos, viewport->Pos + ImVec2(px(kSidebarWidth), height), p.sidebar);
    ImGui::GetWindowDrawList()->AddRectFilled(viewport->Pos + ImVec2(px(kSidebarWidth) - 1.0f, 0), viewport->Pos + ImVec2(px(kSidebarWidth), height),
                                              p.separator);
    sidebar(height);
    ImGui::SetCursorScreenPos(viewport->Pos + ImVec2(px(kSidebarWidth), 0));
    page(viewport->Size.x - px(kSidebarWidth), height);
    ImGui::End();
    popup_was_open_ = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
}

void App::set_interactive() {
    interactive_ = true;
    show_focus_ = SDL_HasGamepad();
}

// What the arrow keys and a gamepad do beyond moving the ring and pressing what it is on (which is ImGui's).
void App::navigation() {
    bool held = false;
    for (int key = ImGuiKey_GamepadStart; key <= ImGuiKey_GamepadRStickDown; key++) {
        held = held || ImGui::IsKeyDown(static_cast<ImGuiKey>(key));
    }
    if (held) {
        ui::request_frames(2);  // (a held button repeats, and a held stick scrolls, only while frames are drawn)
    }
    if (capture_action_ < 0 && !held && nav_hold_) {
        nav_hold_ = false;
        ui::show_focus();  // (the ring is where it was)
    }
    if (show_focus_ && ImGui::GetFocusID() != 0) {
        ui::show_focus();
        show_focus_ = false;
        start_focus_ = starting_ ? ImGui::GetFocusID() : 0;
        starting_ = false;
    }
    // While something is being typed (in the Features page's search box), the arrow keys and Esc are the
    // box's: Esc empties it, a second Esc leaves it
    if (ImGui::GetIO().WantTextInput) {
        left_from_ = 0;
        right_from_sidebar_ = false;
        focus_page_ = false;
        return;
    }
    // Left with nothing to the left of the ring in the page: to the sidebar
    ImGuiID focused = ImGui::GetFocusID();
    bool left_failed = left_from_ != 0 && left_from_ == focused && !in_sidebar_;
    left_from_ = 0;
    // Right in the sidebar: into the page, at its top (ImGui's own search only looks level with the row)
    focus_page_ = right_from_sidebar_ && in_sidebar_;
    right_from_sidebar_ = false;
    if (navigation_paused() || popup_was_open_) {
        focus_page_ = false;
        return;
    }
    if (in_sidebar_ && ui::focus_shown() &&
        (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickRight))) {
        right_from_sidebar_ = true;
        ui::request_frames(3);
    }
    if (left_failed) {
        focus_sidebar_ = true;
    }
    if (ui::focus_shown() && (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickLeft))) {
        left_from_ = focused;
        ui::request_frames(3);
    }
    // The shoulder buttons: the page before, the page after
    int turn = (ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false) ? 1 : 0) - (ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false) ? 1 : 0);
    if (turn != 0) {
        page_ = std::clamp(page_ + turn, 0, page_count() - 1);
        focus_sidebar_ = true;
    }
    // Back (Esc, or a gamepad's B): from a page to the sidebar
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
        focus_sidebar_ = true;
    }
}

void App::sidebar(float height) {
    const ui::Palette& p = ui::palette();
    ImVec2 origin = ImGui::GetMainViewport()->Pos;
    float inner = kSidebarWidth - 40.0f;
    ImGui::SetCursorScreenPos(origin + ImVec2(px(24), px(28)));
    ui::text(LAUNCHER_GAME_NAME, Font::Semibold, 17.0f, p.text);
    ImGui::SetCursorScreenPos(origin + ImVec2(px(24), px(50)));
    // (a collection's subtitle may take two lines)
    ui::text(collection_ ? collection_->subtitle : about_.subtitle, Font::Regular, ui::size::kDetail, p.secondary, collection_ ? px(inner) : 0.0f);
    const char* own[] = {"Game", about_.features_name, "Controllers", "Games", "", ""};
    float top = collection_ ? px(108) : px(100);
    float step = px(38);
    // (a collection's games' pages under a gap and a heading)
    float games_gap = px(52);
    auto row_y = [&](float index) { return top + index * step + (collection_ && index > 2.5f ? games_gap : 0.0f); };
    ImGuiID indicator = ImHashStr("nav-indicator");
    float at = ui::animate(indicator, row_y(static_cast<float>(page_)), 18.0f);
    int custom = collection_ ? 0 : static_cast<int>(about_.pages.size());
    ImVec2 pill = origin + ImVec2(px(20), at);
    ImGui::GetWindowDrawList()->AddRectFilled(pill, pill + ImVec2(px(inner), px(36)), p.control_hover, px(10));
    if (collection_) {
        ImGui::SetCursorScreenPos(origin + ImVec2(px(24), row_y(3.0f) - px(26)));
        ui::text(titled().empty() ? "YOUR GAMES: NONE YET" : "YOUR GAMES", Font::Semibold, ui::size::kSmall, p.tertiary, px(inner));
    }
    // With the arrow keys or a gamepad: from a page, left goes to the row of the page shown and to no
    // other; once there, up and down change the page; right goes into it.
    ImGuiID focused = ImGui::GetFocusID();
    int shown = page_;
    bool in_sidebar = false;
    for (int i = 0; i < page_count(); i++) {
        ImGui::SetCursorScreenPos(origin + ImVec2(px(20), row_y(static_cast<float>(i))));
        ImGui::PushID(i);
        size_t which = 0;
        Page kind = own_page(i, &which);
        const char* name = i < custom ? about_.pages[i].name : kind == Page::Title ? slots_[which]->name.c_str() : own[static_cast<int>(kind)];
        if (ui::nav_item(name, shown == i, inner, in_sidebar_ || shown == i)) {
            page_ = i;
        }
        if (focused == ImGui::GetItemID()) {
            in_sidebar = true;
            if (focused != focus_seen_ && in_sidebar_ && ui::focus_shown()) {
                page_ = i;
            }
        }
        if (focus_sidebar_ && shown == i) {
            ui::focus_item();
            focus_sidebar_ = false;
        }
        ImGui::PopID();
    }
    in_sidebar_ = in_sidebar;
    focus_seen_ = focused;
    ImGui::SetCursorScreenPos(origin + ImVec2(px(24), height - px(36)));
    ui::text(LAUNCHER_DISCLAIMER, Font::Regular, ui::size::kSmall, p.tertiary, px(inner));
}

void App::page(float width, float height) {
    page_ = std::clamp(page_, 0, page_count() - 1);  // (a collection's game's page goes when its file does)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(kPagePaddingX), px(kPagePaddingTop)));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::palette().background);
    // (the page as a whole is not something the ring goes on; what is in it is)
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    ImGui::BeginChild(page_ + 100, ImVec2(width, height), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, false);
    // (The sidebar and the page are apart for the arrow keys and a gamepad: ImGui's own search would go from
    // a sidebar row to whatever in the page is nearest. Left, Right and Back go between them: navigation().)
    // A gamepad's right stick scrolls the page (what is only text is not somewhere the ring goes)
    float stick = ImGui::GetKeyData(ImGuiKey_GamepadRStickDown)->AnalogValue - ImGui::GetKeyData(ImGuiKey_GamepadRStickUp)->AnalogValue;
    if (focus_page_) {
        ImGui::SetScrollY(0.0f);
        ui::focus_next_item();
        focus_page_ = false;
    }
    if (stick != 0.0f && !popup_was_open_) {
        ImGui::SetScrollY(std::clamp(ImGui::GetScrollY() + stick * px(1100) * ImGui::GetIO().DeltaTime, 0.0f, ImGui::GetScrollMaxY()));
    }
    float content = std::min(width - px(kPagePaddingX) * 2, px(kContentMaxWidth));
    size_t which = 0;
    Page kind = own_page(page_, &which);
    slot_ = kind == Page::Title ? which : 0;
    if (!collection_ && page_ < static_cast<int>(about_.pages.size())) {
        launcher::PageContext shown = context(content);
        about_.pages[page_].draw(shown);
    }
    switch (kind) {
    case Page::Game:
        game_page(content);
        break;
    case Page::Features:
        features_page(content);
        break;
    case Page::Controllers:
        controllers_page(content);
        break;
    case Page::Library:
        library_page(content);
        break;
    case Page::Title:
        title_page(which, content);
        break;
    case Page::Count:
        break;
    }
    browser();
    ui::gap(40);
    ImGui::PopItemFlag();
    ImGui::EndChild();
    ImGui::PopItemFlag();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ---- Game --------------------------------------------------------------------------------

// The card with the game's file in it, and the button that chooses it
void App::file_card(size_t which, float width) {
    const ui::Palette& p = ui::palette();
    Slot& one = *slots_[which];
    FileCheck file;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        file = one.file;
    }
    ui::text(one.about->file_heading, Font::Semibold, ui::size::kDetail, p.secondary);
    ui::gap(10);
    Card card(width);
    std::string shown = one.file_path.empty() ? std::string(one.about->file_empty) : one.file_path;
    shown = ui::fit_start(shown, Font::Regular, ui::size::kBody, width - px(200));
    std::string status;
    if (one.checking) {
        status = "Checking…";
    } else if (!one.file_path.empty()) {
        status = file.message;
    }
    ImVec2 row_top = ImGui::GetCursorScreenPos();
    row(width, shown.c_str(), status.c_str(), 130.0f, 36.0f, [&] {
        if (ui::button("Choose…", Kind::Secondary, 130.0f, !one.checking)) {
            // (the system's file dialog wants a mouse or a keyboard)
            if (ui::gamepad_in_use()) {
                open_browser(which, false);
            } else {
                choose_game_file(which);
            }
        }
    });
    if (!one.file_path.empty() && !one.checking) {
        // A mark at the card's left edge: good file, usable file, or not usable.
        ImVec2 center(row_top.x - px(14), row_top.y + px(24));
        if (file.result == FileCheck::Result::Good) {
            ui::check_mark(center, px(12), p.success);
        } else if (file.usable()) {
            ui::check_mark(center, px(12), p.accent);
        } else {
            ui::cross_mark(center, px(12), p.danger);
        }
    }
    card.end();
}

// The Display card (its switches) and the Sound card
void App::display_cards(size_t which, float width) {
    const ui::Palette& p = ui::palette();
    Slot& one = *slots_[which];
    if (!one.about->display.empty()) {
        ui::gap(24);
        ui::text("DISPLAY", Font::Semibold, ui::size::kDetail, p.secondary);
        ui::gap(10);
        Card display(width);
        bool first = true;
        for (const launcher::Toggle& toggle : one.about->display) {
            if (!first) {
                inset_separator(width);
            }
            first = false;
            bool on = toggle_on(one, toggle);
            std::string id = std::string("##") + toggle.key;
            row(width, toggle.title, toggle.detail, 42.0f, 24.0f, [&] {
                if (ui::toggle(id.c_str(), on)) {
                    set_toggle(one, toggle, !on);
                }
            });
        }
        display.end();
    }
    if (one.about->mute_argument || !one.about->folders.empty()) {
        ui::gap(24);
        ui::text("SOUND", Font::Semibold, ui::size::kDetail, p.secondary);
        ui::gap(10);
        Card sound(width);
        bool first = true;
        if (one.about->mute_argument) {
            first = false;
            bool muted = prefs_.get("audio", "muted") == "1";
            row(width, "Muted",
                collection_ ? "Start the games with no sound. A game's menu switches it back on." : "Start the game with no sound. The game's menu switches it back on.",
                42.0f, 24.0f, [&] {
                    if (ui::toggle("##muted", muted)) {
                        prefs_.set("audio", "muted", muted ? "0" : "1");
                        prefs_.save();
                    }
                });
        }
        for (size_t n = 0; n < one.about->folders.size(); n++) {
            const launcher::FolderChoice& folder = one.about->folders[n];
            if (!first) {
                inset_separator(width);
            }
            first = false;
            std::string chosen = prefs_.get(one.file_section, folder.key);
            std::string detail = chosen.empty() ? std::string(folder.detail) : ui::fit_start(chosen, Font::Regular, ui::size::kDetail, width - px(300));
            ImGui::PushID(static_cast<int>(n));
            row(width, folder.title, detail.c_str(), chosen.empty() ? 130.0f : 230.0f, 36.0f, [&] {
                if (!chosen.empty()) {
                    if (ui::button("Clear", Kind::Secondary, 90.0f)) {
                        set_folder_choice(which, static_cast<int>(n), "");
                    }
                    ImGui::SameLine(0.0f, px(10));
                }
                if (ui::button("Choose…", Kind::Secondary, 130.0f)) {
                    // (the system's folder dialog wants a mouse or a keyboard)
                    if (ui::gamepad_in_use()) {
                        open_browser(which, true);
                        browse_choice_ = static_cast<int>(n);
                        if (!chosen.empty()) {
                            list_folder(std::filesystem::u8path(chosen));
                        }
                    } else {
                        choose_folder_for(which, static_cast<int>(n));
                    }
                }
            });
            ImGui::PopID();
        }
        sound.end();
    }
}

// Play, what went wrong with the last start, and (Linux) Add to Steam
void App::play_button(size_t which, float width) {
    const ui::Palette& p = ui::palette();
    Slot& one = *slots_[which];
    FileCheck file;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        file = one.file;
    }
    bool file_ok = !one.checking && file.usable();
    bool have_program = one.has_program();
    // (a port's launcher: while its game runs; a collection's: while this game runs, the others may start)
    bool ready = file_ok && have_program && !one.game.running() && (collection_ || !any_running());
    if (one.about->preset_summary) {
        std::string preset = recomp::Preset(one.settings);
        std::string summary = preset == "pure"       ? std::string("Features: Pure (exactly as ") + one.about->original + ")"
                              : preset == "enhanced" ? "Features: Enhanced"
                                                     : "Features: Custom";
        ui::text(summary.c_str(), Font::Regular, ui::size::kDetail, p.secondary);
        ui::gap(12);
    }
    if (ui::button(one.game.running() ? "Playing…###play" : "Play###play", Kind::Primary, 200.0f, ready, 48.0f)) {
        play(which);
    }
    // With a gamepad in hand the ring starts on Play, once there is a game to play
    if (start_focus_ != 0 && ImGui::GetFocusID() != start_focus_) {
        start_focus_ = 0;
    } else if (start_focus_ != 0 && ready) {
        ui::focus_item();
        start_focus_ = 0;
    }
    if (!have_program) {
        ui::gap(10);
        ui::text("The game program wasn't found next to the launcher.", Font::Regular, ui::size::kDetail, p.danger, width);
    } else if (!one.play_error.empty()) {
        ui::gap(10);
        ui::text(one.play_error.c_str(), Font::Regular, ui::size::kDetail, p.danger, width);
    } else if (!one.game.running() && one.game.exit_code() != 0) {
        ui::gap(10);
        std::string text = "The game stopped with code " + std::to_string(one.game.exit_code()) + ".";
        ui::text(text.c_str(), Font::Regular, ui::size::kDetail, p.danger, width);
    }
    // Linux and Steam Deck: shortcuts for Steam's Game Mode.
    static const bool steam = can_add_to_steam();
    if (steam) {
        ui::gap(28);
        ui::text("STEAM", Font::Semibold, ui::size::kDetail, p.secondary);
        ui::gap(10);
        Card card(width);
        std::string detail = !steam_message_.empty() ? steam_message_
                             : collection_ ? "Adds \"" + one.name + "\" (straight into the game) and \"" LAUNCHER_GAME_NAME "\" to your Steam library, to play from Game Mode."
                                           : "Adds \"" LAUNCHER_GAME_NAME "\" (straight into the game) and \"" LAUNCHER_GAME_NAME " Launcher\" to your Steam library, to play from Game Mode.";
        row(width, "Play from Steam", detail.c_str(), 150.0f, 36.0f, [&] {
            if (ui::button("Add to Steam", Kind::Secondary, 150.0f, ready)) {
                if (collection_) {
                    add_to_steam(steam_message_, one.name, std::string(LAUNCHER_GAME_EXE "-") + one.id, "--play " + one.id);
                } else {
                    add_to_steam(steam_message_);
                }
            }
        });
        card.end();
    }
}

void App::game_page(float width) {
    layout::heading(LAUNCHER_GAME_NAME, about_.file_intro, width);
    file_card(0, width);
    display_cards(0, width);
    ui::gap(28);
    play_button(0, width);
}

// A collection's first page: every game, its file (or where to find them all), and Play
void App::library_page(float width) {
    const ui::Palette& p = ui::palette();
    layout::heading("Games", collection_->intro, width);
    ui::text("FIND YOUR GAMES", Font::Semibold, ui::size::kDetail, p.secondary);
    ui::gap(10);
    Card find(width);
    std::string detail = finding_ ? std::string("Looking…") : find_message_.empty() ? std::string(collection_->find_detail) : find_message_;
    row(width, collection_->find_title, detail.c_str(), 150.0f, 36.0f, [&] {
        if (ui::button("Choose folder…", Kind::Secondary, 150.0f, !finding_)) {
            if (ui::gamepad_in_use()) {
                open_browser(kAnyGame, true);
            } else {
                choose_folder();
            }
        }
    });
    find.end();
    ui::gap(24);
    ui::text("GAMES", Font::Semibold, ui::size::kDetail, p.secondary);
    ui::gap(10);
    Card games(width);
    bool first = true;
    for (size_t which = 0; which < slots_.size(); which++) {
        Slot& one = *slots_[which];
        if (!one.has_program()) {
            continue;  // (not in this build)
        }
        if (!first) {
            inset_separator(width);
        }
        first = false;
        FileCheck file;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            file = one.file;
        }
        bool usable = !one.checking && !one.file_path.empty() && file.usable();
        std::string status = one.checking ? std::string("Checking…") : one.file_path.empty() ? std::string(one.about->file_empty) : file.message;
        ImGui::PushID(static_cast<int>(which));
        ImVec2 row_top = ImGui::GetCursorScreenPos();
        row(width, one.name.c_str(), status.c_str(), 230.0f, 36.0f, [&] {
            if (ui::button("Choose…", Kind::Secondary, 110.0f, !one.checking)) {
                if (ui::gamepad_in_use()) {
                    open_browser(which, false);
                } else {
                    choose_game_file(which);
                }
            }
            ImGui::SameLine(0.0f, px(10));
            if (ui::button(one.game.running() ? "Playing…" : "Play", Kind::Primary, 110.0f, usable && !one.game.running())) {
                play(which);
            }
        });
        if (!one.file_path.empty() && !one.checking) {
            ImVec2 center(row_top.x - px(14), row_top.y + px(24));
            if (file.result == FileCheck::Result::Good) {
                ui::check_mark(center, px(12), p.success);
            } else if (file.usable()) {
                ui::check_mark(center, px(12), p.accent);
            } else {
                ui::cross_mark(center, px(12), p.danger);
            }
        }
        ImGui::PopID();
    }
    games.end();
    // (what went wrong with each game's last start)
    for (const auto& one : slots_) {
        std::string text = !one->play_error.empty() ? one->name + ": " + one->play_error
                           : !one->game.running() && one->game.exit_code() != 0
                               ? one->name + " stopped with code " + std::to_string(one->game.exit_code()) + "."
                               : std::string();
        if (!text.empty()) {
            ui::gap(10);
            ui::text(text.c_str(), Font::Regular, ui::size::kDetail, p.danger, width);
        }
    }
}

// A collection's page for one game: its file, Display and Sound, Play, and the features that are its alone
void App::title_page(size_t which, float width) {
    const ui::Palette& p = ui::palette();
    Slot& one = *slots_[which];
    layout::heading(one.name.c_str(), one.about->file_intro, width);
    file_card(which, width);
    display_cards(which, width);
    ui::gap(28);
    play_button(which, width);
    ui::gap(36);
    ui::text("THIS GAME'S FEATURES", Font::Semibold, ui::size::kDetail, p.secondary);
    ui::gap(6);
    ui::text("What only this game has. What the games share is on the Features page, set for all of them.", Font::Regular, ui::size::kDetail,
             p.secondary, width);
    ui::gap(18);
    if (!one.features_error.empty()) {
        ui::text(one.features_error.c_str(), Font::Regular, ui::size::kBody, p.danger, width);
        return;
    }
    std::vector<int> values = recomp::FeatureValues(one.settings, one.features);
    std::vector<FeatureRow> rows;
    for (size_t i = 0; i < one.features.size(); i++) {
        bool shared = std::any_of(shared_.begin(), shared_.end(), [&](const Shared& s) {
            return std::any_of(s.where.begin(), s.where.end(), [&](const auto& at) { return at.first == which && at.second == i; });
        });
        if (!shared) {
            rows.push_back({&one.features[i], values[i], "", i});
        }
    }
    feature_list(width, rows, "Nothing of its own: every feature it has is on the Features page.",
                 [&](const FeatureRow& feature, int value) { set_feature(one, feature.index, value); });
}

// ---- the game file, chosen with a gamepad --------------------------------------------------

void App::open_browser(size_t which, bool folders) {
    browse_for_ = which;
    browse_folders_ = folders;
    browse_choice_ = -1;
    std::error_code error;
    std::string from = which < slots_.size() ? slots_[which]->file_path : std::string();
    if (from.empty()) {  // (a collection's: where another game's file is)
        for (const auto& one : slots_) {
            from = from.empty() ? one->file_path : from;
        }
    }
    std::filesystem::path start = from.empty() ? std::filesystem::path() : std::filesystem::u8path(from).parent_path();
    if (start.empty() || !std::filesystem::is_directory(start, error)) {
        const char* home = SDL_GetUserFolder(SDL_FOLDER_HOME);
        start = home ? std::filesystem::u8path(home) : std::filesystem::current_path(error);
    }
    list_folder(start);
    browse_open_ = true;
}

// The folders in a folder and the files the port's filter takes ("*", or endings: "zip;bin"), folders first.
void App::list_folder(const std::filesystem::path& folder) {
    std::vector<std::string> endings;
    const launcher::Description& about = browse_for_ < slots_.size() ? *slots_[browse_for_]->about : about_;
    std::string pattern = about.file_filter.pattern ? about.file_filter.pattern : "*";
    std::transform(pattern.begin(), pattern.end(), pattern.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::stringstream parts(pattern);
    for (std::string ending; std::getline(parts, ending, ';');) {
        endings.push_back(ending);
    }
    bool any = std::find(endings.begin(), endings.end(), "*") != endings.end();
    browse_folder_ = folder.lexically_normal();
    if (!browse_folder_.has_filename() && browse_folder_.has_parent_path() && browse_folder_ != browse_folder_.root_path()) {
        browse_folder_ = browse_folder_.parent_path();  // ("a/b/" is "a/b")
    }
    browse_entries_.clear();
    std::error_code error;
    std::filesystem::directory_iterator each(browse_folder_, std::filesystem::directory_options::skip_permission_denied, error);
    for (; !error && each != std::filesystem::directory_iterator(); each.increment(error)) {
        std::string name = each->path().filename().u8string();
        if (name.empty() || name[0] == '.') {
            continue;
        }
        std::error_code unreadable;
        bool is_folder = each->is_directory(unreadable);
        if (!is_folder && !any) {
            std::string ending = each->path().extension().u8string();
            std::transform(ending.begin(), ending.end(), ending.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ending.empty() || std::find(endings.begin(), endings.end(), ending.substr(1)) == endings.end()) {
                continue;
            }
        }
        browse_entries_.push_back({name, is_folder});
    }
    auto lower = [](std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    };
    std::sort(browse_entries_.begin(), browse_entries_.end(), [&](const BrowseEntry& a, const BrowseEntry& b) {
        return a.folder != b.folder ? a.folder : lower(a.name) < lower(b.name);
    });
    browse_moved_ = true;
}

void App::browser() {
    const ui::Palette& p = ui::palette();
    if (browse_open_) {
        ImGui::OpenPopup("##browse");
        browse_open_ = false;
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 size(std::min(px(640), viewport->Size.x - px(48)), std::min(px(520), viewport->Size.y - px(48)));
    float inner = size.x - px(36);
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(18), px(18)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, px(14));
    if (ImGui::BeginPopupModal("##browse", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        ui::text(browse_choice_ >= 0  ? "Choose the folder"
                 : browse_folders_    ? "Choose the folder your games are in"
                                      : "Choose the file",
                 Font::Semibold, 17.0f, p.text);
        ui::gap(4);
        std::string where = ui::fit_start(browse_folder_.u8string(), Font::Regular, ui::size::kDetail, inner);
        ui::text(where.c_str(), Font::Regular, ui::size::kDetail, p.secondary);
        ui::gap(12);
        bool up = browse_folder_.has_parent_path() && browse_folder_.parent_path() != browse_folder_;
        int chosen = -2;  // -1: the folder above
        ImGui::BeginChild("##list", ImVec2(inner, ImGui::GetContentRegionAvail().y - px(36 + 12)), ImGuiChildFlags_NavFlattened);
        bool moved = browse_moved_;
        browse_moved_ = false;
        if (moved) {
            ImGui::SetScrollY(0.0f);
        }
        float row = inner - px(14);
        if (up) {
            if (ui::list_item("..", false, row)) {
                chosen = -1;
            }
            if (moved) {
                ui::focus_item();
            }
        }
        for (size_t i = 0; i < browse_entries_.size(); i++) {
            const BrowseEntry& entry = browse_entries_[i];
            ImGui::PushID(static_cast<int>(i));
            std::string label = ui::fit_start(entry.folder ? entry.name + "/" : entry.name, Font::Regular, ui::size::kBody, row - px(24));
            if (ui::list_item(label.c_str(), false, row)) {
                chosen = static_cast<int>(i);
            }
            if (moved && !up && i == 0) {
                ui::focus_item();
            }
            ImGui::PopID();
        }
        if (browse_entries_.empty()) {
            ui::gap(8);
            ui::text("Nothing here to choose.", Font::Regular, ui::size::kBody, p.secondary);
        }
        ImGui::EndChild();
        ui::gap(12);
        bool close = ui::button("Cancel", Kind::Secondary, 110.0f) || ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                     ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
        if (browse_folders_) {
            ImGui::SameLine(0.0f, px(10));
            if (ui::button("Use this folder", Kind::Primary, 160.0f)) {
                if (browse_choice_ >= 0) {
                    set_folder_choice(browse_for_, browse_choice_, browse_folder_.u8string());
                } else {
                    find_in(browse_folder_.u8string());
                }
                close = true;
            }
        }
        if (chosen == -1) {
            list_folder(browse_folder_.parent_path());
        } else if (chosen >= 0 && browse_entries_[static_cast<size_t>(chosen)].folder) {
            list_folder(browse_folder_ / std::filesystem::u8path(browse_entries_[static_cast<size_t>(chosen)].name));
        } else if (chosen >= 0 && browse_choice_ < 0) {  // (a file: not what a folder of the Sound card's takes)
            std::string path = (browse_folder_ / std::filesystem::u8path(browse_entries_[static_cast<size_t>(chosen)].name)).u8string();
            if (browse_for_ < slots_.size()) {
                use_game_file(browse_for_, path);
            } else {
                find_in(path);
            }
            close = true;
        }
        if (close) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
}

// ---- Features ----------------------------------------------------------------------------

void App::features_page(float width) {
    const ui::Palette& p = ui::palette();
    std::string intro = about_.features_intro
                            ? std::string(about_.features_intro)
                            : std::string("Pure plays the game exactly as it was ") + about_.original + ". Enhanced turns on the improvements; or pick your own.";
    if (collection_) {
        intro = std::string("For every game that has them. Pure plays each game exactly as it was ") + about_.original +
                ". Enhanced turns on the improvements; or pick your own. What only one game has is on that game's page.";
    }
    layout::heading(about_.features_name, intro.c_str(), width);

    if (about_.presets) {
        std::vector<std::string> labels = {std::string("Pure: as ") + about_.original, "Enhanced: recommended", "Custom"};
        // (a collection's: the one every game has, or Custom where they differ)
        std::string preset;
        for (const auto& one : slots_) {
            if (!collection_ || one->has_program()) {
                std::string its = recomp::Preset(one->settings);
                preset = preset.empty() || preset == its ? its : "custom";
            }
            if (!collection_) {
                break;
            }
        }
        int index = 0;
        for (int i = 0; i < 3; i++) {
            if (preset == kPresetNames[i]) {
                index = i;
            }
        }
        if (ui::dropdown("##preset", labels, index, 280.0f)) {
            set_preset(kPresetNames[index]);
        }
        ui::gap(26);
    }

    if (!collection_) {
        Slot& one = slot();
        if (!one.features_error.empty()) {
            ui::text(one.features_error.c_str(), Font::Regular, ui::size::kBody, p.danger, width);
            return;
        }
        std::vector<int> values = recomp::FeatureValues(one.settings, one.features);
        std::vector<FeatureRow> rows;
        for (size_t i = 0; i < one.features.size(); i++) {
            rows.push_back({&one.features[i], values[i], "", i});
        }
        feature_list(width, rows, "No optional features yet.", [&](const FeatureRow& feature, int value) { set_feature(one, feature.index, value); });
        return;
    }
    // A collection's: each as the first game that has it has it (a note where the games differ, or not every
    // game has it)
    std::vector<std::vector<int>> values(slots_.size());
    for (size_t which = 0; which < slots_.size(); which++) {
        values[which] = recomp::FeatureValues(slots_[which]->settings, slots_[which]->features);
    }
    size_t here = 0;
    for (const auto& one : slots_) {
        here += one->has_program();
    }
    std::vector<FeatureRow> rows;
    for (size_t i = 0; i < shared_.size(); i++) {
        const Shared& shared = shared_[i];
        int value = values[shared.where[0].first][shared.where[0].second];
        bool differ = std::any_of(shared.where.begin(), shared.where.end(), [&](const auto& at) { return values[at.first][at.second] != value; });
        std::string note;
        if (shared.where.size() < here) {
            note = "Only in ";
            for (size_t k = 0; k < shared.where.size(); k++) {
                note += (k == 0 ? "" : k + 1 == shared.where.size() ? " and " : ", ") + slots_[shared.where[k].first]->name;
            }
            note += ".";
        }
        if (differ) {
            note += std::string(note.empty() ? "" : " ") + "Not the same in every game yet: what you choose here is.";
        }
        rows.push_back({&shared.info, value, note, i});
    }
    feature_list(width, rows, here < 2 ? "With one game, its features are on its own page." : "The games have no features in common.",
                 [&](const FeatureRow& feature, int value) { set_shared(feature.index, value); });
}

// The rows under their categories, narrowed by the search box: only those whose name, description, category or
// choices have every word typed (Ctrl+F or Cmd+F goes into it)
template <typename Set>
void App::feature_list(float width, const std::vector<FeatureRow>& rows, const char* none, Set set) {
    const ui::Palette& p = ui::palette();
    if (rows.empty()) {
        ui::text(none, Font::Regular, ui::size::kBody, p.secondary, width);
        return;
    }
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F) || ImGui::IsKeyChordPressed(ImGuiMod_Super | ImGuiKey_F)) {
        focus_search_ = true;
    }
    if (focus_search_) {
        ImGui::SetKeyboardFocusHere();
        focus_search_ = false;
    }
    ui::search_box("##search", feature_search_, sizeof(feature_search_), "Search features", width);
    std::vector<std::string> words;
    {
        std::istringstream typed(lower(feature_search_));
        for (std::string word; typed >> word;) {
            words.push_back(word);
        }
    }
    std::vector<bool> shown(rows.size(), true);
    size_t matches = rows.size();
    if (!words.empty()) {
        matches = 0;
        for (size_t i = 0; i < rows.size(); i++) {
            const recomp::FeatureInfo& feature = *rows[i].info;
            std::string text = feature.title + "\n" + feature.description + "\n" + feature.category + "\n" + feature.name;
            for (const std::string& choice : feature.choices) {
                text += "\n" + choice;
            }
            text = lower(text);
            shown[i] = std::all_of(words.begin(), words.end(), [&](const std::string& word) { return text.find(word) != std::string::npos; });
            matches += shown[i];
        }
        ui::gap(8);
        std::string count = matches == 0 ? std::string("No features match.") : std::to_string(matches) + " of " + std::to_string(rows.size()) + " features";
        ui::text(count.c_str(), Font::Regular, ui::size::kDetail, p.secondary);
    }
    ui::gap(20);
    std::vector<std::string> categories;
    for (size_t i = 0; i < rows.size(); i++) {
        const std::string& category = rows[i].info->category;
        if (shown[i] && std::find(categories.begin(), categories.end(), category) == categories.end()) {
            categories.push_back(category);
        }
    }
    for (const std::string& category : categories) {
        std::string upper = category;
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        ui::text(upper.c_str(), Font::Semibold, ui::size::kDetail, p.secondary);
        ui::gap(10);
        Card card(width);
        bool first = true;
        for (size_t i = 0; i < rows.size(); i++) {
            const recomp::FeatureInfo& feature = *rows[i].info;
            if (feature.category != category || !shown[i]) {
                continue;
            }
            if (!first) {
                inset_separator(width);
            }
            first = false;
            ImGui::PushID(feature.name.c_str());
            std::string detail = rows[i].note.empty() ? feature.description : feature.description + " " + rows[i].note;
            if (feature.choices.empty()) {
                bool on = rows[i].value != 0;
                row(width, feature.title.c_str(), detail.c_str(), 42.0f, 24.0f, [&] {
                    if (ui::toggle("##on", on)) {
                        set(rows[i], on ? 0 : 1);
                    }
                });
            } else {
                // One of several things: the first means off
                int chosen = rows[i].value;
                row(width, feature.title.c_str(), detail.c_str(), 190.0f, 36.0f, [&] {
                    if (ui::dropdown("##choice", feature.choices, chosen, 190.0f)) {
                        set(rows[i], chosen);
                    }
                });
            }
            ImGui::PopID();
        }
        card.end();
        ui::gap(24);
    }
}
// ---- Controllers -------------------------------------------------------------------------

namespace {

// A gamepad's button or trigger as the page names it (SDL's own names are "a", "dpup", ...).
std::string pad_label(recomp::PadInput input) {
    static const struct {
        const char* name;
        const char* label;
    } kLabels[] = {
        {"a", "A / Cross (bottom)"},     {"b", "B / Circle (right)"},      {"x", "X / Square (left)"},      {"y", "Y / Triangle (top)"},
        {"leftshoulder", "Left shoulder"}, {"rightshoulder", "Right shoulder"}, {"lefttrigger", "Left trigger"}, {"righttrigger", "Right trigger"},
        {"dpup", "D-pad up"},            {"dpdown", "D-pad down"},         {"dpleft", "D-pad left"},        {"dpright", "D-pad right"},
        {"start", "Start"},              {"back", "Back / Select"},        {"guide", "Guide"},              {"leftstick", "Left stick click"},
        {"rightstick", "Right stick click"}, {"none", "Not set"},
    };
    std::string name = recomp::PadText(input);
    for (const auto& known : kLabels) {
        if (name == known.name) {
            return known.label;
        }
    }
    return name;
}

std::string key_label(SDL_Scancode key) {
    return key == SDL_SCANCODE_UNKNOWN ? "Not set" : recomp::KeyText(key);
}

}  // namespace

// (a collection's: the same for every game, each with the actions it has)
void App::save_bindings() {
    const recomp::BindTable& table = about_.bindings;
    for (auto& one : slots_) {
        for (int player = 0; player < table.players; player++) {
            for (int action = 0; action < table.count; action++) {
                one->settings.set(recomp::BindingSection(false, player), table.actions[action].name, recomp::KeyText(bindings_.key[player][action]));
                one->settings.set(recomp::BindingSection(true, player), table.actions[action].name, recomp::PadText(bindings_.pad[player][action]));
            }
        }
        one->settings.save();
    }
}

// A key or a gamepad input is given to an action. Whatever had it before gets what the action had (for the
// same player: the two change places), so nothing is ever on two actions. A key is one player's only.
void App::bind(int player, bool gamepad, int action, SDL_Scancode key, recomp::PadInput input) {
    const recomp::BindTable& table = about_.bindings;
    if (gamepad) {
        recomp::PadInput was = bindings_.pad[player][action];
        for (int other = 0; other < table.count; other++) {
            if (other != action && input != recomp::PadInput{} && bindings_.pad[player][other] == input) {
                bindings_.pad[player][other] = was;
            }
        }
        bindings_.pad[player][action] = input;
    } else {
        SDL_Scancode was = bindings_.key[player][action];
        for (int who = 0; who < table.players; who++) {
            for (int other = 0; other < table.count; other++) {
                if ((who != player || other != action) && key != SDL_SCANCODE_UNKNOWN && bindings_.key[who][other] == key) {
                    bindings_.key[who][other] = who == player ? was : SDL_SCANCODE_UNKNOWN;
                }
            }
        }
        bindings_.key[player][action] = key;
    }
    save_bindings();
}

// While an action waits to be given a key or a button: the next one pressed is it. Escape leaves things as
// they were; Backspace or Delete leaves the action with nothing.
bool App::capture(const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_DOWN) {
        SDL_Scancode key = event.key.scancode;
        if (event.key.repeat) {
            return true;  // (the key that pressed the button here, still held: not the key for it)
        }
        if (key == SDL_SCANCODE_BACKSPACE || key == SDL_SCANCODE_DELETE) {
            bind(bind_player_, capture_gamepad_, capture_action_, SDL_SCANCODE_UNKNOWN, {});
        } else if (key != SDL_SCANCODE_ESCAPE && !capture_gamepad_) {
            bind(bind_player_, false, capture_action_, key, {});
        } else if (key != SDL_SCANCODE_ESCAPE) {
            return true;  // (a key while a gamepad's button is waited for: nothing)
        }
        capture_action_ = -1;
        return true;
    }
    // (a key let go is ImGui's to hear: it may be the Enter or Space that pressed the button here)
    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        // The button for the action; or, while a key is waited for, any button leaves things as they were
        // (a player with only a gamepad has no Esc). Until it is let go it does nothing else.
        if (capture_gamepad_) {
            bind(bind_player_, true, capture_action_, SDL_SCANCODE_UNKNOWN, {event.gbutton.button, -1});
        }
        capture_action_ = -1;
        nav_hold_ = true;
        return true;
    }
    if (capture_gamepad_ && event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && event.gaxis.value > 16000 &&
        (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) {
        bind(bind_player_, true, capture_action_, SDL_SCANCODE_UNKNOWN, {-1, event.gaxis.axis});
        capture_action_ = -1;
        nav_hold_ = true;
        return true;
    }
    return false;
}

void App::controllers_page(float width) {
    const ui::Palette& p = ui::palette();
    layout::heading("Controllers", "The keyboard always works. Gamepads show up here when connected; press buttons and move the sticks to check they work.",
            width);

    int count = 0;
    SDL_JoystickID* pads = SDL_GetGamepads(&count);
    ui::text("CONNECTED", Font::Semibold, ui::size::kDetail, p.secondary);
    ui::gap(10);
    Card card(width);
    if (count == 0) {
        row(width, "No controllers connected", "", 0.0f, 0.0f, [] {});
    }
    for (int i = 0; i < count && i < 4; i++) {
        if (i > 0) {
            inset_separator(width);
        }
        const char* name = SDL_GetGamepadNameForID(pads[i]);
        std::string slot = "Controller " + std::to_string(i + 1);
        ImGui::PushID(i);
        row(width, name ? name : "?", slot.c_str(), 110.0f, 36.0f, [&] {
            if (ui::button(selected_pad_ == i ? "Testing" : "Test", selected_pad_ == i ? Kind::Primary : Kind::Secondary, 110.0f)) {
                selected_pad_ = i;
            }
        });
        ImGui::PopID();
    }
    card.end();
    ui::gap(24);

    if (count > 0) {
        int index = std::min(selected_pad_, count - 1);
        SDL_Gamepad* pad = SDL_OpenGamepad(pads[index]);  // stays open while the launcher runs
        if (pad) {
            ui::text("TEST", Font::Semibold, ui::size::kDetail, p.secondary);
            ui::gap(10);
            Card test(width);
            std::string pressed;
            for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
                if (SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(b))) {
                    const char* label = SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(b));
                    pressed += pressed.empty() ? "" : "  ";
                    pressed += label ? label : "?";
                }
            }
            row(width, "Buttons", pressed.empty() ? "(press any button)" : pressed.c_str(), 0.0f, 0.0f, [] {});
            inset_separator(width);
            auto axis = [&](SDL_GamepadAxis a) { return static_cast<float>(SDL_GetGamepadAxis(pad, a)) / 32767.0f; };
            char sticks[128];
            std::snprintf(sticks, sizeof sticks, "left %+.2f %+.2f   right %+.2f %+.2f", axis(SDL_GAMEPAD_AXIS_LEFTX), axis(SDL_GAMEPAD_AXIS_LEFTY),
                          axis(SDL_GAMEPAD_AXIS_RIGHTX), axis(SDL_GAMEPAD_AXIS_RIGHTY));
            row(width, "Sticks", sticks, 0.0f, 0.0f, [] {});
            test.end();
            ui::gap(24);
            ui::request_frames(2);  // keep the readings live
        }
    }
    SDL_free(pads);

    const recomp::BindTable& table = about_.bindings;
    if (table.count > 0) {
        // Every gamepad is opened (and stays open while the launcher runs), so that its buttons are heard
        // when one is being chosen below.
        {
            int all = 0;
            SDL_JoystickID* ids = SDL_GetGamepads(&all);
            for (int i = 0; i < all; i++) {
                if (std::find(opened_pads_.begin(), opened_pads_.end(), ids[i]) == opened_pads_.end() && SDL_OpenGamepad(ids[i])) {
                    opened_pads_.push_back(ids[i]);
                }
            }
            SDL_free(ids);
        }

        // What each key and button does (bindings.h; the game reads the same settings.ini).
        ui::text("BUTTONS", Font::Semibold, ui::size::kDetail, p.secondary);
        ui::gap(10);
        if (capture_action_ >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            capture_action_ = -1;  // (a click anywhere: not waiting any more; on another button, waiting for that one)
        }
        for (int player = 0; table.players > 1 && player < table.players; player++) {
            if (player > 0) {
                ImGui::SameLine(0.0f, px(8));
            }
            std::string label = "Player " + std::to_string(player + 1);
            if (ui::button(label.c_str(), bind_player_ == player ? Kind::Primary : Kind::Secondary, 110.0f)) {
                bind_player_ = player;
                capture_action_ = -1;
            }
        }
        if (table.players > 1) {
            ImGui::SameLine(0.0f, px(8));
        }
        if (ui::button("Back to the usual layout", Kind::Ghost, 200.0f)) {
            recomp::Bindings usual = recomp::DefaultBindings(table);
            for (int action = 0; action < table.count; action++) {
                bindings_.key[bind_player_][action] = usual.key[bind_player_][action];
                bindings_.pad[bind_player_][action] = usual.pad[bind_player_][action];
            }
            // (a key the usual layout gives this player is taken from the others)
            for (int who = 0; who < table.players; who++) {
                for (int action = 0; who != bind_player_ && action < table.count; action++) {
                    for (int other = 0; other < table.count; other++) {
                        SDL_Scancode& theirs = bindings_.key[who][other];
                        if (theirs != SDL_SCANCODE_UNKNOWN && theirs == bindings_.key[bind_player_][action]) {
                            theirs = SDL_SCANCODE_UNKNOWN;
                        }
                    }
                }
            }
            capture_action_ = -1;
            save_bindings();
        }
        ui::gap(10);
        constexpr float kChip = 170.0f, kChipGap = 10.0f;
        Card buttons(width);
        static const char* const kNth[recomp::kBindMaxPlayers] = {"first", "second", "third", "fourth"};
        const std::string whose = "Player " + std::to_string(bind_player_ + 1);
        const std::string what = std::string("The keyboard and the ") + kNth[bind_player_] +
                                 " gamepad. Click a button here, then press the key or the gamepad button for it.";
        row(width, table.players < 2 ? "Buttons" : whose.c_str(),
            table.players < 2 ? "The keyboard and the gamepad. Click a button here, then press the key or the gamepad button for it."
                              : what.c_str(),
            kChip * 2 + kChipGap, 18.0f, [&] {
                ImVec2 at = ImGui::GetCursorScreenPos();
                ui::text("Keyboard", Font::Semibold, ui::size::kDetail, p.secondary);
                ImGui::SetCursorScreenPos(at + ImVec2(px(kChip + kChipGap), 0));
                ui::text("Gamepad", Font::Semibold, ui::size::kDetail, p.secondary);
            });
        for (int action = 0; action < table.count; action++) {
            inset_separator(width);
            ImGui::PushID(action);
            row(width, table.actions[action].title, "", kChip * 2 + kChipGap, 34.0f, [&] {
                ImVec2 at = ImGui::GetCursorScreenPos();
                for (int gamepad = 0; gamepad < 2; gamepad++) {
                    bool waiting = capture_action_ == action && capture_gamepad_ == (gamepad != 0);
                    std::string label = waiting ? (gamepad ? "Press a button..." : "Press a key...")
                                        : gamepad ? pad_label(bindings_.pad[bind_player_][action])
                                                  : key_label(bindings_.key[bind_player_][action]);
                    ImRect rect;
                    ImGui::SetCursorScreenPos(at + ImVec2(gamepad ? px(kChip + kChipGap) : 0.0f, 0));
                    if (ui::key_chip(gamepad ? "##pad" : "##key", label.c_str(), waiting, kChip, rect)) {
                        capture_action_ = action;
                        capture_gamepad_ = gamepad != 0;
                    }
                }
            });
            ImGui::PopID();
        }
        buttons.end();
        ui::gap(8);
        ui::text("While a button waits: Esc leaves it as it was, Backspace leaves it with nothing. Giving a key or a button that is in use "
                 "swaps the two. A gamepad's left stick always works as the directions.",
                 Font::Regular, ui::size::kDetail, p.secondary, width);
        ui::gap(24);
        if (capture_action_ >= 0) {
            ui::request_frames(2);
        }
    }

    if (!about_.controls.empty()) {
        ui::text(about_.controls_heading, Font::Semibold, ui::size::kDetail, p.secondary);
        ui::gap(10);
        Card layout(width);
        bool first = true;
        for (const launcher::InfoRow& info : about_.controls) {
            if (!first) {
                inset_separator(width);
            }
            first = false;
            row(width, info.label, info.text, 0.0f, 0.0f, [] {});
        }
        layout.end();
    }
}
