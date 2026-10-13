// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.

#include "romset.h"

#include "cps2crypt.h"

#include <sys/stat.h>
#include <zlib.h>

#include <algorithm>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace cps2recomp {

namespace {
    std::vector<RomFile> ParseFiles(const char *text) {
        std::vector<RomFile> files;
        std::istringstream in(text);
        std::string item;
        while (in >> item) {
            const size_t colon = item.find(':');
            RomFile file;
            file.name = item.substr(0, colon);
            if (colon != std::string::npos) {
                file.crc = static_cast<uint32_t>(strtoul(item.c_str() + colon + 1, nullptr, 16));
            }
            files.push_back(file);
        }
        return files;
    }

    bool ReadWhole(const std::string &path, std::vector<uint8_t> &data) {
        FILE *f = fopen(path.c_str(), "rb");
        if (!f) {
            return false;
        }
        fseek(f, 0, SEEK_END);
        const long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        data.resize(size > 0 ? static_cast<size_t>(size) : 0);
        const size_t got = fread(data.data(), 1, data.size(), f);
        fclose(f);
        return got == data.size();
    }

    // A zip file: its list of files, and one of them unpacked
    struct Zip {
        struct Entry {
            std::string name;
            uint32_t crc = 0, packed = 0, size = 0, offset = 0;
            uint16_t method = 0;
        };
        std::string path;
        std::vector<Entry> entries;

        static uint32_t Le32(const uint8_t *p) {
            return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24);
        }
        static uint16_t Le16(const uint8_t *p) {
            return static_cast<uint16_t>(p[0] | (p[1] << 8));
        }
        bool Open(const std::string &file) {
            path = file;
            FILE *f = fopen(file.c_str(), "rb");
            if (!f) {
                return false;
            }
            // The directory is at the end, after a record that says where
            fseek(f, 0, SEEK_END);
            const long size = ftell(f);
            const long tailSize = std::min<long>(size, 65536 + 22);
            std::vector<uint8_t> tail(static_cast<size_t>(tailSize));
            fseek(f, size - tailSize, SEEK_SET);
            bool ok = fread(tail.data(), 1, tail.size(), f) == tail.size();
            long end = -1;
            for (long at = tailSize - 22; ok && at >= 0; at--) {
                if (Le32(&tail[at]) == 0x06054B50) {
                    end = at;
                    break;
                }
            }
            if (end < 0) {
                fclose(f);
                return false;
            }
            const uint32_t count = Le16(&tail[end + 10]), dirSize = Le32(&tail[end + 12]), dirAt = Le32(&tail[end + 16]);
            std::vector<uint8_t> dir(dirSize);
            fseek(f, dirAt, SEEK_SET);
            ok = fread(dir.data(), 1, dir.size(), f) == dir.size();
            fclose(f);
            size_t at = 0;
            for (uint32_t n = 0; ok && n < count && at + 46 <= dir.size() && Le32(&dir[at]) == 0x02014B50; n++) {
                Entry e;
                e.method = Le16(&dir[at + 10]);
                e.crc = Le32(&dir[at + 16]);
                e.packed = Le32(&dir[at + 20]);
                e.size = Le32(&dir[at + 24]);
                e.offset = Le32(&dir[at + 42]);
                const size_t nameLen = Le16(&dir[at + 28]);
                if (at + 46 + nameLen > dir.size()) {
                    break;
                }
                e.name.assign(reinterpret_cast<const char *>(&dir[at + 46]), nameLen);
                entries.push_back(e);
                at += 46 + nameLen + Le16(&dir[at + 30]) + Le16(&dir[at + 32]);
            }
            return ok;
        }
        bool Read(const Entry &e, std::vector<uint8_t> &data) const {
            FILE *f = fopen(path.c_str(), "rb");
            if (!f) {
                return false;
            }
            uint8_t local[30];
            fseek(f, e.offset, SEEK_SET);
            bool ok = fread(local, 1, 30, f) == 30 && Le32(local) == 0x04034B50;
            std::vector<uint8_t> packed(e.packed);
            if (ok) {
                fseek(f, e.offset + 30 + Le16(&local[26]) + Le16(&local[28]), SEEK_SET);
                ok = fread(packed.data(), 1, packed.size(), f) == packed.size();
            }
            fclose(f);
            if (!ok) {
                return false;
            }
            if (e.method == 0) {
                data = std::move(packed);
                return data.size() == e.size;
            }
            if (e.method != 8) {
                return false;
            }
            data.resize(e.size);
            z_stream z = {};
            if (inflateInit2(&z, -MAX_WBITS) != Z_OK) {
                return false;
            }
            z.next_in = packed.data();
            z.avail_in = static_cast<uInt>(packed.size());
            z.next_out = data.data();
            z.avail_out = static_cast<uInt>(data.size());
            const int result = inflate(&z, Z_FINISH);
            inflateEnd(&z);
            return result == Z_STREAM_END && z.total_out == e.size;
        }
    };

    // Where the set's files are looked for: zips first (by checksum), then loose files in a folder (by name)
    struct Places {
        std::vector<Zip> zips;
        std::string folder;
    };

    bool ReadFile(const Places &places, const RomFile &file, std::vector<uint8_t> &data, std::string &error) {
        for (const Zip &zip : places.zips) {
            for (const Zip::Entry &e : zip.entries) {
                const size_t slash = e.name.find_last_of('/');
                const std::string base = slash == std::string::npos ? e.name : e.name.substr(slash + 1);
                if (file.crc ? e.crc == file.crc : base == file.name) {
                    if (zip.Read(e, data) &&
                        (!file.crc || static_cast<uint32_t>(crc32(0, data.data(), static_cast<uInt>(data.size()))) == file.crc)) {
                        return true;
                    }
                    error = "could not unpack " + e.name + " from " + zip.path;
                    return false;
                }
            }
        }
        if (places.folder.empty()) {
            error = "the set has no " + file.name + " (it may be in the parent set's zip, which should be beside this one)";
            return false;
        }
        const std::string path = places.folder + "/" + file.name;
        if (!ReadWhole(path, data)) {
            error = "missing file: " + path;
            return false;
        }
        const uint32_t crc = static_cast<uint32_t>(crc32(0, data.data(), static_cast<uInt>(data.size())));
        if (file.crc && crc != file.crc) {
            char text[160];
            snprintf(text, sizeof(text), "%s is not the file this port is made from (CRC32 %08x, expected %08x)",
                     file.name.c_str(), crc, file.crc);
            error = text;
            return false;
        }
        return true;
    }

    // The graphics ROMs' address lines are wired in a shuffled order; this undoes it for one bank
    // (the same recursive swap as MAME's cps2 driver)
    void Unshuffle(uint64_t *buf, size_t len) {
        if (len == 2) {
            return;
        }
        len /= 2;
        Unshuffle(buf, len);
        Unshuffle(buf + len, len);
        for (size_t i = 0; i < len / 2; i++) {
            const uint64_t t = buf[len / 2 + i];
            buf[len / 2 + i] = buf[len + i];
            buf[len + i] = t;
        }
    }

    // The key file: ten 16-bit words, bit-reversed and rotated (as MAME reads it)
    bool DecodeKey(const std::vector<uint8_t> &file, RomSet &set) {
        if (file.size() < 20) {
            return false;
        }
        uint16_t decoded[10] = {};
        for (int b = 0; b < 10 * 16; b++) {
            const int bit = (317 - b) % 160;
            if ((file[bit / 8] >> ((bit ^ 7) % 8)) & 1) {
                decoded[b / 16] |= (0x8000 >> (b % 16));
            }
        }
        set.key[0] = (static_cast<uint32_t>(decoded[0]) << 16) | decoded[1];
        set.key[1] = (static_cast<uint32_t>(decoded[2]) << 16) | decoded[3];
        set.encryptedEnd = (((~decoded[9] & 0x3FFu) << 14) | 0x3FFF) + 1;
        return true;
    }
} // namespace

RomList ParseRomList(const char *program, const char *graphics, const char *soundProgram, const char *samples,
                     const char *key, const char *set, const char *parent) {
    RomList list;
    list.set = set;
    list.parent = parent;
    list.program = ParseFiles(program);
    if (graphics[0] == '@') {
        char *end;
        list.graphicsBase = strtoul(graphics + 1, &end, 16);
        graphics = end;
    }
    list.graphics = ParseFiles(graphics);
    list.soundProgram = ParseFiles(soundProgram);
    list.samples = ParseFiles(samples);
    list.key = ParseFiles(key);
    return list;
}

bool LoadRomSet(const std::string &path, const RomList &list, RomSet &set, std::string &error) {
    std::vector<uint8_t> data;
    Places folder;
    {
        struct stat info;
        if (stat(path.c_str(), &info) != 0) {
            error = "there is no " + path;
            return false;
        }
        std::vector<std::string> zips;
        std::string beside;
        if (S_ISDIR(info.st_mode)) {
            folder.folder = path;
            beside = path;
        } else {
            zips.push_back(path);
            const size_t slash = path.find_last_of('/');
            beside = slash == std::string::npos ? "." : path.substr(0, slash);
        }
        for (const std::string &name : {list.set, list.parent}) {
            if (!name.empty() && beside + "/" + name + ".zip" != path) {
                zips.push_back(beside + "/" + name + ".zip");
            }
        }
        for (size_t n = 0; n < zips.size(); n++) {
            Zip zip;
            if (zip.Open(zips[n])) {
                folder.zips.push_back(std::move(zip));
            } else if (n == 0 && folder.folder.empty()) {
                error = path + " is not a zip file";
                return false;
            }
        }
    }

    // Program: the files hold each 16-bit word low byte first
    set.program.clear();
    for (const RomFile &file : list.program) {
        if (!ReadFile(folder, file, data, error)) {
            return false;
        }
        for (size_t i = 0; i + 1 < data.size(); i += 2) {
            set.program.push_back(data[i + 1]);
            set.program.push_back(data[i]);
        }
    }

    if (list.key.empty() || !ReadFile(folder, list.key[0], data, error) || !DecodeKey(data, set)) {
        if (error.empty()) {
            error = "the key file is not a CPS-2 key";
        }
        return false;
    }
    {
        const size_t words = set.program.size() / 2;
        std::vector<uint16_t> in(words), out(words);
        for (size_t i = 0; i < words; i++) {
            in[i] = static_cast<uint16_t>((set.program[i * 2] << 8) | set.program[i * 2 + 1]);
        }
        DecryptProgram(in.data(), out.data(), words, set.key, 0, set.encryptedEnd / 2);
        set.opcodes.resize(set.program.size());
        for (size_t i = 0; i < words; i++) {
            set.opcodes[i * 2] = static_cast<uint8_t>(out[i] >> 8);
            set.opcodes[i * 2 + 1] = static_cast<uint8_t>(out[i]);
        }
    }

    // Graphics: four files side by side make 64-bit words, two bytes from each
    set.graphics.assign(list.graphicsBase, 0);
    for (size_t group = 0; group + 3 < list.graphics.size(); group += 4) {
        std::vector<uint8_t> files[4];
        for (int n = 0; n < 4; n++) {
            if (!ReadFile(folder, list.graphics[group + n], files[n], error)) {
                return false;
            }
            if (files[n].size() != files[0].size()) {
                error = "graphics files of different sizes";
                return false;
            }
        }
        const size_t base = set.graphics.size();
        set.graphics.resize(base + files[0].size() * 4);
        for (size_t i = 0; i + 1 < files[0].size(); i += 2) {
            for (int n = 0; n < 4; n++) {
                set.graphics[base + i * 4 + n * 2] = files[n][i];
                set.graphics[base + i * 4 + n * 2 + 1] = files[n][i + 1];
            }
        }
    }
    {
        const size_t bank = 0x200000;
        std::vector<uint64_t> words(bank / 8);
        for (size_t at = 0; at + bank <= set.graphics.size(); at += bank) {
            memcpy(words.data(), &set.graphics[at], bank);
            Unshuffle(words.data(), bank / 8);
            memcpy(&set.graphics[at], words.data(), bank);
        }
    }

    set.soundProgram.clear();
    for (const RomFile &file : list.soundProgram) {
        if (!ReadFile(folder, file, data, error)) {
            return false;
        }
        set.soundProgram.insert(set.soundProgram.end(), data.begin(), data.end());
    }

    // Samples: 16-bit words low byte first, like the program
    set.samples.clear();
    for (const RomFile &file : list.samples) {
        if (!ReadFile(folder, file, data, error)) {
            return false;
        }
        for (size_t i = 0; i + 1 < data.size(); i += 2) {
            set.samples.push_back(data[i + 1]);
            set.samples.push_back(data[i]);
        }
    }
    return true;
}

} // namespace cps2recomp
