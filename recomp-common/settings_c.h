// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// The shared settings rules (settings.h) for hosts written in C. The same rules; see settings.h.
// The host links settings.cpp and settings_c.cpp (C++17) with the rest of its C.
#ifndef RECOMP_COMMON_SETTINGS_C_H
#define RECOMP_COMMON_SETTINGS_C_H

#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  RFEATURE_ENHANCED = 2,   // on in the "enhanced" preset
  RFEATURE_EXCLUSIVE = 4,  // only one of its category's exclusive features on at a time
  RFEATURE_SESSION = 8,    // for one run only: never kept in the file, not listed
};

typedef struct {
  const char* name;            // as in the file
  const char* category;
  const char* title;
  const char* description;
  unsigned flags;
  const char* const* choices;  // NULL: on (1) or off (0). Else names ended by NULL, the first meaning off.
} RFeature;

// The list for a launcher (the game's --list-features), a feature a line
void rsettings_list_features(const RFeature* features, int count, FILE* out);

// Every feature's value as the file at `path` has it ([features]; no file: the "pure" preset), into
// values[count]
void rsettings_feature_values(const char* path, const RFeature* features, int count, int* values);

// Gives one feature a value in the file, keeping everything else in it (see SetFeature in settings.h).
// False if the file could not be written.
bool rsettings_set_feature(const char* path, const RFeature* features, int count, int index, int value);

// A key's value in the file, into buffer[size]; false (and "") when it is not there
bool rsettings_get(const char* path, const char* section, const char* key, char* buffer, int size);

#ifdef __cplusplus
}
#endif

#endif
