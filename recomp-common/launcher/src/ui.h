// SPDX-License-Identifier: GPL-3.0-or-later
// From the launcher of wiiparty-recomp by arelkair (https://github.com/arelkair/wiiparty-recomp,
// launcher/src/ui.h), which is licensed GPL-3.0-or-later; as it was there.
// Part of recomp-common (https://github.com/tmcleroy/recomp-common); see THIRD_PARTY_NOTICES.md.
#pragma once

#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"

namespace ui {

struct Palette {
    ImU32 background;
    ImU32 sidebar;
    ImU32 surface;
    ImU32 text;
    ImU32 secondary;
    ImU32 tertiary;
    ImU32 separator;
    ImU32 control;
    ImU32 control_hover;
    ImU32 control_active;
    ImU32 primary;
    ImU32 primary_hover;
    ImU32 primary_text;
    ImU32 accent;
    ImU32 accent_soft;
    ImU32 danger;
    ImU32 success;
    ImU32 overlay;
};

enum class Kind { Primary, Secondary, Ghost };
enum class Font { Regular, Semibold };

namespace size {
constexpr float kDisplay = 40.0f;
constexpr float kHeading = 28.0f;
constexpr float kLead = 15.0f;
constexpr float kBody = 14.0f;
constexpr float kDetail = 12.5f;
constexpr float kSmall = 11.5f;
}

void set_dark(bool dark);
bool dark();
const Palette& palette();
void apply_style(float scale);
void load_fonts();
float scale();
float px(float value);

void begin_frame(float delta);
bool animating();
void request_frames(int count);
float animate(ImGuiID id, float target, float rate = 16.0f);
void set_value(ImGuiID id, float value);
float ease_out(float t);
ImU32 mix(ImU32 from, ImU32 to, float t);
ImU32 fade(ImU32 color, float alpha);

void push_font(Font font, float size);
void pop_font();
void text(const char* value, Font font, float size, ImU32 color, float wrap = 0.0f);
ImVec2 text_size(const char* value, Font font, float size, float wrap = 0.0f);
void gap(float value);

bool button(const char* label, Kind kind, float width = 0.0f, bool enabled = true, float height = 36.0f);
bool toggle(const char* id, bool value);
bool dropdown(const char* id, const std::vector<std::string>& labels, int& index, float width);
bool nav_item(const char* label, bool active, float width);
bool list_item(const char* label, bool active, float width);
bool key_chip(const char* id, const char* label, bool capturing, float width, ImRect& rect);
float keycap(const char* key);
std::string fit_start(const std::string& value, Font font, float size, float width);
void separator(float width);
void progress_bar(ImGuiID id, float fraction, float width, float height);
void spinner(ImVec2 center, float radius, ImU32 color);
void check_mark(ImVec2 center, float size, ImU32 color);
void cross_mark(ImVec2 center, float size, ImU32 color);
void die(ImVec2 origin, float size);
void chevron(ImVec2 center, float size, ImU32 color, bool down);

}
