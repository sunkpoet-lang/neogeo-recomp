// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// (Card, row and inset_separator are from the launcher of wiiparty-recomp by arelkair, GPL-3.0-or-later:
// https://github.com/arelkair/wiiparty-recomp, launcher/src/layout.h; see THIRD_PARTY_NOTICES.md.)
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

#include "steam.h"
#include "ui.h"

using launcher::FileCheck;
using ui::Font;
using ui::Kind;
using ui::px;

namespace {

constexpr float kSidebarWidth = 220.0f;
constexpr float kPagePaddingX = 44.0f;
constexpr float kPagePaddingTop = 40.0f;
constexpr float kContentMaxWidth = 720.0f;

// A rounded panel behind its contents.
class Card {
public:
    explicit Card(float width) : list_(ImGui::GetWindowDrawList()), start_(ImGui::GetCursorScreenPos()), width_(width) {
        list_->ChannelsSplit(2);
        list_->ChannelsSetCurrent(1);
    }
    void end() {
        float bottom = ImGui::GetCursorScreenPos().y;
        list_->ChannelsSetCurrent(0);
        list_->AddRectFilled(start_, ImVec2(start_.x + width_, bottom), ui::palette().surface, px(14));
        list_->ChannelsMerge();
    }

private:
    ImDrawList* list_;
    ImVec2 start_;
    float width_;
};

// A card row: label and detail on the left, a control on the right.
template <typename Control>
void row(float width, const char* label, const char* detail, float control_width, float control_height, Control control) {
    const ui::Palette& p = ui::palette();
    ImVec2 top = ImGui::GetCursorScreenPos();
    float pad = px(18);
    ImGui::SetCursorScreenPos(top + ImVec2(pad, px(14)));
    ImGui::BeginGroup();
    float text_width = width - pad * 2 - px(control_width) - px(24);
    ui::text(label, Font::Regular, ui::size::kBody, p.text, text_width);
    if (detail && *detail) {
        ui::gap(3);
        ui::text(detail, Font::Regular, ui::size::kDetail, p.secondary, text_width);
    }
    ImGui::EndGroup();
    float bottom = ImGui::GetItemRectMax().y + px(14);
    float height = bottom - top.y;
    ImGui::SetCursorScreenPos(ImVec2(top.x + width - pad - px(control_width), top.y + std::round((height - px(control_height)) * 0.5f)));
    control();
    ImGui::SetCursorScreenPos(ImVec2(top.x, bottom));
    ImGui::Dummy(ImVec2(width, 0));
}

void inset_separator(float width) {
    ImVec2 at = ImGui::GetCursorScreenPos();
    float thickness = std::max(1.0f, std::floor(ui::scale()));
    ImGui::GetWindowDrawList()->AddRectFilled(at + ImVec2(px(18), 0), at + ImVec2(width - px(18), thickness), ui::palette().separator);
    ImGui::Dummy(ImVec2(width, thickness));
}

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
    return about.arguments(launch);
}

}  // namespace

int play_directly() {
    // Straight into the game with the file chosen earlier (e.g. a Steam shortcut in Game Mode).
    const launcher::Description& about = launcher::describe();
    std::filesystem::path folder = player_folder();
    recomp::Ini prefs(folder / "launcher.ini");
    prefs.load();
    if (!about.check(prefs.get("game", about.file_key)).usable()) {
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
    : about_(launcher::describe()), window_(window), prefs_(data_folder() / "launcher.ini"), settings_(data_folder() / "settings.ini") {
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
    std::string remembered = prefs_.get("game", about_.file_key);
    if (!remembered.empty()) {
        use_game_file(remembered);
    }
    if (about_.find_nearby && !checking_ && !file_.usable()) {
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
    return checking_ || game_.running();
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
    if (event.type == SDL_EVENT_DROP_FILE && event.drop.data) {
        use_game_file(event.drop.data);
        page_ = Page::Game;
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
}

void App::sidebar(float height) {
    const ui::Palette& p = ui::palette();
    ImVec2 origin = ImGui::GetMainViewport()->Pos;
    float inner = kSidebarWidth - 40.0f;
    ImGui::SetCursorScreenPos(origin + ImVec2(px(24), px(28)));
    ui::text(LAUNCHER_GAME_NAME, Font::Semibold, 17.0f, p.text);
    ImGui::SetCursorScreenPos(origin + ImVec2(px(24), px(50)));
    ui::text("native port", Font::Regular, ui::size::kDetail, p.secondary);
    const char* names[] = {"Game", "Features", "Controllers"};
    float top = px(100);
    float step = px(38);
    ImGuiID indicator = ImHashStr("nav-indicator");
    float at = ui::animate(indicator, static_cast<float>(page_), 18.0f);
    ImVec2 pill = origin + ImVec2(px(20), top + at * step);
    ImGui::GetWindowDrawList()->AddRectFilled(pill, pill + ImVec2(px(inner), px(36)), p.control_hover, px(10));
    for (int i = 0; i < static_cast<int>(Page::Count); i++) {
        ImGui::SetCursorScreenPos(origin + ImVec2(px(20), top + step * static_cast<float>(i)));
        ImGui::PushID(i);
        if (ui::nav_item(names[i], static_cast<int>(page_) == i, inner)) {
            page_ = static_cast<Page>(i);
        }
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos(origin + ImVec2(px(24), height - px(36)));
    ui::text(LAUNCHER_DISCLAIMER, Font::Regular, ui::size::kSmall, p.tertiary, px(inner));
}

void App::page(float width, float height) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(kPagePaddingX), px(kPagePaddingTop)));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::palette().background);
    ImGui::BeginChild(static_cast<int>(page_) + 100, ImVec2(width, height), ImGuiChildFlags_AlwaysUseWindowPadding);
    float content = std::min(width - px(kPagePaddingX) * 2, px(kContentMaxWidth));
    switch (page_) {
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
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void App::heading(const char* title, const char* intro, float width) {
    const ui::Palette& p = ui::palette();
    ui::text(title, Font::Semibold, ui::size::kHeading, p.text);
    if (intro) {
        ui::gap(8);
        ui::text(intro, Font::Regular, ui::size::kLead, p.secondary, std::min(width, px(620)));
    }
    ui::gap(26);
}

// ---- Game --------------------------------------------------------------------------------

void App::game_page(float width) {
    const ui::Palette& p = ui::palette();
    heading(LAUNCHER_GAME_NAME, about_.file_intro, width);

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
            choose_game_file();
        }
    });
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
    if (ui::button(game_.running() ? "Playing…" : "Play", Kind::Primary, 200.0f, ready, 48.0f)) {
        play();
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

// ---- Features ----------------------------------------------------------------------------

void App::features_page(float width) {
    const ui::Palette& p = ui::palette();
    std::string intro = std::string("Pure plays the game exactly as it was ") + about_.original + ". Enhanced turns on the improvements; or pick your own.";
    heading("Features", intro.c_str(), width);

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

    if (!features_error_.empty()) {
        ui::text(features_error_.c_str(), Font::Regular, ui::size::kBody, p.danger, width);
        return;
    }
    if (features_.empty()) {
        ui::text("No optional features yet.", Font::Regular, ui::size::kBody, p.secondary, width);
        return;
    }
    std::vector<std::string> categories;
    for (const recomp::FeatureInfo& feature : features_) {
        if (std::find(categories.begin(), categories.end(), feature.category) == categories.end()) {
            categories.push_back(feature.category);
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
            if (feature.category != category) {
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
    if (event.type == SDL_EVENT_KEY_UP) {
        return true;
    }
    if (!capture_gamepad_) {
        return false;
    }
    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        bind(bind_player_, true, capture_action_, SDL_SCANCODE_UNKNOWN, {event.gbutton.button, -1});
        capture_action_ = -1;
        return true;
    }
    if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && event.gaxis.value > 16000 &&
        (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) {
        bind(bind_player_, true, capture_action_, SDL_SCANCODE_UNKNOWN, {-1, event.gaxis.axis});
        capture_action_ = -1;
        return true;
    }
    return false;
}

void App::controllers_page(float width) {
    const ui::Palette& p = ui::palette();
    heading("Controllers", "The keyboard always works. Gamepads show up here when connected; press buttons and move the sticks to check they work.",
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
