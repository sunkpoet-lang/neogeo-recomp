// SPDX-License-Identifier: GPL-3.0-or-later
// From the launcher of wiiparty-recomp by arelkair (https://github.com/arelkair/wiiparty-recomp,
// launcher/src/embedded.h), which is licensed GPL-3.0-or-later; as it was there.
// Part of recomp-common (https://github.com/tmcleroy/recomp-common); see THIRD_PARTY_NOTICES.md.
#pragma once

#include <string_view>

const char* source_revision();
std::string_view blob(const char* name);
