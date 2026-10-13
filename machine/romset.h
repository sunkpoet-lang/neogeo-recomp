// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The game's ROM set: the files of an arcade ROM collection's set, read from a folder (or later a zip),
// checked against the checksums in game.cfg, and laid out the way the board's chips see them.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cps2recomp {

struct RomSet {
    // The 68000's ROM, as bytes in the order the CPU sees them: as it is on the chips (what a data read
    // gives), and deciphered (what a program read gives)
    std::vector<uint8_t> program;
    std::vector<uint8_t> opcodes;
    // Graphics: 16x16 tiles of 128 bytes each (see video/tiles.h for the layout)
    std::vector<uint8_t> graphics;
    // The Z80's ROM: the files one after the other. The first 0x8000 bytes are always mapped;
    // bank n is at 0x8000 + n * 0x4000.
    std::vector<uint8_t> soundProgram;
    // The QSound chip's samples: signed bytes
    std::vector<uint8_t> samples;

    uint32_t key[2] = {};     // the board's 64-bit key
    uint32_t encryptedEnd = 0; // program below this address is enciphered
};

// One file of the set and its CRC32
struct RomFile {
    std::string name;
    uint32_t crc = 0;
};
// The set's files by part, from game.cfg's lists ("name:crc name:crc ...")
struct RomList {
    std::vector<RomFile> program, graphics, soundProgram, samples, key;
    // The set's name in ROM collections, and its parent set's (collections often keep the files a set
    // shares with its parent only in the parent's zip)
    std::string set, parent;
    // Where the first graphics file sits in the graphics space (a list that starts "@800000": boards with
    // few graphics ROMs have them in the last sockets, and the game's tile numbers count from the first)
    size_t graphicsBase = 0;
};
RomList ParseRomList(const char *program, const char *graphics, const char *soundProgram, const char *samples,
                     const char *key, const char *set = "", const char *parent = "");

// Reads the set from `path`: a folder of its files, or a zip (<set>.zip; files not in it are looked for in
// the parent's and the set's zips beside it, and a folder's files can be in such zips inside it). Files in
// zips are found by checksum, whatever they are called. Nothing is written or changed.
// False with a message if a file is missing or is not the one expected.
bool LoadRomSet(const std::string &path, const RomList &list, RomSet &set, std::string &error);

} // namespace cps2recomp
