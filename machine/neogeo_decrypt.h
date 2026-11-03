// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <string>
namespace cps2recomp {
struct NeoGeoRoms;
// Original encrypted cartridge only. Invalid layouts fail without mutation.
bool DecryptKof2002(NeoGeoRoms &roms, std::string &error);
// Magic Plus II bootleg only, including its separate encrypted S1.
bool DecryptKf2k2mp2(NeoGeoRoms &roms, std::string &error);
bool DecryptKof98(NeoGeoRoms &roms, std::string &error);
}
