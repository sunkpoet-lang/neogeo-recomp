// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// The menu bar and the right-click menu (menubar.h).
#include "menubar.h"

#include <algorithm>
#include <cmath>

#include "text.h"

namespace recomp {

using text::Canvas;
using text::Color;
using text::kCharWidth;
using text::TextWidth;

namespace {
    // Sizes in font dots
    constexpr float kEntryDots = 11; // an entry's height
    constexpr float kLineDots = 5;   // a line between entries
    constexpr float kEdgeDots = 2;   // above the first entry and below the last
    constexpr float kMarkDots = 10;  // the room for the mark, left of the label
    constexpr float kArrowDots = 9;  // the room for a submenu's arrow, at the right
    constexpr float kTitleDots = 10; // around a name of the bar
    constexpr float kLeastDots = 70; // the narrowest a menu is

    constexpr Color kWhite{255, 255, 255, 255}, kDim{160, 164, 176, 255}, kOff{96, 100, 112, 255};
    constexpr Color kPanel{20, 22, 30, 255}, kBar{32, 34, 44, 255}, kPicked{255, 255, 255, 45};
    constexpr Color kEdge{255, 255, 255, 60}, kShadow{0, 0, 0, 90};
} // namespace

void MenuBar::Configure(std::vector<BarMenu> menus) {
    m_menus = std::move(menus);
    m_tops.clear();
    for (const BarMenu &menu : m_menus) {
        BarEntry top;
        top.label = menu.title;
        top.children = menu.entries;
        m_tops.push_back(std::move(top));
    }
    Trim();
}

void MenuBar::SetArea(int width, int height, int dot) {
    m_width = std::max(width, 0);
    m_height = std::max(height, 0);
    m_dot = std::max(dot, 1);
}

void MenuBar::ShowBar(bool shown) {
    m_barShown = shown;
}

void MenuBar::OpenMenu(size_t menu) {
    if (menu >= m_tops.size()) {
        return;
    }
    m_popup = false;
    m_levels = 2;
    m_picked = {static_cast<int>(menu), FirstUsable(m_tops[menu].children)};
}

void MenuBar::OpenPopup(float x, float y) {
    if (m_tops.empty()) {
        return;
    }
    m_popup = true;
    m_popupX = x;
    m_popupY = y;
    m_levels = 1;
    m_picked = {-1};
}

void MenuBar::Close() {
    m_levels = 0;
    m_picked.clear();
}

void MenuBar::Trim() {
    if (m_levels == 0) {
        return;
    }
    // (walked from the first: where a level's entry is gone, or no longer opens, what is past it closes)
    const std::vector<BarEntry> *entries = &m_tops;
    int level = 0;
    if (!m_popup) {
        if (m_picked[0] < 0 || m_picked[0] >= static_cast<int>(m_tops.size())) {
            Close();
            return;
        }
        entries = &m_tops[static_cast<size_t>(m_picked[0])].children;
        level = 1;
    }
    for (; level < m_levels; level++) {
        int &picked = m_picked[static_cast<size_t>(level)];
        if (picked >= static_cast<int>(entries->size())) {
            picked = -1;
        }
        if (level + 1 < m_levels) {
            if (picked < 0 || !Opens((*entries)[static_cast<size_t>(picked)])) {
                m_levels = level + 1;
                break;
            }
            entries = &(*entries)[static_cast<size_t>(picked)].children;
        }
    }
    m_picked.resize(static_cast<size_t>(m_levels));
}

float MenuBar::EntryHeight(const BarEntry &entry) const {
    return (entry.IsLine() ? kLineDots : kEntryDots) * m_dot;
}

MenuBar::Box MenuBar::ListBox(const std::vector<BarEntry> &entries, float x, float y) const {
    const float d = static_cast<float>(m_dot);
    float label = 0, shortcut = 0, h = 2 * kEdgeDots * d;
    for (const BarEntry &entry : entries) {
        label = std::max(label, TextWidth(entry.label.c_str(), d));
        shortcut = std::max(shortcut, TextWidth(entry.shortcut.c_str(), d));
        h += EntryHeight(entry);
    }
    const float picture = PictureWidth(entries);
    const float w = kMarkDots * d + (picture > 0 ? picture + 2 * d : 0) + label + (shortcut > 0 ? 3 * kCharWidth * d + shortcut : 0) +
                    kArrowDots * d;
    return {x, y, std::max(w, kLeastDots * d), h};
}

float MenuBar::PictureWidth(const std::vector<BarEntry> &entries) const {
    // (as wide as the first picture's shape makes it at an entry's height, a dot above and below)
    for (const BarEntry &entry : entries) {
        if (entry.pictureWidth > 0 && entry.pictureHeight > 0 &&
            entry.picture.size() >= static_cast<size_t>(entry.pictureWidth) * static_cast<size_t>(entry.pictureHeight)) {
            return std::round((kEntryDots - 2) * m_dot * entry.pictureWidth / static_cast<float>(entry.pictureHeight));
        }
    }
    return 0;
}

MenuBar::Box MenuBar::Place(Box box) const {
    box.x = std::max(0.0f, std::min(box.x, m_width - box.w));
    box.y = std::max(0.0f, std::min(box.y, m_height - box.h));
    return box;
}

MenuBar::Box MenuBar::TitleBox(size_t menu) const {
    const float d = static_cast<float>(m_dot);
    float x = 2 * d;
    for (size_t i = 0; i < m_tops.size(); i++) {
        const float w = TextWidth(m_tops[i].label.c_str(), d) + kTitleDots * d;
        if (i == menu) {
            return {x, 0, w, static_cast<float>(Height())};
        }
        x += w;
    }
    return {0, 0, 0, 0};
}

std::vector<MenuBar::List> MenuBar::Lists() const {
    std::vector<List> lists;
    if (m_levels == 0) {
        return lists;
    }
    const float d = static_cast<float>(m_dot);
    const std::vector<BarEntry> *entries;
    int level;
    if (m_popup) {
        // (the popup's list from the point clicked, or up and to the left of it where there is no room)
        Box box = ListBox(m_tops, m_popupX, m_popupY);
        if (box.x + box.w > m_width) {
            box.x -= box.w;
        }
        if (box.y + box.h > m_height) {
            box.y -= box.h;
        }
        lists.push_back({&m_tops, Place(box), m_picked[0]});
        level = 1;
    } else {
        entries = &m_tops[static_cast<size_t>(m_picked[0])].children;
        const Box title = TitleBox(static_cast<size_t>(m_picked[0]));
        lists.push_back({entries, Place(ListBox(*entries, title.x, title.y + title.h)), m_picked[1]});
        level = 2;
    }
    for (; level < m_levels; level++) {
        // (a submenu beside the entry it is of: at its right, or at its left where there is no room)
        const List &parent = lists.back();
        float y = parent.box.y + kEdgeDots * d;
        for (int i = 0; i < parent.picked; i++) {
            y += EntryHeight((*parent.entries)[static_cast<size_t>(i)]);
        }
        entries = &(*parent.entries)[static_cast<size_t>(parent.picked)].children;
        Box box = ListBox(*entries, parent.box.x + parent.box.w - d, y - kEdgeDots * d);
        if (box.x + box.w > m_width) {
            box.x = parent.box.x - box.w + d;
        }
        lists.push_back({entries, Place(box), m_picked[static_cast<size_t>(level)]});
    }
    return lists;
}

std::vector<MenuBar::Box> MenuBar::Boxes() const {
    std::vector<Box> boxes;
    for (const List &list : Lists()) {
        boxes.push_back(list.box);
    }
    return boxes;
}

std::vector<int> MenuBar::Picked() const {
    return m_picked;
}

int MenuBar::EntryAt(const List &list, float y) const {
    float top = list.box.y + kEdgeDots * m_dot;
    for (size_t i = 0; i < list.entries->size(); i++) {
        const float h = EntryHeight((*list.entries)[i]);
        if (y >= top && y < top + h) {
            return static_cast<int>(i);
        }
        top += h;
    }
    return -1;
}

int MenuBar::FirstUsable(const std::vector<BarEntry> &entries) const {
    return Step(entries, -1, 1);
}

int MenuBar::Step(const std::vector<BarEntry> &entries, int from, int by) const {
    const int n = static_cast<int>(entries.size());
    if (from < 0) {
        from = by > 0 ? -1 : n;
    }
    for (int k = 1; k <= n; k++) {
        const int i = ((from + by * k) % n + n) % n;
        if (!entries[static_cast<size_t>(i)].IsLine() && entries[static_cast<size_t>(i)].enabled) {
            return i;
        }
    }
    return from >= 0 && from < n ? from : -1;
}

bool MenuBar::Hit(float x, float y) const {
    if (m_barShown && x >= 0 && x < m_width && y >= 0 && y < Height()) {
        return true;
    }
    for (const List &list : Lists()) {
        if (list.box.Contains(x, y)) {
            return true;
        }
    }
    return false;
}

bool MenuBar::Pointer(float x, float y) {
    if (m_levels == 0) {
        return false;
    }
    const std::vector<int> before = m_picked;
    const int levelsBefore = m_levels;
    if (!m_popup && m_barShown && y >= 0 && y < Height()) {
        for (size_t t = 0; t < m_tops.size(); t++) {
            if (TitleBox(t).Contains(x, y) && static_cast<int>(t) != m_picked[0]) {
                OpenMenu(t);
                m_picked[1] = -1;
                return true;
            }
        }
        return false;
    }
    const std::vector<List> lists = Lists();
    for (size_t i = lists.size(); i-- > 0;) {
        if (!lists[i].box.Contains(x, y)) {
            continue;
        }
        const size_t level = i + (m_popup ? 0 : 1);
        int e = EntryAt(lists[i], y);
        if (e >= 0) {
            const BarEntry &entry = (*lists[i].entries)[static_cast<size_t>(e)];
            if (entry.IsLine() || !entry.enabled) {
                e = -1;
            }
        }
        if (e >= 0 && e == m_picked[level] && m_levels > static_cast<int>(level) + 1) {
            break; // (still on the entry whose submenu is open: what is open past it stays)
        }
        m_picked[level] = e;
        m_levels = static_cast<int>(level) + 1;
        m_picked.resize(level + 1);
        if (e >= 0 && Opens((*lists[i].entries)[static_cast<size_t>(e)])) {
            m_levels++;
            m_picked.push_back(-1); // (a submenu opened by pointing has nothing picked in it yet)
        }
        break;
    }
    return m_picked != before || m_levels != levelsBefore;
}

MenuBar::Chosen MenuBar::Click(float x, float y) {
    const std::vector<List> lists = Lists();
    for (size_t i = lists.size(); i-- > 0;) {
        if (!lists[i].box.Contains(x, y)) {
            continue;
        }
        const size_t level = i + (m_popup ? 0 : 1);
        const int e = EntryAt(lists[i], y);
        if (e < 0) {
            return {};
        }
        const BarEntry &entry = (*lists[i].entries)[static_cast<size_t>(e)];
        if (entry.IsLine() || !entry.enabled) {
            return {};
        }
        if (Opens(entry)) {
            m_picked[level] = e;
            m_levels = static_cast<int>(level) + 2;
            m_picked.resize(static_cast<size_t>(m_levels), -1);
            m_picked[level + 1] = -1;
            return {};
        }
        const int id = entry.id;
        Close();
        return {true, id};
    }
    if (m_barShown && x >= 0 && x < m_width && y >= 0 && y < Height()) {
        for (size_t t = 0; t < m_tops.size(); t++) {
            if (TitleBox(t).Contains(x, y)) {
                if (m_levels > 0 && !m_popup && m_picked[0] == static_cast<int>(t)) {
                    Close();
                } else {
                    OpenMenu(t);
                    m_picked[1] = -1;
                }
                return {};
            }
        }
    }
    Close();
    return {};
}

MenuBar::Chosen MenuBar::Buttons(unsigned pressed) {
    if (m_levels == 0 || pressed == 0) {
        return {};
    }
    const std::vector<List> lists = Lists();
    const List &deepest = lists.back();
    const size_t level = static_cast<size_t>(m_levels - 1);
    const int first = m_popup ? 0 : 1; // (the level that is the first menu)
    const int count = static_cast<int>(m_tops.size());
    int &picked = m_picked[level];
    const auto openPicked = [&]() {
        m_picked.push_back(FirstUsable((*deepest.entries)[static_cast<size_t>(picked)].children));
        m_levels++;
    };
    if (pressed & (kMenuUp | kMenuDown)) {
        picked = Step(*deepest.entries, picked, (pressed & kMenuUp) ? -1 : 1);
    } else if (pressed & kMenuRight) {
        if (picked >= 0 && Opens((*deepest.entries)[static_cast<size_t>(picked)])) {
            openPicked();
        } else if (!m_popup) {
            OpenMenu(static_cast<size_t>((m_picked[0] + 1) % count));
        }
    } else if (pressed & kMenuLeft) {
        if (static_cast<int>(level) > first) {
            m_levels--;
            m_picked.pop_back();
        } else if (!m_popup) {
            OpenMenu(static_cast<size_t>((m_picked[0] + count - 1) % count));
        }
    } else if (pressed & kMenuConfirm) {
        if (picked >= 0) {
            const BarEntry &entry = (*deepest.entries)[static_cast<size_t>(picked)];
            if (Opens(entry)) {
                openPicked();
            } else if (entry.enabled && !entry.IsLine()) {
                const int id = entry.id;
                Close();
                return {true, id};
            }
        }
    } else if (pressed & kMenuBack) {
        if (static_cast<int>(level) > first) {
            m_levels--;
            m_picked.pop_back();
        } else {
            Close();
        }
    }
    return {};
}

void MenuBar::Draw(uint32_t *pixels, int width, int height) const {
    const Canvas canvas{pixels, width, height};
    const float d = static_cast<float>(m_dot);
    if (m_barShown) {
        canvas.Fill(0, 0, static_cast<float>(width), static_cast<float>(Height()), kBar);
        canvas.Fill(0, Height() - d, static_cast<float>(width), d, kEdge);
        for (size_t t = 0; t < m_tops.size(); t++) {
            const Box box = TitleBox(t);
            if (m_levels > 0 && !m_popup && m_picked[0] == static_cast<int>(t)) {
                canvas.Fill(box.x, box.y, box.w, box.h - d, kPicked);
            }
            canvas.Text(box.x + kTitleDots / 2 * d, 2 * d, d, 0, kWhite, m_tops[t].label.c_str());
        }
    }
    for (const List &list : Lists()) {
        const Box &b = list.box;
        canvas.Fill(b.x + 2 * d, b.y + 2 * d, b.w, b.h, kShadow);
        canvas.Fill(b.x, b.y, b.w, b.h, kPanel);
        canvas.Fill(b.x, b.y, b.w, d, kEdge);
        canvas.Fill(b.x, b.y + b.h - d, b.w, d, kEdge);
        canvas.Fill(b.x, b.y + d, d, b.h - 2 * d, kEdge);
        canvas.Fill(b.x + b.w - d, b.y + d, d, b.h - 2 * d, kEdge);
        float y = b.y + kEdgeDots * d;
        const float pictures = PictureWidth(*list.entries);
        const float labelX = b.x + kMarkDots * d + (pictures > 0 ? pictures + 2 * d : 0);
        for (size_t i = 0; i < list.entries->size(); i++) {
            const BarEntry &entry = (*list.entries)[i];
            const float h = EntryHeight(entry);
            if (entry.IsLine()) {
                canvas.Fill(b.x + 3 * d, y + 2 * d, b.w - 6 * d, d, kEdge);
                y += h;
                continue;
            }
            if (static_cast<int>(i) == list.picked) {
                canvas.Fill(b.x + d, y, b.w - 2 * d, h, kPicked);
            }
            const Color color = entry.enabled ? kWhite : kOff;
            const float ty = y + 2 * d, mx = b.x + 3 * d;
            if (entry.mark == BarEntry::Check) {
                static constexpr int kTick[][2] = {{0, 3}, {1, 4}, {2, 5}, {3, 4}, {4, 3}, {5, 2}, {6, 1}};
                for (const auto &p : kTick) {
                    canvas.Fill(mx + p[0] * d, ty + p[1] * d, d, d, color);
                }
            } else if (entry.mark == BarEntry::Dot) {
                canvas.Fill(mx + 2 * d, ty + 2 * d, 3 * d, 3 * d, color);
            }
            if (pictures > 0 && entry.pictureWidth > 0 && entry.pictureHeight > 0 &&
                entry.picture.size() >= static_cast<size_t>(entry.pictureWidth) * static_cast<size_t>(entry.pictureHeight)) {
                // (each of the room's pixels the picture's nearest; dim with the entry)
                const int x0 = static_cast<int>(b.x + kMarkDots * d), y0 = static_cast<int>(y + d);
                const int w = static_cast<int>(pictures), ph = static_cast<int>(h - 2 * d);
                for (int py = std::max(0, -y0); py < ph && y0 + py < height; py++) {
                    const uint32_t *from = &entry.picture[static_cast<size_t>(py * entry.pictureHeight / ph) * entry.pictureWidth];
                    uint32_t *to = pixels + static_cast<size_t>(y0 + py) * width;
                    for (int px = std::max(0, -x0); px < w && x0 + px < width; px++) {
                        const uint32_t p = from[px * entry.pictureWidth / w];
                        to[x0 + px] = entry.enabled ? (p | 0xFF000000u) : (((p >> 1) & 0x7F7F7Fu) | 0xFF000000u);
                    }
                }
            }
            canvas.Text(labelX, ty, d, 0, color, entry.label.c_str());
            if (!entry.shortcut.empty()) {
                const float sx = b.x + b.w - kArrowDots * d - TextWidth(entry.shortcut.c_str(), d);
                canvas.Text(sx, ty, d, 0, entry.enabled ? kDim : kOff, entry.shortcut.c_str());
            }
            if (!entry.children.empty()) {
                canvas.Text(b.x + b.w - kArrowDots * d + 2 * d, ty, d, 0, color, ">");
            }
            y += h;
        }
    }
}

// The in-game menu's rows as the bar's entries (AddMenuEntries)

namespace {
    // An entry's id: 0x40000000, what it is (4 bits), then two numbers of 12 bits (a slot, a feature and its
    // choice, a row's place...)
    enum : int { kSave = 1, kLoad, kSlot, kSpeed, kCommand, kToggle, kChoice, kAction, kQuit, kMute };
    constexpr int kEntryBase = 0x40000000;

    int EntryId(int what, int a, int b = 0) {
        return kEntryBase | (what << 24) | ((a & 0xFFF) << 12) | (b & 0xFFF);
    }

    BarEntry Item(std::string label, int id, BarEntry::Mark mark = BarEntry::NoMark) {
        BarEntry e;
        e.label = std::move(label);
        e.id = id;
        e.mark = mark;
        return e;
    }

    BarEntry Submenu(std::string label, std::vector<BarEntry> children) {
        BarEntry e;
        e.label = std::move(label);
        e.children = std::move(children);
        return e;
    }

    struct Pages {
        const MenuConfig &config;

        std::string Parent(const std::string &page) const {
            for (const MenuPage &p : config.pages) {
                if (p.name == page) {
                    return p.parent;
                }
            }
            return {};
        }
        bool Holds(const std::string &page) const {
            return std::any_of(config.features.begin(), config.features.end(), [&](const MenuFeature &f) { return f.category == page; }) ||
                   std::any_of(config.commands.begin(), config.commands.end(), [&](const MenuCommand &c) { return c.category == page; });
        }
        // (as Menu has it: something on it, or on a page under it)
        bool Exists(const std::string &page) const {
            if (page.empty()) {
                return false;
            }
            if (Holds(page)) {
                return true;
            }
            return std::any_of(config.pages.begin(), config.pages.end(), [&](const MenuPage &under) {
                return under.parent == page && under.name != page && Holds(under.name);
            });
        }
        // A page: its commands, the pages under it, its features
        BarEntry Page(const std::string &page, int depth = 0) const {
            std::vector<BarEntry> entries;
            for (size_t i = 0; i < config.commands.size(); i++) {
                if (config.commands[i].category == page) {
                    entries.push_back(Item(config.commands[i].title, EntryId(kCommand, static_cast<int>(i))));
                }
            }
            for (const MenuPage &under : config.pages) {
                if (depth < 8 && under.parent == page && under.name != page && Exists(under.name)) {
                    entries.push_back(Page(under.name, depth + 1));
                }
            }
            for (size_t i = 0; i < config.features.size(); i++) {
                const MenuFeature &feature = config.features[i];
                if (feature.category != page) {
                    continue;
                }
                const int value = config.value ? config.value(i) : 0;
                if (feature.choices.empty()) {
                    entries.push_back(Item(feature.title, EntryId(kToggle, static_cast<int>(i)), value ? BarEntry::Check : BarEntry::NoMark));
                } else {
                    std::vector<BarEntry> choices;
                    for (size_t c = 0; c < feature.choices.size(); c++) {
                        choices.push_back(Item(feature.choices[c], EntryId(kChoice, static_cast<int>(i), static_cast<int>(c)),
                                               static_cast<int>(c) == value ? BarEntry::Dot : BarEntry::NoMark));
                    }
                    entries.push_back(Submenu(feature.title, std::move(choices)));
                }
                entries.back().enabled = static_cast<bool>(config.setValue);
            }
            return Submenu(page, std::move(entries));
        }
    };
} // namespace

bool IsMenuEntry(int id) {
    return id > 0 && (id & kEntryBase) != 0;
}

void AddMenuEntries(std::vector<BarMenu> &menus, const MenuConfig &config, const Menu &menu, const BarOptions &options) {
    const Pages pages{config};
    std::vector<int> lastGroup(menus.size(), -1);
    const auto into = [&](const MenuRow &row, const char *byKind, int group) -> std::vector<BarEntry> & {
        const std::string name = row.bar.empty() ? byKind : row.bar;
        size_t m = 0;
        while (m < menus.size() && menus[m].title != name) {
            m++;
        }
        if (m == menus.size()) {
            menus.push_back({name, {}});
            lastGroup.push_back(-1);
        }
        std::vector<BarEntry> &entries = menus[m].entries;
        if (!entries.empty() && lastGroup[m] != group) {
            entries.push_back(BarEntry::Line());
        }
        lastGroup[m] = group;
        return entries;
    };
    const auto slots = [&](int what) {
        std::vector<BarEntry> entries;
        for (int slot = 1; slot <= 9; slot++) {
            std::string label = "Slot " + std::to_string(slot);
            if (options.slotDetail) {
                const std::string detail = options.slotDetail(slot);
                if (!detail.empty()) {
                    label += "  " + detail;
                }
            }
            entries.push_back(Item(label, EntryId(what, slot), slot == menu.Slot() ? BarEntry::Dot : BarEntry::NoMark));
            if (what == kLoad && options.slotUsed && !options.slotUsed(slot)) {
                entries.back().enabled = false;
            }
        }
        return entries;
    };
    // (the pages that have a row of their own are not also among the Pages row's)
    std::vector<std::string> ownRows;
    for (const std::vector<MenuRow> &group : config.groups) {
        for (const MenuRow &row : group) {
            if (row.kind == MenuRow::Page) {
                ownRows.push_back(row.title);
            }
        }
    }
    int place = 0; // (a row's place among all the first page's rows, for an Action's id)
    for (size_t g = 0; g < config.groups.size(); g++) {
        const int group = static_cast<int>(g);
        for (const MenuRow &row : config.groups[g]) {
            const int index = place++;
            switch (row.kind) {
            case MenuRow::Save: into(row, "Game", group).push_back(Submenu("Save state", slots(kSave))); break;
            case MenuRow::Load: into(row, "Game", group).push_back(Submenu("Load state", slots(kLoad))); break;
            case MenuRow::Slot: into(row, "Game", group).push_back(Submenu("Slot", slots(kSlot))); break;
            case MenuRow::Speed:
                if (!config.speeds.empty()) {
                    std::vector<BarEntry> speeds;
                    for (size_t i = 0; i < config.speeds.size(); i++) {
                        speeds.push_back(Item(config.speeds[i], EntryId(kSpeed, static_cast<int>(i)),
                                              static_cast<int>(i) == menu.Speed() ? BarEntry::Dot : BarEntry::NoMark));
                    }
                    into(row, "Features", group).push_back(Submenu("Speed", std::move(speeds)));
                }
                break;
            case MenuRow::Action: into(row, "Game", group).push_back(Item(row.title, EntryId(kAction, index))); break;
            case MenuRow::Quit: into(row, "Game", group).push_back(Item("Quit game", EntryId(kQuit, 0))); break;
            case MenuRow::Resume: break;
            case MenuRow::Mute:
                if (config.muted && config.setMuted) {
                    into(row, "Audio", group).push_back(Item("Mute", EntryId(kMute, 0), config.muted() ? BarEntry::Check : BarEntry::NoMark));
                }
                break;
            case MenuRow::Page:
                if (pages.Exists(row.title)) {
                    into(row, "Features", group).push_back(pages.Page(row.title));
                }
                break;
            case MenuRow::Pages: {
                std::vector<BarEntry> found;
                for (size_t i = 0; i < config.commands.size(); i++) {
                    if (config.commands[i].category.empty()) {
                        found.push_back(Item(config.commands[i].title, EntryId(kCommand, static_cast<int>(i))));
                    }
                }
                std::vector<std::string> seen = ownRows;
                const auto addPage = [&](const std::string &category) {
                    if (!category.empty() && std::find(seen.begin(), seen.end(), category) == seen.end()) {
                        seen.push_back(category);
                        if (pages.Parent(category).empty()) {
                            found.push_back(pages.Page(category));
                        }
                    }
                };
                for (const MenuFeature &feature : config.features) {
                    addPage(feature.category);
                }
                for (const MenuCommand &command : config.commands) {
                    addPage(command.category);
                }
                if (!found.empty()) {
                    std::vector<BarEntry> &entries = into(row, "Features", group);
                    entries.insert(entries.end(), found.begin(), found.end());
                }
                break;
            }
            }
        }
    }
}

Menu::Chosen ChooseMenuEntry(int id, const MenuConfig &config, Menu &menu) {
    if (!IsMenuEntry(id)) {
        return {};
    }
    const int what = (id >> 24) & 0xF, a = (id >> 12) & 0xFFF, b = id & 0xFFF;
    switch (what) {
    case kSave: menu.SetSlot(a); return {Menu::Chosen::Save, 0};
    case kLoad: menu.SetSlot(a); return {Menu::Chosen::Load, 0};
    case kSlot: menu.SetSlot(a); break;
    case kSpeed: menu.SetSpeed(a); break;
    case kCommand:
        if (a < static_cast<int>(config.commands.size())) {
            return {Menu::Chosen::Command, a};
        }
        break;
    case kToggle:
    case kChoice:
        if (a < static_cast<int>(config.features.size()) && config.setValue) {
            const int value = config.value ? config.value(static_cast<size_t>(a)) : 0;
            config.setValue(static_cast<size_t>(a), what == kToggle ? (value ? 0 : 1) : b);
        }
        break;
    case kAction: {
        int place = 0;
        for (const std::vector<MenuRow> &group : config.groups) {
            for (const MenuRow &row : group) {
                if (place++ == a && row.kind == MenuRow::Action) {
                    return {Menu::Chosen::Action, row.id};
                }
            }
        }
        break;
    }
    case kQuit: return {Menu::Chosen::Quit, 0};
    case kMute:
        if (config.muted && config.setMuted) {
            config.setMuted(!config.muted());
        }
        break;
    default: break;
    }
    return {};
}

} // namespace recomp
