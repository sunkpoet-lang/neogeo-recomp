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
} // namespace

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
    const float shape = PictureShape(DisplayShape() == Shape::Arcade ? 384 : NeoGeoVideo::kWidth, NeoGeoVideo::kHeight,
                                     DisplayShape(), 4.0f / 3.0f);
    const bool hidden=SDL_getenv("CPS2RECOMP_HIDDEN_WINDOW")!=nullptr;
    SDL_Window *window = SDL_CreateWindow(GAME_TITLE, static_cast<int>(std::lround(height * shape)), height,
                                          (SDL_WINDOW_RESIZABLE | (hidden?SDL_WINDOW_HIDDEN:0)) | (port.opt.fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
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
    bool overlayHidden=false; // (online-hud: Tab online)
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
    std::vector<uint32_t> menuBase, menuPicture, shown;
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
        if (port.menu.IsOpen()) {
            port.menu.Close();
        } else {
            port.MenuPicture(menuBase, menuWidth, menuHeight);
            port.menu.Open(true);
            menuDirty = true;
        }
    };
    auto quickState = [&](bool save) {
        std::string error;
        const std::string path = port.StatePath(port.menu.Slot());
        if (!(save ? port.SaveState(path, error) : port.LoadState(path, error))) {
            fprintf(stderr, "state %d: %s\n", port.menu.Slot(), error.c_str());
        }
    };
    const double kFrameNs = 1e9 * NeoGeo::kLines * NeoGeo::kCpuPerLine / 12000000.0; // (59.19 a second)
    uint64_t nextFrame = SDL_GetTicksNS();
    auto feedAudio = [&](bool ran, int slowBy) {
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
        for(const auto& event:scripted)if(event.tick==loops){
            if(event.key=="q")running=false;
            else if(event.key=="host")menuAction(port.menu.SelectOnline(0));
            else if(event.key=="copy")menuAction(port.menu.SelectOnline(1));
            else if(event.key=="names")menuAction(port.menu.SelectOnline(2));
            else if(event.key=="streaks")menuAction(port.menu.SelectOnline(3));
            else if(event.key=="menu")toggleMenu();
            // (tests: "k KEY", a key pressed and let go, by SDL's name; "p", the level, the gain and the speed printed)
            else if(event.key.rfind("k ",0)==0){
                SDL_Event put{};put.key.scancode=SDL_GetScancodeFromName(event.key.substr(2).c_str());
                put.key.key=SDL_GetKeyFromScancode(put.key.scancode,SDL_KMOD_NONE,false);
                put.type=SDL_EVENT_KEY_DOWN;put.key.down=true;SDL_PushEvent(&put);
                put.type=SDL_EVENT_KEY_UP;put.key.down=false;SDL_PushEvent(&put);
            }
            else if(event.key=="p")printf("window %llu: muted %s, volume %d%% (gain %.3f), slow by %d\n",static_cast<unsigned long long>(loops),
                                          port.menu.muted?"yes":"no",port.menu.volume,static_cast<double>(gainNow),port.menu.SlowBy());
        }
        ++loops;
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
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
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
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.scancode == SDL_SCANCODE_ESCAPE || ev.key.scancode == SDL_SCANCODE_F1) {
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
                } else if (ev.key.scancode == SDL_SCANCODE_F2) {
                    port.test = !port.test; // the operator's test switch
                } else if (ev.key.scancode == SDL_SCANCODE_F11 ||
                           (ev.key.scancode == SDL_SCANCODE_RETURN && (ev.key.mod & SDL_KMOD_ALT))) {
                    SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                if (PadInputBound(g_bindings, Table(), {ev.gbutton.button, -1})) {
                    // (a button the player has given to the game does only that)
                } else if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_STICK || ev.gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE) {
                    toggleMenu();
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
        unsigned menuNow = 0;
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
        unsigned menuPressed = menuNow & ~previousMenu;
        previousMenu = menuNow;
        if (both) {
            menuPressed = 0;
        }
        if (port.menu.IsOpen()) {
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

        int width = 0, height = 0;
        presenter.Size(width, height);
        presenter.SetScaling(DisplayScaling());
        const SDL_FRect where = Where(width, height);

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
        if (port.tick < port.volumeNoteUntil) {
            // (game-volume-control: the level, as - and = changed it, for a second)
            const recomp::text::Canvas canvas{shown.data(), NeoGeoVideo::kWidth, NeoGeoVideo::kHeight};
            const float wide = recomp::text::TextWidth(port.volumeNote.c_str(), 1);
            canvas.Fill(NeoGeoVideo::kWidth - wide - 8, 4, wide + 6, 11, {20, 22, 30, 200});
            canvas.Text(NeoGeoVideo::kWidth - wide - 5, 6, 1, 0, {255, 255, 255, 255}, port.volumeNote.c_str());
        }
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
        presenter.Show(shown.data(), NeoGeoVideo::kWidth, NeoGeoVideo::kHeight, where);
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
