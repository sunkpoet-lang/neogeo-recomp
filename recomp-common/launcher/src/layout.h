// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// (Card, row and inset_separator are from the launcher of wiiparty-recomp by arelkair, GPL-3.0-or-later:
// https://github.com/arelkair/wiiparty-recomp, launcher/src/layout.h; see THIRD_PARTY_NOTICES.md.)
//
// What the launcher's pages are built from: a heading, cards, and rows in them. A port's own pages
// (launcher.h: Description::pages) use the same, so that they look like the launcher's.
#pragma once

#include <algorithm>
#include <cmath>

#include "ui.h"

namespace layout {

// A rounded panel behind its contents.
class Card {
public:
    // `highlight`: one worth the player's attention now (an invitation, a match to join, a new version): tinted with
    // the accent and edged with it
    explicit Card(float width, bool highlight = false)
        : list_(ImGui::GetWindowDrawList()), start_(ImGui::GetCursorScreenPos()), width_(width), highlight_(highlight) {
        list_->ChannelsSplit(2);
        list_->ChannelsSetCurrent(1);
    }
    void end() {
        const ui::Palette& p = ui::palette();
        ImVec2 end(start_.x + width_, ImGui::GetCursorScreenPos().y);
        list_->ChannelsSetCurrent(0);
        list_->AddRectFilled(start_, end, p.surface, ui::px(14));
        if (highlight_) {
            list_->AddRectFilled(start_, end, p.accent_soft, ui::px(14));
            list_->AddRect(start_, end, p.accent, ui::px(14), std::max(1.0f, ui::px(1.5f)));
        }
        list_->ChannelsMerge();
    }

private:
    ImDrawList* list_;
    ImVec2 start_;
    float width_;
    bool highlight_;
};

// A card row: label and detail on the left, a control on the right. In a narrow window, where the words would have
// too little room beside the control, the control goes under them. `dim`: its words greyed (a row that can't be used
// yet: a game that isn't there, say)
template <typename Control>
void row(float width, const char* label, const char* detail, float control_width, float control_height, Control control, bool dim = false) {
    using ui::px;
    const ui::Palette& p = ui::palette();
    ImVec2 top = ImGui::GetCursorScreenPos();
    float pad = px(18);
    ImGui::SetCursorScreenPos(top + ImVec2(pad, px(14)));
    ImGui::BeginGroup();
    float text_width = width - pad * 2 - px(control_width) - px(24);
    bool under = text_width < px(230);
    if (under) {
        text_width = width - pad * 2;
    }
    ui::text(label, ui::Font::Regular, ui::size::kBody, dim ? p.tertiary : p.text, text_width);
    if (detail && *detail) {
        ui::gap(3);
        ui::text(detail, ui::Font::Regular, ui::size::kDetail, dim ? p.tertiary : p.secondary, text_width);
    }
    ImGui::EndGroup();
    float bottom = ImGui::GetItemRectMax().y + px(14);
    if (under) {
        ImGui::SetCursorScreenPos(ImVec2(top.x + pad, bottom - px(2)));
        control();
        bottom += px(control_height) + px(12);
    } else {
        float height = bottom - top.y;
        ImGui::SetCursorScreenPos(ImVec2(top.x + width - pad - px(control_width), top.y + std::round((height - px(control_height)) * 0.5f)));
        control();
    }
    ImGui::SetCursorScreenPos(ImVec2(top.x, bottom));
    ImGui::Dummy(ImVec2(width, 0));
}

inline void inset_separator(float width) {
    using ui::px;
    ImVec2 at = ImGui::GetCursorScreenPos();
    float thickness = std::max(1.0f, std::floor(ui::scale()));
    ImGui::GetWindowDrawList()->AddRectFilled(at + ImVec2(px(18), 0), at + ImVec2(width - px(18), thickness), ui::palette().separator);
    ImGui::Dummy(ImVec2(width, thickness));
}

// A page's title and the lines under it.
inline void heading(const char* title, const char* intro, float width) {
    const ui::Palette& p = ui::palette();
    ui::text(title, ui::Font::Semibold, ui::size::kHeading, p.text);
    if (intro) {
        ui::gap(8);
        ui::text(intro, ui::Font::Regular, ui::size::kLead, p.secondary, std::min(width, ui::px(620)));
    }
    ui::gap(26);
}

// The small capitals over a card.
inline void label(const char* text) {
    ui::text(text, ui::Font::Semibold, ui::size::kDetail, ui::palette().secondary);
    ui::gap(10);
}

}  // namespace layout
