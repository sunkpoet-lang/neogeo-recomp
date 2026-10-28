// SPDX-License-Identifier: GPL-3.0-only
// Part of neogeo-recomp.

#include "neogeo_video.h"

#include "../runtime/state.h"

#include <cstring>

namespace cps2recomp {

namespace {
    constexpr int kMaxSprites = 381;       // in the list the chip scans (MAX_SPRITES_PER_SCREEN)
    constexpr int kMaxSpritesPerLine = 96; // it draws no more than these on one line

    // Horizontal shrink: which of a tile's 16 columns are drawn at each of the 16 widths (MAME, "verified on real
    // hardware")
    constexpr uint16_t kShrinkX[16] = {0x0080, 0x0880, 0x0888, 0x2888, 0x288A, 0x2A8A, 0x2AAA, 0xAAAA,
                                       0xAAEA, 0xBAEA, 0xBAEB, 0xBBEB, 0xBBEF, 0xFBEF, 0xFBFF, 0xFFFF};

    // A colour's 5-bit levels as the board's resistor networks make them (3.9k, 2.2k, 1k, 470, 220 ohms), with
    // nothing more on the line, with the "dark" bit's 8.2k to ground, with the shadow's 150 ohms, or both
    struct Levels {
        uint8_t level[32][4];
        Levels() {
            const double resistors[5] = {3900, 2200, 1000, 470, 220};
            double conductance = 0;
            for (double r : resistors) {
                conductance += 1 / r;
            }
            const double loads[4] = {0, 1 / 8200.0, 1 / 150.0, 1 / 8200.0 + 1 / 150.0};
            for (int kind = 0; kind < 4; kind++) {
                for (int value = 0; value < 32; value++) {
                    double on = 0;
                    for (int bit = 0; bit < 5; bit++) {
                        if (value & (1 << bit)) {
                            on += 1 / resistors[bit];
                        }
                    }
                    level[value][kind] = static_cast<uint8_t>(255 * on / (conductance + loads[kind]) + 0.5);
                }
            }
        }
    };
    const Levels kLevels;
} // namespace

NeoGeoVideo::NeoGeoVideo(const Sources &sources)
    : m_src(sources)
    , m_vram(0x10000)
    , m_paletteRam(0x2000)
    , m_colours(0x4000)
    , m_picture(kWidth * kHeight) {
    // Sprite graphics are addressed by pixel (two to a byte) and wrap at the next power of two
    uint64_t span = 1;
    while (span < static_cast<uint64_t>(m_src.spritesSize) * 2) {
        span <<= 1;
    }
    m_spriteMask = static_cast<uint32_t>(span - 1);
    Reset();
}

void NeoGeoVideo::Reset() {
    std::fill(m_vram.begin(), m_vram.end(), 0);
    m_address = m_modulo = m_readBuffer = 0;
    m_animationCounter = m_animationWait = m_animationSpeed = 0;
    m_animationOff = false;
}

void NeoGeoVideo::SetAddress(uint16_t value) {
    // (A15 set: the 2 K words of sprite controls at 0x8000; the read happens at once, into a buffer)
    m_address = (value & 0x8000) ? (value & 0x87FF) : value;
    m_readBuffer = m_vram[m_address];
}

void NeoGeoVideo::WriteData(uint16_t value) {
    m_vram[m_address] = value;
    // The address moves on by the modulo, A15 kept
    SetAddress(static_cast<uint16_t>((m_address & 0x8000) | ((m_address + m_modulo) & 0x7FFF)));
}

void NeoGeoVideo::StepAnimation() {
    if (m_animationWait == 0) {
        m_animationWait = m_animationSpeed;
        m_animationCounter++;
    } else {
        m_animationWait--;
    }
}

void NeoGeoVideo::WritePalette(uint32_t index, uint16_t value) {
    const uint32_t at = (index & 0xFFF) | m_paletteBank;
    m_paletteRam[at] = value;
    // DRGB RRRR GGGG BBBB: each colour's low bit apart, and the "dark" bit
    const int dark = value >> 15;
    const int r = ((value >> 14) & 1) | ((value >> 7) & 0x1E);
    const int g = ((value >> 13) & 1) | ((value >> 3) & 0x1E);
    const int b = ((value >> 12) & 1) | ((value << 1) & 0x1E);
    auto colour = [&](int kind) {
        return static_cast<uint32_t>(kLevels.level[r][kind]) | (static_cast<uint32_t>(kLevels.level[g][kind]) << 8) |
               (static_cast<uint32_t>(kLevels.level[b][kind]) << 16) | 0xFF000000u;
    };
    m_colours[at] = colour(dark);
    m_colours[at + 0x2000] = colour(dark + 2);
}

void NeoGeoVideo::RenderLine(int line) {
    if (line < kFirstLine || line >= kFirstLine + kHeight) {
        return;
    }
    uint32_t *row = &m_picture[static_cast<size_t>(line - kFirstLine) * kWidth];
    const uint32_t *pens = &m_colours[m_paletteBank + (m_shadow ? 0x2000 : 0)];
    // The background: the bank's last colour
    std::fill(row, row + kWidth, pens[0xFFF]);
    DrawSprites(line, row, pens);
    DrawFix(line, row, pens);
}

void NeoGeoVideo::DrawSprites(int line, uint32_t *row, const uint32_t *pens) {
    auto onLine = [line](int y, int rows) {
        return rows == 0 || rows >= 0x20 || ((line - y) & 0x1FF) < rows * 0x10;
    };
    // The chip first lists the sprites on this line, in their order (a chained one takes its leader's height)
    uint16_t list[kMaxSpritesPerLine];
    int count = 0;
    {
        int y = 0, rows = 0;
        for (int number = 0; number < kMaxSprites && count < kMaxSpritesPerLine; number++) {
            const uint16_t yControl = m_vram[0x8200 | number];
            if (!(yControl & 0x40)) {
                y = 0x200 - (yControl >> 7);
                rows = yControl & 0x3F;
            }
            if (rows != 0 && onLine(y, rows)) {
                list[count++] = static_cast<uint16_t>(number);
            }
        }
    }
    // ... then draws them, a 16-pixel line of one tile each, the later over the earlier
    int y = 0, x = 0, rows = 0, zoomY = 0, zoomX = 0;
    for (int n = 0; n < count; n++) {
        const uint16_t number = list[n];
        const uint16_t yControl = m_vram[0x8200 | number];
        const uint16_t zoomControl = m_vram[0x8000 | number];
        if (yControl & 0x40) {
            // chained: next to the one before, at its width, with this one's own width
            x = (x + zoomX + 1) & 0x1FF;
            zoomX = (zoomControl >> 8) & 0x0F;
        } else {
            y = 0x200 - (yControl >> 7);
            x = m_vram[0x8400 | number] >> 7;
            zoomY = zoomControl & 0xFF;
            zoomX = (zoomControl >> 8) & 0x0F;
            rows = yControl & 0x3F;
        }
        if (x >= 0x140 && x <= 0x1F0) {
            continue; // wholly off the picture
        }
        if (!onLine(y, rows)) {
            continue;
        }
        const int spriteLine = (line - y) & 0x1FF;
        int zoomLine = spriteLine & 0xFF;
        bool invert = (spriteLine & 0x100) != 0;
        if (invert) {
            zoomLine ^= 0xFF;
        }
        if (rows > 0x20) {
            // (taller than the 32 tiles: the strip repeats, mirrored)
            zoomLine = zoomLine % ((zoomY + 1) << 1);
            if (zoomLine > zoomY) {
                zoomLine = ((zoomY + 1) << 1) - 1 - zoomLine;
                invert = !invert;
            }
        }
        // Which tile of the strip and which of its lines this is, at this height (the system's table)
        const uint8_t yAndTile = m_src.zoom[(zoomY << 8) | zoomLine];
        int tileLine = yAndTile & 0x0F;
        int tile = yAndTile >> 4;
        if (invert) {
            tileLine ^= 0x0F;
            tile ^= 0x1F;
        }
        const uint32_t at = (static_cast<uint32_t>(number) << 6) | (tile << 1);
        const uint16_t attributes = m_vram[at + 1];
        uint32_t code = ((static_cast<uint32_t>(attributes) << 12) & 0xF0000) | m_vram[at];
        if (!m_animationOff) {
            // auto-animation: the tile number's low 3 or 2 bits from the counter
            if (attributes & 0x08) {
                code = (code & ~7u) | (m_animationCounter & 7);
            } else if (attributes & 0x04) {
                code = (code & ~3u) | (m_animationCounter & 3);
            }
        }
        if (attributes & 0x02) {
            tileLine ^= 0x0F; // flipped vertically
        }
        uint16_t columns = kShrinkX[zoomX];
        uint32_t pixel = ((code << 8) | (tileLine << 4)) & m_spriteMask;
        int step = 1;
        if (attributes & 0x01) {
            pixel += 0x0F; // flipped horizontally
            step = -1;
        }
        const uint32_t *linePens = &pens[(attributes >> 8) << 4];
        int px = x;
        for (int i = 0; i < 16; i++) {
            if (columns & 0x8000) {
                // (from 0x1F1 on, the strip comes in from the left: the 9-bit position wraps)
                const int on = px >= 0x200 ? px - 0x200 : (px > 0x1F0 ? px - 0x200 : px);
                if (on >= 0 && on < kWidth) {
                    // The tile's pixel: the graphics hold each 8-pixel row as four bit planes, the right half first
                    const size_t byte = ((pixel & ~0xFFu) >> 1) | (((pixel & 0x8) ^ 0x8) << 3) | ((pixel & 0xF0) >> 2);
                    if (byte + 3 < m_src.spritesSize) {
                        const uint8_t *src = m_src.sprites + byte;
                        const int bit = pixel & 7;
                        const int colour = (((src[3] >> bit) & 1) << 3) | (((src[1] >> bit) & 1) << 2) |
                                           (((src[2] >> bit) & 1) << 1) | ((src[0] >> bit) & 1);
                        if (colour) {
                            row[on] = linePens[colour];
                        }
                    }
                }
                px++;
            }
            columns <<= 1;
            if (columns == 0) {
                break;
            }
            pixel += step;
        }
    }
}

void NeoGeoVideo::DrawFix(int line, uint32_t *row, const uint32_t *pens) {
    const uint8_t *tiles = m_fixFromCartridge ? m_src.fix : m_src.fixBios;
    const size_t size = m_fixFromCartridge ? m_src.fixSize : m_src.fixBiosSize;
    if (!tiles || size == 0) {
        return;
    }
    // 40 columns of 32 tiles, a column after another in memory from 0x7000; each tile 32 bytes, two pixels a byte (low
    // first), its 8-pixel row in four bytes taken in the order 0x10, 0x18, 0x00, 0x08
    static const uint32_t kOffsets[4] = {0x10, 0x18, 0x00, 0x08};
    const uint16_t *cell = &m_vram[0x7000 | (line >> 3)];
    for (int column = 0; column < 40; column++, cell += 0x20) {
        const uint16_t codeAndPalette = *cell;
        const uint32_t base = (static_cast<uint32_t>(codeAndPalette & 0x0FFF) << 5) | (line & 7);
        const uint32_t *tilePens = &pens[(codeAndPalette >> 12) << 4];
        uint32_t *out = row + column * 8;
        for (int part = 0; part < 4; part++) {
            const uint8_t data = tiles[(base + kOffsets[part]) % size];
            if (data & 0x0F) {
                out[part * 2] = tilePens[data & 0x0F];
            }
            if (data & 0xF0) {
                out[part * 2 + 1] = tilePens[data >> 4];
            }
        }
    }
}

void NeoGeoVideo::Serialize(Archive &ar) {
    ar.Mark(0x4C535043); // "LSPC"
    ar.Vec(m_vram);
    ar.Vec(m_paletteRam);
    ar.Pod(m_address);
    ar.Pod(m_modulo);
    ar.Pod(m_readBuffer);
    ar.Pod(m_paletteBank);
    ar.Pod(m_shadow);
    ar.Pod(m_fixFromCartridge);
    ar.Pod(m_animationSpeed);
    ar.Pod(m_animationCounter);
    ar.Pod(m_animationWait);
    ar.Pod(m_animationOff);
    if (!ar.Saving()) {
        // the colours are made from the palette, as it is written
        const uint16_t bank = m_paletteBank;
        for (uint32_t i = 0; i < 0x2000; i++) {
            m_paletteBank = static_cast<uint16_t>(i & 0x1000);
            WritePalette(i, m_paletteRam[i]);
        }
        m_paletteBank = bank;
    }
}

} // namespace cps2recomp
