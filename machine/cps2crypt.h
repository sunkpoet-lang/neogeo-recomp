// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The CPS-2 program cipher (see cps2crypt.cpp for where it comes from).
#pragma once

#include <cstddef>
#include <cstdint>

namespace cps2recomp {

// Deciphers a program as the 68000 sees it when it fetches code. `rom` and `dec` hold `words` 16-bit words
// in host order; words outside [lowerWord, upperWord) are copied as they are. The key is the board's 64-bit
// master key as two 32-bit halves.
void DecryptProgram(const uint16_t *rom, uint16_t *dec, size_t words, const uint32_t masterKey[2], uint32_t lowerWord,
                    uint32_t upperWord);

} // namespace cps2recomp
