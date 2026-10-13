// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// The menu for C hosts (menu_c.h): menu.h's Menu behind plain functions.
#include "menu_c.h"

#include "menu.h"

struct RMenu {
    recomp::Menu menu;
};

namespace {
    std::string Str(const char *s) {
        return s ? s : "";
    }
} // namespace

extern "C" {

RMenu *rmenu_new(void) {
    return new RMenu;
}

void rmenu_free(RMenu *menu) {
    delete menu;
}

void rmenu_configure(RMenu *menu, const RMenuConfig *c) {
    recomp::MenuConfig config;
    for (int i = 0; i < c->rowCount; i++) {
        const RMenuRow &row = c->rows[i];
        if (i == 0 || row.group != c->rows[i - 1].group) {
            config.groups.emplace_back();
        }
        static const recomp::MenuRow::Kind kKinds[] = {
            recomp::MenuRow::Save, recomp::MenuRow::Load, recomp::MenuRow::Slot, recomp::MenuRow::Speed,
            recomp::MenuRow::Pages, recomp::MenuRow::Action, recomp::MenuRow::Quit, recomp::MenuRow::Resume,
            recomp::MenuRow::Mute,
        };
        config.groups.back().push_back({kKinds[row.kind], row.id, Str(row.title), Str(row.description)});
    }
    for (int i = 0; i < c->featureCount; i++) {
        const RMenuFeature &feature = c->features[i];
        recomp::MenuFeature to{Str(feature.category), Str(feature.title), Str(feature.description)};
        for (const char *const *choice = feature.choices; choice && *choice; choice++) {
            to.choices.push_back(*choice);
        }
        config.features.push_back(std::move(to));
    }
    for (int i = 0; i < c->commandCount; i++) {
        config.commands.push_back({Str(c->commands[i].category), Str(c->commands[i].title), Str(c->commands[i].description)});
    }
    if (c->value) {
        config.value = [value = c->value, user = c->user](size_t feature) { return value(user, static_cast<int>(feature)); };
    }
    if (c->setValue) {
        config.setValue = [setValue = c->setValue, user = c->user](size_t feature, int value) { setValue(user, static_cast<int>(feature), value); };
    }
    for (int i = 0; i < c->speedCount; i++) {
        config.speeds.push_back(Str(c->speeds[i]));
    }
    config.speedDescription = Str(c->speedDescription);
    if (c->confirmName) {
        config.confirmName = c->confirmName;
    }
    if (c->closeName) {
        config.closeName = c->closeName;
    }
    if (c->muted && c->setMuted) {
        config.muted = [muted = c->muted, user = c->user] { return muted(user) != 0; };
        config.setMuted = [setMuted = c->setMuted, user = c->user](bool muted) { setMuted(user, muted ? 1 : 0); };
    }
    menu->menu.Configure(std::move(config));
}

void rmenu_open(RMenu *menu) {
    menu->menu.Open();
}

void rmenu_close(RMenu *menu) {
    menu->menu.Close();
}

bool rmenu_isOpen(const RMenu *menu) {
    return menu->menu.IsOpen();
}

RMenuChosen rmenu_buttons(RMenu *menu, unsigned pressed, int *id) {
    const recomp::Menu::Chosen chosen = menu->menu.Buttons(pressed);
    if (id) {
        *id = chosen.id;
    }
    switch (chosen.kind) {
    case recomp::Menu::Chosen::Save: return RMENU_SAVE;
    case recomp::Menu::Chosen::Load: return RMENU_LOAD;
    case recomp::Menu::Chosen::Quit: return RMENU_QUIT;
    case recomp::Menu::Chosen::Action: return RMENU_ACTION;
    case recomp::Menu::Chosen::Command: return RMENU_COMMAND;
    default: return RMENU_NONE;
    }
}

int rmenu_slot(const RMenu *menu) {
    return menu->menu.Slot();
}

void rmenu_setSlot(RMenu *menu, int slot) {
    menu->menu.SetSlot(slot);
}

int rmenu_speed(const RMenu *menu) {
    return menu->menu.Speed();
}

void rmenu_setSpeed(RMenu *menu, int speed) {
    menu->menu.SetSpeed(speed);
}

void rmenu_note(RMenu *menu, const char *text) {
    menu->menu.Note(Str(text));
}

void rmenu_draw(const RMenu *menu, uint32_t *pixels, int width, int height) {
    menu->menu.Draw(pixels, width, height);
}

} // extern "C"
