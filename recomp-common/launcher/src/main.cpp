// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// (Derived from the launcher of wiiparty-recomp by arelkair, GPL-3.0-or-later:
// https://github.com/arelkair/wiiparty-recomp, launcher/src/main.cpp; see THIRD_PARTY_NOTICES.md.)
//
// The launcher the recomp ports share (SDL3 + Dear ImGui; the look from ui.cpp).
//   <id>-launcher [--screenshot PREFIX [--size WxH]]   (PREFIX0.png.. one picture per page, then quit; no
//                                                       window is shown)
//   <id>-launcher --screenshot PREFIX --press KEYS     (one picture, PREFIX.png, after KEYS are pressed one
//                                                       by one: u d l r arrow keys, e Enter, s Space, x Esc;
//                                                       of a gamepad made for the test: U D L R the d-pad,
//                                                       A B the buttons, [ ] the shoulders; 0-9 that page;
//                                                       . a wait; f Ctrl+F; "text" typed)
//   <id>-launcher --play                               (straight into the game set up earlier)
// <ID>_HOME=folder (LAUNCHER_HOME_ENV) uses that folder instead of the player's (settings, launcher.ini).

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "app.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "ui.h"

namespace {

void render(SDL_Renderer* renderer) {
    ImGui::Render();
    ImGuiIO& io = ImGui::GetIO();
    SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    ImVec4 clear = ImGui::ColorConvertU32ToFloat4(ui::palette().background);
    SDL_SetRenderDrawColorFloat(renderer, clear.x, clear.y, clear.z, 1.0f);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
}

bool save_picture(SDL_Renderer* renderer, const std::string& path) {
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
    if (!surface) {
        return false;
    }
    bool saved = SDL_SavePNG(surface, path.c_str());
    SDL_DestroySurface(surface);
    return saved;
}

bool system_dark() {
    return SDL_GetSystemTheme() == SDL_SYSTEM_THEME_DARK;
}

// --press: keys as the window would get them, and the buttons of a gamepad that exists only in the program.
struct Presser {
    SDL_Window* window = nullptr;
    SDL_Joystick* pad = nullptr;

    void start(const std::string& keys) {
        if (keys.find_first_of("UDLRAB[]") == std::string::npos) {
            return;
        }
        // (a window that is not shown is never the one in front, and a gamepad is otherwise heard only there)
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.vendor_id = 0x0001;
        desc.product_id = 0x0001;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.name = "Test gamepad";
        pad = SDL_OpenJoystick(SDL_AttachVirtualJoystick(&desc));
    }
    // A character typed, as a window gets it when its text is being typed in
    void type(char character) {
        static char text[256][2];
        unsigned char c = static_cast<unsigned char>(character);
        text[c][0] = character;
        SDL_Event event{};
        event.type = SDL_EVENT_TEXT_INPUT;
        event.text.windowID = SDL_GetWindowID(window);
        event.text.text = text[c];
        SDL_PushEvent(&event);
    }
    void set(char key, bool down) {
        static const struct {
            char key;
            SDL_Scancode scancode;
            SDL_Keycode keycode;
        } kKeys[] = {{'u', SDL_SCANCODE_UP, SDLK_UP},         {'d', SDL_SCANCODE_DOWN, SDLK_DOWN},   {'l', SDL_SCANCODE_LEFT, SDLK_LEFT},
                     {'r', SDL_SCANCODE_RIGHT, SDLK_RIGHT},   {'e', SDL_SCANCODE_RETURN, SDLK_RETURN}, {'s', SDL_SCANCODE_SPACE, SDLK_SPACE},
                     {'x', SDL_SCANCODE_ESCAPE, SDLK_ESCAPE}, {'f', SDL_SCANCODE_F, SDLK_F}};
        static const struct {
            char key;
            SDL_GamepadButton button;
        } kButtons[] = {{'U', SDL_GAMEPAD_BUTTON_DPAD_UP},   {'D', SDL_GAMEPAD_BUTTON_DPAD_DOWN},    {'L', SDL_GAMEPAD_BUTTON_DPAD_LEFT},
                        {'R', SDL_GAMEPAD_BUTTON_DPAD_RIGHT}, {'A', SDL_GAMEPAD_BUTTON_SOUTH},        {'B', SDL_GAMEPAD_BUTTON_EAST},
                        {'[', SDL_GAMEPAD_BUTTON_LEFT_SHOULDER}, {']', SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER}};
        for (const auto& known : kKeys) {
            if (known.key == key) {
                SDL_Event event{};
                event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
                event.key.windowID = SDL_GetWindowID(window);
                event.key.scancode = known.scancode;
                event.key.key = known.keycode;
                event.key.down = down;
                event.key.mod = key == 'f' ? SDL_KMOD_LCTRL : SDL_KMOD_NONE;  // (f: Ctrl+F)
                SDL_PushEvent(&event);
            }
        }
        for (const auto& known : kButtons) {
            if (known.key == key && pad) {
                SDL_SetJoystickVirtualButton(pad, known.button, down);
            }
        }
    }
};

}  // namespace

int main(int argc, char** argv) {
    std::string screenshot_prefix;
    std::string press;
    bool pressing = false;
    bool play = false;
    int width = 980, height = 660;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot_prefix = argv[++i];
        } else if (std::strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            if (std::sscanf(argv[++i], "%dx%d", &width, &height) != 2 || width < 320 || height < 240) {
                width = 980, height = 660;
            }
        } else if (std::strcmp(argv[i], "--press") == 0 && i + 1 < argc) {
            press = argv[++i];
            pressing = true;
        } else if (std::strcmp(argv[i], "--play") == 0) {
            play = true;
        }
    }
    SDL_SetAppMetadata(LAUNCHER_GAME_NAME, "0.1", LAUNCHER_APP_ID ".launcher");
    if (play) {
        // Straight into the game, e.g. from a Steam shortcut in Game Mode.
        int code = play_directly();
        SDL_Quit();
        return code;
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, LAUNCHER_GAME_NAME, SDL_GetError(), nullptr);
        return 1;
    }
    float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    if (scale <= 0.0f) {
        scale = 1.0f;
    }
    SDL_Window* window = SDL_CreateWindow(LAUNCHER_GAME_NAME, static_cast<int>(width * scale), static_cast<int>(height * scale),
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, LAUNCHER_GAME_NAME, SDL_GetError(), nullptr);
        return 1;
    }
    SDL_SetWindowMinimumSize(window, static_cast<int>(800 * scale), static_cast<int>(540 * scale));
    SDL_SetRenderVSync(renderer, 1);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    // The arrow keys with Space or Enter, and a gamepad (any that is connected), as well as the mouse
    const ImGuiConfigFlags navigation = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= navigation;
    io.ConfigNavEscapeClearFocusItem = false;  // (Back goes to the sidebar: app.cpp)
    ImGui::GetCurrentContext()->ConfigNavWindowingWithGamepad = false;  // (one window: no list of windows)
    ImGui::GetCurrentContext()->ConfigNavWindowingKeyNext = 0;
    ImGui::GetCurrentContext()->ConfigNavWindowingKeyPrev = 0;
    ui::load_fonts();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoAll);

    ui::set_dark(system_dark());
    ui::apply_style(SDL_GetWindowDisplayScale(window));
    App app(window);
    if (screenshot_prefix.empty()) {  // (pictures are taken with nothing put on the screen)
        SDL_ShowWindow(window);
        app.set_interactive();
    }
    Presser presser;
    presser.window = window;
    presser.start(press);
    size_t press_at = 0;
    bool typing = false;  // (between quotes in --press)

    Uint64 previous = SDL_GetTicksNS();
    int shot_page = 0;
    int shot_frames = 0;
    bool running = true;
    while (running) {
        bool idle = screenshot_prefix.empty() && !ui::animating() && !app.busy();
        SDL_Event event;
        if (idle && SDL_WaitEventTimeout(&event, 250)) {
            if (!app.consume(event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);
            }
            running = event.type != SDL_EVENT_QUIT && event.type != SDL_EVENT_WINDOW_CLOSE_REQUESTED;
            ui::request_frames(3);
        }
        while (SDL_PollEvent(&event)) {
            if (!app.consume(event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);
            }
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = false;
            }
            if (event.type == SDL_EVENT_SYSTEM_THEME_CHANGED) {
                ui::set_dark(system_dark());
                ui::apply_style(ui::scale());
            }
            ui::request_frames(3);
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(50);
            continue;
        }
        // (while a key or a button is being chosen for an action, it is not also a press of what the ring is on)
        io.ConfigFlags = app.navigation_paused() ? io.ConfigFlags & ~navigation : io.ConfigFlags | navigation;
        Uint64 now = SDL_GetTicksNS();
        ui::begin_frame(static_cast<float>(now - previous) / 1e9f);
        previous = now;
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        if (!screenshot_prefix.empty() && shot_frames == 0 && !pressing) {
            app.set_page(shot_page);
        }
        if (pressing && shot_frames >= 10 && press_at < press.size() && !app.busy()) {
            // One key every eight frames: down for four of them
            int step = (shot_frames - 10) % 8;
            char key = press[press_at];
            if (key == '"' || typing) {
                if (step == 0 && key == '"') {
                    typing = !typing;
                } else if (step == 0) {
                    presser.type(key);
                }
            } else if (step == 0 && key >= '0' && key <= '9') {
                app.set_page(std::min(key - '0', app.page_count() - 1));
            } else if (step == 0) {
                presser.set(key, true);
            } else if (step == 4) {
                presser.set(key, false);
            }
            if (step == 7 && ++press_at == press.size()) {
                shot_frames = -1;  // (and then the picture, as for a page)
                press.clear();
            }
        }
        app.frame();
        render(renderer);
        if (pressing && !press.empty()) {
            shot_frames++;
        } else if (!screenshot_prefix.empty() && ++shot_frames >= 45 && !app.busy()) {
            save_picture(renderer, screenshot_prefix + (pressing ? std::string() : std::to_string(shot_page)) + ".png");
            shot_frames = 0;
            if (pressing || ++shot_page == app.page_count()) {
                running = false;
            }
        }
        SDL_RenderPresent(renderer);
    }
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
