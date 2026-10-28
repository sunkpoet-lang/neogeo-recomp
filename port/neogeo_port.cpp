// SPDX-License-Identifier: GPL-3.0-only
// Part of neogeo-recomp.

#include "neogeo_port.h"

#include "game_info.h"

#include "../machine/romset.h"
#include "../recomp-common/text.h"
#include "../runtime/neogeo_game.h"
#include "../runtime/state.h"
#include "hud.h"
#include "png_write.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#ifndef GAME_BIOS_SET
#define GAME_BIOS_SET "neogeo"
#endif
#ifndef GAME_ROM_PARENT
#define GAME_ROM_PARENT ""
#endif

using namespace cps2recomp;

namespace port {

namespace {
    constexpr uint32_t kStateMark = 0x4E474F31; // "NGO1"
    constexpr uint32_t kStateVersion = 1;

    bool Exists(const std::string &path) {
        struct stat info;
        return stat(path.c_str(), &info) == 0;
    }

    bool ReadFile(const std::string &path, std::vector<uint8_t> &data) {
        FILE *f = fopen(path.c_str(), "rb");
        if (!f) {
            return false;
        }
        data.clear();
        uint8_t buffer[65536];
        size_t got;
        while ((got = fread(buffer, 1, sizeof(buffer), f)) > 0) {
            data.insert(data.end(), buffer, buffer + got);
        }
        fclose(f);
        return true;
    }

    bool WriteFile(const std::string &path, const std::vector<uint8_t> &data) {
        FILE *f = fopen(path.c_str(), "wb");
        if (!f) {
            return false;
        }
        const bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
        return fclose(f) == 0 && ok;
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

    // The folder a path's file is in, with its slash ("": none)
    std::string FolderOf(const std::string &path) {
        const size_t slash = path.find_last_of("/\\");
        return slash == std::string::npos ? "" : path.substr(0, slash + 1);
    }
} // namespace

void RegisterNeoGeoPortFeatures() {
    // (display_*: how the picture is fitted to the window, present.h; the same as the Capcom games')
    TheGame().features.push_back({"display_scaling", "Display", "Scaling",
                                  "Sharp: as large as fits, each pixel a crisp block. Whole pixels: every pixel the same whole "
                                  "number of the screen's pixels tall (a border round it). Smooth: softened all over.",
                                  0, {"Sharp", "Whole pixels", "Smooth"}});
    TheGame().features.push_back({"display_shape", "Display", "Picture shape",
                                  "4:3: as the arcade's monitor showed it. Square pixels: each pixel as wide as it is tall. Fill: "
                                  "the whole window or screen, stretched to it.",
                                  0, {"4:3", "Square pixels", "Fill"}});
    TheGame().features.push_back({"window_size", "Display", "Window size",
                                  "How large the window opens, in times the board's 224 lines (it can be made any size by "
                                  "dragging its edges; full screen is on the launcher's first page and F11).",
                                  0, {"3x", "2x", "4x", "5x", "6x"}});
}

bool NeoGeoPort::Init(std::string &error) {
    if (opt.rom.empty()) {
        opt.rom = FindRom();
    }
    const NeoGeoList list = ParseNeoGeoList(GAME_PROGRAM, GAME_FIX, GAME_SOUND_PROGRAM, GAME_SAMPLES, GAME_GRAPHICS, GAME_BIOS,
                                            GAME_ROM_SET, GAME_ROM_PARENT, GAME_BIOS_SET);
    NeoGeoRoms roms;
    if (!LoadNeoGeoSet(opt.rom, list, roms, error)) {
        return false;
    }
    board = std::make_unique<NeoGeo>(std::move(roms));
    RegisterGame();
    RegisterNeoGeoPortFeatures();
    LoadFeatures(opt.settingsPath, opt.features);
    menu.muted = opt.mute;
    // The backup RAM as the last run left it (a game started by the launcher: its saves folder); the tests start
    // with none, as a new board
    std::vector<uint8_t> backup;
    // (online, both boards start as new ones do, setting themselves up: each player's backup RAM is their own)
    const bool online = opt.online.mode != OnlineOptions::Mode::Off;
    if (online && !opt.stateLoad.empty()) {
        error = "a state can't be loaded to play online";
        return false;
    }
    if (!online && !opt.savesPath.empty() && ReadFile(BackupPath(), backup) && backup.size() == board->BackupRam().size()) {
        board->BackupRam() = backup;
    }
    if (!opt.stateLoad.empty() && !LoadState(opt.stateLoad, error)) {
        return false;
    }
    return true;
}

std::string NeoGeoPort::BackupPath() const {
    return FolderOf(opt.savesPath) + "backup.bin";
}

std::string NeoGeoPort::StatePath(int slot) const {
    return FolderOf(opt.savesPath) + "state" + std::to_string(slot) + ".bin";
}

void NeoGeoPort::RunFrame(const uint16_t players[2]) {
    // (tests: --menu FRAME:KEYS[:shot.png]: the menu opened at that frame and those buttons pressed in it: u d l r, a
    // for a button, b to go back)
    if (!opt.menuTest.empty() && strtoull(opt.menuTest.c_str(), nullptr, 10) == frame) {
        const size_t first = opt.menuTest.find(':'), second = opt.menuTest.find(':', first + 1);
        const std::string keys = opt.menuTest.substr(first + 1, second == std::string::npos ? std::string::npos : second - first - 1);
        menu.Open(true);
        for (const char key : keys) {
            const unsigned press = key == 'u' ? recomp::kMenuUp : key == 'd' ? recomp::kMenuDown : key == 'l' ? recomp::kMenuLeft :
                                   key == 'r' ? recomp::kMenuRight : key == 'a' ? recomp::kMenuConfirm : key == 'b' ? recomp::kMenuBack : 0;
            MenuAct(menu.Press(press));
        }
        if (second != std::string::npos && menu.IsOpen()) {
            std::vector<uint32_t> pixels;
            uint32_t width = 0, height = 0;
            MenuPicture(pixels, width, height);
            menu.Draw(pixels.data(), static_cast<int>(width), static_cast<int>(height));
            WritePng(opt.menuTest.substr(second + 1).c_str(), pixels.data(), width, height);
        }
        menu.Close();
    }
    uint16_t now[2] = {players[0], players[1]};
    // (online, the frame is the session's, with both players' controls as they were: nothing of this side's added)
    if (!online) {
        if (coinFrames > 0) {
            coinFrames--;
            now[0] |= NeoGeo::kCoin;
        }
        if (start2Frames > 0) {
            start2Frames--;
            now[1] |= NeoGeo::kStart;
        }
        if (testFrames > 0 || test) {
            testFrames = std::max(0, testFrames - 1);
            now[0] |= NeoGeo::kTest;
        }
    }
    board->players[0] = now[0];
    board->players[1] = now[1];
    if (TheNeoGeoGame().beforeFrame) {
        TheNeoGeoGame().beforeFrame(*board);
    }
    board->RunFrame();
    if (TheNeoGeoGame().frame) {
        TheNeoGeoGame().frame(*board);
    }
    frame++;
}

void NeoGeoPort::Key(char key) {
    if (online) {
        return; // (both boards run on together: no stopping or slowing)
    }
    switch (key) {
    case 'p':
        paused = !paused;
        steps = 0;
        break;
    case 'o': // (stops the game if it is running)
        if (paused) {
            steps++;
        }
        paused = true;
        break;
    case '-': menu.SetSlowBy(menu.SlowBy() * 2); break;
    case '=': menu.SetSlowBy(menu.SlowBy() / 2); break;
    default: break;
    }
}

bool NeoGeoPort::Tick(const uint16_t players[2]) {
    if (ProgramEnded() >= 0) {
        quit = true;
        return false;
    }
    // (tests: --keys TICK:KEY,...: keys pressed at those ticks; f and NAME=N: a feature set)
    for (size_t at = 0; at < opt.keys.size();) {
        const size_t colon = opt.keys.find(':', at);
        if (colon == std::string::npos) {
            break;
        }
        const size_t end = std::min(opt.keys.find(',', colon), opt.keys.size());
        if (strtoull(opt.keys.c_str() + at, nullptr, 10) == tick && colon + 1 < opt.keys.size()) {
            const char key = opt.keys[colon + 1];
            if (key == 'f') {
                const std::string what = opt.keys.substr(colon + 2, end - colon - 2);
                const size_t eq = what.find('=');
                if (eq != std::string::npos) {
                    SetFeatureValue(what.substr(0, eq).c_str(), atoi(what.c_str() + eq + 1));
                }
            } else {
                Key(key);
            }
        }
        at = end == opt.keys.size() ? end : end + 1;
    }
    tick++;
    if (paused) {
        if (steps == 0) {
            return false;
        }
        steps--;
    } else if (tick % static_cast<uint64_t>(menu.SlowBy()) != 0) {
        return false;
    }
    RunFrame(players);
    return true;
}

void NeoGeoPort::MenuAct(Menu::Action action) {
    std::string error;
    const std::string path = StatePath(menu.Slot());
    if (online && (action == Menu::Action::Save || action == Menu::Action::Load || action == Menu::Action::Reset ||
                   action == Menu::Action::Test || action == Menu::Action::Start2 || action == Menu::Action::Command)) {
        menu.Note("Not while playing online.");
        return;
    }
    switch (action) {
    case Menu::Action::None: return;
    case Menu::Action::Coin:
        coinFrames = 6;
        menu.Close();
        return;
    case Menu::Action::Start2:
        start2Frames = 6;
        menu.Close();
        return;
    case Menu::Action::Test:
        testFrames = 6;
        menu.Close();
        return;
    case Menu::Action::Reset:
        EndSessionFeatures();
        board->Reset(false);
        menu.Close();
        return;
    case Menu::Action::Quit:
        quit = true;
        menu.Close();
        return;
    case Menu::Action::Command:
        RunCommand(menu.Command());
        menu.Close();
        return;
    case Menu::Action::Save:
        if (SaveState(path, error)) {
            menu.Close();
            return;
        }
        break;
    case Menu::Action::Load:
        if (LoadState(path, error)) {
            menu.Close();
            return;
        }
        error = "Nothing is kept in slot " + std::to_string(menu.Slot()) + ".";
        break;
    }
    menu.Note(error);
}

void NeoGeoPort::RunCommand(size_t index) {
    const auto &commands = TheNeoGeoGame().commands;
    if (index < commands.size() && commands[index]) {
        commands[index](*board);
    }
}

void NeoGeoPort::Shown(std::vector<uint32_t> &pixels) const {
    constexpr int kWidth = NeoGeoVideo::kWidth, kHeight = NeoGeoVideo::kHeight;
    pixels.assign(board->Picture(), board->Picture() + kWidth * kHeight);
    DrawHud(pixels.data(), kWidth, kHeight);
    // Stopped or slowed: said at the bottom, with the frame's number (for counting frames by stepping)
    if (paused || menu.SlowBy() > 1) {
        char status[64];
        if (paused) {
            snprintf(status, sizeof(status), "STOPPED  frame %llu", static_cast<unsigned long long>(frame));
        } else {
            snprintf(status, sizeof(status), "SPEED 1/%d", menu.SlowBy());
        }
        const recomp::text::Canvas canvas{pixels.data(), kWidth, kHeight};
        const float width = recomp::text::TextWidth(status, 1);
        const float x = (kWidth - width) / 2, y = kHeight - 11;
        canvas.Fill(x - 3, y - 2, width + 6, 12, {20, 22, 30, 220});
        canvas.Text(x, y, 1, 0, {255, 255, 255, 255}, status);
    }
}

void NeoGeoPort::MenuPicture(std::vector<uint32_t> &pixels, uint32_t &width, uint32_t &height) const {
    std::vector<uint32_t> shown;
    Shown(shown);
    constexpr uint32_t across = NeoGeoVideo::kWidth;
    height = NeoGeoVideo::kHeight * 3;
    width = height * 4 / 3;
    pixels.resize(static_cast<size_t>(width) * height);
    for (uint32_t y = 0; y < height; y++) {
        const uint32_t *from = &shown[static_cast<size_t>(y / 3) * across];
        uint32_t *to = &pixels[static_cast<size_t>(y) * width];
        for (uint32_t x = 0; x < width; x++) {
            to[x] = from[x * across / width];
        }
    }
}

bool NeoGeoPort::SaveState(const std::string &path, std::string &error) {
    std::vector<uint8_t> data;
    Archive ar(data, true);
    ar.Mark(kStateMark);
    uint32_t version = kStateVersion;
    ar.Pod(version);
    board->Serialize(ar);
    if (TheGame().state) {
        TheGame().state(ar);
    }
    if (!WriteFile(path, data)) {
        error = "cannot write " + path;
        return false;
    }
    return true;
}

bool NeoGeoPort::LoadState(const std::string &path, std::string &error) {
    std::vector<uint8_t> data;
    if (!ReadFile(path, data)) {
        error = "cannot read " + path;
        return false;
    }
    // (read into a copy of the state first: a state that does not fit leaves the game as it was)
    std::vector<uint8_t> before;
    Archive keep(before, true);
    board->Serialize(keep);
    Archive ar(data, false);
    ar.Mark(kStateMark);
    uint32_t version = 0;
    ar.Pod(version);
    const bool known = ar.Ok() && version == kStateVersion;
    if (known) {
        board->Serialize(ar);
        if (TheGame().state) {
            TheGame().state(ar);
        }
    }
    if (!known || !ar.Ok()) {
        Archive back(before, false);
        board->Serialize(back);
        error = path + " is not a state this build wrote";
        return false;
    }
    return true;
}

void NeoGeoPort::Finish() {
    // (not after playing online: the player's own backup RAM was not in it)
    if (!opt.savesPath.empty() && board && opt.online.mode == OnlineOptions::Mode::Off) {
        WriteFile(BackupPath(), board->BackupRam());
    }
}

} // namespace port
