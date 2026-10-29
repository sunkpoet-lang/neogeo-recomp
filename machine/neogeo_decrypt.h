// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <string>
namespace cps2recomp {
struct NeoGeoRoms;
// Original encrypted cartridge only. Invalid layouts fail without mutation.
bool DecryptKof2002(NeoGeoRoms &roms, std::string &error);
}
