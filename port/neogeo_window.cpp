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
#include "../runtime/state.h"
#include "../recomp-common/text.h"
#include "../recomp-common/settings.h"
#include "../recomp-common/folders.h"
#include "../recomp-common/volume.h"
#include "bindings.h"
#include "neogeo_online.h"
#include "neogeo_port.h"
#include "present.h"
#include "png_write.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>
#include <sstream>
#include <ctime>
#include <mutex>
#ifdef FORKADE_GAME_BROKER
#include "game_room.h"
#endif

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
    // playing the ping, delay and rollbacks in rows at the top right (online-hud: see-through, in the small yellow
    // font, one value a row, ending at row 17, above the life bars; Tab hides them, not the session's messages)
    void DrawOnline(std::vector<uint32_t> &pixels, const NeoGeoOnline &online, bool overlayHidden) {
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
        std::vector<std::string> right;
        std::string left;
        online.StatRows(right, left);
        if (overlayHidden) {
            return;
        }
        using recomp::text::Palette;
        const float step = recomp::text::kSmallLineHeight;
        for (size_t n = 0; n < right.size() && n < 3; n++) {
            const float wide = recomp::text::SmallTextWidth(right[n].c_str());
            // (flush with the right edge: in the KOFs the rows then sit over player 2's portrait, clear of the end
            // of the life bar, which stops at about column 280)
            canvas.Fill(kWidth - wide - 1, step * n, wide + 1, step, Palette::kBand);
            canvas.SmallText(kWidth - wide + 1, 1 + step * n, Palette::kReadout, right[n].c_str());
        }
        if (!left.empty()) {
            const float wide = recomp::text::SmallTextWidth(left.c_str());
            canvas.Fill(0, 0, wide + 3, step, Palette::kBand);
            canvas.SmallText(2, 1, Palette::kReadout, left.c_str());
        }
    }
    // The menu bar's own entries (the in-game menu's have ids of their own: recomp::IsMenuEntry)
    enum { kBarQuickSave = 1, kBarQuickLoad, kBarFullscreen, kBarAbout, kBarKey, kBarScreenshot, kBarSource };
    enum { kReplayRecord=100, kReplayPlay, kReplayStop, kReplayOpen, kReplayFolder, kReplayCommandBase=200 };
    enum { kInputPanelBase=400 };
    struct ReplayDialogState {std::mutex mutex;bool busy=false,ready=false,folder=false;std::string path,error;};
    struct ReplayDialogRequest {std::weak_ptr<ReplayDialogState> state;bool folder;std::string initial;};
    void SDLCALL ReplayDialogDone(void* data,const char* const* files,int) {
        std::unique_ptr<ReplayDialogRequest> request(static_cast<ReplayDialogRequest*>(data));
        if(auto state=request->state.lock()) {
            std::lock_guard<std::mutex> lock(state->mutex);state->busy=false;state->ready=true;state->folder=request->folder;
            state->path=files&&files[0]?files[0]:"";state->error=files?"":SDL_GetError();
        }
    }
    std::filesystem::path ReplayFolder(const NeoGeoPort& port) {
        if(!port.opt.autoReplayDir.empty())return std::filesystem::u8path(port.opt.autoReplayDir);
        if(!port.opt.savesPath.empty())return std::filesystem::u8path(port.opt.savesPath).parent_path()/"replays";
        char* pref=SDL_GetPrefPath("ForkAde",GAME_ID);const auto path=std::filesystem::u8path(pref?pref:"")/"replays";SDL_free(pref);return path;
    }

    // The keys that work while playing, for Help's list (each an entry that does nothing)
    const char *const kKeys[][2] = {
        {"In-game menu", "Esc, F1"}, {"Menu bar", "F10, Alt"}, {"Quick save", "F5"}, {"Quick load", "F9"},
        {"Screenshot", "F8"},        {"Slot before, next", "F6, F7"}, {"Fullscreen", "F11"}, {"Test switch", "F2"},
        {"Pause", "P"},              {"One frame", "O"},              {"Slower, faster", "[, ]"},
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
    auto inputPanel=BarItem("Neo Geo input panel",0);
    for(int mode=0;mode<3;++mode){
        auto e=BarItem(mode==0?"Automatic":mode==1?"On":"Off",kInputPanelBase+mode);
        e.mark=cps2recomp::FeatureSetting("input_display")==mode?recomp::BarEntry::Dot:recomp::BarEntry::NoMark;
        inputPanel.children.push_back(e);
    }
    menus[3].entries.push_back(std::move(inputPanel));
    // Help: the keys, and what this is
    recomp::BarEntry keys = BarItem("Keys", 0);
    for (const auto &key : kKeys) {
        keys.children.push_back(BarItem(key[0], kBarKey, key[1]));
    }
    menus[5].entries.push_back(std::move(keys));
    menus[5].entries.push_back(BarItem("About", kBarAbout));
    menus[5].entries.push_back(BarItem("Source code and licences", kBarSource));
    const bool watching=port.replay&&port.replay->Playing();
    recomp::BarEntry replays=BarItem("Replay",0);
    replays.children={BarItem(port.replay&&port.replay->Recording()?"Stop recording":"Record from here",kReplayRecord,"",!port.online&&!watching),BarItem("Play latest replay",kReplayPlay,"",!port.online&&!watching),BarItem("Stop playback",kReplayStop,"",watching)};
    replays.children.insert(replays.children.begin()+1,BarItem("Open replay...",kReplayOpen,"",!port.online));
    replays.children.push_back(BarItem("Recording folder...",kReplayFolder));
    using RC=recomp::replay::Command;
    const std::pair<const char*,RC> controls[]={{"Play / pause",RC::PlayPause},{"Rewind x1-x8",RC::Rewind},{"Forward x2-x8",RC::Faster},{"Slow motion",RC::Slower},{"Normal speed",RC::Normal},{"Previous frame",RC::StepBack},{"Next frame",RC::StepForward},{"Back 5 seconds",RC::JumpBack},{"Forward 5 seconds",RC::JumpForward},{"Start",RC::ToStart},{"End",RC::ToEnd},{"Previous round marker",RC::PreviousMarker},{"Next round marker",RC::NextMarker},{"Mark A",RC::MarkA},{"Mark B",RC::MarkB},{"Clear loop",RC::ClearLoop},{"Frame counter",RC::FrameCounter},{"Pin HUD",RC::PinHud},{"Show / hide HUD",RC::ToggleHud}};
    recomp::BarEntry groups[]={BarItem("Playback",0,"",watching),BarItem("Navigation",0,"",watching),BarItem("A-B loop",0,"",watching),BarItem("HUD",0,"",watching)};
    for(const auto& [label,command]:controls){
        int group=1;
        if(command==RC::PlayPause||command==RC::Rewind||command==RC::Faster||command==RC::Slower||command==RC::Normal)group=0;
        if(command==RC::MarkA||command==RC::MarkB||command==RC::ClearLoop)group=2;
        if(command==RC::FrameCounter||command==RC::PinHud||command==RC::ToggleHud)group=3;
        groups[group].children.push_back(BarItem(label,kReplayCommandBase+static_cast<int>(command),"",watching));
    }
    for(auto& group:groups)replays.children.push_back(std::move(group));
    menus[0].entries.insert(menus[0].entries.begin(),std::move(replays));
    if (!port.online) {
        for (auto &entry : menus[0].entries)
            if (entry.label == "Chat" || entry.label == "Chat history") entry.enabled = false;
    }
    // Online, what would change the match or put the two boards out of step is dim (as the in-game menu says
    // "Not while playing online"): the save states, Reset, player 2's Start, the game's features and its speed
    if (port.online || watching) {
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
    float gainNow = 1.0f; // (game-volume-control: the stream's gain now)

    // Online (neogeo_online.h): the session runs the frames, with this side's controls as its player's
#ifdef FORKADE_GAME_BROKER
    if(port.opt.forkadeOnline){
        std::filesystem::path folder;
        if(const auto home=SDL_getenv("CPS2RECOMP_HOME");home&&*home)folder=std::filesystem::u8path(home);
        else {
            const char* base=SDL_GetBasePath();
            folder=recomp::PortableFolder(base?base:"");
            const auto name=recomp::FolderName("CPS-2 Recomp");
            if(!folder.empty())folder/=name;
            else if(char* pref=SDL_GetPrefPath("",name.c_str())){folder=std::filesystem::u8path(pref);SDL_free(pref);}
        }
        recomp::Ini preferences(folder/"launcher.ini");preferences.load();
        if(port.opt.online.name.empty())port.opt.online.name=preferences.get("online","name");
        if(port.opt.forkadeService.empty())port.opt.forkadeService=preferences.get("online","service");
        if(port.opt.forkadeService.empty())port.opt.forkadeService="off";
    }
#endif
    NeoGeoOnline online;
    bool isOnline = port.opt.online.mode != OnlineOptions::Mode::Off;
    if (isOnline) {
        std::string error;
        if (!online.Start(port, port.opt.online, error)) {
            fprintf(stderr, "online: %s\n", error.c_str());
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GAME_TITLE, ("Online: " + error).c_str(), window);
            return 1;
        }
        const OnlineOptions::Mode mode = port.opt.online.mode;
        const char *who = mode == OnlineOptions::Mode::Host ? "player 1" : mode == OnlineOptions::Mode::Join ? "player 2"
                        : mode == OnlineOptions::Mode::Watch ? "watching" : "stress test";
        SDL_SetWindowTitle(window, (std::string(GAME_TITLE) + " - online, " + who).c_str());
    }

    uint64_t displayedEpoch=online.Epoch(),loops=0;
    std::string lastReplayNote;
    bool overlayHidden=false; // (online-hud: Tab online; also captured by the room callback)
#ifdef FORKADE_GAME_BROKER
    bool ownedSession=false;
    std::vector<uint8_t> savedOffline;
    forkade::GameRoom room([&](std::string& error){
        if(isOnline){error="An online session is already active.";return false;}
        savedOffline.clear();cps2recomp::Archive before(savedOffline,true);port.board->Serialize(before);
        ownedSession=true;
        std::vector<uint8_t> clean;
        {NeoGeo fresh(port.board->Roms());cps2recomp::Archive save(clean,true);fresh.Serialize(save);}
        cps2recomp::Archive load(clean,false);port.board->Serialize(load);
        if(!load.Ok()){error="The clean online board could not be restored";return false;}
        port.frame=0;port.paused=false;port.steps=0;
        port.menu.Close();port.menu.SetSlowBy(1);port.coinFrames=port.start2Frames=0;
        auto options=port.opt.online;options.mode=OnlineOptions::Mode::Host;options.port=7845;
        options.matchFlow=options.arcadeWait=true;ownedSession=true;
        if(!online.Start(port,options,error))return false;
        isOnline=true;return true;
    },[&]{
        if(!ownedSession)return;
        ownedSession=false;online.Stop();isOnline=false;overlayHidden=false;
        cps2recomp::Archive restore(savedOffline,false);port.board->Serialize(restore);port.frame=port.board->Frames();
        port.board->Sound().made.clear();port.menu.hosting=false;
    });
    wchar_t exePath[32768]={};const auto exeLength=GetModuleFileNameW(nullptr,exePath,32768);
    const std::filesystem::path program=exeLength&&exeLength<32768?std::filesystem::path(exePath):std::filesystem::path();
    port.menu.forkade=port.opt.forkadeOnline;
#endif
    struct Event{uint64_t tick;std::string key;};std::vector<Event> scripted;
    std::istringstream input(port.opt.windowEvents);std::string item;
    while(std::getline(input,item,';')){const auto colon=item.find(':');if(colon!=std::string::npos)scripted.push_back({strtoull(item.c_str(),nullptr,10),item.substr(colon+1)});}
    auto menuAction=[&](Menu::Action action){
#ifdef FORKADE_GAME_BROKER
        if(action==Menu::Action::Host){
            if(room.Hosting())room.Stop();else room.Host(GAME_ID,GAME_NAME,program,program.parent_path()/"cps-tunel.exe",port.opt.forkadeService,port.opt.online.name,port.opt.forkadeVersion);
            port.menu.Close();return;
        }
        if(action==Menu::Action::CopyCode){const bool copied=room.Copy();fprintf(stderr,"forkade room: copied=%d\n",int(copied));port.menu.Close();return;}
#endif
        if(action==Menu::Action::HideNames){port.menu.hideNames=!port.menu.hideNames;port.menu.Close();return;}
        if(action==Menu::Action::HideStreaks){port.menu.hideStreaks=!port.menu.hideStreaks;port.menu.Close();return;}
        port.MenuAct(action);
    };
    std::string lastRoomLabel;
    std::vector<SDL_Gamepad *> gamepads;
    std::vector<uint32_t> menuBase, menuPicture, shown, inputPresented;
    uint32_t menuWidth = 0, menuHeight = 0;
    bool menuDirty = false;
    uint16_t previousPad = 0, heldAtClose = 0;
    unsigned previousMenu = 0;
    bool bothHeld = false;
    // ForkAde's in-game chat (neogeo_chat.h): its key (T, or Y where T is a game key) opens a line to type, through the
    // system's text input (the keyboard layout's accents and signs); Enter sends it, Esc puts it away. Meanwhile the
    // keyboard is the chat's, and after it, not the game's until every key is let go
    NeoGeoChat &chat = online.Chat();
    SDL_Scancode chatKey = SDL_SCANCODE_T;
    bool chatWas = false, keysAfterChat = false;
    uint64_t openedByKey = 0;
    auto chatOpen = [&](bool open) {
        if (open == chat.typing || (open && !NeoGeoChat::Available(online))) {
            return;
        }
        chat.typing = open;
        chat.typed.clear();
        if (open) {
            SDL_StartTextInput(window);
            openedByKey = SDL_GetTicks();
        } else {
            SDL_StopTextInput(window);
            keysAfterChat = true;
        }
    };
    auto toggleMenu = [&]() {
        if(port.replay&&port.replay->Playing()){port.replay->Command(recomp::replay::Command::Pause);return;}
        if (port.menu.IsOpen()) {
            port.menu.Close();
        } else {
            port.MenuPicture(menuBase, menuWidth, menuHeight);
            port.menu.Open(true);
            menuDirty = true;
        }
    };
    auto quickState = [&](bool save) {
        if (port.online || (port.replay&&port.replay->Playing())) {
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
    if(port.opt.autoReplays)port.replay->AutoFolder(ReplayFolder(port).string());
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
    auto replayDialog=std::make_shared<ReplayDialogState>();
    auto barChose = [&](const recomp::MenuBar::Chosen &chosen) {
        barDirty = true;
        heldAtClose = previousPad;
        if (!chosen.chosen) {
            return;
        }
        if(chosen.id==kReplayOpen||chosen.id==kReplayFolder) {
            bool busy;{std::lock_guard<std::mutex> lock(replayDialog->mutex);busy=replayDialog->busy;if(!busy)replayDialog->busy=true;}
            if(!busy){
                if(port.replay->Playing())port.replay->Command(recomp::replay::Command::Pause);
                auto request=new ReplayDialogRequest{replayDialog,chosen.id==kReplayFolder,ReplayFolder(port).string()};
                static const SDL_DialogFileFilter filter={"ForkAde replay","fareplay"};
                if(request->folder)SDL_ShowOpenFolderDialog(ReplayDialogDone,request,window,request->initial.c_str(),false);
                else SDL_ShowOpenFileDialog(ReplayDialogDone,request,window,&filter,1,request->initial.c_str(),false);
            }
        } else if(chosen.id>=kInputPanelBase&&chosen.id<kInputPanelBase+3) {
            cps2recomp::SetFeatureValue("input_display",chosen.id-kInputPanelBase);
        } else if(chosen.id==kReplayRecord) {
            if(port.replay->Recording())port.replay->StopRecord();
            else port.replay->Record((ReplayFolder(port)/(std::string(GAME_ID)+"_"+std::to_string(std::time(nullptr))+"_"+std::to_string(SDL_GetTicks())+".fareplay")).string());
            port.notice=port.replay->Note();port.noticeUntil=port.tick+240;
        } else if(chosen.id==kReplayPlay) {
            std::error_code ec;std::filesystem::path latest;std::filesystem::file_time_type time{};
            for(const auto& file:std::filesystem::directory_iterator(ReplayFolder(port),ec))
                if(file.is_regular_file(ec)&&file.path().extension()==".fareplay"&&(latest.empty()||file.last_write_time(ec)>time)){latest=file.path();time=file.last_write_time(ec);}
            if(latest.empty())port.notice="No replay files found";
            else {port.replay->Play(latest.string());port.notice=port.replay->Note();}
            port.noticeUntil=port.tick+240;
        } else if(chosen.id==kReplayStop)port.replay->Stop();
        else if(chosen.id>=kReplayCommandBase&&chosen.id<kReplayCommandBase+100)port.replay->Command(static_cast<recomp::replay::Command>(chosen.id-kReplayCommandBase));
        else if (recomp::IsMenuEntry(chosen.id)) {
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
            port.notice = std::string(GAME_TITLE) + " - Neo Geo runtime (interpreted CPUs)";
            port.noticeUntil = port.tick + 240;
        } else if (chosen.id == kBarSource) {
            SDL_OpenURL("https://github.com/sunkpoet-lang/neogeo-recomp");
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
    // The pointer is hidden over the game after a moment still, and seen again when it moves
    uint64_t pointerMoved = SDL_GetTicksNS();
    bool pointerShown = true;
    bool altAlone = false; // (Alt went down, and nothing else since: let go, it opens the bar's Game menu)
    const double kFrameNs = 1e9 * NeoGeo::kLines * NeoGeo::kCpuPerLine / 12000000.0; // (59.19 a second)
    uint64_t nextFrame = SDL_GetTicksNS();
    auto feedAudio = [&](bool ran, double slowBy) {
            // (game-volume-control) the level (and mute) as the stream's gain, followed over a few frames so a
            // change does not click
            if (audio) {
                const float wanted = port.menu.muted ? 0.0f : recomp::VolumeGain(port.menu.volume);
                const float before = gainNow;
                gainNow += std::clamp(wanted - gainNow, -0.25f, 0.25f);
                if (gainNow != before || SDL_GetAudioStreamGain(audio) != gainNow) {
                    SDL_SetAudioStreamGain(audio, gainNow);
                }
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
            } else if (s.what == "g") {
                put.gbutton.button=SDL_GetGamepadButtonFromString(s.key.c_str());
                put.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;SDL_PushEvent(&put);
                put.type=SDL_EVENT_GAMEPAD_BUTTON_UP;SDL_PushEvent(&put);
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
                if(port.replay&&port.replay->Playing()){printf("viewer %llu: frame %u, audio rate %.2f\n",static_cast<unsigned long long>(loops),port.replay->Frame(),port.replay->AudioRate());fflush(stdout);}
            }
        }
        for(const auto& event:scripted)if(event.tick==loops){
            if(event.key=="q")running=false;
            else if(event.key=="host")menuAction(port.menu.SelectOnline(0));
            else if(event.key=="copy")menuAction(port.menu.SelectOnline(1));
            else if(event.key=="names")menuAction(port.menu.SelectOnline(2));
            else if(event.key=="streaks")menuAction(port.menu.SelectOnline(3));
            else if(event.key=="menu")toggleMenu();
        }
#ifdef FORKADE_GAME_BROKER
        if(port.opt.forkadeOnline){
            room.Tick();if(ownedSession)room.Reconnecting(online.Recovering());
            if(ownedSession&&online.GetStatus()==NeoGeoOnline::Status::Playing)room.Admitted();
            if(ownedSession&&online.GetStatus()==NeoGeoOnline::Status::Ended)room.Stop(online.Message());
            port.menu.hosting=room.Hosting();port.menu.roomLabel=room.Label();
            if(lastRoomLabel!=room.Label()){lastRoomLabel=room.Label();fprintf(stderr,"forkade room: hosting=%d status=%s\n",int(room.Hosting()),lastRoomLabel.c_str());}
        }
#endif
        if(isOnline&&displayedEpoch!=online.Epoch()){displayedEpoch=online.Epoch();if(audio)SDL_ClearAudioStream(audio);port.menu.Close();port.paused=false;port.steps=0;}
        // (a match to chat in, or none: the chat's key chosen from the player's keys as it begins)
        const bool chatNow = isOnline && NeoGeoChat::Available(online);
        if (chatNow && !chatWas) {
            const bool t = KeyBound(SDL_SCANCODE_T), y = KeyBound(SDL_SCANCODE_Y);
            chatKey = t && !y ? SDL_SCANCODE_Y : SDL_SCANCODE_T;
            chat.key = t && !y ? "Y" : "T";
            chat.keyShared = t && y;
        }
        if (!chatNow && chat.typing) {
            chatOpen(false); // (the match over)
        }
        chatWas = chatNow;
        ++loops;
        {
            std::string path,error;bool ready=false,folder=false;
            {std::lock_guard<std::mutex> lock(replayDialog->mutex);ready=replayDialog->ready;if(ready){replayDialog->ready=false;path=replayDialog->path;error=replayDialog->error;folder=replayDialog->folder;}}
            if(ready&&!path.empty()) {
                if(folder){
                    port.opt.autoReplayDir=path;port.replay->AutoFolder(path);
                    if(!port.opt.settingsPath.empty()){recomp::Ini settings(std::filesystem::u8path(port.opt.settingsPath));settings.load();settings.set("replays","folder",path);settings.save();}
                    port.notice="Recording folder updated";
                }else{port.replay->Play(path);port.notice=port.replay->Note();}
                port.noticeUntil=port.tick+240;barDirty=true;
            }else if(ready&&!error.empty()){port.notice=error;port.noticeUntil=port.tick+240;}
        }
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if(port.replay&&port.replay->Playing()&&!bar.IsOpen()&&!port.menu.IsOpen()) {
                if(ev.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN&&ev.gbutton.button==SDL_GAMEPAD_BUTTON_GUIDE){barOpened();bar.OpenMenu(0);continue;}
                if(port.replay->Event(ev))continue;
                if(ev.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&ev.button.button==SDL_BUTTON_LEFT) {
                    float x,y;pixelsOf(ev.button.x,ev.button.y,x,y);int w,h;SDL_GetWindowSizeInPixels(window,&w,&h);
                    const int strip=isFullscreen()?0:BarStrip(h);auto view=Where(w,h-strip);view.y+=strip;
                    if(view.w>0&&view.h>0&&port.replay->Click((x-view.x)*NeoGeoVideo::kWidth/view.w,(y-view.y)*NeoGeoVideo::kHeight/view.h))continue;
                }
            }
            if (chat.typing && ev.type == SDL_EVENT_TEXT_INPUT) {
                // (the chat's key's own letter, if the system sends it as text too, is not the line's)
                const bool keyLetter = ev.text.text[0] && !ev.text.text[1] &&
                                       toupper(static_cast<unsigned char>(ev.text.text[0])) == chat.key[0];
                if (!(keyLetter && openedByKey && SDL_GetTicks() - openedByKey < 250)) {
                    chat.Type(ev.text.text);
                }
                openedByKey = 0;
                continue;
            }
            if (chat.typing && ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.scancode == SDL_SCANCODE_RETURN || ev.key.scancode == SDL_SCANCODE_KP_ENTER) {
                    if (chat.Say(online)) { // (kept, said to wait, when over the rate)
                        chatOpen(false);
                    }
                } else if (ev.key.scancode == SDL_SCANCODE_ESCAPE) {
                    chatOpen(false);
                } else if (ev.key.scancode == SDL_SCANCODE_BACKSPACE) {
                    chat.Backspace();
                }
                continue; // (every other key the line's, as text)
            }
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
                } else if (chatNow && !port.menu.IsOpen() && ev.key.scancode == chatKey && !ev.key.repeat) {
                    chatOpen(true); // (online the chat's key, even where T is a game key too)
                } else if (KeyBound(ev.key.scancode)) {
                    // (a key the player has given to the game does only that)
                } else if (ev.key.scancode == SDL_SCANCODE_TAB && isOnline && (online.GetStatus() == NeoGeoOnline::Status::Playing ||
                                                                             online.GetStatus() == NeoGeoOnline::Status::Recovering)) {
                    // (online-hud: online, Tab shows or hides the names' strip and the connection rows, for the session)
                    if (!ev.key.repeat) {
                        overlayHidden = !overlayHidden;
                    }
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
                } else if (ev.key.scancode == SDL_SCANCODE_MINUS || ev.key.scancode == SDL_SCANCODE_EQUALS ||
                           ev.key.scancode == SDL_SCANCODE_KP_MINUS || ev.key.scancode == SDL_SCANCODE_KP_PLUS) {
                    // (game-volume-control: the level, as Fightcade has it; online too)
                    port.ChangeVolume(ev.key.scancode == SDL_SCANCODE_MINUS || ev.key.scancode == SDL_SCANCODE_KP_MINUS ? -1 : 1);
                } else if (ev.key.scancode == SDL_SCANCODE_LEFTBRACKET || ev.key.scancode == SDL_SCANCODE_RIGHTBRACKET) {
                    port.Key(ev.key.scancode == SDL_SCANCODE_LEFTBRACKET ? '-' : '='); // slower, faster
                } else if (ev.key.scancode == SDL_SCANCODE_BACKSPACE) {
                    port.Key('r');
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
                } else if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) {
                    port.Key('r');
                } else if (chatNow && ev.gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_STICK) {
                    chatOpen(!chat.typing); // (online: the chat, as in the Capcom games)
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
        int keyCount = 0;
        const bool *keys = SDL_GetKeyboardState(&keyCount);
        if (keysAfterChat) {
            keysAfterChat = std::any_of(keys, keys + keyCount, [](bool down) { return down; });
        }
        if (!(SDL_GetModState() & SDL_KMOD_ALT) && !chat.typing && !keysAfterChat) {
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
                const Menu::Action action = port.menu.Press(menuPressed);
                if (action == Menu::Action::Chat || action == Menu::Action::ChatHistory) {
                    // (ForkAde's in-game chat: online only)
                    if (!NeoGeoChat::Available(online)) {
                        port.menu.Note("Only in an online match.");
                    } else {
                        port.menu.Close();
                        if (action == Menu::Action::Chat) {
                            chatOpen(true);
                        } else {
                            chat.historyOpen = !chat.historyOpen;
                        }
                    }
                } else {
                    menuAction(action);
                }
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
        bool replayDialogBusy;{std::lock_guard<std::mutex> lock(replayDialog->mutex);replayDialogBusy=replayDialog->busy;}
        if ((bar.IsOpen() || (replayDialogBusy&&!isOnline)) && !port.menu.IsOpen()) {
            // Keep offline play still while choosing a local replay/folder as well.
            if (shown.empty()) {
                port.Shown(shown);
            }
            const bool detailed=port.InputPresentation(shown,inputPresented);
            presenter.Show(detailed?inputPresented.data():shown.data(), NeoGeoVideo::kWidth*(detailed?3:1), NeoGeoVideo::kHeight*(detailed?3:1), where);
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
                feedAudio(online.Tick(none,false) > 0, 1);
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
        if (port.replay&&port.replay->Playing()) {
            ran=port.replay->Tick();
            if(!port.replay->Playing()){port.notice=port.replay->Note();port.noticeUntil=port.tick+300;fprintf(stderr,"replay: %s\n",port.notice.c_str());}
        } else if (isOnline) {
            // (this side's player: the keyboard's two players' keys and the gamepads together; the stress test's two)
            const bool stress = port.opt.online.mode == OnlineOptions::Mode::Stress;
            const uint16_t local[2] = {static_cast<uint16_t>(stress ? players[0] : players[0] | players[1]), stress ? players[1] : uint16_t{0}};
            ran = online.Tick(local) > 0;
        } else {
            ran = port.Tick(withService);
        }

        const double replayAudioRate=port.replay&&port.replay->Playing()?port.replay->AudioRate():1;
        if(port.replay&&port.replay->Note()!=lastReplayNote){lastReplayNote=port.replay->Note();port.notice=lastReplayNote;port.noticeUntil=port.tick+240;}
        if(audio&&port.replay&&port.replay->Playing()&&!replayAudioRate)SDL_ClearAudioStream(audio);
        feedAudio(ran, port.replay&&port.replay->Playing()&&replayAudioRate?1.0/replayAudioRate:slowBy);

        port.Shown(shown);
        if (port.tick < port.volumeNoteUntil) {
            // (game-volume-control: the level, as - and = changed it, for a second)
            const recomp::text::Canvas canvas{shown.data(), NeoGeoVideo::kWidth, NeoGeoVideo::kHeight};
            const float wide = recomp::text::TextWidth(port.volumeNote.c_str(), 1);
            canvas.Fill(NeoGeoVideo::kWidth - wide - 8, 4, wide + 6, 11, {20, 22, 30, 200});
            canvas.Text(NeoGeoVideo::kWidth - wide - 5, 6, 1, 0, {255, 255, 255, 255}, port.volumeNote.c_str());
        }
        if(port.replay)port.replay->Draw(shown.data(),NeoGeoVideo::kWidth,NeoGeoVideo::kHeight);
        if (isOnline) {
            DrawOnline(shown, online, overlayHidden);
            // (online-hud) The players' names and win streaks in one strip at the top, over the score row and above
            // the life bars and the timer, see-through as Fightcade's: "Name1  N  VS  M  Name2"; names cut to 8
            // letters, so the strip leaves the corners to the connection's rows; Tab hides it
            if(port.opt.forkadeOnline&&online.ChatReady()&&!overlayHidden){
                const recomp::text::Canvas canvas{shown.data(),NeoGeoVideo::kWidth,NeoGeoVideo::kHeight};
                auto cut=[](const std::string &name){ // (UTF-8's continuation bytes are not letters)
                    size_t letters=0,at=0;
                    for(;at<name.size();at++){if((static_cast<unsigned char>(name[at])&0xC0)!=0x80&&letters++==8)break;}
                    return name.substr(0,at);
                };
                struct Part{std::string text;recomp::text::Color colour;};
                using recomp::text::Palette; // (online-hud-palette: player 1 blue, player 2 red)
                std::vector<Part> parts;
                const std::string first=cut(online.ShownName(0)),second=cut(online.ShownName(1));
                if(!port.menu.hideNames&&!first.empty())parts.push_back({first,Palette::kPlayer1});
                if(!port.menu.hideStreaks)parts.push_back({std::to_string(port.streak[0]),Palette::kNumber});
                parts.push_back({"VS",Palette::kVersus});
                if(!port.menu.hideStreaks)parts.push_back({std::to_string(port.streak[1]),Palette::kNumber});
                if(!port.menu.hideNames&&!second.empty())parts.push_back({second,Palette::kPlayer2});
                if(parts.size()>1){
                    const float gap=recomp::text::TextWidth("  ",1);
                    float width=-gap;
                    for(const Part &part:parts)width+=recomp::text::TextWidth(part.text.c_str(),1)+gap;
                    float x=std::floor((NeoGeoVideo::kWidth-width)/2);
                    canvas.Fill(x-4,0,width+8,10,Palette::kBand);
                    for(const Part &part:parts){canvas.Text(x,1,1,0,part.colour,part.text.c_str());x+=recomp::text::TextWidth(part.text.c_str(),1)+gap;}
                }
            }
        if(port.opt.forkadeOnline && !port.menu.roomLabel.empty()){
            const recomp::text::Canvas canvas{shown.data(),NeoGeoVideo::kWidth,NeoGeoVideo::kHeight};
            canvas.Text(4,NeoGeoVideo::kHeight-10,1,0,{200,220,255,255},port.menu.roomLabel.c_str());
        }
        if(port.opt.shotEvery && loops%port.opt.shotEvery==0){
            const auto path=std::filesystem::path(port.opt.shotDir)/("flow-"+std::to_string(loops)+".png");
            const auto u8=path.u8string();const std::string file(reinterpret_cast<const char*>(u8.data()),u8.size());
            WritePng(file.c_str(),shown.data(),NeoGeoVideo::kWidth,NeoGeoVideo::kHeight);
        }
            chat.Draw(shown, NeoGeoVideo::kWidth, NeoGeoVideo::kHeight);
        }
        const bool detailed=port.InputPresentation(shown,inputPresented);
        presenter.Show(detailed?inputPresented.data():shown.data(), NeoGeoVideo::kWidth*(detailed?3:1), NeoGeoVideo::kHeight*(detailed?3:1), where);
        pace();
    }
#ifdef FORKADE_GAME_BROKER
    if(port.opt.forkadeOnline)room.Stop();
#endif
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
