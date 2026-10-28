// SPDX-License-Identifier: GPL-3.0-only
// Part of cps2recomp (the user's fork: the Neo Geo board).
//
// The Neo Geo's video chip, the LSPC: no tilemaps, only sprites (vertical strips of 16x16 tiles, up to 381, any of them
// shrunk) and a fixed layer of 8x8 tiles on top. Its memory (64 K words) is reached through three registers: an
// address, the data (each write moves the address on by the modulo), and the modulo. The palette is two banks of 4,096
// colours (16 of 16 at a time per tile), in the 68000's memory.
//
// Written from the behaviour MAME documents for it (src/mame/snk/neogeo_spr.cpp, neogeo_v.cpp: BSD-3-Clause), drawn a
// line at a time as the board does: each visible line from the sprites and the fixed layer as they are at that line.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cps2recomp {

class Archive;

class NeoGeoVideo {
public:
    static constexpr int kWidth = 320, kHeight = 224;
    static constexpr int kFirstLine = 16; // the first visible line of the 264 (NEOGEO_VBEND)

    struct Sources {
        const uint8_t *sprites = nullptr; // the C graphics, pairs interleaved (romset.h, NeoGeoRoms)
        size_t spritesSize = 0;
        const uint8_t *fix = nullptr; // the cartridge's S tiles
        size_t fixSize = 0;
        const uint8_t *fixBios = nullptr; // the system's (sfix)
        size_t fixBiosSize = 0;
        const uint8_t *zoom = nullptr; // 000-lo.lo: the vertical shrink table
    };
    explicit NeoGeoVideo(const Sources &sources);

    void Reset();

    // The registers at 0x3C0000 (word index 0-7: the address, data, modulo, mode, the timer's two halves, the
    // interrupt acknowledge and an unused one); the board handles the timer and the acknowledge
    uint16_t ReadData() const {
        return m_readBuffer;
    }
    uint16_t Modulo() const {
        return m_modulo;
    }
    void SetAddress(uint16_t value);
    void WriteData(uint16_t value);
    void SetModulo(uint16_t value) {
        m_modulo = value;
    }
    // The mode register's video part: the auto-animation's speed (frames per step) and whether it is off
    void SetMode(uint16_t value) {
        m_animationSpeed = static_cast<uint8_t>(value >> 8);
        m_animationOff = (value & 0x08) != 0;
    }
    uint8_t AnimationCounter() const {
        return m_animationCounter;
    }
    // Once a frame, where the board counts it (line 256, NEOGEO_VSSTART)
    void StepAnimation();

    // The palette: what the 68000 reads and writes at 0x400000 (in the bank in use)
    uint16_t ReadPalette(uint32_t index) const {
        return m_paletteRam[(index & 0xFFF) | m_paletteBank];
    }
    void WritePalette(uint32_t index, uint16_t value);
    void SetPaletteBank(bool second) {
        m_paletteBank = second ? 0x1000 : 0;
    }
    void SetShadow(bool on) {
        m_shadow = on;
    }
    // Which tiles the fixed layer takes: the cartridge's (true) or the system's
    void SetFixFromCartridge(bool cartridge) {
        m_fixFromCartridge = cartridge;
    }

    // One of the 264 lines, drawn into the picture if it is a visible one
    void RenderLine(int line);
    // The picture: kWidth x kHeight, bytes R, G, B, X (port/png_write.h)
    const uint32_t *Picture() const {
        return m_picture.data();
    }

    void Serialize(Archive &ar);

private:
    void DrawSprites(int line, uint32_t *row, const uint32_t *pens);
    void DrawFix(int line, uint32_t *row, const uint32_t *pens);

    Sources m_src;
    std::vector<uint16_t> m_vram;       // 0x10000 words (the LSPC uses 0x0000-0x7FFF and 0x8000-0x87FF)
    std::vector<uint16_t> m_paletteRam; // two banks of 0x1000
    std::vector<uint32_t> m_colours;    // each palette entry as drawn: normal (0x0000-0x1FFF) and shadowed (0x2000-)
    std::vector<uint32_t> m_picture;
    uint16_t m_address = 0, m_modulo = 0, m_readBuffer = 0;
    uint16_t m_paletteBank = 0;
    bool m_shadow = false, m_fixFromCartridge = false;
    uint8_t m_animationSpeed = 0, m_animationCounter = 0, m_animationWait = 0;
    bool m_animationOff = false;
    uint32_t m_spriteMask = 0; // pixel addresses in the sprite graphics wrap at this (a power of two less one)
};

} // namespace cps2recomp
