// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Kabuki: the Z80 with a cipher built in, on Capcom's CPS-1 boards with QSound (Cadillacs and Dinosaurs and the
// like). The first 0x8000 bytes of the sound ROM are enciphered; a byte read as an opcode is deciphered one way,
// any other read another way, both by its address. No set of these games' ROMs carries the key, and none is in this
// repository: FindKabukiKey works it out from the player's own ROM, from how Capcom's sound program starts.
//
// How a byte is deciphered (the facts as MAME's kabuki.cpp publishes them, BSD-3-Clause, Nicola Salmoria): from the
// byte's address a 16-bit select is made (opcode: address + addressKey; data: (address ^ 0x1FC0) + addressKey + 1);
// then, in four steps, each of the byte's four pairs of bits (0-1, 2-3, 4-5, 6-7) is swapped or not as a bit of the
// select says (the low byte of the select in the first two steps, the high byte in the last two; which bit for each
// pair is a nibble of a swap key), the byte rotated left by one after the first three steps, and XORed with the xor
// key after the second.
#pragma once

#include <cstdint>
#include <vector>

namespace cps2recomp {

struct KabukiKey {
    uint32_t swap1 = 0, swap2 = 0; // eight 3-bit nibbles each
    uint16_t address = 0;
    uint8_t xorKey = 0;
};

uint8_t KabukiDecipher(uint8_t byte, const KabukiKey &key, uint16_t select);
// The enciphered part of the sound ROM (its first 0x8000 bytes) as the opcode reads and the data reads see it
void KabukiDecode(const uint8_t *rom, size_t size, const KabukiKey &key, uint8_t *opcodes, uint8_t *data);

// Works out the key from the ROM's first bytes: Capcom's QSound sound program starts as every one of them does
// (interrupts off, interrupt mode 1, the stack at the top of memory, its own memory at F000-FFFF cleared, 77 written
// to CFFF to say it is there, and a jump on), and its interrupt routine, at 0038, starts by turning interrupts off
// and taking the other registers. As for every Kabuki key there is, each swap key's nibbles are 0-7 in some order:
// the search splits the cipher in its two halves (the low byte of the select and the high) and meets in the middle,
// in about a second. False if no key, or more than one, fits.
bool FindKabukiKey(const uint8_t *rom, size_t size, KabukiKey &key);

} // namespace cps2recomp
