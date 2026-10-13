// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The readout over the picture (hud.h).
#include "hud.h"

#include "../runtime/game.h"
#include "../recomp-common/text.h"

#include <algorithm>

namespace port {

std::vector<HudPanel> HudPanels(int width, int height) {
    using namespace recomp::text;
    std::vector<HudPanel> panels;
    // A font dot is a pixel of a 224-line picture
    const float d = static_cast<float>(std::max((height + 112) / 224, 1));
    const int pad = static_cast<int>(3 * d);
    for (const cps2recomp::HudLine &line : cps2recomp::Hud()) {
        if (line.text.empty()) {
            continue;
        }
        HudPanel panel;
        panel.width = static_cast<int>(TextWidth(line.text.c_str(), d)) + 2 * pad;
        panel.height = static_cast<int>(8 * d) + 2 * pad;
        const int at = static_cast<int>(line.x * static_cast<float>(width));
        panel.x = line.align < 0 ? at : line.align > 0 ? at - panel.width : at - panel.width / 2;
        panel.y = static_cast<int>(line.y * static_cast<float>(height));
        panel.x = std::clamp(panel.x, 0, std::max(width - panel.width, 0));
        panel.y = std::clamp(panel.y, 0, std::max(height - panel.height, 0));
        if (panel.width > width || panel.height > height) {
            continue;
        }
        panel.pixels.assign(static_cast<size_t>(panel.width) * panel.height, 0xFF000000u | (30 << 16) | (22 << 8) | 20);
        const Canvas canvas{panel.pixels.data(), panel.width, panel.height};
        canvas.Text(static_cast<float>(pad), static_cast<float>(pad), d, 0, {255, 255, 255, 255}, line.text.c_str());
        panels.push_back(std::move(panel));
    }
    return panels;
}

void DrawHud(uint32_t *pixels, int width, int height) {
    // The boxes first, so that the text is over them
    const recomp::text::Canvas canvas{pixels, width, height};
    for (const cps2recomp::HudBox &box : cps2recomp::Boxes()) {
        const float x = static_cast<float>(box.x), y = static_cast<float>(box.y);
        const float w = static_cast<float>(box.width), h = static_cast<float>(box.height);
        if (box.x >= width || box.y >= height || box.x + box.width <= 0 || box.y + box.height <= 0) {
            continue;
        }
        canvas.Fill(x, y, w, h, {box.r, box.g, box.b, box.fill});
        if (box.fill == 255) {
            continue;
        }
        const recomp::text::Color line{box.r, box.g, box.b, 255};
        canvas.Fill(x, y, w, 1, line);
        canvas.Fill(x, y + h - 1, w, 1, line);
        canvas.Fill(x, y, 1, h, line);
        canvas.Fill(x + w - 1, y, 1, h, line);
    }
    for (const HudPanel &panel : HudPanels(width, height)) {
        for (int row = 0; row < panel.height; row++) {
            std::copy_n(&panel.pixels[static_cast<size_t>(row) * panel.width], panel.width,
                        pixels + static_cast<size_t>(panel.y + row) * width + panel.x);
        }
    }
}

} // namespace port
