// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// A stand-in for a game program, for the launcher's example: it lists some features (--list-features) and,
// started, says what it was started with and what the settings file makes of the features.
#include "settings.h"

#include <cstdio>
#include <cstring>

int main(int argc, char **argv) {
    const std::vector<recomp::FeatureInfo> features = {
        {"skip_intro", "Game", "Skip the notices", "Straight to the title.", recomp::kFeatureEnhanced, {}},
        {"endless_time", "Cheats", "Endless time", "The clock stands still.", 0, {}},
        {"health", "Cheats", "Health", "Whose health is kept full.", 0, {"Off", "Player 1", "Both"}},
        {"easy", "Difficulty", "Easy", "Fewer and slower.", recomp::kFeatureExclusive, {}},
        {"hard", "Difficulty", "Hard", "More and faster.", recomp::kFeatureExclusive, {}},
    };
    const char *settings = "";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--list-features")) {
            fputs(recomp::FeatureListText(features).c_str(), stdout);
            return 0;
        }
        if (!strcmp(argv[i], "--settings") && i + 1 < argc) {
            settings = argv[i + 1];
        }
        printf("argument: %s\n", argv[i]);
    }
    recomp::Ini ini(settings);
    ini.load();
    const std::vector<int> values = recomp::FeatureValues(ini, features);
    for (size_t i = 0; i < features.size(); i++) {
        printf("%s = %d\n", features[i].name.c_str(), values[i]);
    }
    return 0;
}
