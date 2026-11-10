// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#include "app.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iterator>
#include <map>
#include <random>
#include <sstream>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef __ANDROID__
#include <dlfcn.h>
#include <jni.h>
#endif

#include "folders.h"
#include "gamepads.h"
#include "i18n.h"
#include "layout.h"
#include "online.h"
#include "steam.h"
#include "ui.h"
#include "volume.h"

using launcher::FileCheck;
using layout::Card;
using layout::inset_separator;
using layout::row;
using ui::Font;
using ui::Kind;
using ui::px;

namespace {

// A narrow window (a small screen, or large text) has a narrower sidebar and less room round the page
bool narrow() {
    return ImGui::GetMainViewport()->Size.x / ui::scale() < 760.0f;
}
float sidebar_width() {
    return narrow() ? 180.0f : 220.0f;
}
float page_padding_x() {
    return narrow() ? 24.0f : 44.0f;
}
float page_padding_top() {
    return narrow() ? 26.0f : 40.0f;
}
constexpr float kContentMaxWidth = 720.0f;
constexpr size_t kAnyGame = static_cast<size_t>(-1);  // (a file or folder for whichever game takes it)

void wake();

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
#ifdef __ANDROID__
    // (the folder the app's libraries are unpacked to: the launcher's, and each game's beside it)
    Dl_info info;
    if (dladdr(reinterpret_cast<void*>(&wake), &info) && info.dli_fname) {
        return std::filesystem::path(info.dli_fname).parent_path();
    }
#endif
    // (SDL's paths are UTF-8: by their wide name on Windows, where a folder with an accent is not in the system's code page)
    const char* base = SDL_GetBasePath();
    return base ? std::filesystem::u8path(base) : std::filesystem::current_path();
}

void wake() {
    SDL_Event event{};
    event.type = SDL_EVENT_USER;
    SDL_PushEvent(&event);
}

const char* kPresetNames[] = {"pure", "enhanced", "custom"};

// A game's program beside the launcher. In a build folder the player build is <exe>_app (<exe> there is the
// strict test build); in an app bundle or AppImage it's <exe>. On Android it is a library, lib<exe>.so.
std::filesystem::path program_path(const std::string& exe) {
    std::filesystem::path base = base_path();
#ifdef __ANDROID__
    return base / ("lib" + exe + ".so");
#endif
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

// The system's folder for a game's (or a collection's) things, named by it (SDL_GetPrefPath makes it)
std::filesystem::path system_folder(const char* game_name) {
    const std::string name = recomp::FolderName(game_name);  // (what Windows takes as a folder's name)
#ifdef __ANDROID__
    // (SDL_GetPrefPath is one folder for the whole app there: the app's own folder on the shared storage, which a
    // computer sees over USB in Android/data, with a folder for each name in it)
    const char* storage = SDL_GetAndroidExternalStoragePath();
    if (!storage) {
        storage = SDL_GetAndroidInternalStoragePath();
    }
    if (storage) {
        std::filesystem::path folder = std::filesystem::path(storage) / std::filesystem::u8path(name);
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        return folder;
    }
#endif
    char* path = SDL_GetPrefPath("", name.c_str());
    std::filesystem::path folder = path ? std::filesystem::u8path(path) : base_path() / recomp::kPortableFolder / std::filesystem::u8path(name);
    SDL_free(path);
    return folder;
}

// The player's folder for a game (or a collection): named by it, in user/ beside the launcher in portable mode
// (folders.h), or the folder a variable names (tests)
std::filesystem::path player_folder(const char* name, const char* home_env) {
    if (const char* home = home_env && *home_env ? std::getenv(home_env) : nullptr; home && *home) {
        return home;
    }
    if (std::filesystem::path portable = recomp::PortableFolder(base_path()); !portable.empty()) {
        return portable / std::filesystem::u8path(recomp::FolderName(name));
    }
    return system_folder(name);  // where the game keeps settings.ini too
}

// A folder as a file:// URL, for the system to open
std::string file_url(const std::filesystem::path& folder) {
    std::string url = "file://";
    std::string path = folder.generic_u8string();
    if (path.empty() || path[0] != '/') {
        url += '/';  // (file:///C:/...)
    }
    for (unsigned char c : path) {
        if (std::isalnum(c) || std::strchr("/-_.~:", c)) {
            url += static_cast<char>(c);
        } else {
            char escaped[4];
            std::snprintf(escaped, sizeof escaped, "%%%02X", c);
            url += escaped;
        }
    }
    return url;
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
    std::string id, name, exe, series, file_section = "game", video_section = "video";
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
            target.series = game.series ? game.series : "";
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
    if (about.volume_argument) {
        // (the level the game kept, if it kept one: game-volume-control)
        recomp::Ini kept(target.folder / "settings.ini");
        kept.load();
        const std::string level = kept.get("audio", "volume");
        if (!level.empty()) {
            arguments.push_back(about.volume_argument);
            arguments.push_back(std::to_string(recomp::ClampVolume(std::atoi(level.c_str()))));
        }
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

using recomp::Tr;
using recomp::i18n::TrFormat;

// (--play is often typed: what goes wrong is said on the terminal too)
static void say(SDL_MessageBoxFlags kind, const std::string& title, const std::string& message) {
    std::fprintf(stderr, "%s: %s\n", title.c_str(), message.c_str());
    SDL_ShowSimpleMessageBox(kind, title.c_str(), Tr(message).c_str(), nullptr);
}

bool prepare_folders(bool ask) {
    if (const char* home = std::getenv(LAUNCHER_HOME_ENV); home && *home) {
        return true;  // (a test's folder)
    }
    const std::filesystem::path portable = recomp::PortableFolder(base_path());
    if (portable.empty()) {
        return true;
    }
    std::error_code error;
    const bool first = !std::filesystem::exists(portable, error);
    std::filesystem::create_directories(portable, error);
    // (a drive that is read-only, or a folder the player may not write to, such as Program Files)
    const std::filesystem::path probe = portable / ".writable";
    bool writable = !error && std::ofstream(probe).good();
    std::filesystem::remove(probe, error);
    if (!writable) {
        say(SDL_MESSAGEBOX_ERROR, LAUNCHER_GAME_NAME,
            "portable.txt is beside the launcher, so your settings and saves go in " + portable.u8string() +
                ", but that folder cannot be written to.\n\nMove the launcher's folder somewhere you can write to (not a "
                "read-only drive or Program Files), or take portable.txt away to keep them in your user folder.");
        return false;
    }
    if (!first || !ask) {
        return true;
    }
    // The first time: what the system's folders have may be copied here (never moved, never written over)
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> copies;
    auto consider = [&](const char* name) {
        const std::filesystem::path to = portable / std::filesystem::u8path(recomp::FolderName(name));
        for (const auto& copy : copies) {
            if (copy.second == to) {
                return;
            }
        }
        const std::filesystem::path from = system_folder(name);
        std::error_code ignored;
        if (std::filesystem::is_empty(from, ignored)) {
            std::filesystem::remove(from, ignored);  // (made just now by asking where it is: it was not there)
        } else if (!ignored) {
            copies.push_back({from, to});
        }
    };
    consider(LAUNCHER_GAME_NAME);
    for (const Target& target : targets()) {
        consider(target.name.c_str());
    }
    if (copies.empty()) {
        return true;
    }
    std::string message = "portable.txt is beside the launcher: your settings, save states and the boards' memory are kept in " +
                          portable.u8string() + " from now on.\n\nCopy what you have in your user folder there?\n";
    for (const auto& copy : copies) {
        message += "\n" + copy.first.u8string();
    }
    message += "\n\nNothing is taken away from it. Without portable.txt the launcher uses it again, as it was.";
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Copy them"},
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Start afresh"},
    };
    const SDL_MessageBoxData box = {SDL_MESSAGEBOX_INFORMATION, nullptr, LAUNCHER_GAME_NAME, message.c_str(), 2, buttons, nullptr};
    int chosen = 0;
    if (!SDL_ShowMessageBox(&box, &chosen) || chosen != 1) {
        return true;
    }
    std::string failed;
    for (const auto& copy : copies) {
        std::filesystem::copy(copy.first, copy.second,
                              std::filesystem::copy_options::recursive | std::filesystem::copy_options::skip_existing, error);
        if (error) {
            failed += "\n" + copy.first.u8string() + ": " + error.message();
        }
    }
    if (!failed.empty()) {
        say(SDL_MESSAGEBOX_WARNING, LAUNCHER_GAME_NAME, "Not everything could be copied:" + failed);
    }
    return true;
}

// The reason given to a run whose log says the system has no graphics API the game can draw with (no driver)
static const char* const kGpuDriverReason = "gpu:no-driver";

// The game's log is written over at each start: the log of a run that stopped with an error is kept beside it
// (logs/stopped-<when>.log, the newest five), to send with a report
StoppedLog keep_stopped_log(const std::filesystem::path& folder) {
    namespace fs = std::filesystem;
    StoppedLog kept;
    const fs::path logs = folder / "logs";
    std::error_code error;
    if (!fs::exists(logs / "game.log", error)) {
        return kept;
    }
    std::time_t now = std::time(nullptr);
    std::tm parts{};
#ifdef _WIN32
    localtime_s(&parts, &now);
#else
    localtime_r(&now, &parts);
#endif
    char name[64];
    std::strftime(name, sizeof name, "stopped-%Y-%m-%d-%H%M%S.log", &parts);
    fs::copy_file(logs / "game.log", logs / name, fs::copy_options::overwrite_existing, error);
    if (error) {
        return kept;
    }
    kept.path = logs / name;
    // (the newest five kept; the names sort by time)
    std::vector<fs::path> old;
    for (const auto& entry : fs::directory_iterator(logs, error)) {
        const std::string file = entry.path().filename().u8string();
        if (file.rfind("stopped-", 0) == 0 && entry.path().extension() == ".log") {
            old.push_back(entry.path());
        }
    }
    std::sort(old.begin(), old.end());
    for (size_t i = 0; i + 5 < old.size(); i++) {
        fs::remove(old[i], error);
    }
    // What stopped it, when the runtime said ("recomp stop: <id> ..."): its last such line
    std::ifstream in(kept.path, std::ios::binary);
    std::string line;
    bool no_gpu = false;
    while (std::getline(in, line)) {
        // (SDL's own words when the system has no graphics API the game can draw with: a driver's matter, said so)
        no_gpu = no_gpu || line.find("No supported SDL_GPU backend found") != std::string::npos;
        const size_t at = line.find("recomp stop: ");
        if (at != std::string::npos) {
            std::string id = line.substr(at + 13);
            id = id.substr(0, id.find(' '));
            while (!id.empty() && (id.back() == '\r' || id.back() == '\n')) {
                id.pop_back();
            }
            kept.reason = id;
        }
    }
    if (no_gpu) {
        kept.reason = kGpuDriverReason;
    }
    return kept;
}

// "<name> stopped with code N (reason)"
static std::string stopped_text(const std::string& name, int code, const std::string& reason) {
    return reason.empty() ? TrFormat("{} stopped with code {}", {name, std::to_string(code)})
                          : TrFormat("{} stopped with code {} ({})", {name, std::to_string(code), reason});
}

// What a code means, for a player: a crash (a signal, or a Windows exception), or the runtime stopping on
// purpose (its "recomp stop: <area>:..." line names the part of the game it was in), or "" when not known
static std::string stopped_meaning(int code, const std::string& reason) {
    if (reason == kGpuDriverReason) {
#ifdef _WIN32
        return "Your graphics driver has no Direct3D 12 or Vulkan support, which the game needs to draw. Update your "
               "graphics card's driver from NVIDIA, AMD or Intel; a card too old for either can't run it.";
#elif defined(__APPLE__)
        return "This Mac's graphics have no Metal support, which the game needs to draw. It needs macOS on a Mac "
               "with Metal graphics.";
#else
        return "Your graphics driver has no Vulkan support, which the game needs to draw. Install or update your "
               "system's Vulkan driver (Mesa's, or your graphics card maker's).";
#endif
    }
    if (!reason.empty()) {
        static const std::pair<const char*, const char*> kAreas[] = {
            {"dsound:", "its sound"}, {"audio", "its sound"}, {"d3d", "its graphics"}, {"input:", "the controllers"},
            {"memory:", "its memory"}, {"kernel:", "the Xbox system it runs on"}, {"xapi:", "the Xbox system it runs on"},
            {"import:", "the Xbox system it runs on"}, {"save", "its saves"}, {"file", "reading the game's files"},
        };
        std::string area = "it";
        for (const auto& [prefix, name] : kAreas) {
            if (reason.rfind(prefix, 0) == 0) {
                area = name;
                break;
            }
        }
        return TrFormat("The game met something in {} that this version can't handle yet, so it closed itself rather than go on wrong. "
                        "It's a bug in the port, not in your copy of the game.",
                        {Tr(area)});
    }
    if (code < 0 && code > -64) {
        const int signal = -code;
        const char* what = signal == 11 || signal == 10 ? " (a bad memory access)"
                           : signal == 6             ? " (it gave up on an error)"
                           : signal == 9             ? " (it was killed, perhaps by the system for using too much memory)"
                           : signal == 4             ? " (an instruction it couldn't run)"
                           : signal == 8             ? " (a bad calculation)"
                                                     : "";
        return TrFormat("The game crashed{}. It's a bug in the port, not in your copy of the game.", {Tr(what)});
    }
    const unsigned int windows = static_cast<unsigned int>(code);
    if (windows >= 0xC0000000u) {
        char hex[16];
        std::snprintf(hex, sizeof hex, "0x%08X", windows);
        const char* what = windows == 0xC0000005u   ? " (a bad memory access)"
                           : windows == 0xC00000FDu ? " (it ran out of stack)"
                           : windows == 0xC0000409u ? " (it gave up on an error)"
                           : windows == 0xC0000135u ? " (a file it needs, a .dll, is missing beside it)"
                                                    : "";
        return std::string("The game crashed") + what + ", Windows error " + hex +
               ". It's a bug in the port, not in your copy of the game.";
    }
    return std::string();
}

#ifdef LAUNCHER_ISSUES_REPO
// A new GitHub issue in the port's repository, its title filled in
static std::string issue_url(const std::string& title) {
    std::string url = "https://github.com/" LAUNCHER_ISSUES_REPO "/issues/new?title=";
    for (unsigned char c : title) {
        if (std::isalnum(c) || std::strchr("-_.~", c)) {
            url += static_cast<char>(c);
        } else {
            char escaped[4];
            std::snprintf(escaped, sizeof escaped, "%%%02X", c);
            url += escaped;
        }
    }
    return url;
}
#endif

// The language of the launcher and of the games it starts (i18n.h): launcher.ini's [launcher] language, or ("") the
// system's. The games are told in RECOMP_LANGUAGE.
void use_language(const recomp::Ini& prefs) {
    // (RECOMP_LANGUAGE the launcher was started with goes before the system's: a test's, say)
    static const std::string given = std::getenv("RECOMP_LANGUAGE") ? std::getenv("RECOMP_LANGUAGE") : "";
    std::vector<std::string> preferred;
    if (!given.empty()) {
        preferred.push_back(given);
    }
    int count = 0;
    if (SDL_Locale** locales = SDL_GetPreferredLocales(&count)) {
        for (int i = 0; i < count; i++) {
            std::string code = locales[i]->language ? locales[i]->language : "";
            if (locales[i]->country) {
                code += std::string("-") + locales[i]->country;
            }
            preferred.push_back(code);
        }
        SDL_free(locales);
    }
    for (const char* name : {"LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG"}) {
        if (const char* value = std::getenv(name); value && *value) {
            preferred.push_back(value);
        }
    }
    const std::string language = recomp::i18n::Resolve(prefs.get("launcher", "language"), preferred);
    recomp::i18n::SetLanguage(language);
#ifdef _WIN32
    _putenv_s("RECOMP_LANGUAGE", language.c_str());
#else
    setenv("RECOMP_LANGUAGE", language.c_str(), 1);
#endif
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
    use_language(prefs);
    if (about.game_page && !about.check(prefs.get(target->file_section, about.file_key)).usable()) {
        // (nothing usable remembered, e.g. the folder was moved or the USB drive has another letter: one near the
        // program, as the game's page finds it)
        std::string nearby = about.find_nearby ? about.find_nearby(base_path()) : std::string();
        if (nearby.empty() || !about.check(nearby).usable()) {
            say(SDL_MESSAGEBOX_INFORMATION, target->name, about.file_not_set_up);
            return 1;
        }
        prefs.set(target->file_section, about.file_key, nearby);
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
    // (stopped with an error: no launcher window is open to say so, so a box says what to send and where)
    const int code = game.exit_code();
    const bool next = the_collection() && std::any_of(the_collection()->multi.begin(), the_collection()->multi.end(),
                                                       [&](const launcher::MultiMode& m) { return m.next_code == code; });
    if (code != 0 && !next) {
        const StoppedLog kept = keep_stopped_log(target->folder);
        std::string text = stopped_text(target->name, code, kept.reason) + ".";
        if (std::string meaning = stopped_meaning(code, kept.reason); !meaning.empty()) {
            text += "\n\n" + meaning;
        }
        if (!kept.path.empty()) {
#ifdef LAUNCHER_ISSUES_REPO
            text += "\n\nTo report it, open an issue on GitHub, say what you were doing, and attach this run's log:\n" +
                    kept.path.u8string();
#else
            text += "\n\nTo report it, say what you were doing and send this run's log:\n" + kept.path.u8string();
#endif
        }
        std::fprintf(stderr, "%s\n", text.c_str());
        std::vector<SDL_MessageBoxButtonData> buttons;
#ifdef LAUNCHER_ISSUES_REPO
        buttons.push_back({0, 1, "Report on GitHub"});
#endif
#ifndef __ANDROID__
        if (!kept.path.empty()) {
            buttons.push_back({0, 2, "Show the log"});
        }
#endif
        buttons.push_back({SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Close"});
        const std::string title = target->name;
        SDL_MessageBoxData box{};
        box.flags = SDL_MESSAGEBOX_ERROR;
        box.title = title.c_str();
        box.message = text.c_str();
        box.numbuttons = static_cast<int>(buttons.size());
        box.buttons = buttons.data();
        int chosen = 0;
        if (SDL_ShowMessageBox(&box, &chosen)) {
#ifdef LAUNCHER_ISSUES_REPO
            if (chosen == 1) {
                SDL_OpenURL(issue_url(stopped_text(target->name, code, kept.reason)).c_str());
            }
#endif
            if (chosen == 2) {
                SDL_OpenURL(file_url(kept.path.parent_path()).c_str());
            }
        }
    }
    return code;
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
    use_language(prefs_);
    // (controllers SDL doesn't know: gamecontrollerdb.txt beside the launcher or in its folder, as the game reads it)
    recomp::AddGamepadMappings({program_folder_, folder_});
    for (const Target& target : targets()) {
        auto slot = std::make_unique<Slot>(target.folder / "settings.ini");
        slot->about = target.about;
        slot->id = target.id;
        slot->name = target.name;
        slot->exe = target.exe;
        slot->series = target.series;
        slot->file_section = target.file_section;
        slot->video_section = target.video_section;
        slot->folder = target.folder;
        slots_.push_back(std::move(slot));
    }
    load_order();
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
    if (!collection_ && about_.game_page && (about_.open_on_game_page || about_.own_pages_after_game)) {
        page_ = place_of(Page::Game);
    }
    // The keys and buttons: a port's game's, or a collection's first game's that is here (they are set for all)
    size_t first = 0;
    for (size_t which = 0; which < slots_.size(); which++) {
        if (slots_[which]->has_program()) {
            first = which;
            break;
        }
    }
    bindings_ = recomp::LoadBindings(slots_[first]->settings.path().u8string(), shared_table());
    // (and each game's own, for those given one)
    own_bindings_.resize(slots_.size());
    for (size_t which = 0; which < slots_.size(); which++) {
        if (own_layout(which)) {
            own_bindings_[which] = recomp::LoadBindings(slots_[which]->settings.path().u8string(), slots_[which]->about->bindings);
        }
    }
#ifdef LAUNCHER_UPDATE_REPO
#ifdef _WIN32
    const char* launcher_name = LAUNCHER_GAME_EXE "-launcher.exe";
#else
    const char* launcher_name = LAUNCHER_GAME_EXE "-launcher";
#endif
    updater_ = std::make_unique<launcher::Updater>(program_folder_, launcher_name, LAUNCHER_VERSION, LAUNCHER_UPDATE_REPO, wake);
    if (std::getenv("RECOMP_UPDATE_RELEASES")) {
        updater_->check();  // (a test's list of releases: checked for pictures too)
    }
#endif
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
#ifdef LAUNCHER_UPDATE_REPO
    if (updater_->busy()) {
        return true;
    }
#endif
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
    for (size_t which : order_) {
        const Slot& one = *slots_[which];
        std::lock_guard<std::mutex> lock(mutex_);
        if (one.has_program() && !one.file_path.empty() && !one.checking && one.file.usable()) {
            list.push_back(which);
        }
    }
    return list;
}

bool App::installed_only() const {
    return collection_ && prefs_.get("games", "installed_only") == "1" && !titled().empty();
}

bool App::shown(size_t which) const {
    const Slot& one = *slots_[which];
    if (!one.has_program()) {
        return false;
    }
    if (!installed_only()) {
        return true;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return !one.file_path.empty() && (one.checking || one.file.usable());
}

int App::fixed_pages() const {
    return collection_ ? 3 + (collection_->multi.empty() ? 0 : 1) + (collection_->lobby ? 1 : 0) : 0;
}

int App::page_count() const {
    if (collection_) {
        return fixed_pages() + static_cast<int>(rows().size());
    }
    return static_cast<int>(single_layout().size());
}

std::vector<int> App::single_layout() const {
    const auto page = [](Page kind) { return -1 - static_cast<int>(kind); };
    std::vector<int> layout, own;
    for (size_t i = 0; i < about_.pages.size(); i++) {
        own.push_back(static_cast<int>(i));
    }
    const bool online = about_.online_page && about_.online.host_argument && about_.online.join_argument;
    if (!about_.own_pages_after_game) {
        layout = own;
    }
    if (about_.game_page) {
        layout.push_back(page(Page::Game));
    }
    if (online) {
        layout.push_back(page(Page::Online));
    }
    if (about_.own_pages_after_game) {
        layout.insert(layout.end(), own.begin(), own.end());
    }
    layout.push_back(page(Page::Features));
    layout.push_back(page(Page::Controllers));
    return layout;
}

int App::port_page(int index) const {
    if (collection_) {
        return -1;
    }
    const std::vector<int> layout = single_layout();
    return index >= 0 && static_cast<size_t>(index) < layout.size() && layout[static_cast<size_t>(index)] >= 0 ? layout[static_cast<size_t>(index)] : -1;
}

int App::place_of(Page kind) const {
    const std::vector<int> layout = single_layout();
    for (size_t i = 0; i < layout.size(); i++) {
        if (layout[i] == -1 - static_cast<int>(kind)) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

void App::set_page(const std::string& id) {
    if (id == "multi" && !collection_->multi.empty()) {
        page_ = fixed_pages() - 1;
    }
    if (id == "online" && collection_ && collection_->lobby) {
        page_ = 1;
    }
    // (a game in a closed series: the series opened)
    for (const std::vector<size_t>& unit : units()) {
        for (size_t which : unit) {
            if (grouped(unit) && slots_[which]->id == id && !series_open(slots_[which]->series)) {
                open_series(slots_[which]->series, true);
            }
        }
    }
    std::vector<Row> list = rows();
    for (size_t i = 0; i < list.size(); i++) {
        if (!list[i].series && slots_[list[i].game]->id == id) {
            page_ = fixed_pages() + static_cast<int>(i);
        }
    }
}

void App::load_order() {
    order_.clear();
    std::stringstream ids(prefs_.get("games", "order"));
    for (std::string id; std::getline(ids, id, ',');) {
        for (size_t which = 0; which < slots_.size(); which++) {
            if (slots_[which]->id == id && std::find(order_.begin(), order_.end(), which) == order_.end()) {
                order_.push_back(which);
            }
        }
    }
    for (size_t which = 0; which < slots_.size(); which++) {
        if (std::find(order_.begin(), order_.end(), which) == order_.end()) {
            order_.push_back(which);
        }
    }
    group_order();
}

void App::group_order() {
    std::vector<size_t> list = titled();
    std::vector<size_t> grouped;
    for (size_t which : order_) {
        const std::string& series = slots_[which]->series;
        if (series.empty()) {
            grouped.push_back(which);
            continue;
        }
        auto same = [&](size_t one) { return slots_[one]->series == series; };
        auto first = std::find_if(list.begin(), list.end(), same);
        if (which == (first != list.end() ? *first : *std::find_if(order_.begin(), order_.end(), same))) {
            std::copy_if(order_.begin(), order_.end(), std::back_inserter(grouped), same);
        }
    }
    order_ = grouped;
}

std::vector<std::vector<size_t>> App::units() const {
    std::vector<std::vector<size_t>> list;
    for (size_t which : titled()) {
        const std::string& series = slots_[which]->series;
        if (!series.empty() && !list.empty() && slots_[list.back().front()]->series == series) {
            list.back().push_back(which);
        } else {
            list.push_back({which});
        }
    }
    return list;
}

// A unit is a series' row (with its games under it) when its games have a series, even if it is the only game of
// it so far (a kind of game, Shoot-'em-ups, before the second comes)
bool App::grouped(const std::vector<size_t>& unit) const {
    return !unit.empty() && !slots_[unit.front()]->series.empty();
}

std::vector<App::Row> App::rows() const {
    std::vector<Row> list;
    for (const std::vector<size_t>& unit : units()) {
        if (!grouped(unit)) {
            list.push_back({unit.front()});
            continue;
        }
        list.push_back({unit.front(), true});
        if (series_open(slots_[unit.front()]->series)) {
            for (size_t which : unit) {
                list.push_back({which, false, true});
            }
        }
    }
    return list;
}

bool App::series_open(const std::string& series) const {
    std::stringstream names(prefs_.get("games", "open"));
    for (std::string name; std::getline(names, name, ',');) {
        if (name == series) {
            return true;
        }
    }
    return false;
}

void App::open_series(const std::string& series, bool open) {
    std::stringstream names(prefs_.get("games", "open"));
    std::string kept;
    for (std::string name; std::getline(names, name, ',');) {
        if (name != series && !name.empty()) {
            kept += (kept.empty() ? "" : ",") + name;
        }
    }
    if (open) {
        kept += (kept.empty() ? "" : ",") + series;
        opened_ = series;
    }
    prefs_.set("games", "open", kept);
    prefs_.save();
}

void App::keep_page(const std::function<void()>& change) {
    size_t which = 0;
    Page kind = own_page(page_, &which);
    std::string series = kind == Page::Series ? slots_[which]->series : std::string();
    change();
    if (kind != Page::Title && kind != Page::Series) {
        return;
    }
    std::vector<Row> list = rows();
    for (size_t i = 0; i < list.size(); i++) {
        const Row& row = list[i];
        bool same = kind == Page::Series ? row.series && slots_[row.game]->series == series
                                         : !row.series && row.game == which;
        // (a game of a series that has closed: the series)
        bool closed = kind == Page::Title && row.series && slots_[row.game]->series == slots_[which]->series;
        if (same || closed) {
            page_ = fixed_pages() + static_cast<int>(i);
            if (same) {
                break;
            }
        }
    }
}

void App::reorder(const std::vector<size_t>& list) {
    keep_page([&] {
        // (the games with a page take the places such games had; those without stay where they are)
        std::vector<size_t> before = titled();
        size_t next = 0;
        for (size_t& one : order_) {
            if (next < list.size() && std::find(before.begin(), before.end(), one) != before.end()) {
                one = list[next++];
            }
        }
        group_order();
        std::string ids;
        for (size_t one : order_) {
            ids += (ids.empty() ? "" : ",") + slots_[one]->id;
        }
        prefs_.set("games", "order", ids);
        prefs_.save();
    });
}

void App::move_game(size_t which, int place) {
    std::vector<std::vector<size_t>> list = units();
    for (size_t unit = 0; unit < list.size(); unit++) {
        std::vector<size_t>& games = list[unit];
        auto at = std::find(games.begin(), games.end(), which);
        if (at == games.end()) {
            continue;
        }
        if (games.size() == 1) {
            move_unit(unit, place);
            return;
        }
        place = std::clamp(place, 0, static_cast<int>(games.size()) - 1);
        if (at - games.begin() == place) {
            return;
        }
        games.erase(at);
        games.insert(games.begin() + place, which);
        std::vector<size_t> flat;
        for (const std::vector<size_t>& one : list) {
            flat.insert(flat.end(), one.begin(), one.end());
        }
        reorder(flat);
        return;
    }
}

void App::move_unit(size_t unit, int place) {
    std::vector<std::vector<size_t>> list = units();
    if (unit >= list.size() || list.size() < 2) {
        return;
    }
    place = std::clamp(place, 0, static_cast<int>(list.size()) - 1);
    if (static_cast<int>(unit) == place) {
        return;
    }
    std::vector<size_t> moved = list[unit];
    list.erase(list.begin() + static_cast<long>(unit));
    list.insert(list.begin() + place, moved);
    std::vector<size_t> flat;
    for (const std::vector<size_t>& one : list) {
        flat.insert(flat.end(), one.begin(), one.end());
    }
    reorder(flat);
}

// The launcher's own page at a place in the sidebar (a port's pages come first), or Page::Count; for a
// collection's game's page, which game
Page App::own_page(int index, size_t* which) const {
    if (collection_) {
        int fixed = fixed_pages();
        if (index < fixed) {
            // (Games, then Online where there is one: the pages played from; then the settings, then Multi game)
            int online = collection_->lobby ? 1 : 0;
            return index == 0          ? Page::Library
                   : index < 1 + online ? Page::Online
                   : index == 1 + online ? Page::Features
                   : index == 2 + online ? Page::Controllers
                                         : Page::Multi;
        }
        std::vector<Row> list = rows();
        if (index < fixed || static_cast<size_t>(index - fixed) >= list.size()) {
            return Page::Count;
        }
        const Row& row = list[static_cast<size_t>(index - fixed)];
        if (which) {
            *which = row.game;
        }
        return row.series ? Page::Series : Page::Title;
    }
    const std::vector<int> layout = single_layout();
    if (index < 0 || static_cast<size_t>(index) >= layout.size() || layout[static_cast<size_t>(index)] >= 0) {
        return Page::Count;
    }
    return static_cast<Page>(-1 - layout[static_cast<size_t>(index)]);
}

launcher::PageContext App::context(float width) {
    Slot& one = slot();
    launcher::PageContext context{width, window_, prefs_, one.settings, one.folder, program_folder_, one.game.running(), one.game.exit_code(), one.play_error, {}};
    context.play = [this] { play(slot_); };
    return context;
}

// ---- game file ---------------------------------------------------------------------------

#ifdef __ANDROID__
// Android: the system's picker gives documents, not files the games can open. The app's activity (in the port's
// Java) copies those the player chooses into a folder of the launcher's, by their own names:
//   void importFiles(String folder, boolean many)   the picker, then the copies
//   String importResult()   the copies' paths, one a line ("" for none), or null while it is choosing and copying
// Their paths, once that is done (on a thread of the caller's: it waits for the player)
std::vector<std::string> import_files(const std::filesystem::path& folder, bool many) {
    std::vector<std::string> paths;
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    JNIEnv* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) {
        return paths;
    }
    jclass activity_class = env->GetObjectClass(activity);
    jmethodID import = env->GetMethodID(activity_class, "importFiles", "(Ljava/lang/String;Z)V");
    jmethodID result = env->GetMethodID(activity_class, "importResult", "()Ljava/lang/String;");
    if (import && result) {
        jstring folder_text = env->NewStringUTF(folder.string().c_str());
        env->CallVoidMethod(activity, import, folder_text, static_cast<jboolean>(many));
        env->DeleteLocalRef(folder_text);
        for (;;) {
            SDL_Delay(100);
            jstring text = static_cast<jstring>(env->CallObjectMethod(activity, result));
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                break;
            }
            if (!text) {
                continue;
            }
            const char* chars = env->GetStringUTFChars(text, nullptr);
            std::istringstream lines(chars ? chars : "");
            for (std::string line; std::getline(lines, line);) {
                if (!line.empty()) {
                    paths.push_back(line);
                }
            }
            env->ReleaseStringUTFChars(text, chars);
            env->DeleteLocalRef(text);
            break;
        }
    }
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    env->DeleteLocalRef(activity_class);
    env->DeleteLocalRef(activity);
    return paths;
}

// Where those copies go: a folder beside the launcher's own (one game's file is still found among the others there)
std::filesystem::path imported_folder() {
    return player_folder(LAUNCHER_GAME_NAME, LAUNCHER_HOME_ENV) / "roms";
}
#endif

void App::choose_game_file(size_t which) {
#ifdef __ANDROID__
    picked_for_ = which;
    std::thread([this] {
        std::vector<std::string> paths = import_files(imported_folder(), false);
        if (!paths.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            picked_ = paths.front();
            picked_folder_ = false;
        }
        wake();
    }).detach();
    return;
#endif
    const Slot& one = *slots_[which];
    std::string start = one.file_path.empty() ? std::string() : std::filesystem::u8path(one.file_path).parent_path().u8string();
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
#ifdef __ANDROID__
    // (the sets chosen, copied into the launcher's folder for them, which is then looked in for every game)
    std::thread([this] {
        if (!import_files(imported_folder(), true).empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            picked_ = imported_folder().u8string();
            picked_folder_ = true;
        }
        wake();
    }).detach();
    return;
#endif
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

void App::play(size_t which, const std::vector<std::string>& extra) {
    Slot& one = *slots_[which];
    if (one.game.running()) {
        return;  // (one of each game at a time: a second would share its settings and saves)
    }
    one.play_error.clear();
#ifdef LAUNCHER_UPDATE_REPO
    if (updater_->status().state == launcher::Updater::State::Installing || updater_->status().state == launcher::Updater::State::Installed) {
        one.play_error = "The update is being installed: the games start again after it.";
        return;
    }
#endif
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
    std::vector<std::string> arguments = arguments_for(prefs_, target);
    arguments.insert(arguments.end(), extra.begin(), extra.end());
    one.stopped_log.clear();
    one.stopped_reason.clear();
    // (seen as running from here: a game that stops before the next frame still has its end noticed, and its log kept)
    one.was_running = one.game.start(one.program(), arguments, one.folder, one.play_error, one.folder / "logs" / "game.log");
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

bool App::feature_shown(const Slot& one, size_t feature) {
    if (!one.about->feature_shown) {
        return true;
    }
    bool good;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        good = !one.checking && one.file.result == FileCheck::Result::Good;
    }
    return one.about->feature_shown(one.features[feature].name, good ? one.file_path : std::string());
}

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

void App::set_menu_choices(Slot& one, size_t index, const std::vector<bool>& offered) {
    recomp::SetMenuChoices(one.settings, one.features[index], offered);
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
        const int count = page_count();
        const bool first_only = about_.open_on_game_page || about_.own_pages_after_game;
        for (int i = -1; i < (first_only ? 0 : count); i++) {
            const int index = i < 0 ? page_ : i;
            const int own = port_page(index);
            if (own >= 0 && about_.pages[static_cast<size_t>(own)].drop) {
                launcher::PageContext dropped = context(0.0f);
                about_.pages[static_cast<size_t>(own)].drop(dropped, event.drop.data);
                page_ = index;
                return true;
            }
        }
        if (about_.game_page) {
            use_game_file(0, event.drop.data);
            page_ = place_of(Page::Game);
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
            find_message_ = Tr("None of the games was found there.");
        } else {
            std::string names;
            for (size_t i = 0; i < found.size(); i++) {
                names += (i == 0 ? "" : i + 1 == found.size() ? Tr(" and ") : ", ") + slots_[found[i].first]->name;
            }
            find_message_ = TrFormat("Found {} ({} of {}).", {names, std::to_string(found.size()), std::to_string(here)});
        }
    }
    // A game that ended may have changed its settings.ini (its menu's features): what it left is what is shown
    // and kept
    for (size_t which = 0; which < slots_.size(); which++) {
        Slot& one = *slots_[which];
        if (one.was_running && !one.game.running()) {
            one.settings.load();
            after_game(one);
            show_focus_ = interactive_ && SDL_HasGamepad();  // (back from the game with a gamepad in hand)
            one.was_running = false;
            if (one.game.exit_code() != 0 &&
                !(collection_ && std::any_of(collection_->multi.begin(), collection_->multi.end(),
                                             [&](const launcher::MultiMode& m) { return m.next_code == one.game.exit_code(); }))) {
                keep_stopped_log(one);
            }
            if (multi_.mode >= 0 && multi_.at < multi_.games.size() && multi_.games[multi_.at] == which) {
                multi_after(one.game.exit_code() == collection_->multi[static_cast<size_t>(multi_.mode)].next_code);
            }
        }
        one.was_running = one.game.running();
    }
    watch_room();
#ifdef LAUNCHER_UPDATE_REPO
    // The new version installed: it is started, and this one quits (not for pictures, which start nothing)
    if (interactive_ && !relaunched_ && updater_->status().state == launcher::Updater::State::Installed && updater_->relaunch()) {
        relaunched_ = true;
        SDL_Event quit{};
        quit.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quit);
    }
#endif
    navigation();
    const ui::Palette& p = ui::palette();
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::Begin("##root", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    float height = viewport->Size.y;
    ImGui::GetWindowDrawList()->AddRectFilled(viewport->Pos, viewport->Pos + ImVec2(px(sidebar_width()), height), p.sidebar);
    ImGui::GetWindowDrawList()->AddRectFilled(viewport->Pos + ImVec2(px(sidebar_width()) - 1.0f, 0), viewport->Pos + ImVec2(px(sidebar_width()), height),
                                              p.separator);
    sidebar(height);
    ImGui::SetCursorScreenPos(viewport->Pos + ImVec2(px(sidebar_width()), 0));
    page(viewport->Size.x - px(sidebar_width()), height);
    ImGui::End();
    popup_was_open_ = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
}

bool App::scroll_sidebar(float x, float by) {
    if (x - ImGui::GetMainViewport()->Pos.x >= px(sidebar_width())) {
        return false;
    }
    sidebar_scroll_ += by;  // (kept within the list: sidebar)
    scrolled_for_ = page_;  // (and not brought back to the page shown)
    return true;
}

void App::set_interactive() {
    interactive_ = true;
    show_focus_ = SDL_HasGamepad();
#ifdef LAUNCHER_UPDATE_REPO
    if (prefs_.get("update", "check") != "0" && updater_->status().state == launcher::Updater::State::Idle) {
        updater_->check();
    }
#endif
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
    float inner = sidebar_width() - 40.0f;
    ImGui::SetCursorScreenPos(origin + ImVec2(px(24), px(28)));
    ui::text(LAUNCHER_GAME_NAME, Font::Semibold, 17.0f, p.text);
    ImGui::SetCursorScreenPos(origin + ImVec2(px(24), px(50)));
    // (a collection's subtitle may take two lines, or more in a narrow sidebar)
    ui::text(collection_ ? collection_->subtitle : about_.subtitle, Font::Regular, ui::size::kDetail, p.secondary, collection_ ? px(inner) : 0.0f);
#ifdef LAUNCHER_VERSION
    // (the version, under the name: the one online play's matches must have too)
    ui::gap(4);
    ImGui::SetCursorScreenPos(ImVec2(origin.x + px(24), ImGui::GetCursorScreenPos().y));
    ui::text(TrFormat("Version {}", {LAUNCHER_VERSION}).c_str(), Font::Regular, ui::size::kSmall, p.tertiary);
#endif
    const char* own[] = {"Game", about_.features_name, "Controllers", "Games", "", "Multi game", "Online", "", ""};
    float fixed = static_cast<float>(fixed_pages());
    float top = std::max(collection_ ? px(108) : px(100), ImGui::GetCursorScreenPos().y - origin.y + px(24));
    float step = px(38);
    // (a collection's games' pages under a gap and a heading)
    float games_gap = px(52);
    // Where a row is in the list, and on the screen: the list scrolls when it is longer than the room for it (a
    // collection of many games, a small window), with the mouse wheel, and to keep the row the ring is on, or the
    // page shown, in view
    auto row_y = [&](float index) { return top + index * step + (collection_ && index > fixed - 0.5f ? games_gap : 0.0f); };
    int count = page_count();
    float view_top = top - px(4);
    float view_bottom = height - px(56);
    float max_scroll = std::max(0.0f, row_y(static_cast<float>(count - 1)) + px(36) + px(4) - view_bottom);
    // (the row the ring is on, while it is in the sidebar; else the page shown's)
    auto want_row = [&] { return in_sidebar_ && ui::focus_shown() && focus_row_ >= 0 ? focus_row_ : page_; };
    float mouse_y = ImGui::GetIO().MousePos.y - origin.y;
    bool over = ImGui::GetIO().MousePos.x - origin.x < px(sidebar_width()) && ImGui::IsWindowHovered();
    if (max_scroll > 0.0f) {
        auto show_row = [&](int index) {
            float at = row_y(static_cast<float>(index));
            sidebar_scroll_ = std::min(sidebar_scroll_, at - view_top);
            sidebar_scroll_ = std::max(sidebar_scroll_, at + px(36) - view_bottom);
        };
        if (over && ImGui::GetIO().MouseWheel != 0.0f) {
            sidebar_scroll_ -= ImGui::GetIO().MouseWheel * step;
        } else if (dragging_ >= 0) {
            // (a row dragged to the top or the foot of the list scrolls it)
            float speed = px(420) * ImGui::GetIO().DeltaTime;
            sidebar_scroll_ += mouse_y < view_top + px(24) ? -speed : mouse_y > view_bottom - px(24) ? speed : 0.0f;
            ui::request_frames(2);
        } else if (want_row() != scrolled_for_) {
            show_row(want_row());
        }
    }
    scrolled_for_ = want_row();
    sidebar_scroll_ = std::clamp(sidebar_scroll_, 0.0f, max_scroll);
    float scroll = sidebar_scroll_;
    ImGui::PushClipRect(origin + ImVec2(0, view_top), origin + ImVec2(px(sidebar_width()) - 1.0f, view_bottom), true);
    ImGuiID indicator = ImHashStr("nav-indicator");
    // A game's row (a collection's) can be dragged up or down: it follows the mouse, the others make room, and the
    // game takes the place it is over (move_game, once the rows are drawn). A series' row is dragged closed, with its
    // games; a game's under it, among them.
    std::vector<Row> list = collection_ ? rows() : std::vector<Row>();
    int games = static_cast<int>(list.size());
    int dropped = -1;  // (the row let go of: that is not a click on it)
    bool dropped_series = dragging_series_;
    if (dragging_ >= 0 && (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || games < 2)) {
        dropped = dragging_;
        dragging_ = -1;
    }
    // (the dragged row's, and the rows it can go among: its series' games, or the whole list)
    int drag_row = -1, first_row = 0, last_row = games - 1;
    for (int i = 0; i < games && dragging_ >= 0; i++) {
        if (static_cast<int>(list[i].game) == dragging_ && list[i].series == dragging_series_) {
            drag_row = i;
        }
    }
    if (drag_row >= 0 && list[drag_row].inside) {
        const std::string& series = slots_[list[drag_row].game]->series;
        for (first_row = drag_row; first_row > 0 && list[first_row - 1].inside && slots_[list[first_row - 1].game]->series == series; first_row--) {
        }
        for (last_row = drag_row; last_row + 1 < games && list[last_row + 1].inside && slots_[list[last_row + 1].game]->series == series; last_row++) {
        }
    }
    float drag_at = std::clamp(mouse_y + scroll - drag_grab_, row_y(fixed + static_cast<float>(first_row)), row_y(fixed + static_cast<float>(std::max(first_row, last_row))));  // (in the list)
    size_t shown_game = 0;
    Page shown_kind = own_page(page_, &shown_game);
    bool dragging_shown = dragging_ >= 0 && static_cast<int>(shown_game) == dragging_ &&
                          (dragging_series_ ? shown_kind == Page::Series : shown_kind == Page::Title);
    if (dragging_shown) {
        ui::set_value(indicator, drag_at);
    }
    float at = (dragging_shown ? drag_at : ui::animate(indicator, row_y(static_cast<float>(page_)), 18.0f)) - scroll;
    ImVec2 pill = origin + ImVec2(px(20), at);
    ImGui::GetWindowDrawList()->AddRectFilled(pill, pill + ImVec2(px(inner), px(36)), p.control_hover, px(10));
    if (collection_) {
        ImGui::SetCursorScreenPos(origin + ImVec2(px(24), row_y(fixed) - scroll - px(26)));
        ui::text(list.empty() ? "YOUR GAMES: NONE YET" : "YOUR GAMES", Font::Semibold, ui::size::kSmall, p.tertiary, px(inner));
    }
    // With the arrow keys or a gamepad: from a page, left goes to the row of the page shown and to no
    // other; once there, up and down change the page; right goes into it.
    ImGuiID focused = ImGui::GetFocusID();
    int shown = page_;
    bool in_sidebar = false;
    int move_to = -1;
    std::string toggle;       // (a series' row pressed: it opens or closes)
    std::string close_drag;   // (a series' row taken hold of while open: it closes)
    float series_y = 0.0f;    // (the last series' row's place: its games' rows come out from under it)
    focus_row_ = -1;
    for (int i = 0; i < count; i++) {
        size_t which = 0;
        Page kind = own_page(i, &which);
        float y = row_y(static_cast<float>(i));
        int k = i - static_cast<int>(fixed);  // (in the list)
        const Row* row = kind == Page::Title || kind == Page::Series ? &list[static_cast<size_t>(k)] : nullptr;
        if (row) {
            // (by the game's id, or the series' name, so that its row is the same row wherever it is)
            ImGui::PushID(row->series ? ("series:" + slots_[which]->series).c_str() : slots_[which]->id.c_str());
            ImGuiID place = ImGui::GetID("place");
            if (row->series) {
                series_y = y;
            }
            if (row->inside && opened_ == slots_[which]->series) {
                ui::set_value(place, series_y);
            }
            if (k == drag_row) {
                ui::set_value(place, drag_at);
                y = drag_at;
            } else {
                y = ui::animate(place, y, 18.0f);
            }
        } else {
            ImGui::PushID(i);
        }
        y -= scroll;
        ImGui::SetCursorScreenPos(origin + ImVec2(px(20), y));
        const int port = port_page(i);
        const char* name = port >= 0 ? about_.pages[static_cast<size_t>(port)].name
                           : !row     ? own[static_cast<int>(kind)]
                           : row->series ? slots_[which]->series.c_str()
                                         : slots_[which]->name.c_str();
        int fold = row && row->series ? (series_open(slots_[which]->series) ? 1 : 0) : -1;
        bool was_dropped = row && static_cast<int>(which) == dropped && row->series == dropped_series;
        if (ui::nav_item(name, shown == i, inner, in_sidebar_ || shown == i, row && row->inside ? 14.0f : 0.0f, fold) && !was_dropped) {
            page_ = i;
            if (row && row->series) {
                toggle = slots_[which]->series;
            }
        }
        if (kind == Page::Library && update_news()) {
            // (a new version: a dot at the Games row's end, where its update card is)
            const ImVec2 at(ImGui::GetItemRectMax().x - px(18), (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f);
            ImGui::GetWindowDrawList()->AddCircleFilled(at, px(4), ui::palette().accent);
        }
        if (row && games > 1 && ImGui::IsItemActive()) {
            if (dragging_ < 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                dragging_ = static_cast<int>(which);
                dragging_series_ = row->series;
                drag_grab_ = ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Left].y - origin.y - y;
                if (fold == 1) {
                    close_drag = slots_[which]->series;
                }
            }
            if (row && k == drag_row) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                move_to = static_cast<int>(std::floor((drag_at - row_y(fixed)) / step + 0.5f));
            }
        }
        if (focused == ImGui::GetItemID()) {
            in_sidebar = true;
            focus_row_ = i;
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
    opened_.clear();
    ImGui::PopClipRect();
    // (a line where the list goes on out of view)
    float line = std::max(1.0f, std::floor(ui::scale()));
    if (scroll > 0.0f) {
        ImGui::GetWindowDrawList()->AddRectFilled(origin + ImVec2(0, view_top), origin + ImVec2(px(sidebar_width()) - 1.0f, view_top + line), p.separator);
    }
    if (scroll < max_scroll) {
        ImGui::GetWindowDrawList()->AddRectFilled(origin + ImVec2(0, view_bottom - line), origin + ImVec2(px(sidebar_width()) - 1.0f, view_bottom),
                                                  p.separator);
    }
    if (move_to >= 0 && drag_row >= 0) {
        if (list[drag_row].inside) {
            move_game(list[drag_row].game, move_to - first_row);
        } else {
            // (past the places it is over: a series' rows are passed once the dragged row is over its last, going down,
            // or its first, going up)
            std::vector<std::vector<size_t>> places = units();
            int unit = -1, target = -1, place = -1;
            for (int i = 0; i < games; i++) {
                if (list[i].inside) {
                    continue;
                }
                place++;
                int last = i;
                while (last + 1 < games && list[last + 1].inside) {
                    last++;
                }
                if (i == drag_row) {
                    unit = place;
                } else if (i > drag_row && last <= move_to) {
                    target = place;
                } else if (i < drag_row && i >= move_to && target < 0) {
                    target = place;
                }
            }
            if (unit >= 0 && target >= 0) {
                move_unit(static_cast<size_t>(unit), target);
            }
        }
    }
    if (!close_drag.empty()) {
        keep_page([&] { open_series(close_drag, false); });
    }
    if (!toggle.empty()) {
        bool open = !series_open(toggle);
        keep_page([&] { open_series(toggle, open); });
    }
    in_sidebar_ = in_sidebar;
    focus_seen_ = focused;
    ImGui::SetCursorScreenPos(origin + ImVec2(px(24), height - px(36)));
    ui::text(LAUNCHER_DISCLAIMER, Font::Regular, ui::size::kSmall, p.tertiary, px(inner));
}

void App::page(float width, float height) {
    page_ = std::clamp(page_, 0, page_count() - 1);  // (a collection's game's page goes when its file does)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(page_padding_x()), px(page_padding_top())));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::palette().background);
    // (the page as a whole is not something the ring goes on; what is in it is)
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    // (a collection's game's page by the game's id, so that it stays the same page when the game moves in the list)
    size_t shown_game = 0;
    Page shown_kind = own_page(page_, &shown_game);
    ImGuiID child = shown_kind == Page::Title    ? ImHashStr(slots_[shown_game]->id.c_str())
                    : shown_kind == Page::Series ? ImHashStr(("series:" + slots_[shown_game]->series).c_str())
                                                 : static_cast<ImGuiID>(page_ + 100);
    ImGui::BeginChild(child, ImVec2(width, height), ImGuiChildFlags_AlwaysUseWindowPadding);
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
    float content = std::min(width - px(page_padding_x()) * 2, px(kContentMaxWidth));
    size_t which = 0;
    Page kind = own_page(page_, &which);
    slot_ = kind == Page::Title ? which : 0;
    if (port_page(page_) >= 0) {
        launcher::PageContext shown = context(content);
        about_.pages[static_cast<size_t>(port_page(page_))].draw(shown);
    }
    if (room_url()) {
        const bool online_page = collection_ || about_.online_page;
        follow_room(kind == (online_page ? Page::Online : Page::Game) && !any_running());
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
    case Page::Series:
        series_page(which, content);
        break;
    case Page::Online:
        if (collection_) {
            online_page(content);
        } else {
            single_online_page(content);
        }
        break;
    case Page::Multi:
        multi_page(content);
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
    if (one.about->mute_argument || one.about->volume_argument || !one.about->folders.empty()) {
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
        if (one.about->volume_argument) {
            // (the game's own level, in its settings.ini; not while it runs, which writes it when it ends)
            if (!first) {
                inset_separator(width);
            }
            first = false;
            const std::string kept = one.settings.get("audio", "volume");
            int level = kept.empty() ? 100 : recomp::ClampVolume(std::atoi(kept.c_str()));
            const bool running = one.game.running();
            row(width, "Volume",
                running ? "The game is running: its menu, or - and =, change the level." : "The level the game starts at. In the game, - and = change it.",
                220.0f, 24.0f, [&] {
                    if (running) {
                        ImGui::BeginDisabled();
                    }
                    if (ui::slider("##volume", level, 220.0f) && !running) {
                        one.settings.set("audio", "volume", std::to_string(level));
                        one.settings.save();
                    }
                    if (running) {
                        ImGui::EndDisabled();
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
bool App::play_button(size_t which, float width) {
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
    bool online = tunnel_room_.game == which;  // (its room: Play waits for it to end)
    ready = ready && !online;
    if (one.about->preset_summary) {
        std::string preset = recomp::Preset(one.settings);
        std::string summary = preset == "pure"       ? TrFormat("Features: Pure (exactly as {})", {Tr(one.about->original)})
                              : preset == "enhanced" ? Tr("Features: Enhanced")
                                                     : Tr("Features: Custom");
        ui::text(summary.c_str(), Font::Regular, ui::size::kDetail, p.secondary);
        ui::gap(12);
    }
    if (ui::button(online ? "Playing online…###play" : one.game.running() ? "Playing…###play" : "Play###play", Kind::Primary, 200.0f, ready, 48.0f)) {
        if (room_message_for_ == which) {
            room_message_.clear();
            room_message_for_ = SIZE_MAX;
        }
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
    } else if (!one.game.running() && one.game.exit_code() != 0 && room_message_for_ != which &&
               !(collection_ && std::any_of(collection_->multi.begin(), collection_->multi.end(),
                                            [&](const launcher::MultiMode& m) { return m.next_code == one.game.exit_code(); }))) {
        stopped_note(one, width);
    }
    return ready;
}

void App::keep_stopped_log(Slot& one) {
    StoppedLog kept = ::keep_stopped_log(one.folder);
    one.stopped_log = kept.path;
    one.stopped_reason = kept.reason;
}

// A game that stopped with an error: the code and the reason, the log to send, and where to report it
void App::stopped_note(Slot& one, float width) {
    const ui::Palette& p = ui::palette();
    ui::gap(10);
    std::string text = stopped_text(collection_ ? one.name : Tr("The game"), one.game.exit_code(), one.stopped_reason) + ".";
    ui::text(text.c_str(), Font::Regular, ui::size::kDetail, p.danger, width);
    if (std::string meaning = stopped_meaning(one.game.exit_code(), one.stopped_reason); !meaning.empty()) {
        ui::gap(6);
        ui::text(meaning.c_str(), Font::Regular, ui::size::kDetail, p.text, width);
    }
    if (one.stopped_log.empty()) {
        return;
    }
    ui::gap(6);
#ifdef LAUNCHER_ISSUES_REPO
    std::string help = TrFormat("To report it, open an issue on GitHub (Report below), say what you were doing, and attach this "
                                "run's log: {}",
                                {one.stopped_log.u8string()});
#else
    std::string help = TrFormat("To report it, say what you were doing and send this run's log: {}", {one.stopped_log.u8string()});
#endif
#ifdef __ANDROID__
    help += Tr(" (a computer sees it over USB, in Android/data)");
#endif
    ui::text(help.c_str(), Font::Regular, ui::size::kDetail, p.secondary, width);
    ImGui::PushID(one.id.c_str());
    bool row = false;
    const float row_left = ImGui::GetCursorScreenPos().x;
#ifndef __ANDROID__
    ui::gap(8);
    if (ui::button("Show the log", Kind::Secondary, 150.0f) && interactive_) {
        SDL_OpenURL(file_url(one.stopped_log.parent_path()).c_str());
    }
    row = true;
#endif
#ifdef LAUNCHER_ISSUES_REPO
    // (beside Show the log when both fit the page's width, as buttons grow to their labels; under it if not)
    ui::push_font(Font::Semibold, ui::size::kBody);
    const float report_width = std::max(px(180.0f), ImGui::CalcTextSize("Report on GitHub").x + px(40));
    ui::pop_font();
    if (row && ImGui::GetItemRectMax().x + px(10) + report_width <= row_left + width) {
        ImGui::SameLine(0.0f, px(10));
    } else {
        ui::gap(8);
    }
    if (ui::button("Report on GitHub", Kind::Secondary, 180.0f) && interactive_) {
        SDL_OpenURL(issue_url(stopped_text(collection_ ? one.name : std::string(LAUNCHER_GAME_NAME), one.game.exit_code(),
                                           one.stopped_reason)).c_str());
    }
#endif
    (void)row;
    (void)row_left;
    ImGui::PopID();
}

void App::play_cards(size_t which, float width, bool ready) {
    const ui::Palette& p = ui::palette();
    Slot& one = *slots_[which];
    if (collection_ || !about_.online_page) {
        online_card(which, width, ready);
    }
    tunnel_card(which, width); // (this fork: a game played through the tunnel)
    if (one.about->tunnel.port != 0) {
        chat_card(width); // (this fork: ForkAde's in-game chat, for a game played through the tunnel)
    }
    // Linux and Steam Deck: shortcuts for Steam's Game Mode.
    static const bool steam = can_add_to_steam();
    if (steam) {
        ui::gap(28);
        ui::text("STEAM", Font::Semibold, ui::size::kDetail, p.secondary);
        ui::gap(10);
        Card card(width);
        std::string detail = !steam_message_.empty() ? steam_message_
                             : collection_ ? TrFormat("Adds \"{}\" (straight into the game) and \"{}\" to your Steam library, to play from Game Mode.",
                                                      {one.name, LAUNCHER_GAME_NAME})
                                           : TrFormat("Adds \"{}\" (straight into the game) and \"{} Launcher\" to your Steam library, to play from Game Mode.",
                                                      {LAUNCHER_GAME_NAME, LAUNCHER_GAME_NAME});
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
    // Windows: a shortcut on the desktop, straight into the game with the game's icon.
    static const bool desktop = can_add_to_desktop();
    if (desktop) {
        ui::gap(28);
        ui::text("DESKTOP", Font::Semibold, ui::size::kDetail, p.secondary);
        ui::gap(10);
        Card card(width);
        std::string name = collection_ ? one.name : std::string(LAUNCHER_GAME_NAME);
        std::string detail = desktop_message_id_ == one.id && !desktop_message_.empty()
                                 ? desktop_message_
                                 : "Puts \"" + name + "\" on your desktop: a double click starts the game straight away, with its own icon.";
        row(width, "Shortcut on the desktop", detail.c_str(), 150.0f, 36.0f, [&] {
            if (ui::button("Add shortcut", Kind::Secondary, 150.0f, ready)) {
                desktop_message_id_ = one.id;
                add_to_desktop(desktop_message_, name, collection_ ? "--play " + one.id : std::string("--play"), one.program());
            }
        });
        card.end();
    }
}

// ---- online play ---------------------------------------------------------------------------
// The ONLINE card (launcher::Description::Online) and its room: the tunnel (cps-tunel, beside the launcher) carries
// the game's own netplay over Iroh, with a code to copy and paste. One room at a time; the tunnel and the game start
// together and end together. The tunnel's state comes as lines in its log: CODE, LISTEN, CONNECTED [n],
// PATH [n] direct|relay, PING [n] <ms>, CLOSED [n], ERROR [n] <message> (n: a guest, on the host).

namespace {

std::filesystem::path tunnel_path() {
    return program_path("cps-tunel");
}

long own_process_id() {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

}  // namespace

void App::open_room(size_t which, bool host) {
    Slot& one = *slots_[which];
    room_message_.clear();
    room_message_for_ = which;
    std::string code;
    for (const char* c = room_code_; *c; c++) {
        if (std::isalnum(static_cast<unsigned char>(*c))) {
            code += *c;
        }
    }
    if (!host && code.empty()) {
        room_message_ = "Paste the room code your friend sent you first.";
        return;
    }
    std::vector<std::string> args = host ? std::vector<std::string>{"host", "--game-port", std::to_string(one.about->tunnel.port)}
                                         : std::vector<std::string>{"join", code};
    args.push_back("--parent");  // (it ends with the launcher, however the launcher ends)
    args.push_back(std::to_string(own_process_id()));
    std::error_code error;
    std::filesystem::create_directories(one.folder / "logs", error);
    tunnel_room_.log = one.folder / "logs" / "tunel.log";
    tunnel_room_.read = 0;
    tunnel_room_.partial.clear();
    tunnel_room_.code.clear();
    tunnel_room_.started = false;
    tunnel_room_.rival = 0;
    tunnel_room_.connected = tunnel_room_.relay = tunnel_room_.left = tunnel_room_.copied = false;
    tunnel_room_.ping = -1;
    tunnel_room_.error.clear();
    tunnel_room_.host = host;
    std::string message;
    if (!tunnel_room_.tunnel.start(tunnel_path(), args, program_folder_, message, tunnel_room_.log)) {
        room_message_ = message;
        return;
    }
    tunnel_room_.game = which;
    if (host) {
        // (the game waits for a player by itself; the code shows when the tunnel has it)
        std::vector<std::string> arguments = one.about->tunnel.arguments("");
        for (const std::string& more : chat_arguments()) {
            arguments.push_back(more); // (this fork: the in-game chat's name and log)
        }
        play(which, arguments);
        tunnel_room_.started = true;
        if (!one.play_error.empty()) {
            close_room(one.play_error);
        }
    }
}

void App::close_room(const std::string& message) {
    if (tunnel_room_.game == SIZE_MAX) {
        return;
    }
    Slot& one = *slots_[tunnel_room_.game];
    tunnel_room_.tunnel.stop();
    if (tunnel_room_.started) {
        one.game.stop();
    }
    room_message_ = message;
    room_message_for_ = tunnel_room_.game;
    tunnel_room_.game = SIZE_MAX;
}

void App::room_line(const std::string& line) {
    std::istringstream words(line);
    std::string word;
    words >> word;
    // (on the host, a guest's lines carry its number: only the first guest, the rival, is shown)
    int guest = 0;
    if (tunnel_room_.host && word != "CODE") {
        std::istringstream::pos_type at = words.tellg();
        if (!(words >> guest)) {
            words.clear();
            words.seekg(at);
            guest = 0;
        }
        if (word == "CONNECTED" && tunnel_room_.rival == 0) {
            tunnel_room_.rival = guest;
        }
        if (guest != 0 && guest != tunnel_room_.rival) {
            return;
        }
    }
    std::string rest;
    std::getline(words >> std::ws, rest);
    if (word == "CODE") {
        tunnel_room_.code = rest;
    } else if (word == "LISTEN" && !tunnel_room_.host && !tunnel_room_.started) {
        // the tunnel listens: the game joins it
        std::vector<std::string> arguments = slots_[tunnel_room_.game]->about->tunnel.arguments("127.0.0.1:" + rest);
        for (const std::string& more : chat_arguments()) {
            arguments.push_back(more); // (this fork: the in-game chat's name and log)
        }
        play(tunnel_room_.game, arguments);
        tunnel_room_.started = true;
        if (!slots_[tunnel_room_.game]->play_error.empty()) {
            close_room(slots_[tunnel_room_.game]->play_error);
        }
    } else if (word == "CONNECTED") {
        tunnel_room_.connected = true;
        tunnel_room_.left = false;
    } else if (word == "PATH") {
        tunnel_room_.relay = rest == "relay";
    } else if (word == "PING") {
        tunnel_room_.ping = std::atoi(rest.c_str());
    } else if (word == "CLOSED") {
        tunnel_room_.connected = false;
        tunnel_room_.left = true;
    } else if (word == "ERROR") {
        tunnel_room_.error = rest;
    }
}

void App::watch_room() {
    if (tunnel_room_.game == SIZE_MAX) {
        return;
    }
    // The tunnel's new lines
    std::error_code error;
    uintmax_t size = std::filesystem::file_size(tunnel_room_.log, error);
    if (!error && size > tunnel_room_.read) {
        std::ifstream file(tunnel_room_.log, std::ios::binary);
        file.seekg(static_cast<std::streamoff>(tunnel_room_.read));
        std::string got(static_cast<size_t>(size - tunnel_room_.read), '\0');
        file.read(got.data(), static_cast<std::streamsize>(got.size()));
        tunnel_room_.read += static_cast<uintmax_t>(file.gcount());
        tunnel_room_.partial.append(got.data(), static_cast<size_t>(file.gcount()));
        size_t end;
        while ((end = tunnel_room_.partial.find('\n')) != std::string::npos) {
            std::string line = tunnel_room_.partial.substr(0, end);
            tunnel_room_.partial.erase(0, end + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            room_line(line);
            if (tunnel_room_.game == SIZE_MAX) {
                return;  // (closed by a line: the game did not start)
            }
        }
    }
    Slot& one = *slots_[tunnel_room_.game];
    if (tunnel_room_.started && !one.game.running()) {
        close_room("");  // the game ended: so does the room
    } else if (!tunnel_room_.tunnel.running() && !tunnel_room_.started) {
        // The tunnel ended before the game started: a code that was not right, or no internet
        close_room(tunnel_room_.error.find("no es v") != std::string::npos
                       ? "That code isn't complete. Copy it again from the message and paste it whole."
                   : tunnel_room_.error.find("llena") != std::string::npos ? "That room is full."
                   : tunnel_room_.error.empty() ? "The connection couldn't start."
                                         : "The connection couldn't start (" + tunnel_room_.error + ").");
    } else if (!tunnel_room_.tunnel.running()) {
        tunnel_room_.connected = false;
        tunnel_room_.left = true;
    }
}

void App::tunnel_card(size_t which, float width) {
    using ui::px;
    const ui::Palette& p = ui::palette();
    Slot& one = *slots_[which];
    static const bool tunnel = std::filesystem::exists(tunnel_path());
    if (one.about->tunnel.port == 0 || !tunnel) {
        return;
    }
    bool here = tunnel_room_.game == which;
    bool elsewhere = tunnel_room_.game != SIZE_MAX && !here;
    bool can = one.has_program() && !one.game.running() && !elsewhere;
    ui::gap(28);
    ui::text("ONLINE", Font::Semibold, ui::size::kDetail, p.secondary);
    ui::gap(10);
    Card card(width);
    float pad = px(18);
    if (!here) {
        const char* detail = elsewhere ? "Another game has a room open. Leave it first."
                                       : "Create a room and send its code, or paste the code you got.";
        row(width, "Play with a friend over the internet", detail, 150.0f, 36.0f, [&] {
            if (ui::button("Create room", Kind::Secondary, 150.0f, can)) {
                open_room(which, true);
            }
        });
        // The code to join with: pasted (Ctrl+V, or Paste for whoever doesn't know it) and Join
        ImVec2 at = ImGui::GetCursorScreenPos();
        bool wrong = room_message_for_ == which && !room_message_.empty();
        float field = width - pad * 2 - px(90 + 8 + 90 + 8);
        ImGui::SetCursorScreenPos(ImVec2(at.x + pad, at.y - px(2)));
        ui::text_box("##room_code", room_code_, sizeof room_code_, "Paste the room code here", field / ui::scale(), wrong);
        ImGui::SameLine(0, px(8));
        if (ui::button("Paste", Kind::Secondary, 90.0f, can)) {
            if (char* clip = SDL_GetClipboardText()) {
                std::snprintf(room_code_, sizeof room_code_, "%s", clip);
                SDL_free(clip);
            }
        }
        ImGui::SameLine(0, px(8));
        if (ui::button("Join", Kind::Secondary, 90.0f, can)) {
            open_room(which, false);
        }
        if (wrong) {
            ui::gap(8);
            ImGui::SetCursorScreenPos(ImVec2(at.x + pad, ImGui::GetCursorScreenPos().y));
            ui::text(room_message_.c_str(), Font::Regular, ui::size::kDetail, p.danger, width - pad * 2);
        }
        ui::gap(14);
        ImGui::SetCursorScreenPos(ImVec2(at.x, ImGui::GetCursorScreenPos().y));
        ImGui::Dummy(ImVec2(width, 0));
        card.end();
        return;
    }
    // A room: where it stands, and the way out
    std::string status, detail;
    if (!tunnel_room_.error.empty() && !tunnel_room_.connected) {
        status = "Something went wrong with the connection";
        detail = tunnel_room_.error;
    } else if (tunnel_room_.connected) {
        status = std::string(tunnel_room_.relay ? "Connected through a relay" : "Connected (direct)") +
                 (tunnel_room_.ping >= 0 ? ", " + std::to_string(tunnel_room_.ping) + " ms" : "");
        detail = tunnel_room_.host ? "You're player 1. The game started on both sides." : "You're player 2. The game started on both sides.";
    } else if (tunnel_room_.left) {
        status = "The other player left";
        detail = "Close the game, or leave, to play again.";
    } else if (tunnel_room_.host) {
        status = tunnel_room_.code.empty() ? "Opening the room…" : "Waiting for a player";
        detail = tunnel_room_.code.empty() ? "" : "Send this code to your friend (WhatsApp is fine):";
    } else {
        status = "Connecting…";
    }
    const char* way_out = tunnel_room_.connected || tunnel_room_.left || !tunnel_room_.host ? "Leave" : "Close room";
    row(width, status.c_str(), detail.c_str(), 130.0f, 36.0f, [&] {
        if (ui::button(way_out, Kind::Secondary, 130.0f)) {
            close_room("");
        }
    });
    if (tunnel_room_.host && !tunnel_room_.code.empty() && !tunnel_room_.connected && !tunnel_room_.left) {
        ImVec2 at = ImGui::GetCursorScreenPos();
        float text_width = width - pad * 2 - px(110 + 12);
        ImGui::SetCursorScreenPos(ImVec2(at.x + pad, at.y - px(2)));
        ImGui::BeginGroup();
        ui::text(tunnel_room_.code.c_str(), Font::Regular, ui::size::kDetail, p.text, text_width);
        ImGui::EndGroup();
        float bottom = ImGui::GetItemRectMax().y;
        ImGui::SetCursorScreenPos(ImVec2(at.x + width - pad - px(110), at.y - px(2)));
        if (ui::button(tunnel_room_.copied ? "Copied###copy" : "Copy###copy", Kind::Secondary, 110.0f)) {
            SDL_SetClipboardText(tunnel_room_.code.c_str());
            tunnel_room_.copied = true;
        }
        bottom = std::max(bottom, ImGui::GetItemRectMax().y);
        ImGui::SetCursorScreenPos(ImVec2(at.x, bottom + px(14)));
        ImGui::Dummy(ImVec2(width, 0));
    }
    card.end();
}

// (this fork) ForkAde's in-game chat: the name the other player sees (1 to 16 characters, [online] name, which
// Tommy's netplay also says), whether each match's chat is kept on this computer ([chat] keep), and its folder
namespace {
    // (the name as the games clean it, recomp-common's chat.h: no controls, no spaces at its ends, 16 characters)
    std::string ChatName(const char* typed) {
        std::string out;
        size_t characters = 0;
        for (const unsigned char* c = reinterpret_cast<const unsigned char*>(typed); *c; c++) {
            if (*c < 0x20 || *c == 0x7F) {
                continue;
            }
            if ((*c & 0xC0) != 0x80 && ++characters > 16) {
                break;
            }
            out += static_cast<char>(*c);
        }
        const size_t first = out.find_first_not_of(' ');
        return first == std::string::npos ? std::string() : out.substr(first, out.find_last_not_of(' ') - first + 1);
    }
} // namespace

std::filesystem::path App::chat_folder() const {
    return folder_ / "chat";
}

std::vector<std::string> App::chat_arguments() const {
    std::vector<std::string> arguments;
    const std::string name = ChatName(online_name_);
    if (!name.empty()) {
        arguments.insert(arguments.end(), {"--nick", name});
    }
    if (prefs_.get("chat", "keep") != "off") {
        const auto folder = chat_folder().u8string();
        arguments.insert(arguments.end(), {"--chat-log", std::string(folder.begin(), folder.end())});
    }
    return arguments;
}

void App::chat_card(float width) {
    static const bool tunnel = std::filesystem::exists(tunnel_path());
    if (!tunnel) {
        return;
    }
    const ui::Palette& p = ui::palette();
    ui::gap(28);
    ui::text("CHAT", Font::Semibold, ui::size::kDetail, p.secondary);
    ui::gap(10);
    Card card(width);
    row(width, "Your name", "What the other player sees beside your messages (T opens the chat during a match).", 190.0f, 36.0f, [&] {
        if (ui::text_field("##chat_name", online_name_, sizeof(online_name_), "Player 1 or 2", 190.0f)) {
            prefs_.set("online", "name", ChatName(online_name_));
            prefs_.save();
        }
    });
    inset_separator(width);
    const bool keep = prefs_.get("chat", "keep") != "off";
    row(width, "Keep chat logs", "Each match's messages are saved on this computer only, never uploaded.", 60.0f, 36.0f, [&] {
        if (ui::toggle("##chat_keep", keep) != keep) {
            prefs_.set("chat", "keep", keep ? "off" : "on");
            prefs_.save();
        }
    });
    inset_separator(width);
    row(width, "Chat logs folder", "One file a match, with the time of each message.", 150.0f, 36.0f, [&] {
        if (ui::button("Open folder", Kind::Secondary, 150.0f)) {
            std::error_code error;
            std::filesystem::create_directories(chat_folder(), error);
            SDL_OpenURL(file_url(chat_folder()).c_str());
        }
    });
    card.end();
}

// Online play (launcher::Online): Host, with the addresses the other player can use, and Join, with the host's
void App::online_card(size_t which, float width, bool ready) {
    const ui::Palette& p = ui::palette();
    Slot& one = *slots_[which];
    const launcher::Online& online = one.about->online;
    if (!online.host_argument || !online.join_argument) {
        return;
    }
    load_online();
    if (collection_ || !about_.online_page) {
        // (on the Game page; on an Online page of its own, the page's heading says it)
        ui::gap(28);
        ui::text("ONLINE", Font::Semibold, ui::size::kDetail, p.secondary);
        if (online.detail && *online.detail) {
            ui::gap(6);
            ui::text(online.detail, Font::Regular, ui::size::kDetail, p.secondary, width);
        }
        ui::gap(10);
    }
    Card card(width);
    std::string here;
    for (const std::string& address : online_here_) {
        here += (here.empty() ? "" : ", ") + address;
    }
    std::string port = online.port ? TrFormat(" (UDP port {})", {std::to_string(online.port)}) : "";
    std::string detail = online.codes ? TrFormat("Waits for the other player. The game shows a code to give them; on this network they can also join at {}.",
                                                 {(here.empty() ? Tr("this machine's address") : here) + port})
                                      : TrFormat("Waits for the other player, who joins this machine at {}. Over the internet: this network's public "
                                                 "address, with that port forwarded to this machine.",
                                                 {(here.empty() ? Tr("its address") : here) + port});
    const bool lobby = online.lobby && online.list_argument;
    row(width, "Host a match", detail.c_str(), 130.0f, 36.0f, [&] {
        if (ui::button("Host", Kind::Secondary, 130.0f, ready)) {
            play(which, host_arguments(online));
        }
    });
    arcade_row(width, online);
    if (lobby && online.name_argument) {
        inset_separator(width);
        row(width, "Your name",
            "With a name, the matches you host are listed in the lobby under it until someone joins. Without one, only those you "
            "give the code to can join.",
            190.0f, 36.0f, [&] {
                if (ui::text_field("##online_name", online_name_, sizeof(online_name_), "Your name", 190.0f)) {
                    prefs_.set("online", "name", online_name_);
                    prefs_.save();
                }
            });
    }
    inset_separator(width);
    row(width, "Join a match",
        online.codes ? "The code the host's game shows, or the host's address." : "The host's address, as their Online card says it (ADDRESS, or ADDRESS:PORT).",
        330.0f, 36.0f, [&] {
        if (ui::text_field("##online_address", online_address_, sizeof(online_address_), online.codes ? "Code or address" : "The host's address", 190.0f)) {
            prefs_.set("online", "address", online_address_);
            prefs_.save();
        }
        ImGui::SameLine(0.0f, px(10));
        std::string address = online_address_;
        address.erase(0, address.find_first_not_of(" \t"));
        address.erase(address.find_last_not_of(" \t") + 1);
        if (ui::button("Join", Kind::Secondary, 130.0f, ready && !address.empty())) {
            play(which, join_arguments(online, address));
        }
    });
    card.end();
    // (a launcher of one game: its invitations and waiting room here, as a collection's Online page has them)
    if (!collection_ && room_url()) {
        ui::gap(24);
        invitations_card(width);
        room_card(width, {which});
    }
    if (lobby) {
        lobby_card(which, width, ready);
    }
}

const char* App::room_url() const {
    return collection_ ? collection_->room : about_.online.room;
}

int App::room_protocol() const {
    return collection_ ? collection_->room_protocol : about_.online.room_protocol;
}

std::string App::room_game(size_t which) const {
    return collection_ || !about_.online.game ? slots_[which]->id : about_.online.game;
}

std::vector<std::string> App::host_arguments(const launcher::Online& online) const {
    std::vector<std::string> args = {online.host_argument};
    // (a match is in the lobby when the player has a name for it to be listed under; without one, only its code joins it)
    // (the name: the game shows it to the other player as well)
    if (online.name_argument && online_name_[0]) {
        if (online.lobby && online.list_argument) {
            args.push_back(online.list_argument);
        }
        args.push_back(online.name_argument);
        args.push_back(online_name_);
    }
    if (online.arcade_argument && prefs_.get("online", "arcade") == "1") {
        args.push_back(online.arcade_argument);
    }
    return args;
}

std::vector<std::string> App::join_arguments(const launcher::Online& online, const std::string& match) const {
    std::vector<std::string> args = {online.join_argument, match};
    if (online.name_argument && online_name_[0]) {
        args.push_back(online.name_argument);
        args.push_back(online_name_);
    }
    return args;
}

void App::arcade_row(float width, const launcher::Online& online) {
    if (!online.arcade_argument) {
        return;
    }
    inset_separator(width);
    const bool on = prefs_.get("online", "arcade") == "1";
    row(width, "Arcade while waiting",
        "Play the game on your own until someone joins. They come into the game as it is, and press Start to challenge you.",
        42.0f, 24.0f, [&] {
            if (ui::toggle("##online_arcade", on)) {
                prefs_.set("online", "arcade", on ? "0" : "1");
                prefs_.save();
            }
        });
}

void App::load_online() {
    if (online_loaded_) {
        return;
    }
    online_loaded_ = true;
    std::snprintf(online_address_, sizeof(online_address_), "%s", prefs_.get("online", "address").c_str());
    std::snprintf(online_name_, sizeof(online_name_), "%s", prefs_.get("online", "name").c_str());
    for (const launcher::LocalAddress& here : launcher::local_addresses()) {
        online_here_.push_back(here.kind.empty() ? here.address : here.address + " (" + here.kind + ")");
    }
}

Uint64 App::online_every() const {
    return (SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS) ? 10000 : 20000;
}

void App::live_label(const char* text, float width, bool fetching) {
    const ui::Palette& p = ui::palette();
    const char* every = "Updates every 10 seconds";
    const ImVec2 top = ImGui::GetCursorScreenPos();
    const ImVec2 size = ui::text_size(every, Font::Regular, ui::size::kDetail);
    ImGui::SetCursorScreenPos(ImVec2(top.x + width - size.x - px(4), top.y));
    ui::text(every, Font::Regular, ui::size::kDetail, p.tertiary);
    if (fetching) {
        ui::spinner(ImVec2(top.x + width - size.x - px(16), top.y + size.y * 0.5f), px(5), p.tertiary);
    }
    ImGui::SetCursorScreenPos(top);
    layout::label(text);
}

void App::empty_row(float width, const char* text, bool looking) {
    const ui::Palette& p = ui::palette();
    row(width, text, "", looking ? 24.0f : 0.0f, 24.0f, [&] {
        if (looking) {
            const ImVec2 at = ImGui::GetCursorScreenPos();
            ui::spinner(at + ImVec2(px(12), px(12)), px(7), p.secondary);
        }
    });
}

void App::follow_lobby(const std::string& url) {
    const Uint64 now = SDL_GetTicks();
    const bool online_page = collection_ && collection_->lobby && url == collection_->lobby;
    if (lobby_fetch_.valid() && lobby_fetch_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        auto [ok, matches] = lobby_fetch_.get();
        lobby_failed_ = !ok;
        if (ok) {
            lobby_ = std::move(matches);
        }
        lobby_at_ = now;
    }
    if (lobby_url_ != url) {
        // (another list: the one being fetched is for the last, and is let go when it comes)
        lobby_url_ = url;
        lobby_.clear();
        lobby_failed_ = false;
        lobby_at_ = 0;
        if (online_page && room_in_) {
            room_at_ = 0; // (the waiting room's check-in, which brings it, now)
        }
        if (lobby_fetch_.valid()) {
            return;
        }
    }
    if (online_page && room_in_ && room_lists_) {
        return; // (follow_room's check-in brings it)
    }
    if (!lobby_fetch_.valid() && (lobby_at_ == 0 || now - lobby_at_ > online_every())) {
        lobby_fetch_ = std::async(std::launch::async, [url] {
            std::vector<launcher::OpenMatch> matches;
            const bool ok = launcher::fetch_lobby(url, matches);
            return std::make_pair(ok, matches);
        });
    }
}

const char* App::own_version() {
#ifdef LAUNCHER_VERSION
    return LAUNCHER_VERSION;
#else
    return "";
#endif
}

bool App::version_matches(const std::string& version) {
    // (a launcher that knows no version of its own, or a match that says none, can't tell: let the game's handshake)
    return version.empty() || !*own_version() || version == own_version();
}

bool App::can_play(size_t which) const {
    const Slot& one = *slots_[which];
    bool usable;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        usable = one.file.usable();
    }
    return !one.checking && usable && one.has_program() && !one.game.running();
}

// The Online page: host in the lobby in any of the games, and every game's open matches, each to join
void App::online_page(float width) {
    const ui::Palette& p = ui::palette();
    load_online();
    layout::heading("Online", collection_->online_intro, width);
    // (the games that can be played now, in the sidebar's order, for hosting; a game's Online, for its arguments)
    std::vector<size_t> playable;
    for (size_t which : titled()) {
        if (can_play(which) && slots_[which]->about->online.host_argument) {
            playable.push_back(which);
        }
    }
#ifdef LAUNCHER_UPDATE_REPO
    // A new version: said first, as both players of a match need the same one (the Games page has the update)
    if (update_news()) {
        const launcher::Updater::Status status = updater_->status();
        ui::text("NEW VERSION", Font::Semibold, ui::size::kDetail, p.accent);
        ui::gap(10);
        Card news(width, true);
        const std::string title = TrFormat("Version {} is out", {status.version});
        row(width, title.c_str(), "Both players of a match need the same version. Update to play the players who have the new one.", 130.0f,
            36.0f, [&] {
                if (ui::button("Update…", Kind::Primary, 130.0f)) {
                    page_ = 0;  // (the Games page: the update card)
                }
            });
        news.end();
        ui::gap(24);
    }
#endif
    if (collection_->room) {
        invitations_card(width);
    }
    // Every game's open matches, by game, in the sidebar's order; highlighted while one can be joined
    follow_lobby(collection_->lobby);
    const auto joinable = [&](const launcher::OpenMatch& match, size_t which) {
        std::lock_guard<std::mutex> lock(mutex_);
        return slots_[which]->id == match.game && version_matches(match.version) && !slots_[which]->checking && slots_[which]->file.usable();
    };
    bool any_joinable = false;
    for (const launcher::OpenMatch& match : lobby_) {
        for (size_t which = 0; which < slots_.size(); which++) {
            any_joinable = any_joinable || joinable(match, which);
        }
    }
    live_label("OPEN MATCHES", width, lobby_fetch_.valid() || (room_in_ && room_lists_ && room_fetch_.valid()));
    Card list(width, any_joinable);
    if (lobby_.empty()) {
        const char* empty = lobby_failed_    ? "Couldn't reach the online service."
                            : lobby_at_ == 0 ? "Looking…"
                                             : "None right now. They show here as players host them, up to 10 seconds after.";
        empty_row(width, empty, !lobby_failed_ && lobby_at_ == 0);
    }
    // (the games of the collection's in the sidebar's order, then those it hasn't: another version's games, say)
    bool first = true;
    int n = 0;
    const auto waited = [](const launcher::OpenMatch& match) {
        const int minutes = match.waiting / 60;
        return minutes < 1 ? Tr("waiting for a player") : TrFormat("waiting {} min", {std::to_string(minutes)});
    };
    for (size_t which : order_) {
        const Slot& one = *slots_[which];
        for (const launcher::OpenMatch& match : lobby_) {
            if (match.game != one.id) {
                continue;
            }
            if (!first) {
                inset_separator(width);
            }
            first = false;
            // (a game whose file isn't chosen yet: greyed, and Add…, which chooses it as the Games page's Choose… does: it has
            // no page of its own until then)
            bool file;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                file = !one.checking && one.file.usable();
            }
            const bool ready = can_play(which);
            // (a match on another version can't be joined: both need the same)
            const bool other = !version_matches(match.version);
            std::string detail = one.name + ": " + waited(match) + (match.version.empty() ? "" : TrFormat(". Version {}", {match.version})) +
                                 (other ? TrFormat(" (you have {}): both need the same version to play.", {own_version()})
                                  : file ? ""
                                         : TrFormat(". Add your {} {} to join.", {one.name, Tr(collection_->file_name)}));
            ImGui::PushID(n++);
            row(
                width, match.name.c_str(), detail.c_str(), 130.0f, 36.0f,
                [&] {
                    if (other) {
                        ui::button("Join", Kind::Secondary, 130.0f, false);
                    } else if (!file) {
                        if (ui::button("Add…", Kind::Secondary, 130.0f, !one.checking)) {
                            // (the system's file dialog wants a mouse or a keyboard)
                            if (ui::gamepad_in_use()) {
                                open_browser(which, false);
                            } else {
                                choose_game_file(which);
                            }
                        }
                    } else if (ui::button("Join", Kind::Primary, 130.0f, ready)) {
                        play(which, join_arguments(one.about->online, match.code));
                    }
                },
                !file || other);
            ImGui::PopID();
        }
    }
    for (const launcher::OpenMatch& match : lobby_) {
        if (std::any_of(slots_.begin(), slots_.end(), [&](const auto& one) { return one->id == match.game; })) {
            continue;
        }
        if (!first) {
            inset_separator(width);
        }
        first = false;
        // (a game a newer version has: its name as its host's game gave it; the Games page has the updates)
        std::string detail = (match.title.empty() ? match.game : match.title) + ": " + waited(match) +
#ifdef LAUNCHER_UPDATE_REPO
                             TrFormat(". Not in your version of {}: update to play it.", {LAUNCHER_GAME_NAME});
#else
                             TrFormat(". Not in your version of {}.", {LAUNCHER_GAME_NAME});
#endif
        ImGui::PushID(n++);
        row(
            width, match.name.c_str(), detail.c_str(), 130.0f, 36.0f,
            [&] {
#ifdef LAUNCHER_UPDATE_REPO
                if (ui::button("Update…", Kind::Secondary, 130.0f)) {
                    page_ = 0;  // (the Games page: the update card)
                }
#endif
            },
            true);
        ImGui::PopID();
    }
    list.end();
    // Join with a code: which game it is for, asked of the service, and that game started with it
    if (collection_->code_lookup) {
        if (join_lookup_.valid() && join_lookup_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            const launcher::CodeLookup found = join_lookup_.get();
            join_needs_ = -1;
            join_said_.clear();
            size_t which = slots_.size();
            for (size_t n = 0; n < slots_.size(); n++) {
                if (slots_[n]->id == found.game) {
                    which = n;
                }
            }
            if (found.status == 200 && !version_matches(found.version)) {
                join_said_ = TrFormat("That match is on version {}, and you have {}: both need the same version.", {found.version, own_version()});
            } else if (found.status == 200 && which < slots_.size() && can_play(which)) {
                play(which, join_arguments(slots_[which]->about->online, join_code_));
            } else if (found.status == 200 && which < slots_.size()) {
                join_said_ = TrFormat("That's a {} match: add your {} to join it.", {slots_[which]->name, Tr(collection_->file_name)});
                join_needs_ = static_cast<int>(which);
            } else if (found.status == 200) {
                join_said_ = TrFormat("That's a {} match, which your version of {} hasn't: update to play it.", {found.title, LAUNCHER_GAME_NAME});
            } else if (found.status == 404) {
                join_said_ = "There's no match with that code. Codes last 30 minutes, and each is good for one match.";
            } else if (found.status != 0) {
                join_said_ = found.error.empty() ? "That match can't be joined." : found.error;
            } else {
                join_said_ = "Couldn't reach the online service.";
            }
        }
        ui::gap(24);
        layout::label("JOIN WITH A CODE");
        Card join(width);
        // (its letters and digits, and dashes: it goes at the end of a URL)
        std::string code;
        for (const char* c = join_code_; *c; c++) {
            if (std::isalnum(static_cast<unsigned char>(*c)) || *c == '-') {
                code += *c;
            }
        }
        const bool asking = join_lookup_.valid();
        row(width, "Code", join_said_.empty() ? "The code a host gave you, for a match in any of the games." : join_said_.c_str(), 330.0f, 36.0f,
            [&] {
                if (ui::text_field("##join_code", join_code_, sizeof(join_code_), "The host's code", 190.0f)) {
                    join_said_.clear();
                    join_needs_ = -1;
                }
                ImGui::SameLine(0.0f, px(10));
                if (join_needs_ >= 0) {
                    if (ui::button("Add…", Kind::Secondary, 130.0f)) {
                        if (ui::gamepad_in_use()) {
                            open_browser(static_cast<size_t>(join_needs_), false);
                        } else {
                            choose_game_file(static_cast<size_t>(join_needs_));
                        }
                    }
                } else if (ui::button(asking ? "Joining…" : "Join", Kind::Secondary, 130.0f, !asking && !code.empty())) {
                    join_lookup_ = std::async(std::launch::async, [url = std::string(collection_->code_lookup) + code] { return launcher::lookup_code(url); });
                }
            });
        join.end();
    }
    if (collection_->room) {
        room_card(width, playable);
    }
    ui::gap(24);
    layout::label("HOST A MATCH");
    Card host(width);
    row(width, "Your name",
        collection_->room ? "With a name, the matches you host are listed in the lobby under it, and you're in the waiting room below. Without "
                            "one, your matches are joined only by their code."
                          : "With a name, the matches you host are listed in the lobby under it. Without one, they're joined only by their code.",
        190.0f, 36.0f, [&] {
        if (ui::text_field("##online_page_name", online_name_, sizeof(online_name_), "Your name", 190.0f)) {
            prefs_.set("online", "name", online_name_);
            prefs_.save();
        }
    });
    inset_separator(width);
    if (playable.empty()) {
        row(width, "No game to host yet", "Choose a game's ROM set on its page first.", 0.0f, 24.0f, [] {});
    } else {
        std::vector<std::string> names;
        int chosen = 0;
        const std::string last = prefs_.get("online", "game");
        for (size_t n = 0; n < playable.size(); n++) {
            names.push_back(slots_[playable[n]]->name);
            if (slots_[playable[n]]->id == last) {
                chosen = static_cast<int>(n);
            }
        }
        const size_t which = playable[static_cast<size_t>(chosen)];
        const launcher::Online& online = slots_[which]->about->online;
        row(width, "Host a match", "In the game chosen. The game shows the match's code to give the other player.", 400.0f,
            36.0f, [&] {
                if (ui::dropdown("##online_game", names, chosen, 260.0f)) {
                    prefs_.set("online", "game", slots_[playable[static_cast<size_t>(chosen)]]->id);
                    prefs_.save();
                }
                ImGui::SameLine(0.0f, px(10));
                if (ui::button("Host", Kind::Primary, 130.0f)) {
                    play(which, host_arguments(online));
                }
            });
        arcade_row(width, online);
    }
    host.end();
    // The features about a match (Collection::online_features), as the Features page has them: set for every game
    if (collection_->online_features) {
        std::vector<FeatureRow> rows;
        for (size_t i = 0; i < shared_.size(); i++) {
            const Shared& shared = shared_[i];
            if (shared.info.category == collection_->online_features && !shared.where.empty()) {
                const auto& [which, index] = shared.where[0];
                rows.push_back({&shared.info, recomp::FeatureValues(slots_[which]->settings, slots_[which]->features)[index], "", i});
            }
        }
        if (!rows.empty()) {
            ui::gap(24);
            layout::label("IN A MATCH");
            feature_list(
                width, rows, "", [&](const FeatureRow& feature, int value) { set_shared(feature.index, value); },
                [&](const FeatureRow& feature) {
                    const auto& [which, index] = shared_[feature.index].where[0];
                    return recomp::MenuChoices(slots_[which]->settings, slots_[which]->features[index]);
                },
                [&](const FeatureRow& feature, const std::vector<bool>& offered) {
                    for (const auto& [which, index] : shared_[feature.index].where) {
                        set_menu_choices(*slots_[which], index, offered);
                    }
                },
                false);
        }
    }
    (void)p;
}

void App::follow_room(bool here) {
    const std::string url = room_url();
    if (room_fetch_.valid() && room_fetch_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        launcher::Room room = room_fetch_.get();
        room_at_ = SDL_GetTicks();
        if (!room_in_) {
            // (left while it was asked: out again)
            if (room.ok) {
                launcher::room_leave(url, prefs_.get("online", "room"));
            }
        } else if (room.ok) {
            // (the invitations answered here and no longer listed are forgotten)
            room_answered_.erase(std::remove_if(room_answered_.begin(), room_answered_.end(),
                                                [&](const std::string& code) {
                                                    return std::none_of(room.invitations.begin(), room.invitations.end(),
                                                                        [&](const auto& i) { return i.code == code; });
                                                }),
                                 room_answered_.end());
            // (the Online page's lobby list, with it: shown if that page's list is the one followed)
            room_lists_ = room.listed;
            if (room.listed && collection_ && collection_->lobby && lobby_url_ == collection_->lobby) {
                lobby_ = std::move(room.matches);
                lobby_failed_ = false;
                lobby_at_ = room_at_;
            }
            room_ = std::move(room);
        } else {
            room_.ok = false;
            if (room_lists_ && collection_ && collection_->lobby && lobby_url_ == collection_->lobby) {
                lobby_failed_ = true;
                lobby_at_ = room_at_;
            }
        }
    }
    if (!here) {
        if (room_in_) {
            room_in_ = false;
            room_ = {};
            room_at_ = 0;
            if (!room_fetch_.valid()) {
                launcher::room_leave(url, prefs_.get("online", "room"));
            }
        }
        return;
    }
    room_in_ = true;
    if (room_fetch_.valid() || (room_at_ != 0 && SDL_GetTicks() - room_at_ < online_every())) {
        return;
    }
    // (who this player is: a secret of their own, kept, from which the service makes their id)
    if (prefs_.get("online", "room").size() != 32) {
        prefs_.set("online", "room", launcher::new_room_secret());
        prefs_.save();
    }
    launcher::RoomCheckIn me;
    me.secret = prefs_.get("online", "room");
    me.name = online_name_;
    for (size_t which = 0; which < slots_.size(); which++) {
        if (can_play(which) || (slots_[which]->game.running())) {
            me.games.push_back(room_game(which));
        }
    }
    me.protocol = room_protocol();
    me.version = own_version();
    me.invitable = prefs_.get("online", "invitable") != "0";
    me.lobby = collection_ && collection_->lobby;
    const std::string blocked = prefs_.get("online", "blocked");
    for (size_t at = 0; at < blocked.size();) {
        const size_t comma = std::min(blocked.find(',', at), blocked.size());
        if (comma > at) {
            me.blocked.push_back(blocked.substr(at, comma - at));
        }
        at = comma + 1;
    }
    room_fetch_ = std::async(std::launch::async, [url, me] { return launcher::room_check_in(url, me); });
}

void App::block(const std::string& id) {
    std::string blocked = prefs_.get("online", "blocked");
    if (id.empty() || (',' + blocked + ',').find(',' + id + ',') != std::string::npos) {
        return;
    }
    prefs_.set("online", "blocked", blocked.empty() ? id : blocked + "," + id);
    prefs_.save();
    // (gone from here at once; the service's next answer leaves them out too)
    room_.players.erase(std::remove_if(room_.players.begin(), room_.players.end(), [&](const launcher::RoomPlayer& player) { return player.id == id; }),
                        room_.players.end());
    for (const launcher::Invitation& invitation : room_.invitations) {
        if (invitation.from == id) {
            launcher::room_decline(room_url(), prefs_.get("online", "room"), invitation.code);
            room_answered_.push_back(invitation.code);
        }
    }
    room_at_ = 0;
}

std::vector<std::string> App::invite_arguments(const launcher::Online& online, const launcher::RoomPlayer& player) const {
    // (the match isn't listed: it is for the player invited, or anyone given its code)
    std::vector<std::string> args = {online.host_argument, online.name_argument, online_name_, online.invite_argument, player.id + ":" + room_.id};
    if (online.invite_name_argument) {
        args.push_back(online.invite_name_argument);
        args.push_back(player.name);
    }
    if (online.arcade_argument && prefs_.get("online", "arcade") == "1") {
        args.push_back(online.arcade_argument);
    }
    return args;
}

// The invitations waiting for this player, each from a player in the waiting room to a match in one of the games
void App::invitations_card(float width) {
    std::vector<const launcher::Invitation*> shown;
    for (const launcher::Invitation& invitation : room_.invitations) {
        if (std::find(room_answered_.begin(), room_answered_.end(), invitation.code) == room_answered_.end()) {
            shown.push_back(&invitation);
        }
    }
    if (shown.empty()) {
        return;
    }
    ui::text("INVITATIONS", Font::Semibold, ui::size::kDetail, ui::palette().accent);
    ui::gap(10);
    Card card(width, true);
    int n = 0;
    for (const launcher::Invitation* invitation : shown) {
        if (n > 0) {
            inset_separator(width);
        }
        size_t which = slots_.size();
        for (size_t i = 0; i < slots_.size(); i++) {
            if (room_game(i) == invitation->game) {
                which = i;
            }
        }
        const bool known = which < slots_.size();
        bool file = false;
        if (known) {
            std::lock_guard<std::mutex> lock(mutex_);
            file = !slots_[which]->checking && slots_[which]->file.usable();
        }
        const std::string game = known ? slots_[which]->name : invitation->title;
        const std::string title = TrFormat("{} invites you to {}", {invitation->name, game});
        const std::string detail = !known ? TrFormat("Your version of {} hasn't {}: update to play it.", {LAUNCHER_GAME_NAME, game})
                                   : file ? Tr("Accept starts the game and joins their match.")
                                          : TrFormat("Add your {} {} to accept.", {game, Tr(collection_ ? collection_->file_name : "game files")});
        ImGui::PushID(n++);
        row(width, title.c_str(), detail.c_str(), 370.0f, 36.0f, [&] {
            if (known && file) {
                if (ui::button("Accept", Kind::Primary, 130.0f, can_play(which))) {
                    room_answered_.push_back(invitation->code);
                    play(which, join_arguments(slots_[which]->about->online, invitation->code));
                }
            } else if (known) {
                if (ui::button("Add…", Kind::Secondary, 130.0f, !slots_[which]->checking)) {
                    if (ui::gamepad_in_use()) {
                        open_browser(which, false);
                    } else {
                        choose_game_file(which);
                    }
                }
            } else {
                ui::button("Accept", Kind::Primary, 130.0f, false);
            }
            ImGui::SameLine(0.0f, px(10));
            if (ui::button("Decline", Kind::Secondary, 120.0f)) {
                room_answered_.push_back(invitation->code);
                launcher::room_decline(room_url(), prefs_.get("online", "room"), invitation->code);
            }
            ImGui::SameLine(0.0f, px(10));
            if (ui::button("Block", Kind::Secondary, 100.0f)) {
                block(invitation->from);
            }
        });
        ImGui::PopID();
    }
    card.end();
    ui::gap(24);
}

// The waiting room: the other players whose Online page is open, each to invite to a match in a game chosen here
void App::room_card(float width, const std::vector<size_t>& playable) {
    ui::gap(24);
    live_label("WAITING ROOM", width, room_fetch_.valid());
    Card card(width, !room_.players.empty());  // (highlighted while there's someone to invite)
    const bool named = online_name_[0] != '\0';
    const bool invitable = prefs_.get("online", "invitable") != "0";
    // (the game to invite to: one this player can play; a launcher of one game, that one)
    size_t which = collection_ ? slots_.size() : 0;
    if (collection_ && !playable.empty()) {
        std::vector<std::string> names;
        int chosen = 0;
        const std::string last = prefs_.get("online", "invite_game").empty() ? prefs_.get("online", "game") : prefs_.get("online", "invite_game");
        for (size_t n = 0; n < playable.size(); n++) {
            names.push_back(slots_[playable[n]]->name);
            if (slots_[playable[n]]->id == last) {
                chosen = static_cast<int>(n);
            }
        }
        which = playable[static_cast<size_t>(chosen)];
        row(width, "Invite to", "The game a player you invite is asked to play. You host the match; they see your invitation in their launcher.",
            260.0f, 36.0f, [&] {
                if (ui::dropdown("##invite_game", names, chosen, 260.0f)) {
                    prefs_.set("online", "invite_game", slots_[playable[static_cast<size_t>(chosen)]]->id);
                    prefs_.save();
                }
            });
        inset_separator(width);
    }
    if (room_.players.empty()) {
        const char* empty = room_at_ == 0 ? "Looking…"
                            : !room_.ok   ? "Couldn't reach the online service."
                            : collection_ ? "Nobody else is here right now. Players show here while their Online page is open, up to 10 seconds "
                                            "after they open it."
                                          : "Nobody else is here right now. Players show here while their launcher's Game page is open, up "
                                            "to 10 seconds after they open it.";
        empty_row(width, empty, room_at_ == 0);
    }
    for (size_t n = 0; n < room_.players.size(); n++) {
        const launcher::RoomPlayer& player = room_.players[n];
        if (n > 0) {
            inset_separator(width);
        }
        std::string detail;
        if (which < slots_.size()) {
            const std::string& game = slots_[which]->name;
            detail = std::find(player.games.begin(), player.games.end(), room_game(which)) != player.games.end()
                         ? TrFormat("Has {}.", {game})
                         : TrFormat("Hasn't added {} yet: they can when invited.", {game});
        }
        if (!named) {
            detail += std::string(detail.empty() ? "" : " ") + Tr("Give yourself a name above to invite.");
        }
        const launcher::Online* online = which < slots_.size() ? &slots_[which]->about->online : nullptr;
        ImGui::PushID(static_cast<int>(n));
        row(width, player.name.c_str(), detail.c_str(), 240.0f, 36.0f, [&] {
            if (ui::button("Invite", Kind::Secondary, 130.0f, named && online && online->invite_argument && !room_.id.empty() && can_play(which))) {
                play(which, invite_arguments(*online, player));
            }
            ImGui::SameLine(0.0f, px(10));
            if (ui::button("Block", Kind::Secondary, 100.0f)) {
                block(player.id);
            }
        });
        ImGui::PopID();
    }
    inset_separator(width);
    row(width, "Let others invite me",
        (named ? TrFormat("While your {} page is open, you're in the waiting room under your name, and others can invite you to a match. "
                          "An invitation never interrupts a game: it waits here.",
                          {Tr(collection_ ? "Online" : "Game")})
               : TrFormat("With a name, you're in the waiting room while your {} page is open, and others can invite you to a match.",
                          {Tr(collection_ ? "Online" : "Game")}))
            .c_str(),
        42.0f, 24.0f, [&] {
            if (ui::toggle("##online_invitable", invitable)) {
                prefs_.set("online", "invitable", invitable ? "0" : "1");
                prefs_.save();
                room_at_ = 0;  // (said at once)
            }
        });
    const std::string blocked = prefs_.get("online", "blocked");
    if (!blocked.empty()) {
        inset_separator(width);
        const size_t count = static_cast<size_t>(std::count(blocked.begin(), blocked.end(), ',')) + 1;
        const std::string title = count == 1 ? Tr("1 player blocked") : TrFormat("{} players blocked", {std::to_string(count)});
        row(width, title.c_str(), "You see nothing of a player you've blocked, nor they of you.", 130.0f, 36.0f, [&] {
            if (ui::button("Unblock all", Kind::Secondary, 130.0f)) {
                prefs_.set("online", "blocked", "");
                prefs_.save();
                room_at_ = 0;
            }
        });
    }
    card.end();
}

// The lobby's open matches for the game, fetched every 10 seconds while they are shown
void App::lobby_card(size_t which, float width, bool ready) {
    const ui::Palette& p = ui::palette();
    Slot& one = *slots_[which];
    const launcher::Online& online = one.about->online;
    follow_lobby(online.lobby);
    ui::gap(24);
    live_label("OPEN MATCHES", width, lobby_fetch_.valid());
    ui::text("Matches players are hosting in the lobby now. Join one to play its host.", Font::Regular, ui::size::kDetail, p.secondary, width);
    ui::gap(10);
    Card card(width);
    if (lobby_.empty()) {
        const char* empty = lobby_failed_       ? "Couldn't reach the online service."
                            : lobby_at_ == 0    ? "Looking…"
                                                : "None right now. They show here as players host them in the lobby, up to 10 seconds after.";
        empty_row(width, empty, !lobby_failed_ && lobby_at_ == 0);
    }
    for (size_t n = 0; n < lobby_.size(); n++) {
        const launcher::OpenMatch& match = lobby_[n];
        if (n > 0) {
            inset_separator(width);
        }
        const int minutes = match.waiting / 60;
        std::string waited = minutes < 1 ? Tr("Waiting for a player") : TrFormat("Waiting {} min", {std::to_string(minutes)});
        const bool other = !version_matches(match.version);
        if (!match.version.empty()) {
            waited += TrFormat(". Version {}", {match.version}) +
                      (other ? TrFormat(" (you have {}): both need the same version.", {own_version()}) : "");
        }
        ImGui::PushID(static_cast<int>(n));
        row(
            width, match.name.c_str(), waited.c_str(), 130.0f, 36.0f,
            [&] {
                if (ui::button("Join", Kind::Secondary, 130.0f, ready && !other)) {
                    play(which, join_arguments(online, match.code));
                }
            },
            other);
        ImGui::PopID();
    }
    card.end();
}

void App::game_page(float width) {
    layout::heading(LAUNCHER_GAME_NAME, about_.file_intro, width);
    bool ready = play_button(0, width);
    ui::gap(28);
    notice_card(width);
#ifdef LAUNCHER_UPDATE_REPO
    update_card(width, true, true);  // (at the top, new version or not, as a collection's Games page has it)
    ui::gap(28);
#endif
    file_card(0, width);
    display_cards(0, width);
    play_cards(0, width, ready);
    language_card(width);
    files_card(width);
}

// The language of the launcher and the games' menus (i18n.h): the system's, or one chosen
void App::language_card(float width) {
    const ui::Palette& p = ui::palette();
    ui::gap(36);
    ui::text("LANGUAGE", Font::Semibold, ui::size::kDetail, p.secondary);
    ui::gap(10);
    Card card(width);
    std::vector<std::string> names = {"The system's"};
    std::vector<std::string> codes = {""};
    for (const recomp::i18n::Language& language : recomp::i18n::kLanguages) {
        names.push_back(language.name);
        codes.push_back(language.code);
    }
    const std::string chosen = prefs_.get("launcher", "language");
    int index = 0;
    for (size_t i = 0; i < codes.size(); i++) {
        index = codes[i] == chosen ? static_cast<int>(i) : index;
    }
    row(width, "Language", "The launcher's words and the in-game menu's, in every game. The games' own words stay as they are.", 220.0f,
        36.0f, [&] {
            if (ui::dropdown("##language", names, index, 220.0f)) {
                prefs_.set("launcher", "language", codes[static_cast<size_t>(index)]);
                prefs_.save();
                use_language(prefs_);
            }
        });
    card.end();
}

// The port's notice (Description::notice), highlighted as an invitation is, its button opening the port's page
void App::notice_card(float width) {
    if (!about_.notice) {
        return;
    }
    const launcher::Notice notice = about_.notice(slot().folder);
    if (notice.title.empty()) {
        return;
    }
    int place = -1;
    const std::vector<int> layout = single_layout();
    for (size_t i = 0; i < layout.size(); i++) {
        if (layout[i] >= 0 && about_.pages[static_cast<size_t>(layout[i])].name == notice.page) {
            place = static_cast<int>(i);
        }
    }
    ui::text(notice.heading.c_str(), Font::Semibold, ui::size::kDetail, ui::palette().accent);
    ui::gap(10);
    Card card(width, true);
    const bool button = !notice.button.empty() && place >= 0;
    row(width, notice.title.c_str(), notice.detail.c_str(), button ? 150.0f : 0.0f, 36.0f, [&] {
        if (button && ui::button((notice.button + "##notice").c_str(), Kind::Primary, 150.0f)) {
            page_ = place;
        }
    });
    card.end();
    ui::gap(28);
}

// A launcher of one game's Online page (Description::online_page): what the Game page's Online card has, on a page
void App::single_online_page(float width) {
    layout::heading("Online", about_.online.detail, width);
    bool ready;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ready = !slots_[0]->checking && slots_[0]->file.usable();
    }
    ready = ready && slots_[0]->has_program() && !slots_[0]->game.running();
    online_card(0, width, ready);
}

// A collection's first page: every game, its file (or where to find them all), and Play
void App::library_page(float width) {
    const ui::Palette& p = ui::palette();
    layout::heading("Games", collection_->intro, width);
#ifdef LAUNCHER_UPDATE_REPO
    update_card(width, true, true);  // (above the games, new version or not)
#endif
    ui::text("FIND YOUR GAMES", Font::Semibold, ui::size::kDetail, p.secondary);
    ui::gap(10);
    Card find(width);
    std::string detail = finding_ ? std::string("Looking…") : find_message_.empty() ? std::string(collection_->find_detail) : find_message_;
    row(width, collection_->find_title, detail.c_str(), 150.0f, 36.0f, [&] {
#ifdef __ANDROID__
        // (Android: the sets themselves are chosen, and copied into the launcher's folder: choose_folder)
        const char* label = "Add ROM sets…";
        const bool browse = false;
#else
        const char* label = "Choose folder…";
        const bool browse = ui::gamepad_in_use();
#endif
        if (ui::button(label, Kind::Secondary, 150.0f, !finding_)) {
            if (browse) {
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
    // Only the games the player has, for one who plays one or two of them (once one is set up)
    if (!titled().empty()) {
        const bool only = installed_only();
        size_t hidden = 0;
        for (size_t which = 0; which < slots_.size(); which++) {
            hidden += slots_[which]->has_program() && !shown(which);
        }
        std::string detail = !only ? std::string("Leave out the games you have not set up, here and on the Features and Controllers pages.")
                             : hidden == 0 ? std::string("Every game is set up.")
                             : hidden == 1 ? Tr("1 game you have not set up is left out. Choose folder… above still finds them.")
                                           : TrFormat("{} games you have not set up are left out. Choose folder… above still finds them.",
                                                      {std::to_string(hidden)});
        row(width, "Only the games you have", detail.c_str(), 42.0f, 24.0f, [&] {
            if (ui::toggle("##installed-only", only)) {
                prefs_.set("games", "installed_only", only ? "0" : "1");
                prefs_.save();
            }
        });
        first = false;
    }
    // (a series' games under its name, where two or more of them are on the page)
    std::map<std::string, int> in_series;
    for (size_t which : order_) {
        in_series[slots_[which]->series] += shown(which) ? 1 : 0;
    }
    std::string series;
    for (size_t which : order_) {
        Slot& one = *slots_[which];
        if (!shown(which)) {
            continue;  // (not in this build, or not set up and left out)
        }
        if (!first) {
            inset_separator(width);
        }
        first = false;
        if (one.series != series && !one.series.empty() && in_series[one.series] > 1) {
            ImVec2 at = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(at + ImVec2(px(18), px(14)));
            std::string heading = one.series;
            std::transform(heading.begin(), heading.end(), heading.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            ui::text(heading.c_str(), Font::Semibold, ui::size::kSmall, p.secondary);
            ImGui::SetCursorScreenPos(ImVec2(at.x, ImGui::GetCursorScreenPos().y - px(8)));
        }
        series = one.series;
        FileCheck file;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            file = one.file;
        }
        bool usable = !one.checking && !one.file_path.empty() && file.usable();
        std::string status = one.checking ? std::string("Checking…") : one.file_path.empty() ? std::string(one.about->file_empty) : file.message;
        ImGui::PushID(static_cast<int>(which));
        ImVec2 row_top = ImGui::GetCursorScreenPos();
        // (Play first, so that the ring coming into the row from the left is on it)
        row(width, one.name.c_str(), status.c_str(), 230.0f, 36.0f, [&] {
            if (ui::button(one.game.running() ? "Playing…" : "Play", Kind::Primary, 110.0f, usable && !one.game.running())) {
                play(which);
            }
            ImGui::SameLine(0.0f, px(10));
            if (ui::button("Choose…", Kind::Secondary, 110.0f, !one.checking)) {
                if (ui::gamepad_in_use()) {
                    open_browser(which, false);
                } else {
                    choose_game_file(which);
                }
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
        if (one->play_error.empty() && !one->game.running() && one->game.exit_code() != 0) {
            stopped_note(*one, width);
            continue;
        }
        std::string text = !one->play_error.empty() ? one->name + ": " + one->play_error : std::string();
        if (!text.empty()) {
            ui::gap(10);
            ui::text(text.c_str(), Font::Regular, ui::size::kDetail, p.danger, width);
        }
    }
    language_card(width);
    files_card(width);
}

// Where the player's things are, and portable mode (folders.h)
void App::files_card(float width) {
    const ui::Palette& p = ui::palette();
    const std::filesystem::path portable = recomp::PortableFolder(program_folder_);
    const bool home = std::getenv(LAUNCHER_HOME_ENV) && *std::getenv(LAUNCHER_HOME_ENV);
    // (a collection's games each have a folder beside its own)
    const std::filesystem::path where = !portable.empty() && !home ? portable : folder_;
    ui::gap(36);
    ui::text("YOUR FILES", Font::Semibold, ui::size::kDetail, p.secondary);
    ui::gap(10);
    Card card(width);
#ifdef __ANDROID__
    // (no file manager to show it in: a computer sees it over USB)
    std::string shown = ui::fit_start(where.u8string(), Font::Regular, ui::size::kDetail, width - px(18 * 2));
    row(width, "Kept in the app's folder", shown.c_str(), 0.0f, 0.0f, [] {});
    card.end();
    ui::gap(10);
    ui::text("Settings, save states, each board's memory, the logs, and the ROM sets you added. A computer sees them over USB, in "
             "Android/data.",
             Font::Regular, ui::size::kDetail, p.secondary, width);
#else
    std::string shown = ui::fit_start(where.u8string(), Font::Regular, ui::size::kDetail, width - px(18 * 2 + 110 + 24));
    row(width, !portable.empty() && !home ? "Kept with the launcher (portable)" : "Kept in your user folder", shown.c_str(), 110.0f, 36.0f, [&] {
        if (ui::button("Show", Kind::Secondary, 110.0f) && interactive_) {  // (pictures for a test open nothing)
            SDL_OpenURL(file_url(where).c_str());
        }
    });
    card.end();
    ui::gap(10);
#ifdef __APPLE__
    const char* beside = "beside the app (in the folder it is in)";
#else
    const char* beside = "beside the launcher";
#endif
    const std::string about = !portable.empty() ? TrFormat("Settings, saves, save states and the logs. Take portable.txt away from {} to use "
                                                           "your user folder again; nothing is moved.",
                                                           {Tr(beside)})
                                                : TrFormat("Settings, saves, save states and the logs. To keep them with the launcher instead, "
                                                           "on a drive you carry, put a file named portable.txt {}.",
                                                           {Tr(beside)});
    ui::text(about.c_str(), Font::Regular, ui::size::kDetail, p.secondary, width);
#endif
}

#ifdef LAUNCHER_UPDATE_REPO
// The version, whether there is a newer one, and getting it. While there is one (or it is being installed) the card
// is at the top of the first page; otherwise at its end, with the switch for checking when the launcher opens (a
// collection's Games page has it at the top either way: `always`).
void App::update_card(float width, bool top, bool always) {
    using State = launcher::Updater::State;
    const ui::Palette& p = ui::palette();
    const launcher::Updater::Status status = updater_->status();
    const bool news = update_news();
    if (news != top && !always) {
        return;
    }
    if (!top) {
        ui::gap(36);
    }
    ui::text(news ? "NEW VERSION" : "UPDATES", Font::Semibold, ui::size::kDetail, news ? p.accent : p.secondary);
    ui::gap(10);
    Card card(width, news);
    const bool running = any_running();
    std::string title = status.state == State::Installed ? TrFormat("Version {}, installed", {status.version})
                        : news                            ? TrFormat("Version {} is out", {status.version})
                                                          : TrFormat("Version {}", {LAUNCHER_VERSION});
    std::string detail = status.state == State::Idle ? Tr("Not checked yet.") : Tr(status.message);
    if ((status.state == State::Available || status.state == State::Downloaded) && running) {
        detail += Tr(" Quit the games that are running first.");
    }
    row(width, title.c_str(), detail.c_str(), news ? 250.0f : 150.0f, 36.0f, [&] {
        switch (status.state) {
        case State::Available:
        case State::NotInstalled:
        case State::Downloaded:
            // (the release's page: what is new, and its downloads)
            if (ui::button(status.state == State::NotInstalled ? "Releases" : "What's new", Kind::Secondary, 120.0f, !status.notes_url.empty()) &&
                interactive_) {  // (pictures for a test open nothing)
                SDL_OpenURL(status.notes_url.c_str());
            }
            ImGui::SameLine(0.0f, px(10));
            if (status.state == State::NotInstalled && !status.download_url.empty()) {
                // (Android, a release without checksums: the new APK, which the browser downloads, for the system's installer)
                if (ui::button("Download", Kind::Primary, 120.0f) && interactive_) {
                    SDL_OpenURL(status.download_url.c_str());
                }
            } else if (status.state == State::Downloaded) {
                // (Android: the installer again, if the player said not now or had to allow it in Settings first)
                if (ui::button("Install", Kind::Primary, 120.0f, !running)) {
                    updater_->install();
                }
            } else if (ui::button("Update", Kind::Primary, 120.0f, status.state == State::Available && !running)) {
                updater_->install();
            }
            break;
        case State::Installing:
        case State::Installed:
            ImGui::SetCursorScreenPos(ImGui::GetCursorScreenPos() + ImVec2(0, px(15)));
            ui::progress_bar(ImHashStr("update-progress"), static_cast<float>(status.fraction), 250.0f, 6.0f);
            break;
        default:
            if (ui::button(status.state == State::Checking ? "Checking…###check" : "Check now###check", Kind::Secondary, 150.0f, !updater_->busy())) {
                updater_->check();
            }
            break;
        }
    });
    if (!news) {
        inset_separator(width);
        bool on = prefs_.get("update", "check") != "0";
        row(width, "Check when the launcher opens", "Asks GitHub for the list of releases, once each time the launcher opens. Nothing is downloaded until you choose Update.",
            42.0f, 24.0f, [&] {
                if (ui::toggle("##update-check", on)) {
                    prefs_.set("update", "check", on ? "0" : "1");
                    prefs_.save();
                }
            });
    }
    card.end();
    if (top) {
        ui::gap(24);
    }
}
#endif

bool App::update_news() const {
#ifdef LAUNCHER_UPDATE_REPO
    using State = launcher::Updater::State;
    const State state = updater_->status().state;
    return state == State::Available || state == State::Installing || state == State::Installed || state == State::NotInstalled ||
           state == State::Downloaded;
#else
    return false;
#endif
}

// A collection's page for one game: its file, Display and Sound, Play, and the features that are its alone
void App::title_page(size_t which, float width) {
    const ui::Palette& p = ui::palette();
    Slot& one = *slots_[which];
    layout::heading(one.name.c_str(), one.about->file_intro, width);
    bool ready = play_button(which, width);
    ui::gap(28);
    file_card(which, width);
    display_cards(which, width);
    play_cards(which, width, ready);
    // Its place in the sidebar's list (which can be dragged there too), or in its series'
    std::vector<std::vector<size_t>> places = units();
    std::vector<size_t> list;
    for (const std::vector<size_t>& unit : places) {
        if (grouped(unit) && std::find(unit.begin(), unit.end(), which) != unit.end()) {
            list = unit;
        }
    }
    bool in_series = !list.empty();
    if (!in_series) {
        for (const std::vector<size_t>& unit : places) {
            list.push_back(unit.front());
        }
    }
    if (list.size() > 1) {
        int place = static_cast<int>(std::find(list.begin(), list.end(), which) - list.begin());
        ui::gap(36);
        ui::text("YOUR GAMES", Font::Semibold, ui::size::kDetail, p.secondary);
        ui::gap(10);
        Card card(width);
        std::string detail =
            in_series ? TrFormat("{} of {} in {}. Its row there can be dragged up or down among them too.",
                                 {std::to_string(place + 1), std::to_string(list.size()), one.series})
                      : TrFormat("{} of {} in the sidebar. Its row there can be dragged up or down too.", {std::to_string(place + 1), std::to_string(list.size())});
        row(width, "Its place in the list", detail.c_str(), 230.0f, 36.0f, [&] {
            if (ui::button("Move up", Kind::Secondary, 110.0f, place > 0)) {
                move_game(which, place - 1);
            }
            ImGui::SameLine(0.0f, px(10));
            if (ui::button("Move down", Kind::Secondary, 110.0f, place + 1 < static_cast<int>(list.size()))) {
                move_game(which, place + 1);
            }
        });
        card.end();
    }
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
        if (!shared && feature_shown(one, i)) {
            rows.push_back({&one.features[i], values[i], "", i});
        }
    }
    feature_list(
        width, rows, "Nothing of its own: every feature it has is on the Features page.",
        [&](const FeatureRow& feature, int value) { set_feature(one, feature.index, value); },
        [&](const FeatureRow& feature) { return recomp::MenuChoices(one.settings, one.features[feature.index]); },
        [&](const FeatureRow& feature, const std::vector<bool>& offered) { set_menu_choices(one, feature.index, offered); });
}

// A series' page: its games, each to play or go to, and the series' place in the sidebar's list
void App::series_page(size_t which, float width) {
    const std::string series = slots_[which]->series;
    std::vector<std::vector<size_t>> places = units();
    size_t unit = 0;
    while (unit + 1 < places.size() && slots_[places[unit].front()]->series != series) {
        unit++;
    }
    const std::vector<size_t> games = places[unit];
    std::string intro = TrFormat("{} of your games are {} games. Play one here, or go to its page for its ROM set, its Display switches and "
                                 "what only it has.",
                                 {std::to_string(games.size()), series});
    layout::heading(series.c_str(), intro.c_str(), width);
    layout::label("ITS GAMES");
    Card card(width);
    for (size_t i = 0; i < games.size(); i++) {
        Slot& one = *slots_[games[i]];
        if (i > 0) {
            inset_separator(width);
        }
        ImGui::PushID(one.id.c_str());
        std::string status;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            status = one.file.message;
        }
        row(width, one.name.c_str(), status.c_str(), 230.0f, 36.0f, [&] {
            if (ui::button(one.game.running() ? "Playing…" : "Play", Kind::Primary, 110.0f, can_play(games[i]))) {
                play(games[i]);
            }
            ImGui::SameLine(0.0f, px(10));
            if (ui::button("Its page", Kind::Secondary, 110.0f)) {
                set_page(one.id);
            }
        });
        ImGui::PopID();
    }
    card.end();
    if (places.size() > 1) {
        ui::gap(36);
        layout::label("YOUR GAMES");
        Card place(width);
        std::string detail = TrFormat("{} of {} in the sidebar, its games with it. Its row there can be dragged up or down too.",
                                      {std::to_string(unit + 1), std::to_string(places.size())});
        row(width, "Its place in the list", detail.c_str(), 230.0f, 36.0f, [&] {
            if (ui::button("Move up", Kind::Secondary, 110.0f, unit > 0)) {
                move_unit(unit, static_cast<int>(unit) - 1);
            }
            ImGui::SameLine(0.0f, px(10));
            if (ui::button("Move down", Kind::Secondary, 110.0f, unit + 1 < places.size())) {
                move_unit(unit, static_cast<int>(unit) + 1);
            }
        });
        place.end();
    }
}

// ---- Multi game ------------------------------------------------------------------------------

int App::multi_value(size_t mode, size_t choice) const {
    const launcher::MultiMode& m = collection_->multi[mode];
    const launcher::MultiChoice& c = m.choices[choice];
    std::string value = prefs_.get(std::string("multi.") + m.key, c.key);
    int chosen = value.empty() ? c.initial : std::atoi(value.c_str());
    return std::clamp(chosen, 0, std::max(0, static_cast<int>(c.choices.size()) - 1));
}

std::vector<size_t> App::multi_games(size_t mode) const {
    const launcher::MultiMode& m = collection_->multi[mode];
    std::vector<size_t> list;
    for (size_t which : titled()) {
        if (std::any_of(m.games.begin(), m.games.end(), [&](const char* id) { return slots_[which]->id == id; })) {
            list.push_back(which);
        }
    }
    return list;
}

bool App::multi_chosen(size_t mode, size_t which) const {
    std::stringstream ids(prefs_.get(std::string("multi.") + collection_->multi[mode].key, "off"));
    for (std::string id; std::getline(ids, id, ',');) {
        if (id == slots_[which]->id) {
            return false;
        }
    }
    return true;
}

void App::set_multi_chosen(size_t mode, size_t which, bool chosen) {
    const std::string section = std::string("multi.") + collection_->multi[mode].key;
    std::stringstream ids(prefs_.get(section, "off"));
    std::string off;
    for (std::string id; std::getline(ids, id, ',');) {
        if (!id.empty() && id != slots_[which]->id) {
            off += (off.empty() ? "" : ",") + id;
        }
    }
    if (!chosen) {
        off += (off.empty() ? "" : ",") + slots_[which]->id;
    }
    prefs_.set(section, "off", off);
    prefs_.save();
}

bool App::multi_shuffled(size_t mode) const {
    return prefs_.get(std::string("multi.") + collection_->multi[mode].key, "order") == "1";
}

void App::start_multi(size_t mode) {
    multi_ = MultiRun();
    multi_message_.clear();
    for (size_t which : multi_games(mode)) {
        if (multi_chosen(mode, which)) {
            multi_.games.push_back(which);
        }
    }
    if (multi_.games.empty()) {
        return;
    }
    if (multi_shuffled(mode)) {
        std::shuffle(multi_.games.begin(), multi_.games.end(), std::mt19937(std::random_device()()));
    }
    multi_.mode = static_cast<int>(mode);
    multi_play_next();
}

bool App::multi_play_next() {
    const launcher::MultiMode& m = collection_->multi[static_cast<size_t>(multi_.mode)];
    std::vector<int> values;
    for (size_t choice = 0; choice < m.choices.size(); choice++) {
        values.push_back(multi_value(static_cast<size_t>(multi_.mode), choice));
    }
    std::vector<std::string> extra = m.arguments ? m.arguments(values) : std::vector<std::string>();
    // (a game that will not start, or is running already, is passed over)
    for (size_t tries = 0; tries < multi_.games.size(); tries++) {
        Slot& one = *slots_[multi_.games[multi_.at]];
        if (!one.game.running()) {
            play(multi_.games[multi_.at], extra);
            if (one.game.running()) {
                multi_.played++;
                return true;
            }
        }
        multi_.at = (multi_.at + 1) % multi_.games.size();
    }
    multi_message_ = "None of the games would start.";
    multi_.mode = -1;
    return false;
}

void App::multi_after(bool done) {
    Slot& one = *slots_[multi_.games[multi_.at]];
    if (!done) {
        int code = one.game.exit_code();
        multi_message_ = code == 0 ? TrFormat("{} was quit: the run is over.", {one.name})
                                   : TrFormat("{} stopped with code {}: the run is over.", {one.name, std::to_string(code)});
        multi_.mode = -1;
        return;
    }
    if (++multi_.at >= multi_.games.size()) {
        // (round again: shuffled anew, not starting with the game just played)
        multi_.at = 0;
        if (multi_shuffled(static_cast<size_t>(multi_.mode)) && multi_.games.size() > 1) {
            size_t last = multi_.games.back();
            std::mt19937 random(std::random_device{}());
            do {
                std::shuffle(multi_.games.begin(), multi_.games.end(), random);
            } while (multi_.games.front() == last);
        }
    }
    multi_play_next();
}

void App::multi_page(float width) {
    const ui::Palette& p = ui::palette();
    layout::heading("Multi game", collection_->multi_intro, width);
    if (multi_.mode >= 0) {
        const launcher::MultiMode& m = collection_->multi[static_cast<size_t>(multi_.mode)];
        layout::label("NOW PLAYING");
        Card card(width);
        const Slot& one = *slots_[multi_.games[multi_.at]];
        std::string detail = TrFormat("{}: game {} of {}", {one.name, std::to_string(multi_.at + 1), std::to_string(multi_.games.size())}) +
                             (multi_.played > multi_.games.size()
                                  ? TrFormat(" (round {})", {std::to_string((multi_.played - 1) / multi_.games.size() + 1)})
                                  : "") +
                             Tr(". Quit the game (its menu's Quit game, or close its window) to stop, or stop after this game.");
        row(width, m.title, detail.c_str(), 200.0f, 36.0f, [&] {
            if (ui::button("Stop after this game", Kind::Secondary, 200.0f)) {
                multi_.mode = -1;
                multi_message_ = TrFormat("Stopped: {} is the last.", {one.name});
            }
        });
        multi_buttons(m, width, true);
        card.end();
        ui::gap(28);
    } else if (!multi_message_.empty()) {
        ui::text(multi_message_.c_str(), Font::Regular, ui::size::kDetail, p.secondary, width);
        ui::gap(20);
    }
    for (size_t mode = 0; mode < collection_->multi.size(); mode++) {
        ImGui::PushID(static_cast<int>(mode));
        multi_card(mode, width);
        ImGui::PopID();
    }
}

// A mode's keys and buttons for while it plays: a row each, with keycaps (after a separator: under another row)
void App::multi_buttons(const launcher::MultiMode& m, float width, bool after) {
    constexpr float kKeys = 230.0f;
    for (size_t n = 0; n < m.buttons.size(); n++) {
        if (after || n > 0) {
            inset_separator(width);
        }
        const launcher::MultiButton& b = m.buttons[n];
        row(width, b.title, b.detail, kKeys, 24.0f, [&] {
            float keys = 0.0f;  // (at the right, as the other rows' controls end)
            for (size_t k = 0; k < b.keys.size(); k++) {
                keys += std::max(ui::text_size(b.keys[k], Font::Semibold, ui::size::kSmall).x + px(14), px(30)) + (k > 0 ? px(8) : 0.0f);
            }
            ImGui::SetCursorScreenPos(ImGui::GetCursorScreenPos() + ImVec2(std::max(0.0f, px(kKeys) - keys), 0.0f));
            for (size_t k = 0; k < b.keys.size(); k++) {
                if (k > 0) {
                    ImGui::SameLine(0, px(8));
                }
                ui::keycap(b.keys[k]);
            }
        });
    }
}

void App::multi_card(size_t mode, float width) {
    const ui::Palette& p = ui::palette();
    const launcher::MultiMode& m = collection_->multi[mode];
    const std::string section = std::string("multi.") + m.key;
    std::string upper = m.title;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    layout::label(upper.c_str());
    ui::text(m.detail, Font::Regular, ui::size::kDetail, p.secondary, width);
    ui::gap(12);
    Card card(width);
    bool first = true;
    for (size_t choice = 0; choice < m.choices.size(); choice++) {
        const launcher::MultiChoice& c = m.choices[choice];
        if (c.shown_with) {
            auto with = std::find_if(m.choices.begin(), m.choices.end(), [&](const launcher::MultiChoice& other) { return std::strcmp(other.key, c.shown_with) == 0; });
            if (with != m.choices.end() && multi_value(mode, static_cast<size_t>(with - m.choices.begin())) != c.shown_value) {
                continue;
            }
        }
        if (!first) {
            inset_separator(width);
        }
        first = false;
        ImGui::PushID(c.key);
        int chosen = multi_value(mode, choice);
        row(width, c.title, c.detail, 190.0f, 36.0f, [&] {
            if (ui::dropdown("##choice", c.choices, chosen, 190.0f)) {
                prefs_.set(section, c.key, std::to_string(chosen));
                prefs_.save();
            }
        });
        ImGui::PopID();
    }
    if (!first) {
        inset_separator(width);
    }
    int order = multi_shuffled(mode) ? 1 : 0;
    static const std::vector<std::string> kOrders = {"As in the sidebar", "Shuffled"};
    row(width, "Order of the games", "As the games are in the sidebar (drag them there to change it), or shuffled, anew each time round.", 190.0f, 36.0f, [&] {
        if (ui::dropdown("##order", kOrders, order, 190.0f)) {
            prefs_.set(section, "order", order ? "1" : "0");
            prefs_.save();
        }
    });
    card.end();
    if (!m.buttons.empty()) {
        ui::gap(24);
        layout::label("WHILE IT PLAYS");
        Card keys(width);
        multi_buttons(m, width, false);
        keys.end();
    }
    ui::gap(24);
    layout::label("GAMES");
    std::vector<size_t> games = multi_games(mode);
    size_t chosen_count = 0;
    if (games.empty()) {
        ui::text("None of your games has it yet: choose the ROM set of a game that does on the Games page.", Font::Regular, ui::size::kDetail, p.secondary, width);
    } else {
        Card list(width);
        for (size_t n = 0; n < games.size(); n++) {
            if (n > 0) {
                inset_separator(width);
            }
            bool on = multi_chosen(mode, games[n]);
            chosen_count += on;
            ImGui::PushID(slots_[games[n]]->id.c_str());
            row(width, slots_[games[n]]->name.c_str(), nullptr, 42.0f, 24.0f, [&] {
                if (ui::toggle("##on", on)) {
                    set_multi_chosen(mode, games[n], !on);
                }
            });
            ImGui::PopID();
        }
        list.end();
    }
    ui::gap(20);
    std::string start = std::string(multi_.mode == static_cast<int>(mode) ? "Playing…" : "Start") + "###start";
    if (ui::button(start.c_str(), Kind::Primary, 200.0f, multi_.mode < 0 && chosen_count > 0, 48.0f)) {
        start_multi(mode);
    }
    ui::gap(36);
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
                            : TrFormat("Pure plays the game exactly as it was {}. Enhanced turns on the improvements; or pick your own.",
                                       {Tr(about_.original)});
    if (collection_) {
        intro = TrFormat("For every game that has them. Pure plays each game exactly as it was {}. Enhanced turns on the improvements; or "
                         "pick your own. What only one game has is on that game's page.",
                         {Tr(about_.original)});
    }
    layout::heading(about_.features_name, intro.c_str(), width);

    if (about_.presets) {
        std::vector<std::string> labels = {TrFormat("Pure: as {}", {Tr(about_.original)}), "Enhanced: recommended", "Custom"};
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
            if (!feature_shown(one, i)) {
                continue;
            }
            rows.push_back({&one.features[i], values[i], "", i});
        }
        feature_list(
            width, rows, "No optional features yet.", [&](const FeatureRow& feature, int value) { set_feature(one, feature.index, value); },
            [&](const FeatureRow& feature) { return recomp::MenuChoices(one.settings, one.features[feature.index]); },
            [&](const FeatureRow& feature, const std::vector<bool>& offered) { set_menu_choices(one, feature.index, offered); });
        return;
    }
    // A collection's: each as the first game that has it has it (a note where the games differ, or not every
    // game has it)
    std::vector<std::vector<int>> values(slots_.size());
    for (size_t which = 0; which < slots_.size(); which++) {
        values[which] = recomp::FeatureValues(slots_[which]->settings, slots_[which]->features);
    }
    // (the games shown: with only the player's games, a feature none of them has is left out, and the notes name
    // theirs alone; what is chosen is still set for every game that has it)
    size_t here = 0;
    for (size_t which = 0; which < slots_.size(); which++) {
        here += shown(which);
    }
    std::vector<FeatureRow> rows;
    for (size_t i = 0; i < shared_.size(); i++) {
        const Shared& shared = shared_[i];
        std::vector<std::pair<size_t, size_t>> where;
        for (const auto& at : shared.where) {
            if (shown(at.first)) {
                where.push_back(at);
            }
        }
        if (where.empty()) {
            continue;
        }
        int value = values[where[0].first][where[0].second];
        bool differ = std::any_of(where.begin(), where.end(), [&](const auto& at) { return values[at.first][at.second] != value; });
        std::string note;
        if (where.size() < here) {
            std::string names;
            for (size_t k = 0; k < where.size(); k++) {
                names += (k == 0 ? "" : k + 1 == where.size() ? Tr(" and ") : ", ") + slots_[where[k].first]->name;
            }
            note = TrFormat("Only in {}.", {names});
        }
        if (differ) {
            note += std::string(note.empty() ? "" : " ") + Tr("Not the same in every game yet: what you choose here is.");
        }
        rows.push_back({&shared.info, value, note, i});
    }
    // (the menu's choices: as the first game that has the feature has them, and set for every one)
    feature_list(
        width, rows, here < 2 ? "With one game, its features are on its own page." : "The games have no features in common.",
        [&](const FeatureRow& feature, int value) { set_shared(feature.index, value); },
        [&](const FeatureRow& feature) {
            const auto& [which, index] = shared_[feature.index].where[0];
            return recomp::MenuChoices(slots_[which]->settings, slots_[which]->features[index]);
        },
        [&](const FeatureRow& feature, const std::vector<bool>& offered) {
            for (const auto& [which, index] : shared_[feature.index].where) {
                set_menu_choices(*slots_[which], index, offered);
            }
        });
}

// Under a kFeatureMenuChoices feature: how many of its choices the game's menu offers, and (opened) a switch for each
template <typename SetOffered>
void App::menu_choices(float width, const FeatureRow& feature, std::vector<bool> offered, SetOffered set_offered) {
    const std::vector<std::string>& choices = feature.info->choices;
    const int all = static_cast<int>(choices.size()) - 1;
    const int count = static_cast<int>(std::count(offered.begin() + 1, offered.end(), true));
    auto open = std::find(menu_choices_open_.begin(), menu_choices_open_.end(), feature.info->name);
    const bool is_open = open != menu_choices_open_.end();
    std::string label = count == all ? TrFormat("In the game's menu: all {}", {std::to_string(all)})
                                     : TrFormat("In the game's menu: {} of {}", {std::to_string(count), std::to_string(all)});
    inset_separator(width);
    ImGui::PushID("menu choices");
    row(width, label.c_str(), "The ones Left and Right go through in the game's own menu, for a shorter list there. Off always is.", 120.0f,
        36.0f, [&] {
            if (ui::button(is_open ? "Done" : "Choose", Kind::Secondary, 120.0f)) {
                if (is_open) {
                    menu_choices_open_.erase(open);
                } else {
                    menu_choices_open_.push_back(feature.info->name);
                }
            }
        });
    if (is_open) {
        for (int choice = 1; choice <= all; choice++) {
            inset_separator(width);
            ImGui::PushID(choice);
            const bool on = offered[static_cast<size_t>(choice)];
            row(width, choices[static_cast<size_t>(choice)].c_str(), "", 42.0f, 24.0f, [&] {
                if (ui::toggle("##offered", on)) {
                    offered[static_cast<size_t>(choice)] = !on;
                    set_offered(offered);
                }
            });
            ImGui::PopID();
        }
    }
    ImGui::PopID();
}

// The rows under their categories, narrowed by the search box: only those whose name, description, category or
// choices have every word typed (Ctrl+F or Cmd+F goes into it)
template <typename Set, typename Offered, typename SetOffered>
void App::feature_list(float width, const std::vector<FeatureRow>& rows, const char* none, Set set, Offered offered, SetOffered set_offered,
                       bool whole) {
    const ui::Palette& p = ui::palette();
    if (rows.empty()) {
        ui::text(none, Font::Regular, ui::size::kBody, p.secondary, width);
        return;
    }
    // (`whole`: a page of features, with the search box and a heading for each category; else a few, as one card)
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F) || ImGui::IsKeyChordPressed(ImGuiMod_Super | ImGuiKey_F)) {
        focus_search_ = true;
    }
    if (focus_search_) {
        ImGui::SetKeyboardFocusHere();
        focus_search_ = false;
    }
    if (whole) {
        ui::search_box("##search", feature_search_, sizeof(feature_search_), "Search features", width);
    }
    std::vector<std::string> words;
    if (whole) {
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
        std::string count = matches == 0 ? Tr("No features match.")
                                         : TrFormat("{} of {} features", {std::to_string(matches), std::to_string(rows.size())});
        ui::text(count.c_str(), Font::Regular, ui::size::kDetail, p.secondary);
    }
    if (whole) {
        ui::gap(20);
    }
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
        if (whole) {
            ui::text(upper.c_str(), Font::Semibold, ui::size::kDetail, p.secondary);
            ui::gap(10);
        }
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
                if (feature.flags & recomp::kFeatureMenuChoices) {
                    menu_choices(width, rows[i], offered(rows[i]), [&](const std::vector<bool>& to) { set_offered(rows[i], to); });
                }
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

const recomp::BindTable& App::shared_table() const {
    return collection_ && collection_->bindings.count > 0 ? collection_->bindings : about_.bindings;
}

bool App::has_own_layout(size_t which) const {
    return collection_ && which < slots_.size() && slots_[which]->about->bindings.count > 0;
}

bool App::own_layout(size_t which) const {
    return has_own_layout(which) && prefs_.get("controls", slots_[which]->id) == "own";
}

// A game given a layout of its own starts from what every game has (each of its actions as the shared action of
// that name is, or as its own usual layout has it where there is none); one given back has every game's again
void App::set_own_layout(size_t which, bool own) {
    if (!has_own_layout(which) || own == own_layout(which)) {
        return;
    }
    Slot& one = *slots_[which];
    prefs_.set("controls", one.id, own ? "own" : "");
    prefs_.save();
    if (own) {
        const recomp::BindTable& table = one.about->bindings;
        const recomp::BindTable& shared = shared_table();
        recomp::Bindings& b = own_bindings_[which];
        b = recomp::DefaultBindings(table);
        for (int action = 0; action < table.count; action++) {
            for (int other = 0; other < shared.count; other++) {
                if (std::strcmp(table.actions[action].name, shared.actions[other].name) == 0) {
                    for (int player = 0; player < table.players && player < shared.players; player++) {
                        b.key[player][action] = bindings_.key[player][other];
                        b.pad[player][action] = bindings_.pad[player][other];
                    }
                }
            }
        }
        write_bindings(one, table, b);
    } else {
        write_bindings(one, shared_table(), bindings_);
    }
}

const recomp::BindTable& App::bind_table() const {
    return bind_slot_ >= 0 && own_layout(static_cast<size_t>(bind_slot_)) ? slots_[static_cast<size_t>(bind_slot_)]->about->bindings : shared_table();
}

recomp::Bindings& App::bind_layout() {
    return bind_slot_ >= 0 && own_layout(static_cast<size_t>(bind_slot_)) ? own_bindings_[static_cast<size_t>(bind_slot_)] : bindings_;
}

// One game's settings.ini given a layout (a game's own: what the shared table has that it has not got is "none",
// so that the game, which reads the shared table, has only these)
void App::write_bindings(Slot& slot, const recomp::BindTable& table, const recomp::Bindings& bindings) {
    const recomp::BindTable& shared = shared_table();
    for (int player = 0; player < shared.players; player++) {
        for (int other = 0; collection_ && &table != &shared && other < shared.count; other++) {
            slot.settings.set(recomp::BindingSection(false, player), shared.actions[other].name, "none");
            slot.settings.set(recomp::BindingSection(true, player), shared.actions[other].name, "none");
        }
    }
    for (int player = 0; player < table.players; player++) {
        for (int action = 0; action < table.count; action++) {
            slot.settings.set(recomp::BindingSection(false, player), table.actions[action].name, recomp::KeyText(bindings.key[player][action]));
            slot.settings.set(recomp::BindingSection(true, player), table.actions[action].name, recomp::PadText(bindings.pad[player][action]));
        }
    }
    slot.settings.save();
}

// The layout the Controllers page shows, kept: every game's (a collection's: in each game's settings.ini, but
// those given their own), or one game's own
void App::save_bindings() {
    if (bind_slot_ >= 0 && own_layout(static_cast<size_t>(bind_slot_))) {
        write_bindings(*slots_[static_cast<size_t>(bind_slot_)], bind_table(), bind_layout());
        return;
    }
    for (size_t which = 0; which < slots_.size(); which++) {
        if (!own_layout(which)) {
            write_bindings(*slots_[which], shared_table(), bindings_);
        }
    }
}

// A key or a gamepad input is given to an action. Whatever had it before gets what the action had (for the
// same player: the two change places), so nothing is ever on two actions. A key is one player's only.
void App::bind(int player, bool gamepad, int action, SDL_Scancode key, recomp::PadInput input) {
    const recomp::BindTable& table = bind_table();
    recomp::Bindings& b = bind_layout();
    if (gamepad) {
        recomp::PadInput was = b.pad[player][action];
        for (int other = 0; other < table.count; other++) {
            if (other != action && input != recomp::PadInput{} && b.pad[player][other] == input) {
                b.pad[player][other] = was;
            }
        }
        b.pad[player][action] = input;
    } else {
        SDL_Scancode was = b.key[player][action];
        for (int who = 0; who < table.players; who++) {
            for (int other = 0; other < table.count; other++) {
                if ((who != player || other != action) && key != SDL_SCANCODE_UNKNOWN && b.key[who][other] == key) {
                    b.key[who][other] = who == player ? was : SDL_SCANCODE_UNKNOWN;
                }
            }
        }
        b.key[player][action] = key;
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

    if (bind_slot_ >= static_cast<int>(slots_.size()) || (bind_slot_ >= 0 && !slots_[static_cast<size_t>(bind_slot_)]->has_program())) {
        bind_slot_ = -1;
    }
    if (shared_table().count > 0) {
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
        // A collection's: every game's layout, or a game's, which may be one of its own
        std::vector<size_t> own_able;
        for (size_t which : order_) {
            if (has_own_layout(which) && shown(which)) {
                own_able.push_back(which);
            }
        }
        if (bind_slot_ >= 0 && !shown(static_cast<size_t>(bind_slot_))) {
            bind_slot_ = -1;  // (a game left out since)
        }
        if (!own_able.empty()) {
            Card games(width);
            std::vector<std::string> labels = {"Every game"};
            int index = 0;
            for (size_t i = 0; i < own_able.size(); i++) {
                labels.push_back(own_layout(own_able[i]) ? TrFormat("{}  (its own)", {slots_[own_able[i]]->name}) : slots_[own_able[i]]->name);
                index = bind_slot_ == static_cast<int>(own_able[i]) ? static_cast<int>(i) + 1 : index;
            }
            row(width, "Layout for", "Every game has the same keys and buttons, unless it is given a layout of its own.", 280.0f, 36.0f, [&] {
                if (ui::dropdown("##layout-for", labels, index, 280.0f)) {
                    bind_slot_ = index == 0 ? -1 : static_cast<int>(own_able[static_cast<size_t>(index - 1)]);
                    capture_action_ = -1;
                }
            });
            if (bind_slot_ >= 0) {
                const size_t which = static_cast<size_t>(bind_slot_);
                const bool own = own_layout(which);
                inset_separator(width);
                row(width, "Its own layout",
                    own ? "This game has keys and buttons of its own, with its own names for its buttons. Off: every game's again."
                        : "Off: this game has every game's layout (Every game, above). On: keys and buttons of its own, starting as every game's are.",
                    42.0f, 24.0f, [&] {
                        if (ui::toggle("##own-layout", own)) {
                            set_own_layout(which, !own);
                            capture_action_ = -1;
                        }
                    });
            }
            games.end();
            ui::gap(10);
        }
        // (a game with every game's layout: only its switch; the buttons are every game's, set there)
        if (bind_slot_ < 0 || own_layout(static_cast<size_t>(bind_slot_))) {
            const recomp::BindTable& table = bind_table();
            recomp::Bindings& layout_shown = bind_layout();
            bind_player_ = std::min(bind_player_, table.players - 1);
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
                    layout_shown.key[bind_player_][action] = usual.key[bind_player_][action];
                    layout_shown.pad[bind_player_][action] = usual.pad[bind_player_][action];
                }
                // (a key the usual layout gives this player is taken from the others)
                for (int who = 0; who < table.players; who++) {
                    for (int action = 0; who != bind_player_ && action < table.count; action++) {
                        for (int other = 0; other < table.count; other++) {
                            SDL_Scancode& theirs = layout_shown.key[who][other];
                            if (theirs != SDL_SCANCODE_UNKNOWN && theirs == layout_shown.key[bind_player_][action]) {
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
            const std::string what = TrFormat("The keyboard and the {} gamepad. Click a button here, then press the key or the gamepad button for it.",
                                              {Tr(kNth[bind_player_])});
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
                                            : gamepad ? pad_label(layout_shown.pad[bind_player_][action])
                                                      : key_label(layout_shown.key[bind_player_][action]);
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
        } else {
            ui::gap(14);
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
