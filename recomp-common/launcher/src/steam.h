// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#pragma once
// Adding the game to Steam (Linux, e.g. Steam Deck), so it can be played from Game Mode.

#include <string>

// Whether this system can add non-Steam games from here (Linux with Steam installed).
bool can_add_to_steam();

// Writes two desktop entries (the game, straight in with --play; its launcher) and asks the running Steam client to add both. `message` says what happened.
bool add_to_steam(std::string& message);
