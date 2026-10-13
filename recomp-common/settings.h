// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
//
// The settings a port's launcher and the game itself share, and the rules both must agree on. Nothing here
// knows of a game, a system or SDL.
//
//   [features]
//   preset = pure | enhanced | custom
//   name = 0 | 1 | N        (N: the number of one of a feature's choices; no value: as the preset has it)
//
// The game prints its features (FeatureListText, for its --list-features flag); the launcher reads them
// (ParseFeatureList). Both read a value with FeatureValues and change one with SetFeature, which writes only
// the lines it must: everything else in the file stays as it was.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace recomp {

// A small INI file editor that keeps everything it does not touch (comments, sections and keys it was not
// asked about, their order).
class Ini {
public:
    explicit Ini(std::filesystem::path path);
    void load();
    bool save() const;
    // "" when the key is not there
    std::string get(const std::string &section, const std::string &key) const;
    void set(const std::string &section, const std::string &key, const std::string &value);
    const std::filesystem::path &path() const { return path_; }

private:
    std::filesystem::path path_;
    std::vector<std::string> lines_;
};

enum : unsigned {
    kFeatureEnhanced = 2,  // on in the "enhanced" preset
    kFeatureExclusive = 4, // only one of its category's exclusive features on at a time
    kFeatureSession = 8,   // for one run of the game only: never kept in the file, not shown by a launcher
};

struct FeatureInfo {
    std::string name;     // as in the file
    std::string category; // the heading it is under
    std::string title;
    std::string description;
    unsigned flags = 0;
    // For a choice of several things rather than on or off: their names, the first meaning off
    std::vector<std::string> choices;
};

// The list as text, a feature a line: name, category, flags, title, description and (only when there are
// some) the choices with '|' between them, tab-separated. Session features are left out. A reader that
// knows nothing of choices sees the five fields it expects; a list with no sixth field has no choices.
std::string FeatureListText(const std::vector<FeatureInfo> &features);
std::vector<FeatureInfo> ParseFeatureList(const std::string &text);

// A value that means off: 0, off, false, no
bool SettingOff(const std::string &value);

// "pure", "enhanced" or "custom" ("pure" when the file says nothing)
std::string Preset(const Ini &ini);

// Every feature's value, in the list's order: 0 off, 1 on, or the number of the choice. A feature the file
// gives no value has the preset's (on only in "enhanced", and only if it is kFeatureEnhanced); a session
// feature is 0; a choice the feature has not is 0; of a category's exclusive features only the first on is.
std::vector<int> FeatureValues(const Ini &ini, const std::vector<FeatureInfo> &features);

// Gives one feature a value. The preset becomes "custom", and on the way there every feature that followed
// the preset is written down as it was, so nothing else changes; a value already in the file is never
// rewritten. Switching an exclusive feature on switches its category's other exclusive ones off. A session
// feature is not written. (The caller saves.)
void SetFeature(Ini &ini, const std::vector<FeatureInfo> &features, size_t index, int value);

// Chooses a preset. "pure" and "enhanced" take every feature's own value out of the file (they follow the
// preset again); "custom" leaves them. (The caller saves.)
void SetPreset(Ini &ini, const std::vector<FeatureInfo> &features, const std::string &preset);

} // namespace recomp
