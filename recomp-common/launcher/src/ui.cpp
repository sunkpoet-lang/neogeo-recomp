// SPDX-License-Identifier: GPL-3.0-or-later
// From the launcher of wiiparty-recomp by arelkair (https://github.com/arelkair/wiiparty-recomp,
// launcher/src/ui.cpp), which is licensed GPL-3.0-or-later; as it was there but for the ring round what the
// arrow keys or a gamepad are on, and a few lines.
// Part of recomp-common (https://github.com/tmcleroy/recomp-common); see THIRD_PARTY_NOTICES.md.
#include "ui.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "embedded.h"

namespace ui {

namespace {

constexpr Palette kLight = {
    IM_COL32(255, 255, 255, 255), IM_COL32(245, 245, 247, 255), IM_COL32(245, 245, 247, 255), IM_COL32(29, 29, 31, 255),
    IM_COL32(110, 110, 115, 255), IM_COL32(174, 174, 178, 255), IM_COL32(229, 229, 234, 255), IM_COL32(237, 237, 240, 255),
    IM_COL32(227, 227, 232, 255), IM_COL32(214, 214, 220, 255), IM_COL32(29, 29, 31, 255),    IM_COL32(58, 58, 60, 255),
    IM_COL32(255, 255, 255, 255), IM_COL32(224, 40, 125, 255),  IM_COL32(224, 40, 125, 36),   IM_COL32(215, 0, 21, 255),
    IM_COL32(36, 138, 61, 255),   IM_COL32(0, 0, 0, 72),
};

constexpr Palette kDark = {
    IM_COL32(18, 18, 20, 255),    IM_COL32(26, 26, 29, 255),    IM_COL32(28, 28, 31, 255),    IM_COL32(245, 245, 247, 255),
    IM_COL32(161, 161, 166, 255), IM_COL32(99, 99, 102, 255),   IM_COL32(44, 44, 48, 255),    IM_COL32(38, 38, 42, 255),
    IM_COL32(48, 48, 53, 255),    IM_COL32(58, 58, 64, 255),    IM_COL32(245, 245, 247, 255), IM_COL32(255, 255, 255, 255),
    IM_COL32(18, 18, 20, 255),    IM_COL32(255, 79, 154, 255),  IM_COL32(255, 79, 154, 40),   IM_COL32(255, 69, 58, 255),
    IM_COL32(48, 209, 88, 255),   IM_COL32(0, 0, 0, 128),
};

bool g_dark = false;
float g_scale = 1.0f;
float g_delta = 0.0f;
bool g_animating = false;
bool g_focus_next = false;
int g_frames = 0;
ImFont* g_fonts[2] = {};
std::unordered_map<ImGuiID, float> g_values;

ImVec4 color(ImU32 value) {
    return ImGui::ColorConvertU32ToFloat4(value);
}

ImFont* font_for(Font font) {
    return g_fonts[font == Font::Semibold ? 1 : 0];
}

void add_font(int index, const char* name) {
    std::string_view data = blob(name);
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;
    config.OversampleH = 2;
    config.OversampleV = 2;
    g_fonts[index] = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(const_cast<char*>(data.data()), static_cast<int>(data.size()), size::kBody, &config);
}

// The ring round whatever the arrow keys or a gamepad are on (nothing while the mouse is in use); `rounding`
// is the shape's. Called straight after the widget's behavior() and its drawing.
void focus_ring(const ImRect& rect, ImGuiID id, float rounding, bool inside = false) {
    ImGuiContext& g = *GImGui;
    if (g.NavId != id || !g.NavCursorVisible || (g.LastItemData.ItemFlags & ImGuiItemFlags_NoNav)) {
        return;
    }
    float thickness = std::max(2.0f, px(2.5f));
    float out = inside ? -thickness * 0.5f : px(3) + thickness * 0.5f;
    ImRect ring(rect.Min - ImVec2(out, out), rect.Max + ImVec2(out, out));
    ImDrawList* list = g.CurrentWindow->DrawList;
    ImVec2 edge(thickness, thickness);
    list->PushClipRect(ring.Min - edge, ring.Max + edge, true);
    list->AddRect(ring.Min, ring.Max, palette().accent, std::max(0.0f, rounding + out), thickness);
    list->PopClipRect();
}

// `reachable`: false keeps the arrow keys and a gamepad off it. (A widget draws itself, then focus_ring.)
bool behavior(const ImRect& rect, ImGuiID id, bool enabled, bool& hovered, bool& held, bool reachable = true) {
    ImGui::ItemSize(rect);
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, !reachable);
    bool added = ImGui::ItemAdd(rect, id);
    ImGui::PopItemFlag();
    if (!added) {
        hovered = false;
        held = false;
        return false;
    }
    if (g_focus_next && reachable) {
        g_focus_next = false;
        focus_item();
    }
    bool pressed = ImGui::ButtonBehavior(rect, id, &hovered, &held, enabled ? 0 : ImGuiButtonFlags_None);
    if (!enabled) {
        hovered = false;
        held = false;
        pressed = false;
    }
    if (hovered && !ImGui::GetCurrentContext()->NavHighlightItemUnderNav) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    return pressed;
}

ImRect pressed_rect(const ImRect& rect, float press) {
    ImVec2 shrink((rect.Max.x - rect.Min.x) * 0.015f * press, (rect.Max.y - rect.Min.y) * 0.015f * press);
    return ImRect(rect.Min + shrink, rect.Max - shrink);
}

}

void set_dark(bool dark) {
    g_dark = dark;
}

bool dark() {
    return g_dark;
}

const Palette& palette() {
    return g_dark ? kDark : kLight;
}

float scale() {
    return g_scale;
}

float px(float value) {
    return std::round(value * g_scale);
}

void apply_style(float value) {
    g_scale = value;
    ImGuiStyle style;
    style.WindowPadding = ImVec2(0, 0);
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.PopupRounding = 12.0f;
    style.FramePadding = ImVec2(0, 0);
    style.ItemSpacing = ImVec2(0, 0);
    style.ItemInnerSpacing = ImVec2(0, 0);
    style.ScrollbarSize = 10.0f;
    style.ScrollbarRounding = 5.0f;
    style.ScrollbarPadding = 2.0f;
    style.GrabMinSize = 32.0f;
    style.WindowRounding = 0.0f;
    style.FrameRounding = 10.0f;
    style.ScaleAllSizes(value);
    style.FontScaleDpi = value;
    style.FontSizeBase = size::kBody;
    const Palette& p = palette();
    style.Colors[ImGuiCol_Text] = color(p.text);
    style.Colors[ImGuiCol_WindowBg] = color(p.background);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_PopupBg] = color(g_dark ? IM_COL32(36, 36, 40, 255) : IM_COL32(255, 255, 255, 255));
    style.Colors[ImGuiCol_Border] = color(p.separator);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_ScrollbarGrab] = color(fade(p.tertiary, 0.55f));
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = color(p.tertiary);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = color(p.secondary);
    style.Colors[ImGuiCol_NavCursor] = color(p.accent);
    style.Colors[ImGuiCol_ModalWindowDimBg] = color(p.overlay);
    style.Colors[ImGuiCol_TextSelectedBg] = color(p.accent_soft);
    ImGui::GetStyle() = style;
}

void load_fonts() {
    ImGui::GetIO().Fonts->Clear();
    add_font(0, "font_regular");
    add_font(1, "font_semibold");
}

void begin_frame(float delta) {
    g_delta = std::min(delta, 0.1f);
    g_focus_next = false;
    g_animating = g_frames > 0;
    if (g_frames > 0) {
        g_frames--;
    }
}

bool animating() {
    return g_animating;
}

void request_frames(int count) {
    g_frames = std::max(g_frames, count);
}

float animate(ImGuiID id, float target, float rate) {
    auto [it, inserted] = g_values.try_emplace(id, target);
    if (inserted) {
        return target;
    }
    float& value = it->second;
    value += (target - value) * (1.0f - std::exp(-rate * g_delta));
    if (std::fabs(target - value) < 0.002f) {
        value = target;
    } else {
        g_animating = true;
    }
    return value;
}

void set_value(ImGuiID id, float value) {
    g_values[id] = value;
    g_animating = true;
}

float ease_out(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

ImU32 mix(ImU32 from, ImU32 to, float t) {
    ImVec4 a = color(from);
    ImVec4 b = color(to);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t));
}

ImU32 fade(ImU32 value, float alpha) {
    ImVec4 c = color(value);
    c.w *= alpha;
    return ImGui::ColorConvertFloat4ToU32(c);
}

void push_font(Font font, float value) {
    ImGui::PushFont(font_for(font), value);
}

void pop_font() {
    ImGui::PopFont();
}

void text(const char* value, Font font, float value_size, ImU32 text_color, float wrap) {
    push_font(font, value_size);
    ImGui::PushStyleColor(ImGuiCol_Text, text_color);
    if (wrap > 0.0f) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap);
        ImGui::TextUnformatted(value);
        ImGui::PopTextWrapPos();
    } else {
        ImGui::TextUnformatted(value);
    }
    ImGui::PopStyleColor();
    pop_font();
}

ImVec2 text_size(const char* value, Font font, float value_size, float wrap) {
    push_font(font, value_size);
    ImVec2 result = ImGui::CalcTextSize(value, nullptr, false, wrap > 0.0f ? wrap : -1.0f);
    pop_font();
    return result;
}

void focus_item() {
    ImGuiContext& g = *GImGui;
    ImGui::SetFocusID(g.LastItemData.ID, g.CurrentWindow);
    g.NavCursorVisible = true;
}

void focus_next_item() {
    g_focus_next = true;
}

void show_focus() {
    ImGui::GetCurrentContext()->NavCursorVisible = true;
}

bool focus_shown() {
    return ImGui::GetCurrentContext()->NavCursorVisible;
}

bool gamepad_in_use() {
    ImGuiContext& g = *GImGui;
    return g.NavCursorVisible && g.NavInputSource == ImGuiInputSource_Gamepad;
}

void gap(float value) {
    ImGui::Dummy(ImVec2(0, px(value)));
}

bool button(const char* label, Kind kind, float width, bool enabled, float height) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID(label);
    const char* end = ImGui::FindRenderedTextEnd(label);
    float font_size = height >= 44.0f ? 16.0f : size::kBody;
    push_font(Font::Semibold, font_size);
    ImVec2 label_size = ImGui::CalcTextSize(label, end);
    ImVec2 box(std::max(px(width), label_size.x + px(40)), px(height));
    ImRect rect(window->DC.CursorPos, window->DC.CursorPos + box);
    bool hovered = false;
    bool held = false;
    bool pressed = behavior(rect, id, enabled, hovered, held, enabled);
    const Palette& p = palette();
    float hover = animate(id + 1, hovered ? 1.0f : 0.0f, 22.0f);
    float press = animate(id + 2, held ? 1.0f : 0.0f, 30.0f);
    ImU32 fill = 0;
    ImU32 ink = p.text;
    switch (kind) {
    case Kind::Primary:
        fill = mix(p.primary, p.primary_hover, hover);
        ink = p.primary_text;
        break;
    case Kind::Secondary:
        fill = mix(p.control, p.control_hover, hover);
        break;
    case Kind::Ghost:
        fill = fade(p.control, hover);
        break;
    }
    float alpha = enabled ? 1.0f : 0.4f;
    ImRect shown = pressed_rect(rect, press);
    window->DrawList->AddRectFilled(shown.Min, shown.Max, fade(fill, alpha), (shown.Max.y - shown.Min.y) * 0.5f);
    ImVec2 at(std::round(rect.Min.x + (box.x - label_size.x) * 0.5f), std::round(rect.Min.y + (box.y - label_size.y) * 0.5f));
    window->DrawList->AddText(at, fade(ink, alpha), label, end);
    pop_font();
    focus_ring(rect, id, box.y * 0.5f);
    return pressed;
}

bool toggle(const char* id_text, bool value) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID(id_text);
    ImVec2 box(px(42), px(24));
    ImRect rect(window->DC.CursorPos, window->DC.CursorPos + box);
    bool hovered = false;
    bool held = false;
    bool pressed = behavior(rect, id, true, hovered, held);
    const Palette& p = palette();
    float on = animate(id + 1, value ? 1.0f : 0.0f, 18.0f);
    float hover = animate(id + 2, hovered ? 1.0f : 0.0f, 22.0f);
    ImU32 off_track = mix(p.control_active, p.tertiary, hover * 0.35f);
    window->DrawList->AddRectFilled(rect.Min, rect.Max, mix(off_track, p.primary, ease_out(on)), box.y * 0.5f);
    float radius = box.y * 0.5f - px(2);
    float travel = box.x - box.y;
    ImVec2 center(rect.Min.x + box.y * 0.5f + travel * ease_out(on), rect.Min.y + box.y * 0.5f);
    ImU32 knob_off = g_dark ? p.text : IM_COL32(255, 255, 255, 255);
    window->DrawList->AddCircleFilled(center + ImVec2(0, px(1)), radius, IM_COL32(0, 0, 0, 30), 32);
    window->DrawList->AddCircleFilled(center, radius, mix(knob_off, p.background, on), 32);
    focus_ring(rect, id, box.y * 0.5f);
    return pressed;
}

void chevron(ImVec2 center, float value, ImU32 ink, bool down) {
    float h = value * 0.5f;
    ImDrawList* list = ImGui::GetWindowDrawList();
    if (down) {
        list->AddLine(center + ImVec2(-h, -h * 0.5f), center + ImVec2(0, h * 0.5f), ink, px(1.6f));
        list->AddLine(center + ImVec2(0, h * 0.5f), center + ImVec2(h, -h * 0.5f), ink, px(1.6f));
    } else {
        list->AddLine(center + ImVec2(-h, h * 0.5f), center + ImVec2(0, -h * 0.5f), ink, px(1.6f));
        list->AddLine(center + ImVec2(0, -h * 0.5f), center + ImVec2(h, h * 0.5f), ink, px(1.6f));
    }
}

bool dropdown(const char* id_text, const std::vector<std::string>& labels, int& index, float width) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID(id_text);
    ImVec2 box(px(width), px(36));
    ImRect rect(window->DC.CursorPos, window->DC.CursorPos + box);
    bool hovered = false;
    bool held = false;
    bool pressed = behavior(rect, id, true, hovered, held);
    const Palette& p = palette();
    ImGuiID popup = id + 7;
    bool open = ImGui::IsPopupOpen(popup, ImGuiPopupFlags_None);
    float hover = animate(id + 1, hovered || open ? 1.0f : 0.0f, 22.0f);
    window->DrawList->AddRectFilled(rect.Min, rect.Max, mix(p.control, p.control_hover, hover), px(10));
    push_font(Font::Regular, size::kBody);
    const char* current = index >= 0 && index < static_cast<int>(labels.size()) ? labels[static_cast<size_t>(index)].c_str() : "";
    float line = ImGui::GetFontSize();
    window->DrawList->AddText(ImVec2(rect.Min.x + px(14), std::round(rect.Min.y + (box.y - line) * 0.5f)), p.text, current);
    pop_font();
    chevron(ImVec2(rect.Max.x - px(18), rect.Min.y + box.y * 0.5f), px(8), p.secondary, true);
    focus_ring(rect, id, px(10));
    if (pressed) {
        ImGui::OpenPopupEx(popup);
        g_values[popup + 1] = 0.0f;
    }
    bool changed = false;
    ImGui::SetNextWindowPos(ImVec2(rect.Min.x, rect.Max.y + px(6)));
    ImGui::SetNextWindowSizeConstraints(ImVec2(box.x, 0), ImVec2(box.x, px(320)));
    float shown = ease_out(animate(popup + 1, 1.0f, 20.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(6), px(6)));
    if (ImGui::BeginPopupEx(popup, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove)) {
        for (size_t i = 0; i < labels.size(); i++) {
            ImGui::PushID(static_cast<int>(i));
            if (list_item(labels[i].c_str(), static_cast<int>(i) == index, box.x - px(12))) {
                index = static_cast<int>(i);
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            if (static_cast<int>(i) == index) {
                ImGui::SetItemDefaultFocus();  // (the arrow keys and a gamepad start from the one chosen)
            }
            ImGui::PopID();
        }
        if (shown < 1.0f) {
            ImGuiWindow* popup_window = ImGui::GetCurrentWindow();
            popup_window->DrawList->AddRectFilled(popup_window->Pos, popup_window->Pos + popup_window->Size, fade(ImGui::GetColorU32(ImGuiCol_PopupBg), 1.0f - shown),
                                                  ImGui::GetStyle().PopupRounding);
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    return changed;
}

bool search_box(const char* id_text, char* buffer, size_t size, const char* hint, float width) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const Palette& p = palette();
    ImVec2 origin = window->DC.CursorPos;
    ImVec2 box(px(width), px(36));
    ImRect rect(origin, origin + box);
    ImGuiID id = window->GetID(id_text);
    bool active = ImGui::GetActiveID() == id;
    float hover = animate(id + 1, active || ImGui::IsMouseHoveringRect(rect.Min, rect.Max) ? 1.0f : 0.0f, 22.0f);
    window->DrawList->AddRectFilled(rect.Min, rect.Max, mix(p.control, p.control_hover, hover), px(10));
    // (a magnifying glass at the left, the field after it, and a cross that empties it at the right)
    ImVec2 glass(rect.Min.x + px(19), rect.Min.y + box.y * 0.5f - px(1));
    window->DrawList->AddCircle(glass, px(5), p.secondary, 0, px(1.6f));
    window->DrawList->AddLine(glass + ImVec2(px(3.6f), px(3.6f)), glass + ImVec2(px(7.5f), px(7.5f)), p.secondary, px(1.8f));
    push_font(Font::Regular, size::kBody);
    float line = ImGui::GetFontSize();
    ImGui::SetCursorScreenPos(ImVec2(rect.Min.x + px(34), std::round(rect.Min.y + (box.y - line) * 0.5f)));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, p.tertiary);
    ImGui::PushStyleColor(ImGuiCol_Text, p.text);
    ImGui::PushStyleColor(ImGuiCol_NavCursor, IM_COL32(0, 0, 0, 0));  // (the ring is ours, round the whole box)
    // (a gamepad's B leaves the box with what was typed kept; Esc, from a keyboard, empties it first)
    if (active && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
        ImGui::ClearActiveID();
    }
    ImGui::SetNextItemWidth(box.x - px(34 + 36));
    bool changed = ImGui::InputTextWithHint(id_text, hint, buffer, size, ImGuiInputTextFlags_EscapeClearsAll);
    if (g_focus_next) {
        g_focus_next = false;
        focus_item();
    }
    ImGui::PopStyleColor(4);
    pop_font();
    focus_ring(rect, id, px(10));
    if (buffer[0] != '\0') {
        ImRect clear(ImVec2(rect.Max.x - px(34), rect.Min.y), rect.Max);
        ImGuiID clear_id = id + 2;
        ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
        ImGui::ItemAdd(clear, clear_id);
        ImGui::PopItemFlag();
        bool hovered = false;
        bool held = false;
        if (ImGui::ButtonBehavior(clear, clear_id, &hovered, &held)) {
            buffer[0] = '\0';
            changed = true;
        }
        if (hovered) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        cross_mark(clear.GetCenter(), px(10), hovered ? p.text : p.secondary);
    }
    ImGui::SetCursorScreenPos(origin);
    ImGui::ItemSize(rect);
    return changed;
}

bool nav_item(const char* label, bool active, float width, bool reachable) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID(label);
    ImVec2 box(px(width), px(36));
    ImRect rect(window->DC.CursorPos, window->DC.CursorPos + box);
    bool hovered = false;
    bool held = false;
    bool pressed = behavior(rect, id, true, hovered, held, reachable);
    const Palette& p = palette();
    float hover = animate(id + 1, hovered && !active ? 1.0f : 0.0f, 22.0f);
    float on = animate(id + 2, active ? 1.0f : 0.0f, 18.0f);
    window->DrawList->AddRectFilled(rect.Min, rect.Max, fade(p.control, hover * 0.6f), px(10));
    push_font(on > 0.5f ? Font::Semibold : Font::Regular, size::kBody);
    float line = ImGui::GetFontSize();
    window->DrawList->AddText(ImVec2(rect.Min.x + px(14), std::round(rect.Min.y + (box.y - line) * 0.5f)), mix(p.secondary, p.text, on), label);
    pop_font();
    focus_ring(rect, id, px(10), true);
    return pressed;
}

bool list_item(const char* label, bool active, float width) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID(label);
    ImVec2 box(px(width), px(34));
    ImRect rect(window->DC.CursorPos, window->DC.CursorPos + box);
    bool hovered = false;
    bool held = false;
    bool pressed = behavior(rect, id, true, hovered, held);
    const Palette& p = palette();
    float hover = animate(id + 1, hovered ? 1.0f : 0.0f, 22.0f);
    float on = animate(id + 2, active ? 1.0f : 0.0f, 18.0f);
    window->DrawList->AddRectFilled(rect.Min, rect.Max, mix(fade(p.control, hover * 0.7f), p.control_hover, on), px(8));
    push_font(active ? Font::Semibold : Font::Regular, size::kBody);
    float line = ImGui::GetFontSize();
    window->DrawList->AddText(ImVec2(rect.Min.x + px(12), std::round(rect.Min.y + (box.y - line) * 0.5f)), p.text, label);
    pop_font();
    focus_ring(rect, id, px(8), true);
    return pressed;
}

bool key_chip(const char* id_text, const char* label, bool capturing, float width, ImRect& rect) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID(id_text);
    ImVec2 box(px(width), px(34));
    rect = ImRect(window->DC.CursorPos, window->DC.CursorPos + box);
    bool hovered = false;
    bool held = false;
    bool pressed = behavior(rect, id, !capturing, hovered, held);
    const Palette& p = palette();
    float hover = animate(id + 1, hovered ? 1.0f : 0.0f, 22.0f);
    float focus = animate(id + 2, capturing ? 1.0f : 0.0f, 20.0f);
    window->DrawList->AddRectFilled(rect.Min, rect.Max, mix(mix(p.control, p.control_hover, hover), p.accent_soft, focus), px(10));
    if (focus > 0.01f) {
        window->DrawList->AddRect(rect.Min, rect.Max, fade(p.accent, focus), px(10), px(1.5f));
    }
    push_font(Font::Regular, size::kDetail + 0.5f);
    float line = ImGui::GetFontSize();
    ImVec2 at(rect.Min.x + px(12), std::round(rect.Min.y + (box.y - line) * 0.5f));
    ImVec4 clip(rect.Min.x, rect.Min.y, rect.Max.x - px(8), rect.Max.y);
    window->DrawList->AddText(nullptr, 0.0f, at, capturing ? p.accent : p.text, label, nullptr, 0.0f, &clip);
    pop_font();
    focus_ring(rect, id, px(10));
    return pressed;
}

float keycap(const char* key) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const Palette& p = palette();
    push_font(Font::Semibold, size::kSmall);
    ImVec2 label = ImGui::CalcTextSize(key);
    ImVec2 box(std::max(label.x + px(14), px(30)), px(24));
    ImRect rect(window->DC.CursorPos, window->DC.CursorPos + box);
    ImGui::ItemSize(rect);
    window->DrawList->AddRectFilled(rect.Min, rect.Max, p.control, px(6));
    window->DrawList->AddLine(ImVec2(rect.Min.x + px(3), rect.Max.y - px(0.5f)), ImVec2(rect.Max.x - px(3), rect.Max.y - px(0.5f)), p.separator, px(1.5f));
    window->DrawList->AddText(ImVec2(std::round(rect.Min.x + (box.x - label.x) * 0.5f), std::round(rect.Min.y + (box.y - label.y) * 0.5f)), p.text, key);
    pop_font();
    return box.x;
}

std::string fit_start(const std::string& value, Font font, float value_size, float width) {
    if (text_size(value.c_str(), font, value_size).x <= width) {
        return value;
    }
    size_t cut = 0;
    std::string shown;
    while (cut < value.size()) {
        cut++;
        while (cut < value.size() && (static_cast<unsigned char>(value[cut]) & 0xC0) == 0x80) {
            cut++;
        }
        shown = "…" + value.substr(cut);
        if (text_size(shown.c_str(), font, value_size).x <= width) {
            break;
        }
    }
    return shown;
}

void separator(float width) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImVec2 at = window->DC.CursorPos;
    window->DrawList->AddRectFilled(at, at + ImVec2(px(width), std::max(1.0f, std::floor(scale()))), palette().separator);
    ImGui::Dummy(ImVec2(px(width), std::max(1.0f, std::floor(scale()))));
}

void progress_bar(ImGuiID id, float fraction, float width, float height) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImVec2 box(px(width), px(height));
    ImRect rect(window->DC.CursorPos, window->DC.CursorPos + box);
    ImGui::ItemSize(rect);
    const Palette& p = palette();
    float shown = animate(id, std::clamp(fraction, 0.0f, 1.0f), 6.0f);
    window->DrawList->AddRectFilled(rect.Min, rect.Max, p.control, box.y * 0.5f);
    if (shown > 0.001f) {
        float filled = std::max(box.y, box.x * shown);
        window->DrawList->AddRectFilled(rect.Min, ImVec2(rect.Min.x + filled, rect.Max.y), p.accent, box.y * 0.5f);
    }
}

void spinner(ImVec2 center, float radius, ImU32 ink) {
    ImDrawList* list = ImGui::GetWindowDrawList();
    float t = static_cast<float>(ImGui::GetTime());
    float start = t * 5.5f;
    list->PathArcTo(center, radius, start, start + 4.2f, 24);
    list->PathStroke(ink, px(1.8f));
    g_animating = true;
}

void check_mark(ImVec2 center, float value, ImU32 ink) {
    ImDrawList* list = ImGui::GetWindowDrawList();
    float h = value * 0.5f;
    ImVec2 points[] = {center + ImVec2(-h, 0.0f), center + ImVec2(-h * 0.3f, h * 0.65f), center + ImVec2(h, -h * 0.6f)};
    list->AddPolyline(points, 3, ink, px(1.8f));
}

void cross_mark(ImVec2 center, float value, ImU32 ink) {
    ImDrawList* list = ImGui::GetWindowDrawList();
    float h = value * 0.45f;
    list->AddLine(center + ImVec2(-h, -h), center + ImVec2(h, h), ink, px(1.8f));
    list->AddLine(center + ImVec2(-h, h), center + ImVec2(h, -h), ink, px(1.8f));
}

void die(ImVec2 origin, float value) {
    ImDrawList* list = ImGui::GetWindowDrawList();
    float radius = value * 0.215f;
    ImVec2 end = origin + ImVec2(value, value);
    int first = list->VtxBuffer.Size;
    list->AddRectFilled(origin, end, IM_COL32(255, 255, 255, 255), radius);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(list, first, list->VtxBuffer.Size, origin, ImVec2(origin.x, end.y), IM_COL32(255, 111, 180, 255),
                                                  IM_COL32(212, 20, 111, 255));
    ImVec2 center = origin + ImVec2(value * 0.5f, value * 0.5f);
    float angle = 14.0f * 3.14159265f / 180.0f;
    float cosine = std::cos(angle);
    float sine = std::sin(angle);
    auto place = [&](float x, float y) { return ImVec2(center.x + x * cosine - y * sine, center.y + x * sine + y * cosine); };
    float half = value * 0.285f;
    float corner = half * 0.48f;
    auto face = [&](ImVec2 offset, ImU32 fill) {
        const float corners[4][3] = {{half - corner, half - corner, 0.0f}, {-(half - corner), half - corner, 0.5f}, {-(half - corner), -(half - corner), 1.0f}, {half - corner, -(half - corner), 1.5f}};
        for (const auto& c : corners) {
            for (int step = 0; step <= 6; step++) {
                float a = (c[2] + step / 12.0f) * 3.14159265f;
                list->PathLineTo(place(c[0] + std::cos(a) * corner, c[1] + std::sin(a) * corner) + offset);
            }
        }
        list->PathFillConvex(fill);
    };
    face(ImVec2(0, value * 0.035f), IM_COL32(106, 0, 53, 70));
    face(ImVec2(0, 0), IM_COL32(250, 249, 252, 255));
    float pip = value * 0.063f;
    float spacing = half * 0.52f;
    for (int i = -1; i <= 1; i++) {
        list->AddCircleFilled(place(spacing * i, spacing * i), pip, IM_COL32(27, 27, 38, 255), 24);
    }
}

}
