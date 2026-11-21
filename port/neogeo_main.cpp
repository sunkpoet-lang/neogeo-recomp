// SPDX-License-Identifier: GPL-3.0-only
// Part of neogeo-recomp.
//
// A Neo Geo game (game.cfg's board = neogeo): its port (neogeo_port.h) in a window (neogeo_window.cpp), or with no
// window for the checks (--headless: pictures as PNG files, a hash of the whole state each frame, input from a script,
// the sound to a WAV file). The launcher starts it as it starts the Capcom games.
//
//   <game> [--rom FILE_OR_FOLDER] [--eeprom FILE] [--settings FILE] [--features A,-B] [--fullscreen] [--mute]
//          [--scale N] [--renderer sdl|gpu]
//   <game> --host [PORT] | --join HOST:PORT [--delay N]   (online: neogeo_online.h; with --headless too, for tests)
//   <game> --stress [--delay N]            (GekkoNet's stress session: rolled back every frame, the checksums compared)
//   online, ForkAde's in-game chat (neogeo_chat.h): --nick NAME, --chat-log DIR; for the tests --net-say [FRAME:]TEXT
//   (again for more) and --chat-loss PCT
//   <game> --list-features                 (for the launcher: the optional features, one a line)
//   <game> --headless [--rom ...] [--frames N] [--shot-every N] [--shot-dir DIR] [--hash-log FILE] [--audio-dump FILE]
//          [--script "FRAME:BUTTON:FRAMES,..."] [--state-save FRAME:FILE] [--state-load FILE] [--menu FRAME:KEYS[:PNG]]
//          [--keys TICK:KEY,...] [--stats] [--present-test PNG [--present-bar [N[.K]]]]
//   <game> --window-events "LOOP:WHAT;..."   (tests: mouse and key events put to the window; neogeo_window.cpp)
//
// Without --rom: executable-relative roms/, ROM/, rom/, then beside the executable;
// finally the legacy working-directory roms/ and current-directory ZIP.
// Script buttons: coin1 start1 up1 down1 left1 right1 a1 b1 c1 d1 (and the same ending in 2), service, test.

#include "game_info.h"

#include "../recomp-common/settings.h"
#include "../runtime/game.h"
#include "../runtime/state.h"
#include "neogeo_port.h"
#include "neogeo_match_flow.h"
#include <filesystem>
#include "png_write.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace cps2recomp;

namespace {
struct Press {
    uint64_t frame, frames;
    int player;
    uint16_t bits;
};

bool ParseScript(const std::string &text, std::vector<Press> &presses) {
    static const struct {
        const char *name;
        uint16_t bits;
    } kButtons[] = {
        {"coin", NeoGeo::kCoin}, {"start", NeoGeo::kStart}, {"select", NeoGeo::kSelect}, {"up", NeoGeo::kUp},
        {"down", NeoGeo::kDown}, {"left", NeoGeo::kLeft},   {"right", NeoGeo::kRight},   {"a", NeoGeo::kA},
        {"b", NeoGeo::kB},       {"c", NeoGeo::kC},         {"d", NeoGeo::kD},
    };
    std::istringstream in(text);
    std::string item;
    while (std::getline(in, item, ',')) {
        unsigned long long frame = 0, frames = 0;
        char name[32] = {};
        if (sscanf(item.c_str(), "%llu:%31[^:]:%llu", &frame, name, &frames) != 3) {
            fprintf(stderr, "--script: cannot read \"%s\" (FRAME:BUTTON:FRAMES)\n", item.c_str());
            return false;
        }
        Press press{frame, frames, 0, 0};
        std::string button = name;
        if (button == "service" || button == "test") {
            press.bits = button == "service" ? NeoGeo::kService : NeoGeo::kTest;
        } else {
            const char last = button.empty() ? 0 : button.back();
            if (last != '1' && last != '2') {
                fprintf(stderr, "--script: \"%s\" needs a player, 1 or 2\n", name);
                return false;
            }
            press.player = last - '1';
            button.pop_back();
            for (const auto &b : kButtons) {
                if (button == b.name) {
                    press.bits = b.bits;
                }
            }
        }
        if (!press.bits) {
            fprintf(stderr, "--script: no button \"%s\"\n", name);
            return false;
        }
        presses.push_back(press);
    }
    return true;
}

// A 16-bit stereo WAV of what the sound chip made
void WriteWav(const std::string &path, const std::vector<int16_t> &sound, uint32_t rate) {
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path.c_str());
        return;
    }
    const uint32_t bytes = static_cast<uint32_t>(sound.size() * 2);
    auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f);
    u32(36 + bytes);
    fwrite("WAVEfmt ", 1, 8, f);
    u32(16);
    u16(1);
    u16(2);
    u32(rate);
    u32(rate * 4);
    u16(4);
    u16(16);
    fwrite("data", 1, 4, f);
    u32(bytes);
    fwrite(sound.data(), 2, sound.size(), f);
    fclose(f);
}
} // namespace

int main(int argc, char **argv) {
    port::NeoGeoPort port;
    port::NeoGeoOptions &o = port.opt;
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        auto next = [&]() -> const char * { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--rom") {
            o.rom = next();
        } else if (a == "--eeprom") {
            o.savesPath = next();
        } else if (a == "--settings") {
            o.settingsPath = next();
        } else if (a == "--features") {
            o.features = next();
        } else if (a == "--fullscreen") {
            o.fullscreen = true;
        } else if (a == "--frames") {
            o.frames = strtoull(next(), nullptr, 10);
        } else if (a == "--headless") {
            o.headless = true;
        } else if (a == "--volume") {
            o.volume = std::clamp(atoi(next()), 0, 100); // (game-volume-control)
        } else if (a == "--mute") {
            o.mute = true;
        } else if (a == "--scale") {
            o.scale = atoi(next());
        } else if (a == "--renderer") {
            o.renderer = next();
        } else if (a == "--shot-every") {
            o.shotEvery = strtoull(next(), nullptr, 10);
        } else if (a == "--shot-dir") {
            o.shotDir = next();
        } else if (a == "--hash-log") {
            o.hashLog = next();
        } else if (a == "--audio-dump") {
            o.audioDump = next();
        } else if (a == "--script") {
            o.script = next();
        } else if (a == "--menu") {
            o.menuTest = next();
        } else if (a == "--keys") {
            o.keys = next();
        } else if (a == "--state-save") {
            const std::string v = next();
            const size_t colon = v.find(':');
            o.saveAt = strtoull(v.c_str(), nullptr, 10);
            o.stateSave = colon == std::string::npos ? "" : v.substr(colon + 1);
        } else if (a == "--state-load") {
            o.stateLoad = next();
        } else if (a == "--record-replay") {
            o.recordReplay = next();
        } else if (a == "--replay") {
            o.replayPath = next();
        } else if (a == "--viewer-script") {
            o.viewerScript = next();
        } else if (a == "--replay-dir") {
            o.autoReplayDir = next();
        } else if (a == "--no-auto-replays") {
            o.autoReplays = false;
        } else if (a == "--stats") {
            o.stats = true;
        } else if(a=="--forkade-online") {
            if(!port::VerifiedMatchFlowGame(GAME_ID)){fprintf(stderr,"ForkAde match flow is not verified for this game\n");return 2;}
            o.forkadeOnline=true;o.online.matchFlow=true;o.online.arcadeWait=true;
        } else if(a=="--forkade-service") {o.forkadeService=next();
        } else if(a=="--forkade-version") {o.forkadeVersion=next();
        } else if(a=="--window-events") {o.windowEvents=next();
        } else if(a=="--gekko-checkpoint-log") {o.online.checkpointLog=next();
        } else if(a=="--gekko-flow-log") {o.online.flowLog=next();
        } else if(a=="--memory-watch") {o.memoryWatch=next();
        } else if(a=="--memory-log") {o.memoryLog=next();
        } else if(a=="--observe-flow") {o.observeFlow=true;
        } else if (a == "--present-test") {
            o.presentTest = next();
        } else if (a == "--present-bar") {
            o.presentBar = i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9' ? next() : "0";
        } else if (a == "--host") {
            o.online.mode = port::OnlineOptions::Mode::Host;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                o.online.port = static_cast<uint16_t>(atoi(next()));
            }
        } else if (a == "--join") {
            const std::string v = next();
            const size_t colon = v.rfind(':');
            o.online.mode = port::OnlineOptions::Mode::Join;
            o.online.host = v.substr(0, colon);
            if (colon != std::string::npos) {
                o.online.port = static_cast<uint16_t>(atoi(v.c_str() + colon + 1));
            }
        } else if (a == "--watch") {
            // (match-spectators) a spectator of the match at HOST:PORT
            const std::string v = next();
            const size_t colon = v.rfind(':');
            o.online.mode = port::OnlineOptions::Mode::Watch;
            o.online.host = v.substr(0, colon);
            if (colon != std::string::npos) {
                o.online.port = static_cast<uint16_t>(atoi(v.c_str() + colon + 1));
            }
        } else if (a == "--spectate" || a == "--gekko-spectate") {
            o.online.spectate = true; // (ForkAde's launcher's Watch: its --join, as a spectator)
        } else if (a == "--spectators") {
            o.online.spectators = atoi(next());
        } else if (a == "--spectator-delay") {
            o.online.spectatorDelay = atoi(next());
        } else if (a == "--delay") {
            o.online.delay = atoi(next());
        } else if (a == "--stress") {
            o.online.mode = port::OnlineOptions::Mode::Stress;
        } else if (a == "--nick") {
            o.online.name = next();
        } else if (a == "--chat-log") {
            o.online.chatLog = next();
        } else if (a == "--chat-loss") {
            o.online.chatLoss = atoi(next());
        } else if (a == "--net-say") {
            std::string say = next();
            uint64_t at = 120;
            const size_t colon = say.find(':');
            if (colon != std::string::npos && colon > 0 && say.find_first_not_of("0123456789") == colon) {
                at = strtoull(say.c_str(), nullptr, 10);
                say.erase(0, colon + 1);
            }
            o.online.says.push_back({at, say});
        } else if (a == "--list-features") {
            // For the launcher: one optional feature per line (recomp-common/settings.h), not those whose switch is on
            // its first page (game.h, kFeatureFirstPage)
            RegisterGame();
            port::RegisterNeoGeoPortFeatures();
            std::vector<recomp::FeatureInfo> list = FeatureList();
            list.erase(std::remove_if(list.begin(), list.end(), [](const recomp::FeatureInfo &f) { return f.flags & kFeatureFirstPage; }), list.end());
            fputs(recomp::FeatureListText(list).c_str(), stdout);
            return 0;
        } else {
            fprintf(stderr, "%s: unknown option %s\n", argv[0], a.c_str());
            return 2;
        }
    }
    if (o.online.spectate && o.online.mode == port::OnlineOptions::Mode::Join) {
        o.online.mode = port::OnlineOptions::Mode::Watch; // (match-spectators: --spectate, in a window too)
    }
    std::vector<Press> presses;
    if(o.observeFlow && (!o.headless || !o.forkadeOnline || o.online.mode!=port::OnlineOptions::Mode::Off || o.online.flowLog.empty())){
        fprintf(stderr,"--observe-flow requires headless KOF97 ForkAde mode, no session, and --gekko-flow-log\n");return 2;
    }
    if (!ParseScript(o.script, presses)) {
        return 2;
    }
    std::string error;
    if (!port.Init(error)) {
        fprintf(stderr, "%s: %s\n", GAME_NAME, error.c_str());
        if (!o.headless) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GAME_NAME, (std::string(GAME_NAME) + ": " + error).c_str(), nullptr);
        }
        return 1;
    }

    if (!o.headless) {
        const int code = port::RunNeoGeoWindow(port);
        port.Finish();
        return code;
    }
    port::NeoGeoOnline online;
    const bool isOnline = o.online.mode != port::OnlineOptions::Mode::Off;
    if (isOnline && !online.Start(port, o.online, error)) {
        fprintf(stderr, "%s: %s\n", GAME_NAME, error.c_str());
        return 1;
    }

    // With no window: the frames asked for, as fast as they go
    NeoGeo &board = *port.board;
    NeoGeoSound &sound = board.Sound();
    sound.keep = !o.audioDump.empty() && !o.mute;
    std::vector<int16_t> recorded;
    port::MatchFlow observedFlow;
    FILE* observation=nullptr;
    if(o.observeFlow){
#ifdef _WIN32
        observation=_wfopen(std::filesystem::u8path(o.online.flowLog).c_str(),L"w");
#else
        observation=fopen(o.online.flowLog.c_str(),"w");
#endif
        if(!observation)return 1;
    }
    FILE *log = o.hashLog.empty() ? nullptr : fopen(o.hashLog.c_str(), "w");
    std::vector<uint32_t> watches;
    std::istringstream watchIn(o.memoryWatch);std::string watch;
    while(std::getline(watchIn,watch,',')){char* end=nullptr;auto value=strtoul(watch.c_str(),&end,0);if(!end||*end||value<0x100000||value>0x10ffff){fprintf(stderr,"memory watch must be in work RAM\n");return 2;}watches.push_back(static_cast<uint32_t>(value));}
    FILE* memory=o.memoryLog.empty()?nullptr:fopen(o.memoryLog.c_str(),"w");
    if(!o.memoryLog.empty()&&!memory)return 1;
    const auto started = std::chrono::steady_clock::now();
    uint64_t first = board.Frames(),epoch=online.Epoch();
    bool arcade=online.ArcadeWaiting();
    const uint64_t frames = o.frames ? o.frames : 600;
    uint64_t replayTicks=0;
    // (by ticks, as the window goes: --keys can stop the game; a run stopped for good still ends)
    while (!port.quit && ((!o.replayPath.empty()) ? replayTicks < frames : board.Frames() - first < frames) && port.tick < (frames + 60) * 64) {
        uint64_t frame = board.Frames() + 1; // (the frame about to be run, counting from 1)
        uint16_t players[2] = {};
        for (const Press &p : presses) {
            if (frame >= p.frame && frame < p.frame + p.frames) {
                players[p.player] |= p.bits;
            }
        }
        if (!o.replayPath.empty()) {
            if(!port.replay->Playing()){fprintf(stderr,"replay: %s\n",port.replay->Note().c_str());return 1;}
            port.replay->Script(replayTicks,o.viewerScript);
            port.replay->Tick();++replayTicks;frame=port.replay->Frame();
        } else if (isOnline) {
            // Online: the script's player 1 is this side's controls (its player 2's, too, with --join), the session runs
            // the frames; a tick a frame's time, so that the two sides keep pace
            const bool joined = o.online.mode == port::OnlineOptions::Mode::Join;
            const uint16_t local[2] = {static_cast<uint16_t>(joined ? players[0] | players[1] : players[0]), players[1]};
            const int shown = online.Tick(local);
            if(epoch!=online.Epoch() || (arcade&&!online.ArcadeWaiting())){first=0;epoch=online.Epoch();}
            arcade=online.ArcadeWaiting();
            if (online.GetStatus() == port::NeoGeoOnline::Status::Ended) {
                break;
            }
            if (o.online.mode != port::OnlineOptions::Mode::Stress) {
                std::this_thread::sleep_for(std::chrono::microseconds(online.FramesAhead() > 1.0f ? 17200 : 16900));
            }
            port.tick++;
            if (shown == 0) {
                continue;
            }
            frame = board.Frames();
        } else if (!port.Tick(players)) {
            continue; // (stopped by --keys: p; o goes on by a frame)
        } else frame=board.Frames(); // a command may have restored a training checkpoint
        if(observation){
            port::AdvanceNeoGeoFlow(observedFlow,board,players);
            const auto& s=observedFlow.state;
            fprintf(observation,"%llu %d %d %d %d %d %d\n",static_cast<unsigned long long>(frame),int(s.joined[0]),int(s.joined[1]),s.streak[0],s.streak[1],s.score[0],s.score[1]);
        }
        if (sound.keep) {
            recorded.insert(recorded.end(), sound.made.begin(), sound.made.end());
        }
        sound.made.clear();
        if(memory){fprintf(memory,"%llu",static_cast<unsigned long long>(frame));for(auto at:watches)fprintf(memory," %02x",board.Peek8(at));fprintf(memory,"\n");}
        if (log) {
            fprintf(log, "%llu %08x\n", static_cast<unsigned long long>(frame), board.StateHash());
        }
        if (o.shotEvery && frame % o.shotEvery == 0) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s_%06llu.png", o.shotDir.c_str(), GAME_ID, static_cast<unsigned long long>(frame));
            std::vector<uint32_t> shown;
            port.Shown(shown);
            if(port.replay)port.replay->Draw(shown.data(),NeoGeoVideo::kWidth,NeoGeoVideo::kHeight);
            WritePng(path, shown.data(), NeoGeoVideo::kWidth, NeoGeoVideo::kHeight);
        }
        if (!o.stateSave.empty() && frame == o.saveAt) {
            if (!port.SaveState(o.stateSave, error)) {
                fprintf(stderr, "%s: %s\n", GAME_NAME, error.c_str());
            }
        }
    }
    if(observation)fclose(observation);
    if(memory)fclose(memory);
    if (log) {
        fclose(log);
    }
    if (!o.audioDump.empty()) {
        WriteWav(o.audioDump, recorded, NeoGeoSound::SampleRate());
    }
    if (o.stats) {
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        fprintf(stderr, "%llu frames in %.2f s: %.0f a second; watchdog resets %u; unmapped addresses %zu\n",
                static_cast<unsigned long long>(board.Frames() - first), seconds,
                (board.Frames() - first) / (seconds > 0 ? seconds : 1), board.watchdogResets, board.unmapped.size());
    }
    if(!o.replayPath.empty()&&!port.replay->Playing()){fprintf(stderr,"replay: %s\n",port.replay->Note().c_str());port.Finish();return 1;}
    fprintf(stderr, "state hash %08x at frame %llu\n", board.StateHash(), static_cast<unsigned long long>(board.Frames()));
    if (!o.presentTest.empty()) {
        // (the picture the run ended on, through the GPU path and read back: what a window 672 lines tall would show;
        // --present-bar: as tall again as the menu bar's strip above it, menu N open, and its entry K's submenu)
        std::vector<uint32_t> shown, drawn, overlay;
        std::vector<SDL_Rect> regions;
        port.Shown(shown);
        if(port.replay)port.replay->Draw(shown.data(),NeoGeoVideo::kWidth,NeoGeoVideo::kHeight);
        const int outHeight = 672;
        const int outWidth = static_cast<int>(std::lround(outHeight * port::PictureShape(
            port::DisplayShapeNow() == port::Shape::Arcade ? 384 : NeoGeoVideo::kWidth, NeoGeoVideo::kHeight, port::DisplayShapeNow(), 4.0f / 3.0f)));
        int windowHeight = outHeight;
        SDL_FRect where = port::NeoGeoWhere(outWidth, outHeight);
        if (!o.presentBar.empty()) {
            windowHeight = outHeight + recomp::MenuBar::kHeightDots * std::max(1, outHeight / NeoGeoVideo::kHeight);
            where.y += static_cast<float>(port::BarStrip(windowHeight));
            recomp::MenuBar bar;
            port::BarMenus(port, bar, false);
            bar.SetArea(outWidth, windowHeight, port::BarDot(windowHeight));
            const int open = atoi(o.presentBar.c_str());
            const size_t sub = o.presentBar.find('.');
            if (open > 0) {
                bar.OpenMenu(static_cast<size_t>(open - 1));
                if (sub != std::string::npos) {
                    for (int k = atoi(o.presentBar.c_str() + sub + 1); k > 1; k--) {
                        bar.Buttons(recomp::kMenuDown);
                    }
                    bar.Buttons(recomp::kMenuRight);
                }
            }
            port::BarPicture(bar, outWidth, windowHeight, overlay, regions);
        }
        std::vector<uint32_t> inputPresented;const bool detailed=port.InputPresentation(shown,inputPresented);
        if (SDL_Init(SDL_INIT_VIDEO) &&
            port::Presenter::Render(detailed?inputPresented.data():shown.data(), NeoGeoVideo::kWidth*(detailed?3:1), NeoGeoVideo::kHeight*(detailed?3:1), outWidth, windowHeight, where, port::DisplayScalingNow(),
                                    drawn, error, regions.empty() ? nullptr : &overlay, regions.empty() ? nullptr : &regions)) {
            WritePng(o.presentTest.c_str(), drawn.data(), outWidth, windowHeight);
        } else {
            fprintf(stderr, "--present-test: %s\n", error.empty() ? SDL_GetError() : error.c_str());
        }
    }
    port.Finish();
    if (isOnline) {
        if (!online.Message().empty()) {
            fprintf(stderr, "online: %s\n", online.Message().c_str());
        }
        return online.Failed() ? 3 : 0;
    }
    return 0;
}
