// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Minimal PNG writer (RGB, zlib) for screenshots.
#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>
#include <zlib.h>

// pixels: little-endian XBGR8888 (bytes R, G, B, X), as the Saturn software renderer produces.
inline void WritePng(const char *path, const uint32_t *pixels, uint32_t w, uint32_t h) {
    std::vector<uint8_t> raw(static_cast<size_t>(w * 3 + 1) * h);
    for (uint32_t y = 0; y < h; y++) {
        uint8_t *row = raw.data() + static_cast<size_t>(y) * (w * 3 + 1);
        row[0] = 0;
        for (uint32_t x = 0; x < w; x++) {
            const uint32_t p = pixels[static_cast<size_t>(y) * w + x];
            row[1 + x * 3] = static_cast<uint8_t>(p);
            row[2 + x * 3] = static_cast<uint8_t>(p >> 8);
            row[3 + x * 3] = static_cast<uint8_t>(p >> 16);
        }
    }
    uLongf zSize = compressBound(raw.size());
    std::vector<uint8_t> z(zSize);
    compress2(z.data(), &zSize, raw.data(), raw.size(), 6);

    FILE *f = fopen(path, "wb");
    if (!f) {
        return;
    }
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);
    auto put32 = [&](uint32_t v) {
        const uint8_t b[4] = {static_cast<uint8_t>(v >> 24), static_cast<uint8_t>(v >> 16),
                              static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v)};
        fwrite(b, 1, 4, f);
    };
    auto chunk = [&](const char *type, const uint8_t *data, uint32_t size) {
        put32(size);
        fwrite(type, 1, 4, f);
        if (size) {
            fwrite(data, 1, size, f);
        }
        uLong crc = crc32(0, reinterpret_cast<const Bytef *>(type), 4);
        if (size) {
            crc = crc32(crc, data, size);
        }
        put32(static_cast<uint32_t>(crc));
    };
    const uint8_t ihdr[13] = {static_cast<uint8_t>(w >> 24), static_cast<uint8_t>(w >> 16),
                              static_cast<uint8_t>(w >> 8),  static_cast<uint8_t>(w),
                              static_cast<uint8_t>(h >> 24), static_cast<uint8_t>(h >> 16),
                              static_cast<uint8_t>(h >> 8),  static_cast<uint8_t>(h),
                              8, 2, 0, 0, 0};
    chunk("IHDR", ihdr, 13);
    chunk("IDAT", z.data(), static_cast<uint32_t>(zSize));
    chunk("IEND", nullptr, 0);
    fclose(f);
}
