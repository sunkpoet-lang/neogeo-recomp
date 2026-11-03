// SPDX-License-Identifier: GPL-3.0-only
// Part of neogeo-recomp.
//
// A Neo Geo game's window (neogeo_port.h), as the Capcom games' (frontend_sdl.cpp) is: the picture through the shared
// presenter (present.h: GPU or SDL's renderer, the Display page's scaling and shape), the board's sound kept a frame or
// two ahead, the launcher's controls (bindings.h), the in-game menu and the same keys. The controls' table is the
// Capcom games' (a stick's six buttons): A and C are its first two of the top row, B and D the first two of the bottom.

#include "game_info.h"

#include "../machine/machine.h"
#include "../runtime/game.h"
#include "../recomp-common/text.h"
#include "bindings.h"
#include "neogeo_online.h"
#include "neogeo_port.h"
#include "present.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

#ifndef GAME_TITLE
#define GAME_TITLE GAME_NAME
#endif

namespace port {

namespace {
    using cps2recomp::NeoGeo;
    using cps2recomp::NeoGeoSound;
    using cps2recomp::NeoGeoVideo;
    namespace cps = cps2recomp;

#ifdef _WIN32
    constexpr const char *kExeSuffix = ".exe";
#else
    constexpr const char *kExeSuffix = "";
#endif
    // The game was started by the collection's launcher (neogeo-recomp-launcher, or cps2recomp-launcher, beside it,
    // and no launcher of its own)
    bool InCollection() {
        static const bool collection = [] {
            const char *base = SDL_GetBasePath();
            std::error_code missing;
            return base && !std::filesystem::exists(std::string(base) + GAME_ID "-launcher" + kExeSuffix, missing) &&
                   (std::filesystem::exists(std::string(base) + "neogeo-recomp-launcher" + kExeSuffix, missing) ||
                    std::filesystem::exists(std::string(base) + "cps2recomp-launcher" + kExeSuffix, missing));
        }();
        return collection;
    }

    const BoundTable &Table() {
        static const BoundTable &table = MakeBoundTable(GamePlayers(), GameButtons(), InCollection());
        return table;
    }
    Bindings g_bindings = recomp::DefaultBindings(Table().table);

    bool KeyBound(SDL_Scancode key) {
        for (int player = 0; player < Table().table.players; player++) {
            for (int action = 0; action < Table().table.count; action++) {
                if (g_bindings.key[player][action] == key) {
                    return true;
                }
            }
        }
        return false;
    }

    constexpr int kDead = 16000;

    // A player's controls as the table's bits (cps2recomp::Button: the Capcom games')
    uint16_t ReadKeyboard(const bool *keys, int player) {
        uint16_t pad = 0;
        for (int action = 0; action < Table().table.count; action++) {
            const SDL_Scancode key = g_bindings.key[player][action];
            if (key != SDL_SCANCODE_UNKNOWN && keys[key]) {
                pad |= Table().bits[action];
            }
        }
        if (player == 0 && keys[SDL_SCANCODE_RETURN] && !KeyBound(SDL_SCANCODE_RETURN)) {
            pad |= cps::kStart;
        }
        return pad;
    }
    uint16_t ReadGamepad(SDL_Gamepad *gp, int player) {
        uint16_t pad = 0;
        for (int action = 0; action < Table().table.count; action++) {
            const PadInput input = g_bindings.pad[player][action];
            if ((input.button >= 0 && SDL_GetGamepadButton(gp, static_cast<SDL_GamepadButton>(input.button))) ||
                (input.axis >= 0 && SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(input.axis)) > kDead)) {
                pad |= Table().bits[action];
            }
        }
        const int x = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTX), y = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTY);
        pad |= x < -kDead ? cps::kLeft : 0;
        pad |= x > kDead ? cps::kRight : 0;
        pad |= y < -kDead ? cps::kUp : 0;
        pad |= y > kDead ? cps::kDown : 0;
        return pad;
    }
    // ... as the Neo Geo's: light punch A, medium punch C, light kick B, medium kick D
    uint16_t ToNeoGeo(uint16_t pad) {
        uint16_t bits = 0;
        bits |= (pad & cps::kUp) ? NeoGeo::kUp : 0;
        bits |= (pad & cps::kDown) ? NeoGeo::kDown : 0;
        bits |= (pad & cps::kLeft) ? NeoGeo::kLeft : 0;
        bits |= (pad & cps::kRight) ? NeoGeo::kRight : 0;
        bits |= (pad & cps::kPunch1) ? NeoGeo::kA : 0;
        bits |= (pad & cps::kPunch2) ? NeoGeo::kC : 0;
        bits |= (pad & cps::kKick1) ? NeoGeo::kB : 0;
        bits |= (pad & cps::kKick2) ? NeoGeo::kD : 0;
        bits |= (pad & cps::kStart) ? NeoGeo::kStart : 0;
        bits |= (pad & cps::kCoin) ? NeoGeo::kCoin : 0;
        return bits;
    }

    Scaling DisplayScaling() {
        return static_cast<Scaling>(std::clamp(cps::FeatureValue("display_scaling"), 0, 2));
    }
    Shape DisplayShape() {
        return static_cast<Shape>(std::clamp(cps::FeatureValue("display_shape"), 0, 2));
    }
    int WindowSize() {
        static const int kTimes[] = {3, 2, 4, 5, 6};
        return kTimes[std::clamp(cps::FeatureValue("window_size"), 0, 4)];
    }
    // Where the picture goes: the presenter's 4:3 is for the CPS boards' 384 columns, so the MVS's 320 are shaped as
    // those for 4:3, and as they are for square pixels and fill
    SDL_FRect Where(int width, int height) {
        const Shape shape = DisplayShape();
        const int columns = shape == Shape::Arcade ? 384 : NeoGeoVideo::kWidth;
        return FitPicture(width, height, columns, NeoGeoVideo::kHeight, shape, DisplayScaling());
    }

    // Online: what the session says (waiting, connecting, a desync, the other player gone) in the middle, and while
    // playing the ping, delay and rollbacks in a corner
    void DrawOnline(std::vector<uint32_t> &pixels, const NeoGeoOnline &online) {
        constexpr int kWidth = NeoGeoVideo::kWidth, kHeight = NeoGeoVideo::kHeight;
        const recomp::text::Canvas canvas{pixels.data(), kWidth, kHeight};
        const std::string &message = online.Message();
        if (!message.empty()) {
            const float width = recomp::text::TextWidth(message.c_str(), 1);
            const float x = (kWidth - width) / 2, y = kHeight / 2 - 6;
            canvas.Fill(x - 6, y - 5, width + 12, 18, {20, 22, 30, 230});
            const recomp::text::Color colour = online.Failed() ? recomp::text::Color{255, 120, 110, 255} : recomp::text::Color{255, 255, 255, 255};
            canvas.Text(x, y, 1, 0, colour, message.c_str());
        }
        const std::string stats = online.Stats();
        if (!stats.empty()) {
            const float width = recomp::text::TextWidth(stats.c_str(), 1);
            canvas.Fill(kWidth - width - 6, 1, width + 4, 11, {20, 22, 30, 160});
            canvas.Text(kWidth - width - 4, 2, 1, 0, {200, 220, 255, 255}, stats.c_str());
        }
    }
    // The menu bar's own entries (the in-game menu's have ids of their own: recomp::IsMenuEntry)
    enum { kBarQuickSave = 1, kBarQuickLoad, kBarFullscreen, kBarAbout, kBarKey, kBarScreenshot };

    // The keys that work while playing, for Help's list (each an entry that does nothing)
    const char *const kKeys[][2] = {
        {"In-game menu", "Esc, F1"}, {"Menu bar", "F10, Alt"}, {"Quick save", "F5"}, {"Quick load", "F9"},
        {"Screenshot", "F8"},        {"Slot before, next", "F6, F7"}, {"Fullscreen", "F11"}, {"Test switch", "F2"},
        {"Pause", "P"},              {"One frame", "O"},              {"Slower, faster", "-, ="},
    };

    recomp::BarEntry BarItem(const char *label, int id, const char *shortcut = "", bool enabled = true) {
        recomp::BarEntry e;
        e.label = label;
        e.id = id;
        e.shortcut = shortcut;
        e.enabled = enabled;
        return e;
    }
} // namespace

Shape DisplayShapeNow() {
    return DisplayShape();
}
Scaling DisplayScalingNow() {
    return DisplayScaling();
}
SDL_FRect NeoGeoWhere(int width, int height) {
    return Where(width, height);
}

void BarMenus(NeoGeoPort &port, recomp::MenuBar &bar, bool fullscreen) {
    std::vector<recomp::BarMenu> menus = {{"Game", {}}, {"Video", {}}, {"Audio", {}}, {"Input", {}}, {"Features", {}}, {"Help", {}}};
    std::error_code error;
    recomp::BarOptions options;
    options.slotUsed = [&](int slot) { return std::filesystem::exists(std::filesystem::u8path(port.StatePath(slot)), error); };
    // (a slot kept says when: the time today, the date before)
    options.slotDetail = [&](int slot) -> std::string {
        SDL_PathInfo info{};
        SDL_Time now = 0;
        SDL_DateTime then{}, today{};
        if (!SDL_GetPathInfo(port.StatePath(slot).c_str(), &info) || info.type != SDL_PATHTYPE_FILE || !SDL_GetCurrentTime(&now) ||
            !SDL_TimeToDateTime(info.modify_time, &then, true) || !SDL_TimeToDateTime(now, &today, true)) {
            return "";
        }
        char text[24];
        if (then.year == today.year && then.month == today.month && then.day == today.day) {
            snprintf(text, sizeof(text), "%02d:%02d", then.hour, then.minute);
        } else {
            snprintf(text, sizeof(text), "%04d-%02d-%02d", then.year, then.month, then.day);
        }
        return text;
    };
    port.menu.AddBarEntries(menus, true, options);
    // (and its small picture, where one was kept with it)
    for (recomp::BarEntry &entry : menus[0].entries) {
        if (entry.label != "Save state" && entry.label != "Load state") {
            continue;
        }
        for (size_t i = 0; i < entry.children.size(); i++) {
            SDL_Surface *loaded = SDL_LoadPNG(NeoGeoPort::StatePicture(port.StatePath(static_cast<int>(i) + 1)).c_str());
            SDL_Surface *picture = loaded ? SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_XBGR8888) : nullptr;
            if (picture) {
                recomp::BarEntry &slot = entry.children[i];
                slot.pictureWidth = picture->w;
                slot.pictureHeight = picture->h;
                slot.picture.resize(static_cast<size_t>(picture->w) * picture->h);
                for (int y = 0; y < picture->h; y++) {
                    memcpy(&slot.picture[static_cast<size_t>(y) * picture->w], static_cast<const uint8_t *>(picture->pixels) + y * picture->pitch,
                           static_cast<size_t>(picture->w) * 4);
                }
                SDL_DestroySurface(picture);
            }
            if (loaded) {
                SDL_DestroySurface(loaded);
            }
        }
    }
    // Game: the quick save and load (F5, F9) and a screenshot (F8) after the save states
    std::vector<recomp::BarEntry> &game = menus[0].entries;
    auto slot = std::find_if(game.begin(), game.end(), [](const recomp::BarEntry &e) { return e.label == "Slot"; });
    const size_t after = slot == game.end() ? 0 : static_cast<size_t>(slot - game.begin()) + 1;
    game.insert(game.begin() + static_cast<std::ptrdiff_t>(after),
                {recomp::BarEntry::Line(), BarItem("Quick save", kBarQuickSave, "F5"),
                 BarItem("Quick load", kBarQuickLoad, "F9", options.slotUsed(port.menu.Slot())), BarItem("Screenshot", kBarScreenshot, "F8")});
    // Video: full screen (F11, Alt+Enter)
    std::vector<recomp::BarEntry> &video = menus[1].entries;
    if (!video.empty()) {
        video.push_back(recomp::BarEntry::Line());
    }
    video.push_back(BarItem("Fullscreen", kBarFullscreen, "F11"));
    video.back().mark = fullscreen ? recomp::BarEntry::Check : recomp::BarEntry::NoMark;
    // Input: the controls are the launcher's (its Controllers page)
    menus[3].entries.push_back(recomp::BarEntry::Line());
    menus[3].entries.push_back(BarItem("Controls: in the launcher", 0, "", false));
    // Help: the keys, and what this is
    recomp::BarEntry keys = BarItem("Keys", 0);
    for (const auto &key : kKeys) {
        keys.children.push_back(BarItem(key[0], kBarKey, key[1]));
    }
    menus[5].entries.push_back(std::move(keys));
    menus[5].entries.push_back(BarItem("About", kBarAbout));
    // Online, what would change the match or put the two boards out of step is dim (as the in-game menu says
    // "Not while playing online"): the save states, Reset, player 2's Start, the game's features and its speed
    if (port.online) {
        for (recomp::BarMenu &menu : menus) {
            for (recomp::BarEntry &entry : menu.entries) {
                const std::string &l = entry.label;
                if (menu.title == "Features" || l == "Save state" || l == "Load state" || l == "Quick save" || l == "Quick load" ||
                    l == "Reset" || l == "Player 2 joins") {
                    entry.enabled = false;
                }
            }
        }
    }
    // (a menu with nothing in it is left out)
    menus.erase(std::remove_if(menus.begin(), menus.end(), [](const recomp::BarMenu &m) { return m.entries.empty(); }), menus.end());
    bar.Configure(std::move(menus));
}

int BarDot(int windowHeight) {
    // (a window opens the board's lines and the bar's dots tall, so many times: a dot is as many pixels)
    return std::max(1, windowHeight / (NeoGeoVideo::kHeight + recomp::MenuBar::kHeightDots));
}

int BarStrip(int windowHeight) {
    return recomp::MenuBar::kHeightDots * BarDot(windowHeight);
}

void BarPicture(const recomp::MenuBar &bar, int width, int height, std::vector<uint32_t> &pixels, std::vector<SDL_Rect> &regions) {
    pixels.assign(static_cast<size_t>(std::max(width, 0)) * std::max(height, 0), 0xFF000000u);
    regions.clear();
    if (width <= 0 || height <= 0) {
        return;
    }
    bar.Draw(pixels.data(), width, height);
    if (bar.BarShown()) {
        regions.push_back({0, 0, width, bar.Height()});
    }
    for (const recomp::MenuBar::Box &box : bar.Boxes()) {
        regions.push_back({static_cast<int>(box.x), static_cast<int>(box.y), static_cast<int>(std::ceil(box.w)), static_cast<int>(std::ceil(box.h))});
    }
}

int RunNeoGeoWindow(NeoGeoPort &port) {
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "256");
    if (!port.opt.settingsPath.empty()) {
        g_bindings = LoadBindings(port.opt.settingsPath, Table());
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    const int scale = port.opt.scale > 0 ? port.opt.scale : WindowSize();
    const int height = NeoGeoVideo::kHeight * scale;
    auto windowShape = []() {
        return PictureShape(DisplayShape() == Shape::Arcade ? 384 : NeoGeoVideo::kWidth, NeoGeoVideo::kHeight, DisplayShape(), 4.0f / 3.0f);
    };
    // (and above the picture the menu bar's strip, as many times its dots: none full screen)
    const int strip = port.opt.fullscreen ? 0 : recomp::MenuBar::kHeightDots * scale;
    SDL_Window *window = SDL_CreateWindow(GAME_TITLE, static_cast<int>(std::lround(height * windowShape())), height + strip,
                                          SDL_WINDOW_RESIZABLE | (port.opt.fullscreen ? SDL_WINDOW_FULLSCREEN : 0) |
                                              (getenv("CPS2RECOMP_HIDDEN_WINDOW") ? SDL_WINDOW_HIDDEN : 0)); // (hidden: for tests)
    Presenter presenter;
    std::string presentError;
    const bool wantGpu = port.opt.renderer != "sdl" && strcmp(SDL_GetCurrentVideoDriver(), "dummy") != 0;
    if (!window || !presenter.Open(window, wantGpu, presentError)) {
        fprintf(stderr, "SDL window: %s\n", window ? presentError.c_str() : SDL_GetError());
        return 1;
    }

    // The board's sound at the chip's own rate, kept a frame or two ahead of the device, played a touch faster or
    // slower as it piles up or runs out (as the Capcom games' is)
    constexpr double kAudioWaiting = 0.028, kAudioMost = 0.11;
    NeoGeoSound &sound = port.board->Sound();
    SDL_AudioStream *audio = nullptr;
    {
        const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, static_cast<int>(NeoGeoSound::SampleRate())};
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (audio) {
            SDL_ResumeAudioStreamDevice(audio);
        } else {
            fprintf(stderr, "Audio: %s\n", SDL_GetError());
        }
    }
    sound.keep = audio != nullptr;
    bool silent = false;

    // Online (neogeo_online.h): the session runs the frames, with this side's controls as its player's
    NeoGeoOnline online;
    const bool isOnline = port.opt.online.mode != OnlineOptions::Mode::Off;
    if (isOnline) {
        std::string error;
        if (!online.Start(port, port.opt.online, error)) {
            fprintf(stderr, "online: %s\n", error.c_str());
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GAME_TITLE, ("Online: " + error).c_str(), window);
            return 1;
        }
        const OnlineOptions::Mode mode = port.opt.online.mode;
        const char *who = mode == OnlineOptions::Mode::Host ? "player 1" : mode == OnlineOptions::Mode::Join ? "player 2" : "stress test";
        SDL_SetWindowTitle(window, (std::string(GAME_TITLE) + " - online, " + who).c_str());
    }

    std::vector<SDL_Gamepad *> gamepads;
    std::vector<uint32_t> menuBase, menuPicture, shown;
    uint32_t menuWidth = 0, menuHeight = 0;
    bool menuDirty = false;
    uint16_t previousPad = 0, heldAtClose = 0;
    unsigned previousMenu = 0;
    bool bothHeld = false;
    auto toggleMenu = [&]() {
        if (port.menu.IsOpen()) {
            port.menu.Close();
        } else {
            port.MenuPicture(menuBase, menuWidth, menuHeight);
            port.menu.Open(true);
            menuDirty = true;
        }
    };
    auto quickState = [&](bool save) {
        if (port.online) {
            return; // (no save states online)
        }
        std::string error;
        const std::string path = port.StatePath(port.menu.Slot());
        if (!(save ? port.SaveState(path, error) : port.LoadState(path, error))) {
            fprintf(stderr, "state %d: %s\n", port.menu.Slot(), error.c_str());
        }
    };
    // The menu bar above the picture, and the same menus where the picture is right-clicked (neogeo_port.h's Bar...).
    // While one of its menus is open the game stands still, as with the in-game menu (online it goes on, as there); a
    // click or a key that works the bar does not reach the game.
    recomp::MenuBar bar;
    std::vector<uint32_t> barPixels;
    std::vector<SDL_Rect> barRegions;
    bool barDirty = true;
    int barWidth = 0, barHeight = 0;
    auto isFullscreen = [&]() { return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0; };
    // Full screen there is no strip: the bar is drawn over the top of the picture while the pointer is at the
    // screen's top edge, or one of its menus is open (not the right-click menu's, which needs no bar)
    bool barRevealed = false, barPopup = false;
    auto showBar = [&]() {
        const bool shownNow = !isFullscreen() || barRevealed || (bar.IsOpen() && !barPopup);
        if (shownNow != bar.BarShown()) {
            bar.ShowBar(shownNow);
            barDirty = true;
        }
    };
    // (no wider than the board's picture says: 53 of its letters)
    const std::string kFullscreenHint = "F11 leaves full screen. Menu: top, right click, F10";
    auto fullscreenHint = [&]() {
        port.notice = kFullscreenHint;
        port.noticeUntil = port.tick + 300;
    };
    auto toggleFullscreen = [&]() {
        const bool to = !isFullscreen();
        SDL_SetWindowFullscreen(window, to);
        barRevealed = false;
        if (to) {
            fullscreenHint();
        } else if (port.notice == kFullscreenHint) {
            port.noticeUntil = 0; // (back in a window, the hint has nothing to say)
        }
        barDirty = true;
    };
    if (port.opt.fullscreen) {
        fullscreenHint();
    }
    auto barOpened = [&](bool popup = false) {
        BarMenus(port, bar, isFullscreen());
        barPopup = popup;
        barDirty = true;
    };
    BarMenus(port, bar, isFullscreen());
    auto screenshot = [&]() {
        std::string file, error;
        if (port.Screenshot(file, error)) {
            port.notice = "Screenshot: " + file;
        } else {
            port.notice = "No screenshot: " + error;
            fprintf(stderr, "screenshot: %s\n", error.c_str());
        }
        port.noticeUntil = port.tick + 150;
    };
    auto barChose = [&](const recomp::MenuBar::Chosen &chosen) {
        barDirty = true;
        heldAtClose = previousPad;
        if (!chosen.chosen) {
            return;
        }
        if (recomp::IsMenuEntry(chosen.id)) {
            port.MenuAct(port.menu.Choose(chosen.id));
        } else if (chosen.id == kBarQuickSave || chosen.id == kBarQuickLoad) {
            if (!port.online) {
                quickState(chosen.id == kBarQuickSave);
            }
        } else if (chosen.id == kBarFullscreen) {
            toggleFullscreen();
        } else if (chosen.id == kBarScreenshot) {
            screenshot();
        } else if (chosen.id == kBarAbout) {
            port.notice = std::string(GAME_TITLE) + " - ForkAde, native by recompilation";
            port.noticeUntil = port.tick + 240;
        }
        BarMenus(port, bar, isFullscreen());
    };
    // (the pointer's place in the window's pixels: events give it in the window's points)
    auto pixelsOf = [&](float x, float y, float &px, float &py) {
        const float density = SDL_GetWindowPixelDensity(window);
        px = x * (density > 0 ? density : 1.0f);
        py = y * (density > 0 ? density : 1.0f);
    };
    // (tests: --window-events, what is put to the window at which turn of the loop; a turn is a frame, or a
    // wait of the menus')
    struct Scripted {
        uint64_t loop;
        std::string what;
        float x = 0, y = 0;
        std::string key;
    };
    std::vector<Scripted> script;
    for (size_t at = 0; at < port.opt.windowEvents.size();) {
        size_t end = port.opt.windowEvents.find(';', at);
        if (end == std::string::npos) {
            end = port.opt.windowEvents.size();
        }
        const std::string one = port.opt.windowEvents.substr(at, end - at);
        const size_t colon = one.find(':');
        if (colon != std::string::npos) {
            Scripted s{strtoull(one.c_str(), nullptr, 10), {}};
            char what[8] = {};
            if (sscanf(one.c_str() + colon + 1, "%7s %f %f", what, &s.x, &s.y) >= 1) {
                s.what = what;
                // (a key's name is the rest: "Left Alt")
                const size_t space = one.find(' ', colon + 1);
                if (space != std::string::npos) {
                    s.key = one.substr(space + 1);
                }
                script.push_back(s);
            }
        }
        at = end + 1;
    }
    uint64_t loops = 0;
    // The pointer is hidden over the game after a moment still, and seen again when it moves
    uint64_t pointerMoved = SDL_GetTicksNS();
    bool pointerShown = true;
    bool altAlone = false; // (Alt went down, and nothing else since: let go, it opens the bar's Game menu)
    const double kFrameNs = 1e9 * NeoGeo::kLines * NeoGeo::kCpuPerLine / 12000000.0; // (59.19 a second)
    uint64_t nextFrame = SDL_GetTicksNS();
    auto feedAudio = [&](bool ran, int slowBy) {
            if (audio && silent != port.menu.muted) {
                silent = port.menu.muted;
                SDL_SetAudioStreamGain(audio, silent ? 0.0f : 1.0f);
            }
            if (audio && ran && !sound.made.empty()) {
                double waiting = SDL_GetAudioStreamQueued(audio) / (4.0 * NeoGeoSound::SampleRate()) * slowBy;
                if (waiting > kAudioMost) {
                    SDL_ClearAudioStream(audio);
                    waiting = 0;
                }
                const double ratio = (1.0 + std::clamp((waiting - kAudioWaiting) * 0.5, -0.02, 0.02)) / slowBy;
                SDL_SetAudioStreamFrequencyRatio(audio, static_cast<float>(ratio));
                SDL_PutAudioStreamData(audio, sound.made.data(), static_cast<int>(sound.made.size() * sizeof(int16_t)));
            }
            sound.made.clear();
    };
    // A frame's time, against the clock (online, a touch longer while this side is ahead of the other's)
    auto pace = [&]() {
        nextFrame += static_cast<uint64_t>(kFrameNs * (isOnline && online.FramesAhead() > 1.0f ? 1.016 : 1.0));
        const uint64_t now = SDL_GetTicksNS();
        if (nextFrame > now) {
            SDL_DelayPrecise(nextFrame - now);
        } else if (now - nextFrame > 100000000ull) {
            nextFrame = now;
        }
    };
    bool running = true;
    while (running && !port.quit && (port.opt.frames == 0 || port.frame < port.opt.frames)) {
        for (const Scripted &s : script) {
            if (s.loop != loops) {
                continue;
            }
            SDL_Event put{};
            const SDL_WindowID id = SDL_GetWindowID(window);
            if (s.what == "m") {
                put.type = SDL_EVENT_MOUSE_MOTION;
                put.motion.windowID = id;
                put.motion.x = s.x;
                put.motion.y = s.y;
                SDL_PushEvent(&put);
            } else if (s.what == "l" || s.what == "r") {
                put.button.windowID = id;
                put.button.button = s.what == "l" ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT;
                put.button.x = s.x;
                put.button.y = s.y;
                put.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                put.button.down = true;
                SDL_PushEvent(&put);
                put.type = SDL_EVENT_MOUSE_BUTTON_UP;
                put.button.down = false;
                SDL_PushEvent(&put);
            } else if (s.what == "k" || s.what == "kd" || s.what == "ku") {
                put.key.windowID = id;
                put.key.scancode = SDL_GetScancodeFromName(s.key.c_str());
                put.key.key = SDL_GetKeyFromScancode(put.key.scancode, SDL_KMOD_NONE, false);
                if (s.what != "ku") {
                    put.type = SDL_EVENT_KEY_DOWN;
                    put.key.down = true;
                    SDL_PushEvent(&put);
                }
                if (s.what != "kd") {
                    put.type = SDL_EVENT_KEY_UP;
                    put.key.down = false;
                    SDL_PushEvent(&put);
                }
            } else if (s.what == "p") {
                int windowWidth = 0, windowHeight = 0;
                SDL_GetWindowSize(window, &windowWidth, &windowHeight);
                std::string picked;
                for (const int p : bar.Picked()) {
                    picked += (picked.empty() ? "" : ",") + std::to_string(p);
                }
                printf("window %llu: bar %s [%s] (%zu open, strip %s), in-game menu %s, frame %llu, pointer %s, fullscreen %s, "
                       "window %dx%d, slot %d, muted %s, slow by %d, notice \"%s\", online %s\"%s\"\n",
                       static_cast<unsigned long long>(loops), bar.IsOpen() ? "open" : "closed", picked.c_str(), bar.Boxes().size(),
                       bar.BarShown() ? "shown" : "hidden", port.menu.IsOpen() ? "open" : "closed", static_cast<unsigned long long>(port.frame),
                       pointerShown ? "shown" : "hidden", isFullscreen() ? "yes" : "no", windowWidth, windowHeight, port.menu.Slot(),
                       port.menu.muted ? "yes" : "no", port.menu.SlowBy(), port.tick < port.noticeUntil ? port.notice.c_str() : "",
                       online.Failed() ? "FAILED " : "", online.Message().c_str());
                fflush(stdout);
            }
        }
        loops++;
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_EVENT_QUIT: running = false; break;
            case SDL_EVENT_WINDOW_FOCUS_LOST: altAlone = false; break;
            case SDL_EVENT_MOUSE_MOTION:
                pointerMoved = SDL_GetTicksNS();
                if (!pointerShown) {
                    SDL_ShowCursor();
                    pointerShown = true;
                }
                {
                    float x, y;
                    pixelsOf(ev.motion.x, ev.motion.y, x, y);
                    if (isFullscreen()) {
                        // (at the top edge the bar comes; below it, with none of its menus open, it goes)
                        if (y < 4) {
                            barRevealed = true;
                        } else if (y >= bar.Height() && !bar.IsOpen()) {
                            barRevealed = false;
                        }
                        showBar();
                    }
                    if (bar.IsOpen() && !port.menu.IsOpen()) {
                        barDirty = bar.Pointer(x, y) || barDirty;
                    }
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (!port.menu.IsOpen()) {
                    float x, y;
                    pixelsOf(ev.button.x, ev.button.y, x, y);
                    if (ev.button.button == SDL_BUTTON_LEFT && (bar.IsOpen() || bar.Hit(x, y))) {
                        if (!bar.IsOpen()) {
                            barOpened();
                        }
                        barChose(bar.Click(x, y));
                    } else if (ev.button.button == SDL_BUTTON_RIGHT) {
                        barOpened(true);
                        bar.OpenPopup(x, y);
                    } else if (bar.IsOpen()) {
                        bar.Close();
                        barDirty = true;
                    }
                }
                break;
            case SDL_EVENT_KEY_UP:
                if ((ev.key.scancode == SDL_SCANCODE_LALT || ev.key.scancode == SDL_SCANCODE_RALT) && altAlone &&
                    !KeyBound(ev.key.scancode) && !port.menu.IsOpen()) {
                    if (bar.IsOpen()) {
                        bar.Close();
                        barDirty = true;
                    } else {
                        barOpened();
                        bar.OpenMenu(0);
                    }
                }
                altAlone = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (!ev.key.repeat) {
                    altAlone = ev.key.scancode == SDL_SCANCODE_LALT || ev.key.scancode == SDL_SCANCODE_RALT;
                }
                if (bar.IsOpen() && ev.key.scancode == SDL_SCANCODE_ESCAPE) {
                    barChose(bar.Buttons(recomp::kMenuBack)); // (a level of the bar's menus closes)
                } else if (bar.IsOpen() && !port.menu.IsOpen() && ev.key.scancode != SDL_SCANCODE_F10) {
                    // While the bar's menu is open the keys work it, and nothing else (the gamepad: below)
                    unsigned button = 0;
                    switch (ev.key.scancode) {
                    case SDL_SCANCODE_UP: button = recomp::kMenuUp; break;
                    case SDL_SCANCODE_DOWN: button = recomp::kMenuDown; break;
                    case SDL_SCANCODE_LEFT: button = recomp::kMenuLeft; break;
                    case SDL_SCANCODE_RIGHT: button = recomp::kMenuRight; break;
                    case SDL_SCANCODE_RETURN:
                    case SDL_SCANCODE_KP_ENTER:
                    case SDL_SCANCODE_SPACE: button = (ev.key.mod & SDL_KMOD_ALT) ? 0 : recomp::kMenuConfirm; break;
                    case SDL_SCANCODE_BACKSPACE: button = recomp::kMenuBack; break;
                    default: break;
                    }
                    if (button) {
                        barChose(bar.Buttons(button));
                    }
                } else if (ev.key.scancode == SDL_SCANCODE_F10 && !KeyBound(SDL_SCANCODE_F10) && !port.menu.IsOpen()) {
                    if (bar.IsOpen()) {
                        bar.Close();
                        barDirty = true;
                    } else {
                        barOpened();
                        bar.OpenMenu(0);
                    }
                } else if (ev.key.scancode == SDL_SCANCODE_ESCAPE || ev.key.scancode == SDL_SCANCODE_F1) {
                    toggleMenu();
                } else if (KeyBound(ev.key.scancode)) {
                    // (a key the player has given to the game does only that)
                } else if (ev.key.scancode == SDL_SCANCODE_F5 || ev.key.scancode == SDL_SCANCODE_F9) {
                    if (!port.menu.IsOpen()) {
                        quickState(ev.key.scancode == SDL_SCANCODE_F5);
                    }
                } else if (ev.key.scancode == SDL_SCANCODE_F8 && !ev.key.repeat) {
                    if (!port.menu.IsOpen()) {
                        screenshot(); // the board's picture, as it is now
                    }
                } else if (ev.key.scancode == SDL_SCANCODE_F6 || ev.key.scancode == SDL_SCANCODE_F7) {
                    port.menu.SetSlot(port.menu.Slot() + (ev.key.scancode == SDL_SCANCODE_F7 ? 1 : -1));
                } else if (!port.menu.IsOpen() && ev.key.scancode == SDL_SCANCODE_P && !ev.key.repeat) {
                    port.Key('p');
                } else if (!port.menu.IsOpen() && ev.key.scancode == SDL_SCANCODE_O) {
                    port.Key('o');
                } else if (ev.key.scancode == SDL_SCANCODE_MINUS || ev.key.scancode == SDL_SCANCODE_EQUALS) {
                    port.Key(ev.key.scancode == SDL_SCANCODE_MINUS ? '-' : '=');
                } else if (ev.key.scancode == SDL_SCANCODE_F2) {
                    port.test = !port.test; // the operator's test switch
                } else if (ev.key.scancode == SDL_SCANCODE_F11 ||
                           (ev.key.scancode == SDL_SCANCODE_RETURN && (ev.key.mod & SDL_KMOD_ALT))) {
                    toggleFullscreen();
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                if (PadInputBound(g_bindings, Table(), {ev.gbutton.button, -1})) {
                    // (a button the player has given to the game does only that)
                } else if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_STICK || ev.gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE) {
                    toggleMenu();
                } else if (!port.menu.IsOpen() && ev.gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_STICK) {
                    port.Key('p');
                }
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                if (SDL_Gamepad *gp = SDL_OpenGamepad(ev.gdevice.which)) {
                    gamepads.push_back(gp);
                }
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                for (auto it = gamepads.begin(); it != gamepads.end(); ++it) {
                    if (SDL_GetGamepadID(*it) == ev.gdevice.which) {
                        SDL_CloseGamepad(*it);
                        gamepads.erase(it);
                        break;
                    }
                }
                break;
            default: break;
            }
        }

        // The keyboard is both players' (each has the keys chosen for it); the first gamepad is player 1's, a second
        // player 2's
        uint16_t pads[2] = {};
        const bool *keys = SDL_GetKeyboardState(nullptr);
        if (!(SDL_GetModState() & SDL_KMOD_ALT)) {
            for (int player = 0; player < 2 && player < Table().table.players; player++) {
                pads[player] |= ReadKeyboard(keys, player);
            }
        }
        bool both = false;
        for (size_t i = 0; i < gamepads.size() && i < 2; i++) {
            pads[i] |= ReadGamepad(gamepads[i], static_cast<int>(i));
            both = both || (SDL_GetGamepadButton(gamepads[i], SDL_GAMEPAD_BUTTON_START) &&
                            SDL_GetGamepadButton(gamepads[i], SDL_GAMEPAD_BUTTON_BACK));
        }
        const uint16_t all = pads[0] | pads[1];
        uint16_t pressed = all & ~previousPad;
        previousPad = all;
        if (both && !bothHeld) {
            toggleMenu();
            pressed = 0;
            heldAtClose = previousPad;
        }
        bothHeld = both;
        // The menu's own buttons: the arrows, the d-pad and the left stick (and the game's directions); Space, Enter
        // and A choose; Backspace and B go back
        unsigned menuNow = 0, padMenu = 0;
        menuNow |= (previousPad & cps::kUp) || keys[SDL_SCANCODE_UP] ? recomp::kMenuUp : 0;
        menuNow |= (previousPad & cps::kDown) || keys[SDL_SCANCODE_DOWN] ? recomp::kMenuDown : 0;
        menuNow |= (previousPad & cps::kLeft) || keys[SDL_SCANCODE_LEFT] ? recomp::kMenuLeft : 0;
        menuNow |= (previousPad & cps::kRight) || keys[SDL_SCANCODE_RIGHT] ? recomp::kMenuRight : 0;
        menuNow |= keys[SDL_SCANCODE_SPACE] || (keys[SDL_SCANCODE_RETURN] && !(SDL_GetModState() & SDL_KMOD_ALT)) ? recomp::kMenuConfirm : 0;
        menuNow |= keys[SDL_SCANCODE_BACKSPACE] ? recomp::kMenuBack : 0;
        for (SDL_Gamepad *gp : gamepads) {
            menuNow |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_UP) ? recomp::kMenuUp : 0;
            menuNow |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_DOWN) ? recomp::kMenuDown : 0;
            menuNow |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_LEFT) ? recomp::kMenuLeft : 0;
            menuNow |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) ? recomp::kMenuRight : 0;
            menuNow |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_SOUTH) ? recomp::kMenuConfirm : 0;
            menuNow |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_EAST) ? recomp::kMenuBack : 0;
        }
        // (the gamepad's part of it)
        for (SDL_Gamepad *gp : gamepads) {
            padMenu |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_UP) ? recomp::kMenuUp : 0;
            padMenu |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_DOWN) ? recomp::kMenuDown : 0;
            padMenu |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_LEFT) ? recomp::kMenuLeft : 0;
            padMenu |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) ? recomp::kMenuRight : 0;
            padMenu |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_SOUTH) ? recomp::kMenuConfirm : 0;
            padMenu |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_EAST) ? recomp::kMenuBack : 0;
        }
        unsigned menuPressed = menuNow & ~previousMenu;
        previousMenu = menuNow;
        if (both) {
            menuPressed = 0;
        }
        if (bar.IsOpen() && !port.menu.IsOpen()) {
            // (the gamepad's presses: the keys' came as events)
            if (menuPressed & padMenu) {
                barChose(bar.Buttons(menuPressed & padMenu));
            }
            heldAtClose = previousPad;
        } else if (port.menu.IsOpen()) {
            if (menuPressed) {
                port.MenuAct(port.menu.Press(menuPressed));
                menuDirty = true;
            } else if (pressed) {
                port.menu.Note("");
                menuDirty = true;
            }
            if (!port.menu.IsOpen()) {
                heldAtClose = previousPad;
            }
        }
        heldAtClose &= previousPad;
        const uint16_t players[2] = {ToNeoGeo(pads[0] & ~heldAtClose), ToNeoGeo(pads[1] & ~heldAtClose)};
        uint16_t service = keys[SDL_SCANCODE_9] && !KeyBound(SDL_SCANCODE_9) ? NeoGeo::kService : 0;

        // The window takes the Display page's shape and size again when they change: a window only, not full screen
        // or maximized
        {
            static int shownSize = scale;
            static Shape shownShape = DisplayShape();
            const int size = port.opt.scale > 0 ? port.opt.scale : WindowSize();
            if (DisplayShape() != shownShape || size != shownSize) {
                if (!(SDL_GetWindowFlags(window) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED))) {
                    int windowWidth = 0, windowHeight = 0;
                    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
                    // (the picture's part of it: below the bar's strip, in the window's points)
                    const float density = SDL_GetWindowPixelDensity(window) > 0 ? SDL_GetWindowPixelDensity(window) : 1.0f;
                    int pictureHeight = windowHeight - static_cast<int>(std::lround(BarStrip(static_cast<int>(windowHeight * density)) / density));
                    if (size != shownSize) {
                        pictureHeight = NeoGeoVideo::kHeight * size;
                        windowHeight = pictureHeight + recomp::MenuBar::kHeightDots * size;
                    }
                    SDL_SetWindowSize(window, static_cast<int>(std::lround(pictureHeight * windowShape())), windowHeight);
                }
                shownShape = DisplayShape();
                shownSize = size;
            }
        }
        if (pointerShown && !bar.IsOpen() && !port.menu.IsOpen() && SDL_GetTicksNS() - pointerMoved > 2000000000ull) {
            SDL_HideCursor();
            pointerShown = false;
        }
        int width = 0, height = 0;
        presenter.Size(width, height);
        presenter.SetScaling(DisplayScaling());
        // (in a window, below the menu bar's strip; full screen, the whole screen)
        const int barStrip = isFullscreen() ? 0 : BarStrip(height);
        SDL_FRect where = Where(width, height - barStrip);
        where.y += barStrip;
        showBar();
        if (barDirty || width != barWidth || height != barHeight) {
            bar.SetArea(width, height, BarDot(height));
            BarPicture(bar, width, height, barPixels, barRegions);
            presenter.SetOverlay(barPixels, width, height, barRegions);
            barDirty = false;
            barWidth = width;
            barHeight = height;
        }
        if (bar.IsOpen() && !port.menu.IsOpen()) {
            // The game's picture as it was, with the bar's menu open over it
            if (shown.empty()) {
                port.Shown(shown);
            }
            presenter.Show(shown.data(), NeoGeoVideo::kWidth, NeoGeoVideo::kHeight, where);
            if (isOnline) {
                // (online, the game goes on behind the menu, as behind the in-game menu: this side's controls let go)
                const uint16_t none[2] = {};
                feedAudio(online.Tick(none) > 0, 1);
                pace();
                continue;
            }
            SDL_Delay(8);
            nextFrame = SDL_GetTicksNS();
            continue;
        }

        if (port.menu.IsOpen()) {
            if (menuDirty) {
                menuDirty = false;
                menuPicture = menuBase;
                port.menu.Draw(menuPicture.data(), static_cast<int>(menuWidth), static_cast<int>(menuHeight));
            }
            presenter.Show(menuPicture.data(), static_cast<int>(menuWidth), static_cast<int>(menuHeight), where);
            if (isOnline) {
                // (online, the game goes on behind the menu, with this side's controls let go)
                const uint16_t none[2] = {};
                feedAudio(online.Tick(none) > 0, 1);
                pace();
                continue;
            }
            SDL_Delay(8);
            nextFrame = SDL_GetTicksNS();
            continue;
        }

        const uint16_t withService[2] = {static_cast<uint16_t>(players[0] | service), players[1]};
        const int slowBy = port.paused || isOnline ? 1 : port.menu.SlowBy();
        bool ran;
        if (isOnline) {
            // (this side's player: the keyboard's two players' keys and the gamepads together; the stress test's two)
            const bool stress = port.opt.online.mode == OnlineOptions::Mode::Stress;
            const uint16_t local[2] = {static_cast<uint16_t>(stress ? players[0] : players[0] | players[1]), stress ? players[1] : uint16_t{0}};
            ran = online.Tick(local) > 0;
        } else {
            ran = port.Tick(withService);
        }

        feedAudio(ran, slowBy);

        port.Shown(shown);
        if (isOnline) {
            DrawOnline(shown, online);
        }
        presenter.Show(shown.data(), NeoGeoVideo::kWidth, NeoGeoVideo::kHeight, where);
        pace();
    }
    for (SDL_Gamepad *gp : gamepads) {
        SDL_CloseGamepad(gp);
    }
    if (audio) {
        SDL_DestroyAudioStream(audio);
    }
    presenter.Close();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace port
