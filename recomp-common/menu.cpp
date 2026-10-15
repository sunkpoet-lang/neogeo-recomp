// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// The in-game menu (menu.h).
#include "menu.h"

#include "text.h"

#include <algorithm>
#include <cstring>

namespace recomp {

using namespace text;

namespace {
    // (after MenuRow's kinds, which the first page's rows keep)
    enum { kRowSave, kRowLoad, kRowSlot, kRowSpeed, kRowAction, kRowQuit, kRowResume, kRowCommand, kRowPage, kRowFeature, kRowBack, kRowMute };
} // namespace

void Menu::Configure(MenuConfig config) {
    m_config = std::move(config);
    m_speed = std::clamp(m_speed, 0, std::max(static_cast<int>(m_config.speeds.size()) - 1, 0));
    if (m_open) {
        Fill();
    }
}

void Menu::SetSpeed(int speed) {
    m_speed = std::clamp(speed, 0, std::max(static_cast<int>(m_config.speeds.size()) - 1, 0));
}

const MenuPage *Menu::PageOf(const std::string &name) const {
    for (const MenuPage &page : m_config.pages) {
        if (page.name == name) {
            return &page;
        }
    }
    return nullptr;
}

std::string Menu::Parent(const std::string &page) const {
    const MenuPage *of = PageOf(page);
    return of ? of->parent : std::string();
}

// A page is there when something is on it: a feature, a command, or a page under it that is there
bool Menu::Exists(const std::string &page) const {
    if (page.empty()) {
        return true;
    }
    if (std::any_of(m_config.features.begin(), m_config.features.end(), [&](const MenuFeature &f) { return f.category == page; }) ||
        std::any_of(m_config.commands.begin(), m_config.commands.end(), [&](const MenuCommand &c) { return c.category == page; })) {
        return true;
    }
    // (or a page under it has: looked at one level down, which is what a page of pages needs, and cannot go
    // round for ever whatever the host has named as whose parent)
    for (const MenuPage &under : m_config.pages) {
        if (under.parent == page && under.name != page &&
            (std::any_of(m_config.features.begin(), m_config.features.end(), [&](const MenuFeature &f) { return f.category == under.name; }) ||
             std::any_of(m_config.commands.begin(), m_config.commands.end(), [&](const MenuCommand &c) { return c.category == under.name; }))) {
            return true;
        }
    }
    return false;
}

// Back to the page this one was reached from, on the row it was left on
void Menu::Leave() {
    m_page = Parent(m_page);
    m_selected = m_left.empty() ? 0 : m_left.back();
    if (!m_left.empty()) {
        m_left.pop_back();
    }
    if (m_page.empty()) {
        m_left.clear();
    }
    Fill();
}

// Into a page from the one shown, at its top
void Menu::Enter(const std::string &page) {
    m_left.push_back(m_selected);
    m_page = page;
    m_selected = 0;
    Fill();
}

// The rows of the page being shown
void Menu::Fill() {
    m_rows.clear();
    m_first.clear();
    // (a page that is no longer there: the first page)
    if (!m_page.empty() && !Exists(m_page)) {
        m_page.clear();
        m_selected = m_left.empty() ? 0 : m_left.front();
        m_left.clear();
    }
    if (m_page.empty()) {
        for (const std::vector<MenuRow> &group : m_config.groups) {
            bool divider = !m_rows.empty();
            auto add = [&](Row row) {
                row.divider = divider;
                divider = false;
                m_rows.push_back(std::move(row));
            };
            for (const MenuRow &row : group) {
                const size_t index = m_first.size();
                m_first.push_back(row);
                switch (row.kind) {
                case MenuRow::Save: add({kRowSave, index, ""}); break;
                case MenuRow::Load: add({kRowLoad, index, ""}); break;
                case MenuRow::Slot: add({kRowSlot, index, ""}); break;
                case MenuRow::Speed:
                    if (!m_config.speeds.empty()) {
                        add({kRowSpeed, index, ""});
                    }
                    break;
                case MenuRow::Action: add({kRowAction, index, ""}); break;
                case MenuRow::Quit: add({kRowQuit, index, ""}); break;
                case MenuRow::Resume: add({kRowResume, index, ""}); break;
                case MenuRow::Mute:
                    if (m_config.muted && m_config.setMuted) {
                        add({kRowMute, index, ""});
                    }
                    break;
                case MenuRow::Page:
                    if (!row.title.empty() && Exists(row.title)) {
                        add({kRowPage, index, row.title});
                    }
                    break;
                case MenuRow::Pages: {
                    for (size_t i = 0; i < m_config.commands.size(); i++) {
                        if (m_config.commands[i].category.empty()) {
                            add({kRowCommand, i, ""});
                        }
                    }
                    // A page for each category, in the order they first come up: but for the pages that are
                    // under another page, and those the host has given a row of their own
                    std::vector<std::string> pages;
                    for (const std::vector<MenuRow> &others : m_config.groups) {
                        for (const MenuRow &other : others) {
                            if (other.kind == MenuRow::Page) {
                                pages.push_back(other.title);
                            }
                        }
                    }
                    auto addPage = [&](const std::string &category) {
                        if (!category.empty() && std::find(pages.begin(), pages.end(), category) == pages.end()) {
                            pages.push_back(category);
                            if (Parent(category).empty()) {
                                add({kRowPage, 0, category});
                            }
                        }
                    };
                    for (const MenuFeature &feature : m_config.features) {
                        addPage(feature.category);
                    }
                    for (const MenuCommand &command : m_config.commands) {
                        addPage(command.category);
                    }
                    break;
                }
                }
            }
        }
    } else {
        for (size_t i = 0; i < m_config.commands.size(); i++) {
            if (m_page == m_config.commands[i].category) {
                m_rows.push_back({kRowCommand, i, ""});
            }
        }
        for (const MenuPage &under : m_config.pages) {
            if (under.parent == m_page && under.name != m_page && Exists(under.name)) {
                m_rows.push_back({kRowPage, 0, under.name});
            }
        }
        for (size_t i = 0; i < m_config.features.size(); i++) {
            if (m_page == m_config.features[i].category) {
                m_rows.push_back({kRowFeature, i, ""});
            }
        }
        m_rows.push_back({kRowBack, 0, ""});
    }
    m_selected = std::clamp(m_selected, 0, std::max(static_cast<int>(m_rows.size()) - 1, 0));
}

void Menu::Open() {
    m_note.clear();
    Fill(); // (on the page, and the row, it was closed on)
    m_open = true;
}

Menu::Chosen Menu::Buttons(unsigned pressed) {
    if (!m_open || !pressed || m_rows.empty()) {
        return {};
    }
    m_note.clear();
    if (pressed & kMenuBack) {
        if (m_page.empty()) {
            m_open = false;
        } else {
            Leave();
        }
        return {};
    }
    const int count = static_cast<int>(m_rows.size());
    if (pressed & kMenuUp) {
        m_selected = (m_selected + count - 1) % count;
    }
    if (pressed & kMenuDown) {
        m_selected = (m_selected + 1) % count;
    }
    const Row row = m_rows[static_cast<size_t>(m_selected)];
    const bool press = pressed & kMenuConfirm;
    // Rows with several values: Left and Right go through them (Confirm goes on to the next, around)
    const int step = (pressed & kMenuLeft) ? -1 : (press || (pressed & kMenuRight)) ? 1 : 0;
    if (row.kind == kRowSlot && step) { // 1-9, around
        SetSlot(m_slot + step);
        return {};
    }
    if (row.kind == kRowMute && step) {
        m_config.setMuted(!m_config.muted());
        return {};
    }
    if (row.kind == kRowSpeed && step) {
        const int speeds = static_cast<int>(m_config.speeds.size());
        m_speed = press ? (m_speed + 1) % speeds : std::clamp(m_speed + step, 0, speeds - 1);
        return {};
    }
    if (row.kind == kRowFeature && step && (press || !m_config.features[row.index].choices.empty())) {
        const MenuFeature &feature = m_config.features[row.index];
        const int value = m_config.value ? m_config.value(row.index) : 0;
        if (!m_config.setValue) {
            return {};
        }
        if (feature.choices.empty()) {
            if (press) {
                m_config.setValue(row.index, value ? 0 : 1);
            }
        } else {
            const int choices = static_cast<int>(feature.choices.size());
            m_config.setValue(row.index, press ? (value + 1) % choices : std::clamp(value + step, 0, choices - 1));
        }
        return {};
    }
    if (!press) {
        // (Left on a page's other rows: back to the page it was reached from; Right on a page's row: into it)
        if ((pressed & kMenuLeft) && !m_page.empty()) {
            Leave();
        } else if ((pressed & kMenuRight) && row.kind == kRowPage) {
            Enter(row.page);
        }
        return {};
    }
    switch (row.kind) {
    case kRowSave: return {Chosen::Save, 0};
    case kRowLoad: return {Chosen::Load, 0};
    case kRowAction: return {Chosen::Action, m_first[row.index].id};
    case kRowQuit: return {Chosen::Quit, 0};
    case kRowResume: m_open = false; break;
    case kRowCommand: return {Chosen::Command, static_cast<int>(row.index)};
    case kRowPage: Enter(row.page); break;
    case kRowBack: Leave(); break;
    default: break;
    }
    return {};
}

void Menu::Draw(uint32_t *pixels, int width, int height) const {
    static constexpr Color white{255, 255, 255, 255}, dim{160, 164, 176, 255}, on{110, 220, 120, 255};
    if (m_rows.empty()) {
        return;
    }
    const Canvas canvas{pixels, width, height};
    // Sizes in font dots: d for the small text, 2d for the rows
    const float d = static_cast<float>(std::max(height / 300, 1));
    const float pad = 8 * d;
    const float panelWidth = std::min(150 * kCharWidth * d / 2, width * 0.9f); // 75 small characters, if there is room
    const float inner = panelWidth - 2 * pad;
    // A row's words: its title, its value (and the value's colour) and its description
    const auto Describe = [&](const Row &row, std::string &title, std::string &value, Color &valueColor, std::string &rowDescription) {
        title = "Resume";
        rowDescription = "Back to the game.";
        switch (row.kind) {
        case kRowSave:
            title = "Save state";
            rowDescription = "Keeps the game as it is now in the slot.";
            break;
        case kRowLoad:
            title = "Load state";
            rowDescription = "Goes back to the game kept in the slot.";
            break;
        case kRowSlot:
            title = "Slot";
            value = std::to_string(m_slot);
            rowDescription = "Left/Right: the slot (1-9) that Save state and Load state use.";
            break;
        case kRowSpeed:
            title = "Speed";
            value = m_config.speeds[static_cast<size_t>(m_speed)];
            valueColor = m_speed == 0 ? dim : on;
            rowDescription = m_config.speedDescription;
            break;
        case kRowMute:
            title = "Sound";
            value = m_config.muted() ? "Muted" : "On";
            valueColor = m_config.muted() ? on : dim;
            rowDescription = "Switches the game's sound off and on.";
            break;
        case kRowAction:
            title = m_first[row.index].title;
            rowDescription = m_first[row.index].description;
            break;
        case kRowQuit:
            title = "Quit game";
            rowDescription = "Closes the game.";
            break;
        case kRowCommand: {
            const MenuCommand &command = m_config.commands[row.index];
            title = command.title;
            rowDescription = command.description;
            break;
        }
        case kRowPage: {
            title = row.page;
            value = ">";
            valueColor = dim;
            // (what is on the page, and how much of it is on, unless the page is one that does not say)
            const MenuPage *page = PageOf(row.page);
            int onCount = 0;
            for (size_t f = 0; f < m_config.features.size() && (!page || page->counts); f++) {
                if (row.page == m_config.features[f].category) {
                    onCount += m_config.value && m_config.value(f) != 0;
                }
            }
            if (onCount) {
                value = std::to_string(onCount) + " on  >";
                valueColor = on;
            }
            rowDescription = page && !page->description.empty() ? page->description : "The page of " + row.page + " choices.";
            break;
        }
        case kRowBack:
            title = "Back";
            rowDescription = Parent(m_page).empty() ? "To the first page (Left does the same)." : "To the " + Parent(m_page) + " page (Left does the same).";
            break;
        case kRowFeature: {
            const MenuFeature &feature = m_config.features[row.index];
            const int chosen = m_config.value ? m_config.value(row.index) : 0;
            title = feature.title;
            rowDescription = feature.description;
            if (feature.choices.empty()) {
                value = chosen ? "On" : "Off";
            } else {
                value = feature.choices[static_cast<size_t>(std::clamp(chosen, 0, static_cast<int>(feature.choices.size()) - 1))];
            }
            valueColor = chosen ? on : dim;
            break;
        }
        default: break;
        }
    };
    // (the hint is one line unless the picture is a narrow one)
    const std::string hint = "Up/Down: choose   Left/Right or " + m_config.confirmName + ": change   " + m_config.closeName + ": close";
    const int hintLines = TextLines(hint.c_str(), d, inner);
    // (rows closer together when there are many; and when even that does not fit, the rows about the chosen
    // one are shown)
    // (room for the longest description on the page, three lines at least, so that it never runs into the hint)
    int descriptionLines = 3;
    for (const Row &row : m_rows) {
        std::string title, value, rowDescription;
        Color valueColor = white;
        Describe(row, title, value, valueColor, rowDescription);
        descriptionLines = std::max(descriptionLines, TextLines(rowDescription.c_str(), d, inner));
    }
    descriptionLines = std::max(descriptionLines, TextLines(m_note.c_str(), d, inner));
    const float around = 8 + 16 + 6 + 8 + descriptionLines * kLineHeight + 4 + hintLines * kLineHeight + 8;
    constexpr float kDivider = 9; // (the room a line between groups takes)
    const float dividers = static_cast<float>(std::count_if(m_rows.begin(), m_rows.end(), [](const Row &row) { return row.divider; }));
    const float room = height / d - around - dividers * kDivider;
    const size_t shown = std::min(m_rows.size(), static_cast<size_t>(std::max(room / 15.0f, 1.0f)));
    const float rowHeight = std::clamp(room / shown, 15.0f, 22.0f) * d;
    const size_t first = std::min(static_cast<size_t>(std::max(m_selected - static_cast<int>(shown) / 2, 0)), m_rows.size() - shown);
    const float panelHeight = pad + 16 * d + 6 * d + shown * rowHeight + dividers * kDivider * d + 8 * d + descriptionLines * kLineHeight * d + 4 * d + hintLines * kLineHeight * d + pad;
    const float px = (width - panelWidth) / 2, py = (height - panelHeight) / 2;

    canvas.Fill(0, 0, static_cast<float>(width), static_cast<float>(height), {0, 0, 0, 150});
    canvas.Fill(px, py, panelWidth, panelHeight, {20, 22, 30, 255}); // (opaque: the game's own text would show through)

    const float x = px + pad;
    float y = py + pad;
    canvas.Text(x, y, 2 * d, 0, white, m_page.empty() ? "Menu" : m_page.c_str());
    if (shown < m_rows.size()) {
        const std::string place = std::to_string(m_selected + 1) + " of " + std::to_string(m_rows.size());
        canvas.Text(x + inner - TextWidth(place.c_str(), d), y + 4 * d, d, 0, dim, place.c_str());
    }
    y += 16 * d + 6 * d;
    std::string description;
    for (size_t i = first; i < first + shown; i++, y += rowHeight) {
        const bool selected = static_cast<int>(i) == m_selected;
        if (m_rows[i].divider && i > first) {
            canvas.Fill(x, y + (kDivider * d - d) / 2, inner, d, {255, 255, 255, 70});
            y += kDivider * d;
        }
        if (selected) {
            canvas.Fill(px + pad / 2, y, panelWidth - pad, rowHeight, {255, 255, 255, 40});
        }
        std::string title, value, rowDescription;
        Color valueColor = white;
        Describe(m_rows[i], title, value, valueColor, rowDescription);
        const float ty = y + (rowHeight - 14 * d) / 2;
        canvas.Text(x, ty, 2 * d, 0, white, title.c_str());
        if (!value.empty()) {
            // (a value too long for the room beside the title: in the small letters, and cut short if even
            // those do not fit)
            const float room = inner - TextWidth(title.c_str(), 2 * d) - 6 * d;
            if (TextWidth(value.c_str(), 2 * d) <= room) {
                canvas.Text(x + inner - TextWidth(value.c_str(), 2 * d), ty, 2 * d, 0, valueColor, value.c_str());
            } else {
                std::string small = value;
                if (TextWidth(small.c_str(), d) > room) {
                    while (small.size() > 1 && TextWidth((small + "..").c_str(), d) > room) {
                        small.pop_back();
                    }
                    small += "..";
                }
                canvas.Text(x + inner - TextWidth(small.c_str(), d), ty + 4 * d, d, 0, valueColor, small.c_str());
            }
        }
        if (selected) {
            description = rowDescription;
        }
    }
    y += 8 * d;
    canvas.Text(x, y, d, inner, m_note.empty() ? dim : white, m_note.empty() ? description.c_str() : m_note.c_str());
    y += descriptionLines * kLineHeight * d + 4 * d;
    canvas.Text(x, y, d, inner, dim, hint.c_str());
}

} // namespace recomp
