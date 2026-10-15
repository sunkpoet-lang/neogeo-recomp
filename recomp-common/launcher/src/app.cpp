// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#include "app.h"

#include <algorithm>
#include <cmath>
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

std::filesystem::path game_program_path() {
    // In a build folder the player build is <id>_app (<id> there is the strict test build);
    // in an app bundle or AppImage it's <id>.
    std::filesystem::path base = base_path();
    std::error_code error;
    if (std::filesystem::exists(base / LAUNCHER_GAME_EXE "_app", error)) {
        return base / LAUNCHER_GAME_EXE "_app";
    }
#ifdef _WIN32
    return base / LAUNCHER_GAME_EXE ".exe";
#else
    return base / LAUNCHER_GAME_EXE;
#endif
}

std::filesystem::path player_folder() {
    if (const char* home = std::getenv(LAUNCHER_HOME_ENV); home && *home) {  // tests: another folder
        return home;
    }
    char* path = SDL_GetPrefPath("", LAUNCHER_GAME_NAME);  // where the game keeps settings.ini too
    std::filesystem::path folder = path ? std::filesystem::path(path) : base_path() / "user";
    SDL_free(path);
    return folder;
}

bool toggle_state(const recomp::Ini& prefs, const launcher::Toggle& toggle) {
    std::string value = prefs.get("video", toggle.key);
    return toggle.on_by_default ? value != "0" : value == "1";
}

// What the game is started with: the port says (the player's file, read only; their folder; the switches).
std::vector<std::string> arguments_for(const recomp::Ini& prefs, const std::filesystem::path& folder) {
    const launcher::Description& about = launcher::describe();
    launcher::Launch launch;
    launch.file = prefs.get("game", about.file_key);
    launch.folder = folder;
    launch.on = [&](const char* key) {
        for (const launcher::Toggle& toggle : about.display) {
            if (std::strcmp(toggle.key, key) == 0) {
                // (a switch that needs another is only on with it)
                return toggle_state(prefs, toggle) && (!toggle.needs || launch.on(toggle.needs));
            }
        }
        return false;
    };
    std::vector<std::string> arguments = about.arguments(launch);
    if (about.mute_argument && prefs.get("audio", "muted") == "1") {
        arguments.push_back(about.mute_argument);
    }
    return arguments;
}

}  // namespace

int play_directly() {
    // Straight into the game with the file chosen earlier (e.g. a Steam shortcut in Game Mode).
    const launcher::Description& about = launcher::describe();
    std::filesystem::path folder = player_folder();
    recomp::Ini prefs(folder / "launcher.ini");
    prefs.load();
    if (about.game_page && !about.check(prefs.get("game", about.file_key)).usable()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, LAUNCHER_GAME_NAME, about.file_not_set_up, nullptr);
        return 1;
    }
    GameProcess game;
    std::string message;
    std::error_code error;
    if (about.saves_folder) {
        std::filesystem::create_directories(folder / "saves", error);
    }
    std::filesystem::create_directories(folder / "logs", error);
    if (!game.start(game_program_path(), arguments_for(prefs, folder), folder, message, folder / "logs" / "game.log")) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, LAUNCHER_GAME_NAME, message.c_str(), nullptr);
        return 1;
    }
    while (game.running()) {
        SDL_Delay(100);
    }
    return game.exit_code();
}

App::App(SDL_Window* window)
    : about_(launcher::describe()),
      window_(window),
      prefs_(data_folder() / "launcher.ini"),
      settings_(data_folder() / "settings.ini"),
      folder_(data_folder()),
      program_folder_(base_path()) {
    prefs_.load();
    settings_.load();
    // The launcher's default is the Enhanced preset (the game alone, with no settings, runs Pure).
    if (prefs_.get("launcher", "preset_chosen").empty()) {
        if (settings_.get("features", "preset").empty()) {
            settings_.set("features", "preset", "enhanced");
            settings_.save();
        }
        prefs_.set("launcher", "preset_chosen", "1");
        prefs_.save();
    }
    load_features();
    bindings_ = recomp::LoadBindings(settings_.path().string(), about_.bindings);
    std::string remembered = about_.game_page ? prefs_.get("game", about_.file_key) : std::string();
    if (!remembered.empty()) {
        use_game_file(remembered);
    }
    if (about_.game_page && about_.find_nearby && !checking_ && !file_.usable()) {
        // Nothing usable remembered: the port may know of one near the program
        std::string nearby = about_.find_nearby(base_path());
        if (!nearby.empty()) {
            use_game_file(nearby);
        }
    }
}

App::~App() {
    if (checker_.joinable()) {
        checker_.join();
    }
}

bool App::busy() const {
    for (const launcher::CustomPage& page : about_.pages) {
        if (page.busy && page.busy()) {
            return true;
        }
    }
    return checking_ || game_.running();
}

int App::page_count() const {
    return static_cast<int>(about_.pages.size()) + (about_.game_page ? 3 : 2);
}

// The launcher's own page at a place in the sidebar (the port's pages come first), or Page::Count
Page App::own_page(int index) const {
    int own = index - static_cast<int>(about_.pages.size());
    if (own < 0) {
        return Page::Count;
    }
    return static_cast<Page>(about_.game_page ? own : own + 1);
}

launcher::PageContext App::context(float width) {
    launcher::PageContext context{width, window_, prefs_, settings_, folder_, program_folder_, game_.running(), game_.exit_code(), play_error_, {}};
    context.play = [this] { play(); };
    return context;
}

std::filesystem::path App::game_program() const {
    return game_program_path();
}

std::filesystem::path App::data_folder() const {
    return player_folder();
}

// ---- game file ---------------------------------------------------------------------------

void App::choose_game_file() {
    std::string start = file_path_.empty() ? std::string() : std::filesystem::path(file_path_).parent_path().string();
    SDL_ShowOpenFileDialog(
        [](void* self, const char* const* files, int) {
            if (files && files[0]) {
                App* app = static_cast<App*>(self);
                std::lock_guard<std::mutex> lock(app->mutex_);
                app->picked_ = files[0];
            }
            wake();
        },
        this, window_, &about_.file_filter, 1, start.empty() ? nullptr : start.c_str(), false);
}

void App::use_game_file(const std::string& path) {
    if (checking_) {
        return;
    }
    if (checker_.joinable()) {
        checker_.join();
    }
    file_path_ = path;
    // Remember where it is; the file itself is only ever read.
    prefs_.set("game", about_.file_key, path);
    prefs_.save();
    if (!about_.check_on_thread) {
        file_ = about_.check(path);
        return;
    }
    checking_ = true;
    checker_ = std::thread([this, path] {
        FileCheck result = about_.check(path);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            file_ = result;
        }
        checking_ = false;
        wake();
    });
}

void App::play() {
    play_error_.clear();
    std::error_code error;
    if (about_.saves_folder) {
        std::filesystem::create_directories(data_folder() / "saves", error);
    }
    std::filesystem::create_directories(data_folder() / "logs", error);
    settings_.save();
    game_.start(game_program(), arguments_for(prefs_, data_folder()), data_folder(), play_error_, data_folder() / "logs" / "game.log");
}

bool App::toggle_on(const launcher::Toggle& toggle) const {
    if (toggle.needs) {
        for (const launcher::Toggle& other : about_.display) {
            if (std::strcmp(other.key, toggle.needs) == 0 && !toggle_on(other)) {
                return false;
            }
        }
    }
    return toggle_state(prefs_, toggle);
}

void App::set_toggle(const launcher::Toggle& toggle, bool on) {
    prefs_.set("video", toggle.key, on ? "1" : "0");
    for (const launcher::Toggle& other : about_.display) {
        if (on && toggle.needs && std::strcmp(other.key, toggle.needs) == 0) {
            prefs_.set("video", other.key, "1");  // (what it needs comes on with it)
        }
        if (!on && other.needs && std::strcmp(other.needs, toggle.key) == 0) {
            prefs_.set("video", other.key, "0");  // (what needs it goes off with it)
        }
    }
    prefs_.save();
}

// ---- features ----------------------------------------------------------------------------

void App::load_features() {
    features_.clear();
    features_error_.clear();
    std::error_code error;
    if (!std::filesystem::exists(game_program(), error)) {
        features_error_ = "The game program wasn't found next to the launcher.";
        return;
    }
    features_ = recomp::ParseFeatureList(GameProcess::output_of(game_program(), {"--list-features"}));
}

void App::set_preset(const std::string& name) {
    recomp::SetPreset(settings_, features_, name);
    settings_.save();
}

void App::set_feature(size_t index, int value) {
    recomp::SetFeature(settings_, features_, index, value);
    settings_.save();
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
            use_game_file(event.drop.data);
            page_ = count;
        }
        return true;
    }
    return false;
}

void App::frame() {
    std::string picked;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        picked.swap(picked_);
    }
    if (!picked.empty()) {
        use_game_file(picked);
    }
    // The game may have changed settings.ini (its menu's features): what it left is what is shown and kept
    if (was_running_ && !game_.running()) {
        settings_.load();
        show_focus_ = interactive_ && SDL_HasGamepad();  // (back from the game with a gamepad in hand)
    }
    was_running_ = game_.running();
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
    ui::text(about_.subtitle, Font::Regular, ui::size::kDetail, p.secondary);
    const char* own[] = {"Game", about_.features_name, "Controllers"};
    float top = px(100);
    float step = px(38);
    ImGuiID indicator = ImHashStr("nav-indicator");
    float at = ui::animate(indicator, static_cast<float>(page_), 18.0f);
    int custom = static_cast<int>(about_.pages.size());
    ImVec2 pill = origin + ImVec2(px(20), top + at * step);
    ImGui::GetWindowDrawList()->AddRectFilled(pill, pill + ImVec2(px(inner), px(36)), p.control_hover, px(10));
    // With the arrow keys or a gamepad: from a page, left goes to the row of the page shown and to no
    // other; once there, up and down change the page; right goes into it.
    ImGuiID focused = ImGui::GetFocusID();
    int shown = page_;
    bool in_sidebar = false;
    for (int i = 0; i < page_count(); i++) {
        ImGui::SetCursorScreenPos(origin + ImVec2(px(20), top + step * static_cast<float>(i)));
        ImGui::PushID(i);
        const char* name = i < custom ? about_.pages[i].name : own[static_cast<int>(own_page(i))];
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
    if (page_ < static_cast<int>(about_.pages.size())) {
        launcher::PageContext shown = context(content);
        about_.pages[page_].draw(shown);
    }
    switch (own_page(page_)) {
    case Page::Game:
        game_page(content);
        break;
    case Page::Features:
        features_page(content);
        break;
    case Page::Controllers:
        controllers_page(content);
        break;
    case Page::Count:
        break;
    }
    ui::gap(40);
    ImGui::PopItemFlag();
    ImGui::EndChild();
    ImGui::PopItemFlag();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ---- Game --------------------------------------------------------------------------------

void App::game_page(float width) {
    const ui::Palette& p = ui::palette();
    layout::heading(LAUNCHER_GAME_NAME, about_.file_intro, width);

    FileCheck file;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        file = file_;
    }
    ui::text(about_.file_heading, Font::Semibold, ui::size::kDetail, p.secondary);
    ui::gap(10);
    Card card(width);
    std::string shown = file_path_.empty() ? std::string(about_.file_empty) : file_path_;
    shown = ui::fit_start(shown, Font::Regular, ui::size::kBody, width - px(200));
    std::string status;
    if (checking_) {
        status = "Checking…";
    } else if (!file_path_.empty()) {
        status = file.message;
    }
    ImVec2 row_top = ImGui::GetCursorScreenPos();
    row(width, shown.c_str(), status.c_str(), 130.0f, 36.0f, [&] {
        if (ui::button("Choose…", Kind::Secondary, 130.0f, !checking_)) {
            // (the system's file dialog wants a mouse or a keyboard)
            if (ui::gamepad_in_use()) {
                open_browser();
            } else {
                choose_game_file();
            }
        }
    });
    browser();
    if (!file_path_.empty() && !checking_) {
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

    if (!about_.display.empty()) {
        ui::gap(24);
        ui::text("DISPLAY", Font::Semibold, ui::size::kDetail, p.secondary);
        ui::gap(10);
        Card display(width);
        bool first = true;
        for (const launcher::Toggle& toggle : about_.display) {
            if (!first) {
                inset_separator(width);
            }
            first = false;
            bool on = toggle_on(toggle);
            std::string id = std::string("##") + toggle.key;
            row(width, toggle.title, toggle.detail, 42.0f, 24.0f, [&] {
                if (ui::toggle(id.c_str(), on)) {
                    set_toggle(toggle, !on);
                }
            });
        }
        display.end();
    }
    if (about_.mute_argument) {
        ui::gap(24);
        ui::text("SOUND", Font::Semibold, ui::size::kDetail, p.secondary);
        ui::gap(10);
        Card sound(width);
        bool muted = prefs_.get("audio", "muted") == "1";
        row(width, "Muted", "Start the game with no sound. The game's menu switches it back on.", 42.0f, 24.0f, [&] {
            if (ui::toggle("##muted", muted)) {
                prefs_.set("audio", "muted", muted ? "0" : "1");
                prefs_.save();
            }
        });
        sound.end();
    }
    ui::gap(28);

    bool file_ok = !checking_ && file.usable();
    std::error_code error;
    bool have_program = std::filesystem::exists(game_program(), error);
    bool ready = file_ok && have_program && !game_.running();
    if (about_.preset_summary) {
        std::string preset = recomp::Preset(settings_);
        std::string summary = preset == "pure"       ? std::string("Features: Pure (exactly as ") + about_.original + ")"
                              : preset == "enhanced" ? "Features: Enhanced"
                                                     : "Features: Custom";
        ui::text(summary.c_str(), Font::Regular, ui::size::kDetail, p.secondary);
        ui::gap(12);
    }
    if (ui::button(game_.running() ? "Playing…###play" : "Play###play", Kind::Primary, 200.0f, ready, 48.0f)) {
        play();
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
    } else if (!play_error_.empty()) {
        ui::gap(10);
        ui::text(play_error_.c_str(), Font::Regular, ui::size::kDetail, p.danger, width);
    } else if (!game_.running() && game_.exit_code() != 0) {
        ui::gap(10);
        std::string text = "The game stopped with code " + std::to_string(game_.exit_code()) + ".";
        ui::text(text.c_str(), Font::Regular, ui::size::kDetail, p.danger, width);
    }
    // Linux and Steam Deck: shortcuts for Steam's Game Mode.
    static const bool steam = can_add_to_steam();
    if (steam) {
        ui::gap(28);
        ui::text("STEAM", Font::Semibold, ui::size::kDetail, p.secondary);
        ui::gap(10);
        Card card(width);
        const char* detail = steam_message_.empty() ? "Adds \"" LAUNCHER_GAME_NAME "\" (straight into the game) and \"" LAUNCHER_GAME_NAME
                                                      " Launcher\" to your Steam library, to play from Game Mode."
                                                    : steam_message_.c_str();
        row(width, "Play from Steam", detail, 150.0f, 36.0f, [&] {
            if (ui::button("Add to Steam", Kind::Secondary, 150.0f, ready)) {
                add_to_steam(steam_message_);
            }
        });
        card.end();
    }
}

// ---- the game file, chosen with a gamepad --------------------------------------------------

void App::open_browser() {
    std::error_code error;
    std::filesystem::path start = file_path_.empty() ? std::filesystem::path() : std::filesystem::u8path(file_path_).parent_path();
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
    std::string pattern = about_.file_filter.pattern ? about_.file_filter.pattern : "*";
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
        ui::text("Choose the file", Font::Semibold, 17.0f, p.text);
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
        if (chosen == -1) {
            list_folder(browse_folder_.parent_path());
        } else if (chosen >= 0 && browse_entries_[static_cast<size_t>(chosen)].folder) {
            list_folder(browse_folder_ / std::filesystem::u8path(browse_entries_[static_cast<size_t>(chosen)].name));
        } else if (chosen >= 0) {
            use_game_file((browse_folder_ / std::filesystem::u8path(browse_entries_[static_cast<size_t>(chosen)].name)).u8string());
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
    layout::heading(about_.features_name, intro.c_str(), width);

    if (about_.presets) {
        std::vector<std::string> labels = {std::string("Pure: as ") + about_.original, "Enhanced: recommended", "Custom"};
        std::string preset = recomp::Preset(settings_);
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

    if (!features_error_.empty()) {
        ui::text(features_error_.c_str(), Font::Regular, ui::size::kBody, p.danger, width);
        return;
    }
    if (features_.empty()) {
        ui::text("No optional features yet.", Font::Regular, ui::size::kBody, p.secondary, width);
        return;
    }
    // The search box: only the features whose name, description, category or choices have every word typed
    // (Ctrl+F or Cmd+F goes into it)
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F) || ImGui::IsKeyChordPressed(ImGuiMod_Super | ImGuiKey_F)) {
        focus_search_ = true;
    }
    if (focus_search_) {
        ImGui::SetKeyboardFocusHere();
        focus_search_ = false;
    }
    ui::search_box("##search", feature_search_, sizeof(feature_search_), "Search features", width);
    auto lower = [](std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    };
    std::vector<std::string> words;
    {
        std::istringstream typed(lower(feature_search_));
        for (std::string word; typed >> word;) {
            words.push_back(word);
        }
    }
    std::vector<bool> shown(features_.size(), true);
    size_t matches = features_.size();
    if (!words.empty()) {
        matches = 0;
        for (size_t i = 0; i < features_.size(); i++) {
            const recomp::FeatureInfo& feature = features_[i];
            std::string text = feature.title + "\n" + feature.description + "\n" + feature.category + "\n" + feature.name;
            for (const std::string& choice : feature.choices) {
                text += "\n" + choice;
            }
            text = lower(text);
            shown[i] = std::all_of(words.begin(), words.end(), [&](const std::string& word) { return text.find(word) != std::string::npos; });
            matches += shown[i];
        }
        ui::gap(8);
        std::string count = matches == 0 ? std::string("No features match.")
                                         : std::to_string(matches) + " of " + std::to_string(features_.size()) + " features";
        ui::text(count.c_str(), Font::Regular, ui::size::kDetail, p.secondary);
    }
    ui::gap(20);
    std::vector<std::string> categories;
    for (size_t i = 0; i < features_.size(); i++) {
        const std::string& category = features_[i].category;
        if (shown[i] && std::find(categories.begin(), categories.end(), category) == categories.end()) {
            categories.push_back(category);
        }
    }
    std::vector<int> values = recomp::FeatureValues(settings_, features_);
    for (const std::string& category : categories) {
        std::string upper = category;
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        ui::text(upper.c_str(), Font::Semibold, ui::size::kDetail, p.secondary);
        ui::gap(10);
        Card card(width);
        bool first = true;
        for (size_t i = 0; i < features_.size(); i++) {
            const recomp::FeatureInfo& feature = features_[i];
            if (feature.category != category || !shown[i]) {
                continue;
            }
            if (!first) {
                inset_separator(width);
            }
            first = false;
            ImGui::PushID(feature.name.c_str());
            if (feature.choices.empty()) {
                bool on = values[i] != 0;
                row(width, feature.title.c_str(), feature.description.c_str(), 42.0f, 24.0f, [&] {
                    if (ui::toggle("##on", on)) {
                        set_feature(i, on ? 0 : 1);
                    }
                });
            } else {
                // One of several things: the first means off
                int chosen = values[i];
                row(width, feature.title.c_str(), feature.description.c_str(), 190.0f, 36.0f, [&] {
                    if (ui::dropdown("##choice", feature.choices, chosen, 190.0f)) {
                        set_feature(i, chosen);
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

void App::save_bindings() {
    const recomp::BindTable& table = about_.bindings;
    for (int player = 0; player < table.players; player++) {
        for (int action = 0; action < table.count; action++) {
            settings_.set(recomp::BindingSection(false, player), table.actions[action].name, recomp::KeyText(bindings_.key[player][action]));
            settings_.set(recomp::BindingSection(true, player), table.actions[action].name, recomp::PadText(bindings_.pad[player][action]));
        }
    }
    settings_.save();
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
            // (a key the usual layout gives this player is taken from the other)
            for (int action = 0; table.players > 1 && action < table.count; action++) {
                for (int other = 0; other < table.count; other++) {
                    SDL_Scancode& theirs = bindings_.key[1 - bind_player_][other];
                    if (theirs != SDL_SCANCODE_UNKNOWN && theirs == bindings_.key[bind_player_][action]) {
                        theirs = SDL_SCANCODE_UNKNOWN;
                    }
                }
            }
            capture_action_ = -1;
            save_bindings();
        }
        ui::gap(10);
        constexpr float kChip = 170.0f, kChipGap = 10.0f;
        Card buttons(width);
        row(width, table.players < 2 ? "Buttons" : bind_player_ == 0 ? "Player 1" : "Player 2",
            table.players < 2    ? "The keyboard and the gamepad. Click a button here, then press the key or the gamepad button for it."
            : bind_player_ == 0 ? "The keyboard and the first gamepad. Click a button here, then press the key or the gamepad button for it."
                                : "The keyboard and the second gamepad. Click a button here, then press the key or the gamepad button for it.",
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
