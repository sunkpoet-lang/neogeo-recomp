// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The board's picture: three scroll screens of tiles (8, 16 and 32 pixels) and the objects (sprites made of
// 16-pixel tiles), with the CPS-2's object priorities; drawn a line at a time, so that what the game changes
// part way down the screen (raster interrupts) lands on the right lines.
//
// The hardware facts (register layout, tile maps, priority rules, palette) follow MAME's cps1/cps2 video
// code (BSD-3-Clause, Paul Leaman et al.); the renderer is cps2recomp's own.
#pragma once

#include <cstdint>
#include <vector>

namespace cps2recomp {

class Archive;

class Video {
public:
    static constexpr int kWidth = 384, kHeight = 224;
    // Of the 512 x 262 raster, the picture starts here
    static constexpr int kFirstColumn = 64, kFirstLine = 16;

    // What the renderer reads: the machine's memory and registers (big-endian words as the 68000 wrote them)
    struct Sources {
        const uint8_t *graphics = nullptr; // the graphics ROMs
        size_t graphicsSize = 0;
        const uint8_t *videoRam = nullptr;   // 0x900000-0x92FFFF
        const uint16_t *cpsA = nullptr;      // 32 registers
        const uint16_t *cpsB = nullptr;      // 32 registers
        const uint16_t *objectRegs = nullptr; // the six words at 0x400000
    };
    explicit Video(const Sources &sources)
        : m_src(sources) {}

    void Reset();
    // The palette is copied from video RAM when the game writes its address register
    void CopyPalette();
    // At the start of vertical blanking the board keeps a copy of the object list for the next picture
    void LatchObjects(const uint8_t *objectRam);
    // One line of the picture (0..kHeight-1): Width() pixels with red in the low byte, then green, then blue
    void RenderLine(int line, uint32_t *out);
    void Serialize(Archive &ar);

    // (for tools: which layers are drawn; bit 0 objects, bits 1-3 scroll screens 1-3)
    unsigned layerMask = 0xF;
    // For a game's own sources: an object is not drawn when this says so of its tile's number and its
    // attributes (the colour set is the low five bits). A game that can tell its players' objects from its
    // stage's can show one without the other. Not part of a save state: who sets it keeps it set.
    bool (*hideObject)(uint32_t tile, uint16_t attributes) = nullptr;
    // For a game's own sources that put pictures together: the picture's pixels that an object is drawn at
    // have their top byte set (0x80 and the object's colour set, 0-31), the others clear, so that a picture
    // of the objects alone can be laid over another picture, and one object's pixels told from another's.
    bool markObjects = false;

    // For a game's own sources that draw something the game's list has not got (a thing the game stopped
    // drawing because the board's picture would not show it): called when the board has taken the object
    // list, with the copy the picture is made from, to change. `list` is 1024 entries of four words;
    // `last` is where the last entry in use starts in it (-4: none), and is to be kept right.
    void (*objectsTaken)(uint16_t *list, int &last) = nullptr;

    // A wider picture, for a game's own sources (a widescreen feature). The board draws 384 of the 512
    // columns its raster counts; the scroll screens and the object list reach further, and `side` more
    // columns (kMaxSide at most) are drawn on each side of the board's own. With 64, the picture is the
    // whole raster: 16:9 where the board's is 4:3.
    // What the game put beyond its picture's edges is not always what should be there, so each scroll
    // screen can be drawn moved to the right by `scroll` pixels, and each object by what `object` says of
    // it (its tile's number and its attributes, as hideObject has them, and which entry of the object
    // list it is, 0-1023; none: not moved).
    // A game that fills a scroll screen's map as its camera moves has only kept up what its own picture
    // shows. `tile` is asked for every tile that is not wholly inside the board's own 384 columns: which
    // scroll screen (0-2), the tile's column and row in the map (0-63), and the picture's column its left
    // edge is at (for telling apart two columns of the picture that are the same column of the map). True:
    // the tile's two words are the ones it gave; false: the map's.
    // Not part of a save state: who sets it sets it before every frame. All zero is the board's picture.
    static constexpr int kMaxSide = 64, kMaxWidth = kWidth + 2 * kMaxSide;
    struct Wide {
        int side = 0;
        int scroll[3] = {0, 0, 0};
        // (the scroll screen of 16-pixel tiles can have a place of its own on every line, from the row
        // scroll table: so can this. If given, 1,024 numbers, one for each row of that table, added to
        // scroll[1] on the line that shows the row; not looked at while the board's row scroll is off.)
        const int16_t *rows = nullptr;
        int (*object)(uint32_t tile, uint16_t attributes, int entry) = nullptr;
        bool (*tile)(int which, int column, int row, int at, uint16_t &code, uint16_t &attributes) = nullptr;
        // (a stage painted narrower than the wide picture in places leaves holes where nothing is drawn and
        // the backdrop shows, in the added columns or where a scroll screen is drawn moved: with this set, a
        // run of such pixels no wider than `side` is the line's own picture mirrored at the nearest drawn pixel)
        bool fillHoles = false;
    } wide;
    int Width() const {
        return kWidth + 2 * wide.side;
    }

private:
    void FillHoles(uint16_t *pens, int width) const;
    Sources m_src;
    uint32_t m_palette[0xC00] = {};
    uint16_t m_objects[0x1000] = {}; // the latched object list: 1024 entries of four words
    uint16_t m_priorityControl = 0;
    int m_lastObject = -4;

    // (each in two forms: the board's own picture, with its width known to the compiler and nothing asked
    // about; and a wider or moved one. The first is as fast as it was before there was a second.)
    template <bool kWide> void RenderLineAs(int line, uint32_t *out);
    template <bool kWide> void DrawScroll(int which, int rasterLine, uint16_t *pens, uint8_t *priority, uint8_t mark);
    template <bool kWide> void DrawObjects(int rasterLine, uint16_t *pens, uint8_t *priority, const uint32_t *masks);
};

} // namespace cps2recomp
