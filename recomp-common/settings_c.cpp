// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#include "settings_c.h"

#include "settings.h"

#include <cstring>

namespace {

std::vector<recomp::FeatureInfo> List(const RFeature *features, int count) {
    std::vector<recomp::FeatureInfo> list;
    for (int i = 0; i < count; i++) {
        recomp::FeatureInfo info;
        info.name = features[i].name;
        info.category = features[i].category ? features[i].category : "";
        info.title = features[i].title ? features[i].title : "";
        info.description = features[i].description ? features[i].description : "";
        info.flags = features[i].flags;
        for (const char *const *choice = features[i].choices; choice && *choice; choice++) {
            info.choices.push_back(*choice);
        }
        list.push_back(info);
    }
    return list;
}

} // namespace

extern "C" {

void rsettings_list_features(const RFeature *features, int count, FILE *out) {
    fputs(recomp::FeatureListText(List(features, count)).c_str(), out);
}

void rsettings_feature_values(const char *path, const RFeature *features, int count, int *values) {
    recomp::Ini ini(path ? path : "");
    ini.load();
    const std::vector<int> all = recomp::FeatureValues(ini, List(features, count));
    for (int i = 0; i < count; i++) {
        values[i] = all[i];
    }
}

bool rsettings_set_feature(const char *path, const RFeature *features, int count, int index, int value) {
    if (!path || index < 0 || index >= count) {
        return false;
    }
    recomp::Ini ini(path);
    ini.load();
    recomp::SetFeature(ini, List(features, count), static_cast<size_t>(index), value);
    return ini.save();
}

bool rsettings_get(const char *path, const char *section, const char *key, char *buffer, int size) {
    recomp::Ini ini(path ? path : "");
    ini.load();
    const std::string value = ini.get(section, key);
    if (size > 0) {
        snprintf(buffer, static_cast<size_t>(size), "%s", value.c_str());
    }
    return !value.empty();
}

} // extern "C"
