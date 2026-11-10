// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// Text for what a port draws itself (the in-game menu, a readout over the picture): a 5x7 dot matrix font on
// a canvas of 32-bit pixels (red in the low byte). UTF-8, with the accented letters of Latin-1 (á, ç, ñ...). No
// dependencies.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace recomp::text {

// The classic 5x7 dot matrix font, ASCII 32-126, a column per byte (bit 0 on top; g j p q y reach a row
// lower)
inline constexpr uint8_t kFont[95][5] = {
  {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7F,0x14,0x7F,0x14},
  {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00},
  {0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00}, {0x14,0x08,0x3E,0x08,0x14}, {0x08,0x08,0x3E,0x08,0x08},
  {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
  {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
  {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}, {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},
  {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
  {0x32,0x49,0x79,0x41,0x3E}, {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
  {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01}, {0x3E,0x41,0x49,0x49,0x7A},
  {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41},
  {0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
  {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},
  {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F},
  {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x00},
  {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7F,0x00}, {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40},
  {0x00,0x01,0x02,0x04,0x00}, {0x20,0x54,0x54,0x54,0x78}, {0x7F,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20},
  {0x38,0x44,0x44,0x48,0x7F}, {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7E,0x09,0x01,0x02}, {0x18,0xA4,0xA4,0xA4,0x7C},
  {0x7F,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7D,0x40,0x00}, {0x40,0x80,0x88,0x7A,0x00}, {0x7F,0x10,0x28,0x44,0x00},
  {0x00,0x41,0x7F,0x40,0x00}, {0x7C,0x04,0x18,0x04,0x78}, {0x7C,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38},
  {0xF8,0x28,0x28,0x28,0x10}, {0x10,0x28,0x28,0x30,0xF8}, {0x7C,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x20},
  {0x04,0x3F,0x44,0x40,0x20}, {0x3C,0x40,0x40,0x20,0x7C}, {0x1C,0x20,0x40,0x20,0x1C}, {0x3C,0x40,0x30,0x40,0x3C},
  {0x44,0x28,0x10,0x28,0x44}, {0x18,0xA0,0xA0,0xA0,0x78}, {0x44,0x64,0x54,0x4C,0x44}, {0x00,0x08,0x36,0x41,0x00},
  {0x00,0x00,0x7F,0x00,0x00}, {0x00,0x41,0x36,0x08,0x00}, {0x08,0x04,0x08,0x10,0x08},
};

// (ForkAde's fork) The Spanish letters and signs, for the in-game chat: a column per 16 bits, bit 0 two rows above
// the cell (a capital's accent), bit 2 its top row; any other character outside ASCII is drawn as "?"
struct ExtraGlyph {
    uint32_t code;
    uint16_t columns[5];
};
inline constexpr ExtraGlyph kSpanish[] = {
  {0xE1, {0x080,0x150,0x154,0x152,0x1E0}}, {0xE9, {0x0E0,0x150,0x154,0x152,0x060}}, // á é
  {0xED, {0x000,0x110,0x1F4,0x102,0x000}}, {0xF3, {0x0E0,0x110,0x114,0x112,0x0E0}}, // í ó
  {0xFA, {0x0F0,0x100,0x104,0x082,0x1F0}}, {0xFC, {0x0F0,0x104,0x100,0x084,0x1F0}}, // ú ü
  {0xF1, {0x1F4,0x022,0x014,0x012,0x1E0}}, {0xC1, {0x1F8,0x044,0x046,0x045,0x1F8}}, // ñ Á
  {0xC9, {0x1FC,0x124,0x126,0x125,0x104}}, {0xCD, {0x000,0x104,0x1FE,0x105,0x000}}, // É Í
  {0xD3, {0x0F8,0x104,0x106,0x105,0x0F8}}, {0xDA, {0x0FC,0x100,0x102,0x101,0x0FC}}, // Ó Ú
  {0xDC, {0x0FC,0x102,0x100,0x102,0x0FC}}, {0xD1, {0x1FC,0x012,0x021,0x042,0x1FD}}, // Ü Ñ
  {0xBF, {0x0C0,0x120,0x114,0x100,0x080}}, {0xA1, {0x000,0x000,0x1F4,0x000,0x000}}, // ¿ ¡
};

// (ForkAde's fork) The next character of UTF-8 text, and the bytes it takes; a byte that does not start a whole
// character is one character of its own (drawn "?")
inline uint32_t NextChar(const char *s, size_t left, size_t &used) {
    const auto b = [&](size_t i) { return static_cast<unsigned char>(s[i]); };
    const unsigned c = b(0);
    size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
    if (n == 0 || n > left) {
        used = 1;
        return c < 0x80 ? c : 0xFFFD;
    }
    uint32_t code = n == 1 ? c : c & (0x7F >> n);
    for (size_t i = 1; i < n; i++) {
        if ((b(i) & 0xC0) != 0x80) {
            used = 1;
            return 0xFFFD;
        }
        code = code << 6 | (b(i) & 0x3F);
    }
    used = n;
    return code;
}

// (ForkAde's fork) How many characters the first `bytes` of UTF-8 text are, and how many bytes its first `chars` are
inline size_t CharCount(const char *s, size_t bytes) {
    size_t count = 0;
    for (size_t at = 0, used = 0; at < bytes; at += used, count++) {
        NextChar(s + at, bytes - at, used);
    }
    return count;
}
inline size_t BytesOf(const char *s, size_t bytes, size_t chars) {
    size_t at = 0;
    for (size_t used = 0; at < bytes && chars > 0; at += used, chars--) {
        NextChar(s + at, bytes - at, used);
    }
    return at;
}

constexpr int kCharWidth = 6, kLineHeight = 10; // a character's cell and a line, in font dots

// UTF-8: the character at s (of the n bytes left), and how many bytes it takes (a broken one: '?', a byte)
inline uint32_t Decode(const char *s, size_t n, size_t &bytes) {
    const unsigned char b = static_cast<unsigned char>(s[0]);
    bytes = 1;
    if (b < 0x80) {
        return b;
    }
    const size_t want = b >= 0xF0 ? 4 : b >= 0xE0 ? 3 : b >= 0xC0 ? 2 : 0;
    if (!want || want > n) {
        return '?';
    }
    uint32_t c = b & (0x7F >> want);
    for (size_t i = 1; i < want; i++) {
        const unsigned char more = static_cast<unsigned char>(s[i]);
        if ((more & 0xC0) != 0x80) {
            return '?';
        }
        c = c << 6 | (more & 0x3F);
    }
    bytes = want;
    return c;
}

// The characters in the first n bytes of s
inline size_t Count(const char *s, size_t n) {
    size_t count = 0;
    for (size_t at = 0, bytes = 0; at < n; at += bytes, count++) {
        Decode(s + at, n - at, bytes);
    }
    return count;
}

// The bytes of s's first `chars` characters (at most n)
inline size_t Bytes(const char *s, size_t n, size_t chars) {
    size_t at = 0;
    for (size_t bytes = 0; at < n && chars > 0; at += bytes, chars--) {
        Decode(s + at, n - at, bytes);
    }
    return at;
}

// A character's dots: the 5 columns of its cell (rows 0-7, bit 0 on top) and two rows over it (bit 0 the upper),
// where a capital's accent goes. Latin-1's letters with accents are their letter and a mark.
struct Glyph {
    uint8_t columns[5];
    uint8_t over[5];
};

inline Glyph GlyphOf(uint32_t c) {
    Glyph g{};
    auto copy = [&](unsigned ascii) {
        for (int i = 0; i < 5; i++) {
            g.columns[i] = kFont[ascii - 32][i];
        }
    };
    if (c >= 32 && c <= 126) {
        copy(c);
        return g;
    }
    // (marks, each two rows of the five columns, bit 0 the left column)
    enum Mark { kNone, kAcute, kGrave, kCircumflex, kTilde, kDiaeresis, kCedilla };
    static constexpr uint8_t kMarks[][2] = {{0, 0}, {0x08, 0x04}, {0x02, 0x04}, {0x04, 0x0A}, {0x16, 0x09}, {0, 0x0A}, {0, 0}};
    struct Accented {
        uint16_t c;
        char base;
        uint8_t mark;
    };
    static constexpr Accented kAccented[] = {
        {0xC0, 'A', kGrave}, {0xC1, 'A', kAcute}, {0xC2, 'A', kCircumflex}, {0xC3, 'A', kTilde}, {0xC4, 'A', kDiaeresis},
        {0xC7, 'C', kCedilla}, {0xC8, 'E', kGrave}, {0xC9, 'E', kAcute}, {0xCA, 'E', kCircumflex}, {0xCB, 'E', kDiaeresis},
        {0xCC, 'I', kGrave}, {0xCD, 'I', kAcute}, {0xCE, 'I', kCircumflex}, {0xCF, 'I', kDiaeresis}, {0xD1, 'N', kTilde},
        {0xD2, 'O', kGrave}, {0xD3, 'O', kAcute}, {0xD4, 'O', kCircumflex}, {0xD5, 'O', kTilde}, {0xD6, 'O', kDiaeresis},
        {0xD9, 'U', kGrave}, {0xDA, 'U', kAcute}, {0xDB, 'U', kCircumflex}, {0xDC, 'U', kDiaeresis}, {0xDD, 'Y', kAcute},
        {0xE0, 'a', kGrave}, {0xE1, 'a', kAcute}, {0xE2, 'a', kCircumflex}, {0xE3, 'a', kTilde}, {0xE4, 'a', kDiaeresis},
        {0xE7, 'c', kCedilla}, {0xE8, 'e', kGrave}, {0xE9, 'e', kAcute}, {0xEA, 'e', kCircumflex}, {0xEB, 'e', kDiaeresis},
        {0xEC, 'i', kGrave}, {0xED, 'i', kAcute}, {0xEE, 'i', kCircumflex}, {0xEF, 'i', kDiaeresis}, {0xF1, 'n', kTilde},
        {0xF2, 'o', kGrave}, {0xF3, 'o', kAcute}, {0xF4, 'o', kCircumflex}, {0xF5, 'o', kTilde}, {0xF6, 'o', kDiaeresis},
        {0xF9, 'u', kGrave}, {0xFA, 'u', kAcute}, {0xFB, 'u', kCircumflex}, {0xFC, 'u', kDiaeresis}, {0xFD, 'y', kAcute},
        {0xFF, 'y', kDiaeresis},
    };
    for (const Accented &a : kAccented) {
        if (a.c != c) {
            continue;
        }
        copy(static_cast<unsigned char>(a.base));
        if (a.base == 'i') {
            g.columns[2] = 0x7C; // (no dot under the mark)
        }
        const bool capital = a.base >= 'A' && a.base <= 'Z';
        for (int i = 0; i < 5; i++) {
            const uint8_t upper = (kMarks[a.mark][0] >> i) & 1, lower = (kMarks[a.mark][1] >> i) & 1;
            if (capital) {
                g.over[i] = static_cast<uint8_t>(upper | lower << 1);
            } else {
                g.columns[i] = static_cast<uint8_t>(g.columns[i] | upper | lower << 1);
            }
        }
        if (a.mark == kCedilla) {
            g.columns[2] |= 0x80;
            g.columns[3] |= 0x80;
        }
        return g;
    }
    struct Own {
        uint16_t c;
        uint8_t columns[5];
    };
    static constexpr Own kOwn[] = {
        {0xA1, {0x00, 0x00, 0x7D, 0x00, 0x00}}, // ¡
        {0xBF, {0x30, 0x48, 0x45, 0x40, 0x20}}, // ¿
        {0xBA, {0x00, 0x12, 0x15, 0x12, 0x00}}, // º
        {0xAA, {0x00, 0x16, 0x15, 0x17, 0x00}}, // ª
        {0xB0, {0x00, 0x02, 0x05, 0x02, 0x00}}, // °
        {0xB7, {0x00, 0x00, 0x08, 0x00, 0x00}}, // ·
        {0xA0, {0x00, 0x00, 0x00, 0x00, 0x00}}, // (a space that does not break)
        {0x2026, {0x40, 0x00, 0x40, 0x00, 0x40}}, // …
        {0x2013, {0x08, 0x08, 0x08, 0x08, 0x08}}, // –
        {0x2014, {0x08, 0x08, 0x08, 0x08, 0x08}}, // —
        {0x2018, {0x00, 0x06, 0x05, 0x00, 0x00}}, // ‘
        {0x2019, {0x00, 0x05, 0x03, 0x00, 0x00}}, // ’
        {0x201C, {0x06, 0x05, 0x00, 0x06, 0x05}}, // “
        {0x201D, {0x05, 0x03, 0x00, 0x05, 0x03}}, // ”
        {0x2192, {0x08, 0x08, 0x2A, 0x1C, 0x08}}, // →
    };
    for (const Own &o : kOwn) {
        if (o.c == c) {
            for (int i = 0; i < 5; i++) {
                g.columns[i] = o.columns[i];
            }
            return g;
        }
    }
    copy('?');
    return g;
}

struct Color {
    uint8_t r, g, b, a;
};

// (ForkAde's fork, online-hud-palette) The colours of what is drawn over an online match, the same in every port:
// each side's player, the connection's readout, the streak numbers, VS, and the see-through band behind them
struct Palette {
    static constexpr Color kPlayer1{90, 160, 255, 255};  // blue
    static constexpr Color kPlayer2{255, 90, 90, 255};   // red
    static constexpr Color kReadout{255, 220, 70, 255};  // yellow
    static constexpr Color kNumber{255, 255, 255, 255};
    static constexpr Color kVersus{170, 175, 190, 255};
    static constexpr Color kBand{20, 22, 30, 140};
};

// (ForkAde's fork) A small font for a readout: 3x5 dots in a 4x6 cell, the space, 0-9 and A-Z (a-z drawn as
// capitals; anything else a blank cell), a row of 3 bits per byte (the left dot the highest bit), the top row first
constexpr int kSmallCharWidth = 4, kSmallLineHeight = 6;
inline constexpr uint8_t kSmallFont[37][5] = {
    {0b000, 0b000, 0b000, 0b000, 0b000}, // ' '
    {0b111, 0b101, 0b101, 0b101, 0b111}, // '0'
    {0b010, 0b110, 0b010, 0b010, 0b111}, // '1'
    {0b111, 0b001, 0b111, 0b100, 0b111}, // '2'
    {0b111, 0b001, 0b011, 0b001, 0b111}, // '3'
    {0b101, 0b101, 0b111, 0b001, 0b001}, // '4'
    {0b111, 0b100, 0b111, 0b001, 0b111}, // '5'
    {0b111, 0b100, 0b111, 0b101, 0b111}, // '6'
    {0b111, 0b001, 0b010, 0b010, 0b010}, // '7'
    {0b111, 0b101, 0b111, 0b101, 0b111}, // '8'
    {0b111, 0b101, 0b111, 0b001, 0b111}, // '9'
    {0b010, 0b101, 0b111, 0b101, 0b101}, // 'A'
    {0b110, 0b101, 0b110, 0b101, 0b110}, // 'B'
    {0b011, 0b100, 0b100, 0b100, 0b011}, // 'C'
    {0b110, 0b101, 0b101, 0b101, 0b110}, // 'D'
    {0b111, 0b100, 0b110, 0b100, 0b111}, // 'E'
    {0b111, 0b100, 0b110, 0b100, 0b100}, // 'F'
    {0b011, 0b100, 0b101, 0b101, 0b011}, // 'G'
    {0b101, 0b101, 0b111, 0b101, 0b101}, // 'H'
    {0b111, 0b010, 0b010, 0b010, 0b111}, // 'I'
    {0b001, 0b001, 0b001, 0b101, 0b010}, // 'J'
    {0b101, 0b101, 0b110, 0b101, 0b101}, // 'K'
    {0b100, 0b100, 0b100, 0b100, 0b111}, // 'L'
    {0b101, 0b111, 0b111, 0b101, 0b101}, // 'M'
    {0b110, 0b101, 0b101, 0b101, 0b101}, // 'N'
    {0b010, 0b101, 0b101, 0b101, 0b010}, // 'O'
    {0b110, 0b101, 0b110, 0b100, 0b100}, // 'P'
    {0b010, 0b101, 0b101, 0b110, 0b011}, // 'Q'
    {0b110, 0b101, 0b110, 0b101, 0b101}, // 'R'
    {0b011, 0b100, 0b010, 0b001, 0b110}, // 'S'
    {0b111, 0b010, 0b010, 0b010, 0b010}, // 'T'
    {0b101, 0b101, 0b101, 0b101, 0b111}, // 'U'
    {0b101, 0b101, 0b101, 0b101, 0b010}, // 'V'
    {0b101, 0b101, 0b111, 0b111, 0b101}, // 'W'
    {0b101, 0b101, 0b010, 0b101, 0b101}, // 'X'
    {0b101, 0b101, 0b010, 0b010, 0b010}, // 'Y'
    {0b111, 0b001, 0b010, 0b100, 0b111}, // 'Z'
};

// The small font's glyph for c: its place in kSmallFont, or -1 (a blank cell)
inline int SmallGlyph(char c) {
    if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - 'a' + 'A');
    }
    if (c == ' ') {
        return 0;
    }
    if (c >= '0' && c <= '9') {
        return 1 + (c - '0');
    }
    if (c >= 'A' && c <= 'Z') {
        return 11 + (c - 'A');
    }
    return -1;
}

// A text's width in the small font, in dots (a cell a byte: the small font has no letters over 127)
inline float SmallTextWidth(const char *s) {
    return static_cast<float>(strlen(s) * kSmallCharWidth);
}

struct Canvas {
    uint32_t *pixels;
    int width, height;

    void Fill(float x, float y, float w, float h, Color c) const {
        const int x0 = std::max(0, static_cast<int>(x)), y0 = std::max(0, static_cast<int>(y));
        const int x1 = std::min(width, static_cast<int>(x + w)), y1 = std::min(height, static_cast<int>(y + h));
        for (int py = y0; py < y1; py++) {
            uint32_t *row = pixels + static_cast<size_t>(py) * width;
            for (int px = x0; px < x1; px++) {
                const uint32_t under = row[px];
                const uint32_t r = ((under & 0xFF) * (255 - c.a) + c.r * c.a) / 255;
                const uint32_t g = (((under >> 8) & 0xFF) * (255 - c.a) + c.g * c.a) / 255;
                const uint32_t b = (((under >> 16) & 0xFF) * (255 - c.a) + c.b * c.a) / 255;
                row[px] = 0xFF000000u | (b << 16) | (g << 8) | r;
            }
        }
    }
    // The first n bytes of s (UTF-8)
    void Chars(float x, float y, float dot, const char *s, size_t n, Color c) const {
        for (size_t at = 0, bytes = 0; at < n; at += bytes, x += kCharWidth * dot) {
            const Glyph glyph = GlyphOf(Decode(s + at, n - at, bytes));
            for (int col = 0; col < 5; col++) {
                for (int row = 0; row < 8; row++) {
                    if (glyph.columns[col] & (1 << row)) {
                        Fill(x + col * dot, y + row * dot, dot, dot, c);
                    }
                }
                for (int row = 0; row < 2; row++) {
                    if (glyph.over[col] & (1 << row)) {
                        Fill(x + col * dot, y + (row - 2) * dot, dot, dot, c);
                    }
                }
            }
        }
    }
    // Text in lines of at most maxWidth, broken at spaces (0: one line); the lines drawn
    int Text(float x, float y, float dot, float maxWidth, Color c, const char *s) const;
    // (ForkAde's fork) One line in the small font, its cells' top left at x, y, a dot a pixel
    void SmallText(float x, float y, Color c, const char *s) const {
        for (; *s; s++, x += kSmallCharWidth) {
            const int glyph = SmallGlyph(*s);
            if (glyph < 0) {
                continue;
            }
            for (int row = 0; row < 5; row++) {
                for (int col = 0; col < 3; col++) {
                    if (kSmallFont[glyph][row] & (4 >> col)) {
                        Fill(x + col, y + row, 1, 1, c);
                    }
                }
            }
        }
    }
};

// The bytes of the next line of s, of at most perLine characters, broken at a space; and where the line after begins
inline size_t NextLine(const char *s, size_t perLine, size_t &next) {
    const size_t length = strlen(s);
    size_t n = Bytes(s, length, perLine);
    if (n < length) {
        size_t cut = n;
        while (cut > 0 && s[cut] != ' ') {
            cut--;
        }
        if (cut > 0) {
            n = cut; // (else a word longer than the line)
        }
    }
    next = n;
    while (s[next] == ' ') {
        next++;
    }
    return n;
}

inline size_t PerLine(const char *s, float dot, float maxWidth) {
    const size_t perLine = maxWidth > 0 ? static_cast<size_t>(maxWidth / (kCharWidth * dot)) : Count(s, strlen(s));
    return std::max<size_t>(perLine, 1);
}

inline int Canvas::Text(float x, float y, float dot, float maxWidth, Color c, const char *s) const {
    const size_t perLine = PerLine(s, dot, maxWidth);
    int lines = 0;
    while (*s) {
        size_t next = 0;
        const size_t n = NextLine(s, perLine, next);
        Chars(x, y + lines * kLineHeight * dot, dot, s, n, c);
        lines++;
        s += next;
    }
    return lines;
}

inline float TextWidth(const char *s, float dot) {
    return Count(s, strlen(s)) * kCharWidth * dot;
}

// The lines Canvas::Text would draw the text in
inline int TextLines(const char *s, float dot, float maxWidth) {
    const size_t perLine = PerLine(s, dot, maxWidth);
    int lines = 0;
    while (*s) {
        size_t next = 0;
        NextLine(s, perLine, next);
        lines++;
        s += next;
    }
    return lines;
}

} // namespace recomp::text
