// SPDX-License-Identifier: GPL-3.0-only
// Part of cps2recomp (the user's fork: the Neo Geo board).

#include "neogeo_window.h"

#include "../machine/neogeo.h"
#include "png_write.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace cps2recomp {

namespace {
    constexpr double kFramesPerSecond = 12000000.0 / (NeoGeo::kLines * NeoGeo::kCpuPerLine); // 59.19

    struct Key {
        SDL_Scancode code;
        int player;
        uint16_t bits;
    };
    const Key kKeys[] = {
        {SDL_SCANCODE_UP, 0, NeoGeo::kUp},     {SDL_SCANCODE_DOWN, 0, NeoGeo::kDown},  {SDL_SCANCODE_LEFT, 0, NeoGeo::kLeft},
        {SDL_SCANCODE_RIGHT, 0, NeoGeo::kRight}, {SDL_SCANCODE_A, 0, NeoGeo::kA},      {SDL_SCANCODE_Z, 0, NeoGeo::kB},
        {SDL_SCANCODE_S, 0, NeoGeo::kC},       {SDL_SCANCODE_X, 0, NeoGeo::kD},        {SDL_SCANCODE_1, 0, NeoGeo::kStart},
        {SDL_SCANCODE_5, 0, NeoGeo::kCoin},    {SDL_SCANCODE_9, 0, NeoGeo::kService},  {SDL_SCANCODE_F2, 0, NeoGeo::kTest},
        {SDL_SCANCODE_I, 1, NeoGeo::kUp},      {SDL_SCANCODE_K, 1, NeoGeo::kDown},     {SDL_SCANCODE_J, 1, NeoGeo::kLeft},
        {SDL_SCANCODE_L, 1, NeoGeo::kRight},   {SDL_SCANCODE_T, 1, NeoGeo::kA},        {SDL_SCANCODE_G, 1, NeoGeo::kB},
        {SDL_SCANCODE_Y, 1, NeoGeo::kC},       {SDL_SCANCODE_H, 1, NeoGeo::kD},        {SDL_SCANCODE_2, 1, NeoGeo::kStart},
        {SDL_SCANCODE_6, 1, NeoGeo::kCoin},
    };

    uint16_t GamepadBits(SDL_Gamepad *pad) {
        uint16_t bits = 0;
        auto button = [pad](SDL_GamepadButton b) { return SDL_GetGamepadButton(pad, b); };
        const int16_t x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX), y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
        constexpr int16_t kDead = 16000;
        if (button(SDL_GAMEPAD_BUTTON_DPAD_UP) || y < -kDead) {
            bits |= NeoGeo::kUp;
        }
        if (button(SDL_GAMEPAD_BUTTON_DPAD_DOWN) || y > kDead) {
            bits |= NeoGeo::kDown;
        }
        if (button(SDL_GAMEPAD_BUTTON_DPAD_LEFT) || x < -kDead) {
            bits |= NeoGeo::kLeft;
        }
        if (button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || x > kDead) {
            bits |= NeoGeo::kRight;
        }
        // (the light attacks on the left, as KOF is usually played: West A, South B, North C, East D)
        if (button(SDL_GAMEPAD_BUTTON_WEST)) {
            bits |= NeoGeo::kA;
        }
        if (button(SDL_GAMEPAD_BUTTON_SOUTH)) {
            bits |= NeoGeo::kB;
        }
        if (button(SDL_GAMEPAD_BUTTON_NORTH)) {
            bits |= NeoGeo::kC;
        }
        if (button(SDL_GAMEPAD_BUTTON_EAST)) {
            bits |= NeoGeo::kD;
        }
        if (button(SDL_GAMEPAD_BUTTON_START)) {
            bits |= NeoGeo::kStart;
        }
        if (button(SDL_GAMEPAD_BUTTON_BACK)) {
            bits |= NeoGeo::kCoin;
        }
        return bits;
    }
} // namespace

int RunNeoGeoWindow(NeoGeo &board, const NeoGeoWindowOptions &options) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "%s: SDL: %s\n", options.title, SDL_GetError());
        return 1;
    }
    const int height = NeoGeoVideo::kHeight * std::max(1, options.scale);
    SDL_Window *window = SDL_CreateWindow(options.title, static_cast<int>(std::lround(height * 4.0 / 3.0)), height,
                                          SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    SDL_Texture *texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
                                                        NeoGeoVideo::kWidth, NeoGeoVideo::kHeight)
                                    : nullptr;
    if (!texture) {
        fprintf(stderr, "%s: SDL: %s\n", options.title, SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    SDL_SetRenderVSync(renderer, 0); // (the pace is the sound's, not the screen's)

    // The sound at the chip's own rate (SDL converts it to the device's)
    NeoGeoSound &sound = board.Sound();
    SDL_AudioStream *audio = nullptr;
    if (!options.mute) {
        const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, static_cast<int>(NeoGeoSound::SampleRate())};
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (audio) {
            SDL_ResumeAudioStreamDevice(audio);
        } else {
            fprintf(stderr, "%s: no sound: %s\n", options.title, SDL_GetError());
        }
    }
    sound.keep = audio != nullptr;
    const double frameBytes = NeoGeoSound::SampleRate() / kFramesPerSecond * 4;

    SDL_Gamepad *pads[2] = {};
    auto reopenPads = [&]() {
        for (SDL_Gamepad *&pad : pads) {
            if (pad) {
                SDL_CloseGamepad(pad);
                pad = nullptr;
            }
        }
        int count = 0;
        SDL_JoystickID *ids = SDL_GetGamepads(&count);
        for (int n = 0; ids && n < count && n < 2; n++) {
            pads[n] = SDL_OpenGamepad(ids[n]);
        }
        SDL_free(ids);
    };
    reopenPads();

    const uint64_t started = SDL_GetTicksNS();
    const uint64_t firstFrame = board.Frames();
    uint64_t nextFrameAt = started;
    const uint64_t frameNs = static_cast<uint64_t>(1e9 / kFramesPerSecond);
    bool running = true, fullscreen = false;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_GAMEPAD_ADDED || event.type == SDL_EVENT_GAMEPAD_REMOVED) {
                reopenPads();
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    running = false;
                } else if (event.key.scancode == SDL_SCANCODE_F11) {
                    fullscreen = !fullscreen;
                    SDL_SetWindowFullscreen(window, fullscreen);
                } else if (event.key.scancode == SDL_SCANCODE_F12) {
                    char path[1024];
                    snprintf(path, sizeof(path), "%s/%s_%06llu.png", options.shotDir.c_str(), options.id,
                             static_cast<unsigned long long>(board.Frames()));
                    WritePng(path, board.Picture(), NeoGeoVideo::kWidth, NeoGeoVideo::kHeight);
                }
            }
        }
        // The board's pace: by the sound queue (about three frames ahead), or by the clock with no sound
        if (audio) {
            if (SDL_GetAudioStreamQueued(audio) > frameBytes * 3) {
                SDL_Delay(1);
                continue;
            }
        } else {
            const uint64_t now = SDL_GetTicksNS();
            if (now < nextFrameAt) {
                SDL_DelayNS(std::min<uint64_t>(nextFrameAt - now, 1000000));
                continue;
            }
            nextFrameAt = std::max(nextFrameAt + frameNs, now - frameNs * 3);
        }

        const bool *keys = SDL_GetKeyboardState(nullptr);
        board.players[0] = board.players[1] = 0;
        for (const Key &key : kKeys) {
            if (keys[key.code]) {
                board.players[key.player] |= key.bits;
            }
        }
        for (int p = 0; p < 2; p++) {
            if (pads[p]) {
                board.players[p] |= GamepadBits(pads[p]);
            }
        }
        board.RunFrame();
        if (audio && !sound.made.empty()) {
            SDL_PutAudioStreamData(audio, sound.made.data(), static_cast<int>(sound.made.size() * sizeof(int16_t)));
        }
        sound.made.clear();

        // The picture at 4:3, as large as the window holds
        SDL_UpdateTexture(texture, nullptr, board.Picture(), NeoGeoVideo::kWidth * 4);
        int w = 0, h = 0;
        SDL_GetRenderOutputSize(renderer, &w, &h);
        const float drawW = std::min(static_cast<float>(w), h * 4.0f / 3.0f), drawH = drawW * 3.0f / 4.0f;
        const SDL_FRect to{(w - drawW) / 2, (h - drawH) / 2, drawW, drawH};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, &to);
        SDL_RenderPresent(renderer);
    }

    const double seconds = (SDL_GetTicksNS() - started) / 1e9;
    const uint64_t ran = board.Frames() - firstFrame;
    fprintf(stderr, "%s: %llu frames in %.1f s (%.2f a second)\n", options.title, static_cast<unsigned long long>(ran),
            seconds, seconds > 0 ? ran / seconds : 0.0);
    for (SDL_Gamepad *pad : pads) {
        if (pad) {
            SDL_CloseGamepad(pad);
        }
    }
    if (audio) {
        SDL_DestroyAudioStream(audio);
    }
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace cps2recomp
