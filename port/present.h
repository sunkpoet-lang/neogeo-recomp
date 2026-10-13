// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Putting the picture on the screen. With SDL's GPU API (Metal, Vulkan, Direct3D 12) where there is a
// device for it; with SDL's renderer otherwise, or when asked (--renderer sdl).
#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

namespace port {

class Presenter {
public:
    ~Presenter();
    // False with the reason in `error` if the window cannot be drawn to either way
    bool Open(SDL_Window *window, bool gpu, std::string &error);
    void Close();
    bool UsesGpu() const {
        return m_device != nullptr;
    }
    SDL_GPUDevice *Device() const {
        return m_device;
    }
    SDL_Renderer *Renderer() const {
        return m_renderer;
    }
    // The window's size in pixels
    void Size(int &width, int &height) const;
    // Shows a picture (`width` x `height` pixels, each red, green, blue and a spare byte in memory) in the
    // rectangle `where` of the window, the rest black
    void Show(const uint32_t *pixels, int width, int height, const SDL_FRect &where);

    // For tests: the GPU path with no window. The picture is drawn `outWidth` x `outHeight` as Show would
    // draw it and read back. False (with the reason) where there is no GPU device.
    static bool Render(const uint32_t *pixels, int width, int height, int outWidth, int outHeight, std::vector<uint32_t> &out,
                       std::string &error);

private:
    bool OpenGpu(SDL_Window *window, std::string &error);
    bool MakePipeline(SDL_GPUTextureFormat target, std::string &error);
    bool Upload(SDL_GPUCommandBuffer *commands, const uint32_t *pixels, int width, int height);
    void Draw(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *target, const SDL_FRect &where);

    SDL_Window *m_window = nullptr;
    // GPU
    SDL_GPUDevice *m_device = nullptr;
    SDL_GPUGraphicsPipeline *m_pipeline = nullptr;
    SDL_GPUSampler *m_sampler = nullptr;
    SDL_GPUTexture *m_picture = nullptr;
    SDL_GPUTransferBuffer *m_transfer = nullptr;
    int m_width = 0, m_height = 0;
    // SDL's renderer
    SDL_Renderer *m_renderer = nullptr;
    SDL_Texture *m_texture = nullptr, *m_enlarged = nullptr;
    int m_enlargedBy = 0;
};

} // namespace port
