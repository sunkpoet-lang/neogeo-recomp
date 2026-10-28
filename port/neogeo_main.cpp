// SPDX-License-Identifier: GPL-3.0-only
// Part of cps2recomp (the user's fork: the Neo Geo board).
//
// A Neo Geo game (game.cfg's board = neogeo) on the board of machine/neogeo.h, in the 68000's interpreter: in a window
// (port/neogeo_window.cpp), or with no window for the checks (--headless: pictures as PNG files, a hash of the whole
// state each frame, input from a script, the sound to a WAV file).
//
//   <game> [--rom FILE_OR_FOLDER] [--mute] [--scale N]
//   <game> --headless [--rom ...] [--frames N] [--shot-every N] [--shot-dir DIR] [--hash-log FILE] [--audio-dump FILE]
//          [--script "FRAME:BUTTON:FRAMES,..."] [--state-save FRAME:FILE] [--state-load FILE] [--stats] [--mute]
//
// With no --rom, the game's set is looked for in roms/ beside the program, then in the current folder.
// Script buttons: coin1 start1 up1 down1 left1 right1 a1 b1 c1 d1 (and the same ending in 2), service, test.

#include "game_info.h"

#include "../machine/neogeo.h"
#include "../machine/romset.h"
#include "../runtime/state.h"
#include "neogeo_window.h"
#include "png_write.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

#ifndef GAME_BIOS_SET
#define GAME_BIOS_SET "neogeo"
#endif
#ifndef GAME_ROM_PARENT
#define GAME_ROM_PARENT ""
#endif

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

bool Exists(const std::string &path) {
    struct stat info;
    return stat(path.c_str(), &info) == 0;
}

// The set beside the program (roms/<set>.zip), or in the current folder
std::string FindRom() {
    const char *base = SDL_GetBasePath();
    const std::string here = base ? base : "";
    for (const std::string &path : {here + "roms/" GAME_ROM_SET ".zip", std::string("roms/" GAME_ROM_SET ".zip"),
                                    std::string(GAME_ROM_SET ".zip")}) {
        if (Exists(path)) {
            return path;
        }
    }
    return here + "roms/" GAME_ROM_SET ".zip";
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
    std::string rom, shotDir = ".", hashLog, script, stateSave, stateLoad, audioDump;
    uint64_t frames = 600, shotEvery = 0, saveAt = 0;
    bool stats = false, headless = false, mute = false;
    int scale = 3;
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        auto next = [&]() -> const char * { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--rom") {
            rom = next();
        } else if (a == "--frames") {
            frames = strtoull(next(), nullptr, 10);
        } else if (a == "--headless") {
            headless = true;
        } else if (a == "--mute") {
            mute = true;
        } else if (a == "--scale") {
            scale = atoi(next());
        } else if (a == "--shot-every") {
            shotEvery = strtoull(next(), nullptr, 10);
        } else if (a == "--shot-dir") {
            shotDir = next();
        } else if (a == "--hash-log") {
            hashLog = next();
        } else if (a == "--audio-dump") {
            audioDump = next();
        } else if (a == "--script") {
            script = next();
        } else if (a == "--state-save") {
            const std::string v = next();
            const size_t colon = v.find(':');
            saveAt = strtoull(v.c_str(), nullptr, 10);
            stateSave = colon == std::string::npos ? "" : v.substr(colon + 1);
        } else if (a == "--state-load") {
            stateLoad = next();
        } else if (a == "--stats") {
            stats = true;
        } else {
            fprintf(stderr, "%s: unknown option %s\n", argv[0], a.c_str());
            return 2;
        }
    }
    if (rom.empty()) {
        rom = FindRom();
    }
    std::vector<Press> presses;
    if (!ParseScript(script, presses)) {
        return 2;
    }

    const NeoGeoList list = ParseNeoGeoList(GAME_PROGRAM, GAME_FIX, GAME_SOUND_PROGRAM, GAME_SAMPLES, GAME_GRAPHICS,
                                            GAME_BIOS, GAME_ROM_SET, GAME_ROM_PARENT, GAME_BIOS_SET);
    NeoGeoRoms roms;
    std::string error;
    if (!LoadNeoGeoSet(rom, list, roms, error)) {
        fprintf(stderr, "%s: %s\n", GAME_NAME, error.c_str());
        if (!headless) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GAME_TITLE, (std::string(GAME_NAME) + ": " + error).c_str(), nullptr);
        }
        return 1;
    }
    auto board = std::make_unique<NeoGeo>(std::move(roms));
    if (!stateLoad.empty()) {
        FILE *f = fopen(stateLoad.c_str(), "rb");
        std::vector<uint8_t> data;
        if (f) {
            uint8_t buffer[65536];
            size_t got;
            while ((got = fread(buffer, 1, sizeof(buffer), f)) > 0) {
                data.insert(data.end(), buffer, buffer + got);
            }
            fclose(f);
        }
        Archive ar(data, false);
        board->Serialize(ar);
        if (!f || !ar.Ok()) {
            fprintf(stderr, "%s: cannot load the state %s\n", GAME_NAME, stateLoad.c_str());
            return 1;
        }
    }

    if (!headless) {
        NeoGeoWindowOptions options;
        options.title = GAME_TITLE;
        options.id = GAME_ID;
        options.scale = scale;
        options.mute = mute;
        options.shotDir = shotDir;
        return RunNeoGeoWindow(*board, options);
    }

    // With no window: the frames asked for, as fast as they go
    NeoGeoSound &sound = board->Sound();
    sound.keep = !audioDump.empty() && !mute;
    std::vector<int16_t> recorded;
    FILE *log = hashLog.empty() ? nullptr : fopen(hashLog.c_str(), "w");
    const auto started = std::chrono::steady_clock::now();
    const uint64_t first = board->Frames();
    for (uint64_t n = 0; n < frames; n++) {
        const uint64_t frame = board->Frames() + 1; // (the frame about to be run, counting from 1)
        board->players[0] = board->players[1] = 0;
        for (const Press &p : presses) {
            if (frame >= p.frame && frame < p.frame + p.frames) {
                board->players[p.player] |= p.bits;
            }
        }
        board->RunFrame();
        if (sound.keep) {
            recorded.insert(recorded.end(), sound.made.begin(), sound.made.end());
        }
        sound.made.clear();
        if (log) {
            fprintf(log, "%llu %08x\n", static_cast<unsigned long long>(frame), board->StateHash());
        }
        if (shotEvery && frame % shotEvery == 0) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s_%06llu.png", shotDir.c_str(), GAME_ID, static_cast<unsigned long long>(frame));
            WritePng(path, board->Picture(), NeoGeoVideo::kWidth, NeoGeoVideo::kHeight);
        }
        if (!stateSave.empty() && frame == saveAt) {
            std::vector<uint8_t> data;
            Archive ar(data, true);
            board->Serialize(ar);
            FILE *f = fopen(stateSave.c_str(), "wb");
            if (!f || fwrite(data.data(), 1, data.size(), f) != data.size()) {
                fprintf(stderr, "%s: cannot write the state %s\n", GAME_NAME, stateSave.c_str());
            }
            if (f) {
                fclose(f);
            }
        }
    }
    if (log) {
        fclose(log);
    }
    if (!audioDump.empty()) {
        WriteWav(audioDump, recorded, NeoGeoSound::SampleRate());
    }
    if (stats) {
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        fprintf(stderr, "%llu frames in %.2f s: %.0f a second; watchdog resets %u; unmapped addresses %zu\n",
                static_cast<unsigned long long>(board->Frames() - first), seconds,
                (board->Frames() - first) / (seconds > 0 ? seconds : 1), board->watchdogResets, board->unmapped.size());
    }
    fprintf(stderr, "state hash %08x at frame %llu\n", board->StateHash(), static_cast<unsigned long long>(board->Frames()));
    return 0;
}
