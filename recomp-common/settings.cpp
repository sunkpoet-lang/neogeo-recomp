// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#include "settings.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace recomp {

namespace {

    std::string Trimmed(const std::string &text) {
        const size_t first = text.find_first_not_of(" \t\r");
        if (first == std::string::npos) {
            return "";
        }
        const size_t last = text.find_last_not_of(" \t\r");
        return text.substr(first, last - first + 1);
    }

    bool SectionLine(const std::string &line, std::string &name) {
        const std::string t = Trimmed(line);
        if (t.size() >= 2 && t.front() == '[' && t.back() == ']') {
            name = Trimmed(t.substr(1, t.size() - 2));
            return true;
        }
        return false;
    }

    bool KeyLine(const std::string &line, std::string &key, std::string &value) {
        const std::string t = Trimmed(line);
        if (t.empty() || t[0] == ';' || t[0] == '#') {
            return false;
        }
        const size_t equals = t.find('=');
        if (equals == std::string::npos) {
            return false;
        }
        key = Trimmed(t.substr(0, equals));
        value = Trimmed(t.substr(equals + 1));
        return true;
    }

    const char kSection[] = "features";

    // What the file says a feature is, or what the preset makes it
    int ValueOf(const Ini &ini, const FeatureInfo &feature, bool enhanced) {
        if (feature.flags & kFeatureSession) {
            return 0;
        }
        const std::string text = ini.get(kSection, feature.name);
        int value = enhanced && (feature.flags & kFeatureEnhanced) ? 1 : 0;
        if (!text.empty()) {
            value = SettingOff(text) ? 0 : std::max(std::atoi(text.c_str()), 1);
        }
        if (feature.choices.empty()) {
            return value != 0;
        }
        return value < static_cast<int>(feature.choices.size()) ? value : 0;
    }

    bool Exclusive(const FeatureInfo &a, const FeatureInfo &b) {
        return (a.flags & kFeatureExclusive) && (b.flags & kFeatureExclusive) && a.category == b.category;
    }

} // namespace

Ini::Ini(std::filesystem::path path) : path_(std::move(path)) {}

void Ini::load() {
    lines_.clear();
    std::ifstream file(path_, std::ios::binary);
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines_.push_back(line);
    }
}

bool Ini::save() const {
    std::error_code error;
    if (path_.has_parent_path()) {
        std::filesystem::create_directories(path_.parent_path(), error);
    }
    std::ofstream file(path_, std::ios::binary | std::ios::trunc);
    for (const std::string &line : lines_) {
        file << line << '\n';
    }
    return static_cast<bool>(file);
}

std::string Ini::get(const std::string &section, const std::string &key) const {
    std::string current;
    for (const std::string &line : lines_) {
        std::string name, k, v;
        if (SectionLine(line, name)) {
            current = name;
        } else if (current == section && KeyLine(line, k, v) && k == key) {
            return v;
        }
    }
    return "";
}

void Ini::set(const std::string &section, const std::string &key, const std::string &value) {
    std::string current;
    size_t sectionEnd = std::string::npos; // where to put a new key of a section that is there
    for (size_t i = 0; i < lines_.size(); i++) {
        std::string name, k, v;
        if (SectionLine(lines_[i], name)) {
            current = name;
            continue;
        }
        if (current != section) {
            continue;
        }
        sectionEnd = i + 1;
        if (KeyLine(lines_[i], k, v) && k == key) {
            lines_[i] = key + "=" + value;
            return;
        }
    }
    if (sectionEnd == std::string::npos) {
        for (size_t i = 0; i < lines_.size(); i++) {
            std::string name;
            if (SectionLine(lines_[i], name) && name == section) {
                sectionEnd = i + 1;
            }
        }
    }
    if (sectionEnd == std::string::npos) {
        if (!lines_.empty() && !lines_.back().empty()) {
            lines_.push_back("");
        }
        lines_.push_back("[" + section + "]");
        sectionEnd = lines_.size();
    }
    lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(sectionEnd), key + "=" + value);
}

std::string FeatureListText(const std::vector<FeatureInfo> &features) {
    std::string text;
    for (const FeatureInfo &feature : features) {
        if (feature.flags & kFeatureSession) {
            continue;
        }
        text += feature.name + "\t" + feature.category + "\t" + std::to_string(feature.flags) + "\t" + feature.title + "\t" + feature.description;
        for (size_t i = 0; i < feature.choices.size(); i++) {
            text += (i ? "|" : "\t") + feature.choices[i];
        }
        text += "\n";
    }
    return text;
}

std::vector<FeatureInfo> ParseFeatureList(const std::string &text) {
    std::vector<FeatureInfo> features;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        // (every field, an empty one too: a feature may have no description)
        std::vector<std::string> parts;
        std::string part;
        for (size_t at = 0;;) {
            const size_t tab = line.find('\t', at);
            parts.push_back(line.substr(at, tab == std::string::npos ? tab : tab - at));
            if (tab == std::string::npos) {
                break;
            }
            at = tab + 1;
        }
        if (parts.size() < 5) {
            continue;
        }
        FeatureInfo feature;
        feature.name = parts[0];
        feature.category = parts[1];
        feature.flags = static_cast<unsigned>(std::strtoul(parts[2].c_str(), nullptr, 10));
        feature.title = parts[3];
        feature.description = parts[4];
        if (parts.size() > 5 && !parts[5].empty()) {
            std::stringstream choices(parts[5]);
            while (std::getline(choices, part, '|')) {
                feature.choices.push_back(part);
            }
        }
        features.push_back(feature);
    }
    return features;
}

bool SettingOff(const std::string &value) {
    return value == "0" || value == "off" || value == "false" || value == "no";
}

std::string Preset(const Ini &ini) {
    const std::string value = ini.get(kSection, "preset");
    return value.empty() ? "pure" : value;
}

std::vector<int> FeatureValues(const Ini &ini, const std::vector<FeatureInfo> &features) {
    const bool enhanced = Preset(ini) == "enhanced";
    std::vector<int> values(features.size());
    for (size_t i = 0; i < features.size(); i++) {
        values[i] = ValueOf(ini, features[i], enhanced);
        for (size_t j = 0; j < i && values[i]; j++) {
            if (values[j] && Exclusive(features[i], features[j])) {
                values[i] = 0;
            }
        }
    }
    return values;
}

void SetFeature(Ini &ini, const std::vector<FeatureInfo> &features, size_t index, int value) {
    if (index >= features.size() || (features[index].flags & kFeatureSession)) {
        return;
    }
    if (Preset(ini) != "custom") {
        const std::vector<int> values = FeatureValues(ini, features);
        for (size_t i = 0; i < features.size(); i++) {
            if (!(features[i].flags & kFeatureSession) && ini.get(kSection, features[i].name).empty()) {
                ini.set(kSection, features[i].name, std::to_string(values[i]));
            }
        }
        ini.set(kSection, "preset", "custom");
    }
    ini.set(kSection, features[index].name, std::to_string(value));
    for (size_t i = 0; value && i < features.size(); i++) {
        if (i != index && Exclusive(features[i], features[index]) && !SettingOff(ini.get(kSection, features[i].name))) {
            ini.set(kSection, features[i].name, "0");
        }
    }
}

void SetPreset(Ini &ini, const std::vector<FeatureInfo> &features, const std::string &preset) {
    ini.set(kSection, "preset", preset);
    if (preset != "custom") {
        for (const FeatureInfo &feature : features) {
            if (!ini.get(kSection, feature.name).empty()) {
                ini.set(kSection, feature.name, ""); // (as the preset has it)
            }
        }
    }
}

} // namespace recomp
