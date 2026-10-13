// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// (Derived from the launcher of wiiparty-recomp by arelkair, GPL-3.0-or-later:
// https://github.com/arelkair/wiiparty-recomp, launcher/src/main.cpp; see THIRD_PARTY_NOTICES.md.)
//
// The launcher the recomp ports share (SDL3 + Dear ImGui; the look from ui.cpp).
//   <id>-launcher [--screenshot PREFIX [--size WxH]]   (PREFIX0.png.. one picture per page, then quit; no
//                                                       window is shown)
//   <id>-launcher --play                               (straight into the game set up earlier)
// <ID>_HOME=folder (LAUNCHER_HOME_ENV) uses that folder instead of the player's (settings, launcher.ini).

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

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

}  // namespace

int main(int argc, char** argv) {
    std::string screenshot_prefix;
    bool play = false;
    int width = 980, height = 660;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot_prefix = argv[++i];
        } else if (std::strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            if (std::sscanf(argv[++i], "%dx%d", &width, &height) != 2 || width < 320 || height < 240) {
                width = 980, height = 660;
            }
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
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ui::load_fonts();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    ui::set_dark(system_dark());
    ui::apply_style(SDL_GetWindowDisplayScale(window));
    App app(window);
    if (screenshot_prefix.empty()) {  // (pictures are taken with nothing put on the screen)
        SDL_ShowWindow(window);
    }

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
        Uint64 now = SDL_GetTicksNS();
        ui::begin_frame(static_cast<float>(now - previous) / 1e9f);
        previous = now;
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        if (!screenshot_prefix.empty() && shot_frames == 0) {
            app.set_page(static_cast<Page>(shot_page));
        }
        app.frame();
        render(renderer);
        if (!screenshot_prefix.empty() && ++shot_frames >= 45 && !app.busy()) {
            save_picture(renderer, screenshot_prefix + std::to_string(shot_page) + ".png");
            shot_frames = 0;
            if (++shot_page == static_cast<int>(Page::Count)) {
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
