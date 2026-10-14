// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.

#include "game.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace cps2recomp {

Game &TheGame() {
    static Game game;
    return game;
}

namespace {
    uint8_t *g_ram = nullptr;
} // namespace
void SetRam(uint8_t *ram) {
    g_ram = ram;
}
uint8_t *Ram(uint32_t address) {
    return g_ram + (address & 0xFFFF);
}

namespace {
    uint8_t *g_program = nullptr;
    uint32_t g_programSize = 0;
} // namespace
void SetProgram(uint8_t *program, uint32_t size) {
    g_program = program;
    g_programSize = size;
}
uint32_t ProgramSize() {
    return g_programSize;
}
uint8_t *Program(uint32_t address) {
    return g_program + address;
}

namespace {
    std::map<std::string, int> g_on;
    std::set<std::string> g_aids; // the features marked kFeatureTraining (LoadFeatures)
    bool g_fastForward = false;
    bool g_runningAhead = false;
    int g_slowMotion = 1;
    std::string g_settingsPath;

    std::string Trim(const std::string &s) {
        const size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
        return a == std::string::npos ? "" : s.substr(a, b - a + 1);
    }
} // namespace

std::vector<recomp::FeatureInfo> FeatureList() {
    std::vector<recomp::FeatureInfo> list;
    for (const Feature &feature : TheGame().features) {
        recomp::FeatureInfo info{feature.name, feature.category, feature.title, feature.description, feature.flags, {}};
        info.choices.assign(feature.choices.begin(), feature.choices.end());
        list.push_back(info);
    }
    return list;
}

const std::string &SettingsPath() {
    return g_settingsPath;
}

void LoadFeatures(const std::string &settingsPath, const std::string &list) {
    g_settingsPath = settingsPath;
    g_on.clear();
    g_aids.clear();
    for (const Feature &feature : TheGame().features) {
        if (feature.flags & kFeatureTraining) {
            g_aids.insert(feature.name);
        }
    }
    // (the rules the launcher follows too: recomp-common/settings.h)
    recomp::Ini settings(settingsPath);
    settings.load();
    const std::vector<int> values = recomp::FeatureValues(settings, FeatureList());
    for (size_t i = 0; i < values.size(); i++) {
        g_on[TheGame().features[i].name] = values[i];
    }
    std::stringstream names(list);
    std::string name;
    while (std::getline(names, name, ',')) {
        name = Trim(name);
        if (!name.empty()) {
            const bool off = name[0] == '-';
            // (NAME, -NAME, or NAME=N for one of a feature's choices)
            const size_t eq = name.find('=');
            if (eq != std::string::npos) {
                g_on[name.substr(0, eq)] = atoi(name.c_str() + eq + 1);
            } else {
                g_on[off ? name.substr(1) : name] = off ? 0 : 1;
            }
        }
    }
}

bool FeatureOn(const char *name) {
    return FeatureValue(name) != 0;
}

int FeatureSetting(const char *name) {
    const auto it = g_on.find(name);
    return it == g_on.end() ? 0 : it->second;
}

int FeatureValue(const char *name) {
    const auto it = g_on.find(name);
    if (it == g_on.end() || it->second == 0) {
        return 0;
    }
    // (a training aid, outside a training session: off)
    if (TheGame().inTraining && g_aids.count(it->first) && !TheGame().inTraining()) {
        return 0;
    }
    return it->second;
}

void TrainingAids(std::initializer_list<const char *> names) {
    for (const char *name : names) {
        for (Feature &feature : TheGame().features) {
            if (!strcmp(feature.name, name)) {
                feature.flags |= kFeatureTraining;
            }
        }
    }
}

void EndSessionFeatures() {
    for (const Feature &feature : TheGame().features) {
        if (feature.flags & kFeatureSession) {
            g_on[feature.name] = 0;
        }
    }
}

void SetFastForward(bool on) {
    g_fastForward = on;
}
bool FastForward() {
    return g_fastForward;
}

bool RunningAhead() {
    return g_runningAhead;
}
void SetRunningAhead(bool on) {
    g_runningAhead = on;
}

void SetSlowMotion(int by) {
    g_slowMotion = by < 1 ? 1 : by;
}
int SlowMotion() {
    return g_slowMotion;
}

void SetFeature(const char *name, bool on) {
    SetFeatureValue(name, on ? 1 : 0);
}

void SetFeatureValue(const char *name, int value) {
    const bool on = value != 0;
    g_on[name] = value;
    // (one of a category's exclusive features at a time, as in the launcher)
    for (const Feature &changed : TheGame().features) {
        if (on && (changed.flags & kFeatureExclusive) && !strcmp(changed.name, name)) {
            for (const Feature &other : TheGame().features) {
                if ((other.flags & kFeatureExclusive) && !strcmp(other.category, changed.category) && strcmp(other.name, name)) {
                    g_on[other.name] = 0;
                }
            }
        }
    }
    if (g_settingsPath.empty()) {
        return;
    }
    // The file as it is but for this feature (and, coming from a preset, the others written down as they
    // were): as the launcher writes it
    const std::vector<recomp::FeatureInfo> features = FeatureList();
    for (size_t i = 0; i < features.size(); i++) {
        if (features[i].name == name) {
            recomp::Ini settings(g_settingsPath);
            settings.load();
            recomp::SetFeature(settings, features, i, value);
            settings.save();
        }
    }
    // (a feature whose switch is on the launcher's first page: that switch too, in launcher.ini beside
    // settings.ini, or the launcher would turn it back the next time the game is started)
    for (const Feature &feature : TheGame().features) {
        if ((feature.flags & kFeatureFirstPage) && !strcmp(feature.name, name)) {
            const std::string launcherIni = (std::filesystem::path(g_settingsPath).parent_path() / "launcher.ini").string();
            if (std::filesystem::exists(launcherIni)) {
                recomp::Ini prefs(launcherIni);
                prefs.load();
                prefs.set("video", name, on ? "1" : "0");
                prefs.save();
            }
        }
    }
}

namespace {
    std::vector<HudLine> g_hud;
    uint64_t g_hudSerial = 0;
} // namespace

namespace {
    std::vector<HudBox> g_boxes;
} // namespace
void SetBoxes(std::vector<HudBox> boxes) {
    if (boxes != g_boxes) {
        g_boxes = std::move(boxes);
        g_hudSerial++;
    }
}
const std::vector<HudBox> &Boxes() {
    return g_boxes;
}

void SetHud(std::vector<HudLine> lines) {
    if (lines != g_hud) {
        g_hud = std::move(lines);
        g_hudSerial++;
    }
}
const std::vector<HudLine> &Hud() {
    return g_hud;
}
uint64_t HudSerial() {
    return g_hudSerial;
}

} // namespace cps2recomp
