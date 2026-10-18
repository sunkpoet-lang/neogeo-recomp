// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.

#include "kabuki.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace cps2recomp {

namespace {
    // The four pairs of bits swapped where `pairs` has a bit (bit j: bits 2j and 2j+1)
    inline uint8_t SwapPairs(uint8_t x, unsigned pairs) {
        const uint8_t mask = static_cast<uint8_t>(((pairs & 1) ? 0x03 : 0) | ((pairs & 2) ? 0x0C : 0) | ((pairs & 4) ? 0x30 : 0) |
                                                  ((pairs & 8) ? 0xC0 : 0));
        const uint8_t swapped = static_cast<uint8_t>(((x & 0x55) << 1) | ((x & 0xAA) >> 1));
        return static_cast<uint8_t>((x & ~mask) | (swapped & mask));
    }
    inline uint8_t Rotate(uint8_t x) {
        return static_cast<uint8_t>((x << 1) | (x >> 7));
    }
    inline uint8_t RotateBack(uint8_t x) {
        return static_cast<uint8_t>((x >> 1) | (x << 7));
    }
    inline unsigned Nibble(uint32_t key, int n) {
        return (key >> (4 * n)) & 7;
    }
    // Which pairs each step swaps for a select byte. The first and last steps take the swap key's nibbles 0-3 for
    // pairs 0-3 (the first step: swap key 1's low half; the last: swap key 2's high half, nibbles 4-7); the second
    // and third take them the other way round (pair 0 from the highest nibble of the half).
    inline unsigned PairsForward(uint32_t key, int first, uint8_t select) {
        unsigned pairs = 0;
        for (int j = 0; j < 4; j++) {
            pairs |= ((select >> Nibble(key, first + j)) & 1u) << j;
        }
        return pairs;
    }
    inline unsigned PairsBackward(uint32_t key, int first, uint8_t select) {
        unsigned pairs = 0;
        for (int j = 0; j < 4; j++) {
            pairs |= ((select >> Nibble(key, first + 3 - j)) & 1u) << j;
        }
        return pairs;
    }
    // The steps, with the pairs each swaps
    inline uint8_t LowSteps(uint8_t x, unsigned a, unsigned b) { // (before the xor key)
        return SwapPairs(Rotate(SwapPairs(x, a)), b);
    }
    inline uint8_t HighSteps(uint8_t x, unsigned a, unsigned b) { // (after it)
        return SwapPairs(Rotate(SwapPairs(Rotate(x), a)), b);
    }
    inline uint8_t HighStepsBack(uint8_t y, unsigned a, unsigned b) {
        return RotateBack(SwapPairs(RotateBack(SwapPairs(y, b)), a));
    }
} // namespace

uint8_t KabukiDecipher(uint8_t byte, const KabukiKey &key, uint16_t select) {
    const uint8_t low = static_cast<uint8_t>(select), high = static_cast<uint8_t>(select >> 8);
    const uint8_t mid = LowSteps(byte, PairsForward(key.swap1, 0, low), PairsBackward(key.swap1, 4, low));
    return HighSteps(static_cast<uint8_t>(mid ^ key.xorKey), PairsBackward(key.swap2, 0, high), PairsForward(key.swap2, 4, high));
}

void KabukiDecode(const uint8_t *rom, size_t size, const KabukiKey &key, uint8_t *opcodes, uint8_t *data) {
    for (size_t a = 0; a < size; a++) {
        opcodes[a] = KabukiDecipher(rom[a], key, static_cast<uint16_t>(a + key.address));
        data[a] = KabukiDecipher(rom[a], key, static_cast<uint16_t>((a ^ 0x1FC0) + key.address + 1));
    }
}

namespace {
    struct Known {
        uint16_t address;
        uint8_t plain;
        bool opcode;
    };
    // How Capcom's QSound sound programs start (kabuki.h). Two forms: with the clear of F000-FFFF (the CPS-1's,
    // "version 1.00 1992") and without (the CPS-2's). Opcodes and operands as the Z80 reads them.
    struct Start {
        std::vector<Known> known;
    };
    std::vector<Start> Starts() {
        auto op = [](uint16_t a, uint8_t v) { return Known{a, v, true}; };
        auto arg = [](uint16_t a, uint8_t v) { return Known{a, v, false}; };
        // 0038: di; ex af,af'; exx (the interrupt routine)
        const std::vector<Known> interrupt = {op(0x38, 0xF3), op(0x39, 0x08), op(0x3A, 0xD9)};
        Start cleared{{op(0, 0xF3), op(1, 0xED), op(2, 0x56),               // di; im 1
                       op(3, 0x31), arg(4, 0xFF), arg(5, 0xFF),             // ld sp,FFFF
                       op(6, 0x21), arg(7, 0x00), arg(8, 0xF0),             // ld hl,F000
                       op(9, 0x11), arg(10, 0x01), arg(11, 0xF0),           // ld de,F001
                       op(12, 0x01), arg(13, 0xFF), arg(14, 0x0F),          // ld bc,0FFF
                       op(15, 0x36), arg(16, 0x00), op(17, 0xED), op(18, 0xB0), // ld (hl),0; ldir
                       op(19, 0x3E), arg(20, 0x77),                         // ld a,77
                       op(21, 0x32), arg(22, 0xFF), arg(23, 0xCF),          // ld (CFFF),a
                       op(24, 0xC3)}};                                      // jp
        Start plain{{op(0, 0xF3), op(1, 0xED), op(2, 0x56), op(3, 0x31), arg(4, 0xFF), arg(5, 0xFF), op(6, 0x3E), arg(7, 0x77),
                     op(8, 0x32), arg(9, 0xFF), arg(10, 0xCF)}};
        for (Start *start : {&cleared, &plain}) {
            start->known.insert(start->known.end(), interrupt.begin(), interrupt.end());
        }
        return {cleared, plain};
    }

    // A known byte's select, less the address key: its low byte and what goes into the high byte (the high byte is
    // this plus the key's high byte, plus a carry from the low)
    struct Place {
        uint8_t low, high;
    };
    Place PlaceOf(const Known &k) {
        const uint16_t base = k.opcode ? k.address : static_cast<uint16_t>((k.address ^ 0x1FC0) + 1);
        return {static_cast<uint8_t>(base), static_cast<uint8_t>(base >> 8)};
    }

    std::vector<std::array<uint8_t, 8>> Orders() { // the 40320 orders of 0-7
        std::vector<std::array<uint8_t, 8>> all;
        std::array<uint8_t, 8> p = {0, 1, 2, 3, 4, 5, 6, 7};
        do {
            all.push_back(p);
        } while (std::next_permutation(p.begin(), p.end()));
        return all;
    }
    uint32_t KeyOf(const std::array<uint8_t, 8> &order) {
        uint32_t key = 0;
        for (int n = 0; n < 8; n++) {
            key |= static_cast<uint32_t>(order[n]) << (4 * n);
        }
        return key;
    }
} // namespace

// The candidates for one form of the start (see FindKabukiKey)
static void Candidates(const uint8_t *rom, const Start &start, std::vector<KabukiKey> &found) {
    static const std::vector<std::array<uint8_t, 8>> orders = Orders();
    const std::vector<Known> &known = start.known;
    const size_t n = known.size();
    std::vector<Place> places(n);
    for (size_t i = 0; i < n; i++) {
        places[i] = PlaceOf(known[i]);
    }
    for (unsigned lowKey = 0; lowKey < 256; lowKey++) {
        // The known bytes in groups that share the select's high byte (whatever the key's high byte): the high
        // steps are then the same bit permutation for the group, and the xor key drops out of the differences
        std::vector<uint8_t> low(n), group(n);
        std::vector<int> groupBase; // (each group's high part)
        for (size_t i = 0; i < n; i++) {
            const unsigned s = places[i].low + lowKey;
            low[i] = static_cast<uint8_t>(s);
            const int high = places[i].high + static_cast<int>(s >> 8);
            auto it = std::find(groupBase.begin(), groupBase.end(), high);
            group[i] = static_cast<uint8_t>(it - groupBase.begin());
            if (it == groupBase.end()) {
                groupBase.push_back(high);
            }
        }
        const size_t groups = groupBase.size();
        std::vector<size_t> first(groups, n);
        for (size_t i = 0; i < n; i++) {
            first[group[i]] = std::min(first[group[i]], i);
        }
        // (each group's second byte, and the pairs by the difference they need there)
        std::vector<size_t> second(groups, n);
        for (size_t i = 0; i < n; i++) {
            if (i != first[group[i]] && second[group[i]] == n) {
                second[group[i]] = i;
            }
        }
        std::vector<std::array<std::vector<unsigned>, 256>> needs(groups);
        std::vector<unsigned> all(256);
        for (unsigned pairs = 0; pairs < 256; pairs++) {
            all[pairs] = pairs;
            for (size_t g = 0; g < groups; g++) {
                if (second[g] < n) {
                    const uint8_t plainDifference = static_cast<uint8_t>(known[second[g]].plain ^ known[first[g]].plain);
                    needs[g][HighStepsBack(plainDifference, pairs & 15, pairs >> 4)].push_back(pairs);
                }
            }
        }
        for (const auto &order1 : orders) {
            const uint32_t swap1 = KeyOf(order1);
            uint8_t mid[64];
            for (size_t i = 0; i < n; i++) {
                mid[i] = LowSteps(rom[known[i].address], PairsForward(swap1, 0, low[i]), PairsBackward(swap1, 4, low[i]));
            }
            // For each group, the high steps' pairs (8 bits: first step's, then last's) that take every byte's
            // difference from the group's first to the known plain difference (looked up by the first such
            // difference: needs[g]); the xor key then follows
            std::vector<std::vector<unsigned>> fits(groups);
            bool any = true;
            for (size_t g = 0; g < groups && any; g++) {
                const size_t f = first[g];
                if (second[g] == n) {
                    fits[g] = all; // (a group of one byte: any pairs, the xor key decides)
                } else {
                    for (const unsigned pairs : needs[g][static_cast<uint8_t>(mid[second[g]] ^ mid[f])]) {
                        bool ok = true;
                        for (size_t i = second[g] + 1; i < n && ok; i++) {
                            if (group[i] == g) {
                                ok = HighSteps(static_cast<uint8_t>(mid[i] ^ mid[f]), pairs & 15, pairs >> 4) ==
                                     static_cast<uint8_t>(known[i].plain ^ known[f].plain);
                            }
                        }
                        if (ok) {
                            fits[g].push_back(pairs);
                        }
                    }
                }
                any = !fits[g].empty();
            }
            if (!any) {
                continue;
            }
            // Every choice of pairs for every group with one xor key; then the key's high byte and swap key 2 that
            // give those pairs
            std::vector<size_t> pick(groups, 0);
            while (true) {
                int xorKey = -1;
                bool ok = true;
                for (size_t g = 0; g < groups && ok; g++) {
                    const unsigned pairs = fits[g][pick[g]];
                    const int x = HighStepsBack(known[first[g]].plain, pairs & 15, pairs >> 4) ^ mid[first[g]];
                    ok = xorKey < 0 || xorKey == x;
                    xorKey = x;
                }
                if (ok) {
                    for (unsigned highKey = 0; highKey < 256; highKey++) {
                        for (const auto &order2 : orders) {
                            const uint32_t swap2 = KeyOf(order2);
                            bool fitsAll = true;
                            for (size_t g = 0; g < groups && fitsAll; g++) {
                                const uint8_t high = static_cast<uint8_t>(groupBase[g] + static_cast<int>(highKey));
                                const unsigned pairs = fits[g][pick[g]];
                                fitsAll = PairsBackward(swap2, 0, high) == (pairs & 15) && PairsForward(swap2, 4, high) == (pairs >> 4);
                            }
                            if (fitsAll) {
                                found.push_back({swap1, swap2, static_cast<uint16_t>((highKey << 8) | lowKey), static_cast<uint8_t>(xorKey)});
                            }
                        }
                    }
                }
                size_t g = 0;
                while (g < groups && ++pick[g] == fits[g].size()) {
                    pick[g++] = 0;
                }
                if (g == groups) {
                    break;
                }
            }
        }
    }
}

// How badly a key's deciphering reads as a Z80 program: following its jumps and calls from the start and the
// interrupt routine, the things a program does not do (running into the ROM's unused space, which is left as FF
// unenciphered; a jump into the sound ROM's banks or its shared memory; a prefix on a prefix; an ED that is no
// instruction; an instruction that starts in the middle of another). The start's bytes fit every candidate; past
// them, in the program's other 256-byte pages, a wrong key deciphers nonsense, which soon does one of these.
static bool Unused(const uint8_t *rom, uint16_t at) {
    for (unsigned k = 0; k < 8; k++) {
        if (at + k >= 0x8000 || rom[at + k] != 0xFF) {
            return false;
        }
    }
    return true;
}

static int Score(const uint8_t *rom, const KabukiKey &key) {
    // (only the bytes looked at are deciphered)
    struct Lazy {
        const uint8_t *rom;
        const KabukiKey &key;
        bool opcode;
        uint8_t operator[](unsigned at) const {
            at &= 0x7FFF;
            return KabukiDecipher(rom[at], key, static_cast<uint16_t>(opcode ? at + key.address : (at ^ 0x1FC0) + key.address + 1));
        }
    };
    const Lazy ops{rom, key, true}, data{rom, key, false};
    static std::vector<uint8_t> seen(0x8000);
    std::fill(seen.begin(), seen.end(), 0); // 1: an instruction starts here; 2: inside one
    std::vector<uint16_t> todo = {0x0000, 0x0038};
    int good = 0, bad = 0;
    while (!todo.empty() && bad < 64) {
        uint16_t pc = todo.back();
        todo.pop_back();
        while (true) {
            if (pc >= 0x8000) {
                bad += pc < 0xF000; // (the banks and the shared memory hold no code)
                break;
            }
            if (seen[pc] == 1) {
                break;
            }
            if (Unused(rom, pc)) {
                bad++; // (the ROM's unused space: no program runs into it)
                break;
            }
            if (seen[pc] == 2) {
                bad++;
                break;
            }
            const uint8_t op = ops[pc];
            unsigned length = 1;
            bool stop = false;
            int target = -1;
            auto word = [&](unsigned at) { return static_cast<uint16_t>(data[pc + at] | (data[pc + at + 1] << 8)); };
            if (op == 0xDD || op == 0xFD) {
                const uint8_t next = ops[pc + 1u];
                if (next == 0xDD || next == 0xFD || next == 0xED) {
                    bad++;
                    break;
                }
                const unsigned x = next >> 6, y = (next >> 3) & 7, z = next & 7;
                const bool memory = (x == 1 && next != 0x76 && (z == 6 || y == 6)) || (x == 2 && z == 6) || next == 0x34 || next == 0x35 || next == 0x36;
                length = next == 0xCB ? 4 : next == 0x21 || next == 0x22 || next == 0x2A ? 4 : next == 0x36 ? 4 : memory ? 3 : 2;
                stop = next == 0xE9;
            } else if (op == 0xED) {
                const uint8_t next = ops[pc + 1u];
                const bool valid = (next >= 0x40 && next < 0x80) || ((next & 0xE4) == 0xA0);
                if (!valid) {
                    bad++;
                    break;
                }
                length = (next & 0xC7) == 0x43 ? 4 : 2;
                stop = next == 0x45 || next == 0x4D;
            } else if (op == 0xCB) {
                length = 2;
            } else {
                const unsigned x = op >> 6, z = op & 7;
                if ((x == 0 && z == 6) || op == 0x10 || op == 0x18 || (op & 0xE7) == 0x20 || (x == 3 && z == 6) || op == 0xD3 || op == 0xDB) {
                    length = 2;
                } else if ((x == 0 && (op & 0x0F) == 0x01) || op == 0x22 || op == 0x2A || op == 0x32 || op == 0x3A || (x == 3 && (z == 2 || z == 4)) ||
                           op == 0xC3 || op == 0xCD) {
                    length = 3;
                }
                if (op == 0x18 || op == 0x10 || (op & 0xE7) == 0x20) {
                    target = static_cast<uint16_t>(pc + 2 + static_cast<int8_t>(data[pc + 1u]));
                    stop = op == 0x18;
                } else if (op == 0xC3 || op == 0xCD || (x == 3 && (z == 2 || z == 4))) {
                    target = word(1);
                    stop = op == 0xC3;
                } else if (x == 3 && z == 7) {
                    target = op & 0x38; // (rst)
                } else if (op == 0xC9 || op == 0xE9) {
                    stop = true;
                }
            }
            for (unsigned k = 0; k < length; k++) {
                if (((pc + k) & 0x7FFF) != pc && seen[(pc + k) & 0x7FFF] == 1) {
                    bad++;
                }
                seen[(pc + k) & 0x7FFF] = k ? 2 : 1;
            }
            good++;
            if (target >= 0) {
                todo.push_back(static_cast<uint16_t>(target));
            }
            if (stop) {
                break;
            }
            pc = static_cast<uint16_t>(pc + length);
        }
    }
    return bad * 100000 - good; // (fewest faults; then, of those, the least code: nonsense runs on)
}

bool FindKabukiKey(const uint8_t *rom, size_t size, KabukiKey &key) {
    if (size < 0x8000) {
        return false;
    }
    for (const Start &start : Starts()) {
        std::vector<KabukiKey> found;
        Candidates(rom, start, found);
        if (found.empty()) {
            continue;
        }
        // The one that reads best: no faults, and more of the program reached than any other with none (a wrong key
        // can read without a fault where it deciphers the same as the right one, and stop sooner)
        int best = 1 << 30, next = 1 << 30;
        for (const KabukiKey &candidate : found) {
            const int score = Score(rom, candidate);
            if (score < best) {
                next = best;
                best = score;
                key = candidate;
            } else if (score < next) {
                next = score;
            }
        }
        return found.size() == 1 || (best < 100000 && next - best >= 20);
    }
    return false;
}

} // namespace cps2recomp
