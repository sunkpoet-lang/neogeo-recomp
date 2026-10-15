// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.

#include "present.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace port {

namespace {
#include "shaders.inc"

    SDL_GPUShader *MakeShader(SDL_GPUDevice *device, SDL_GPUShaderStage stage, const unsigned char *spirv, size_t spirvSize,
                              const char *msl, int samplers, int uniforms) {
        SDL_GPUShaderCreateInfo info = {};
        if (SDL_GetGPUShaderFormats(device) & SDL_GPU_SHADERFORMAT_MSL) {
            info.format = SDL_GPU_SHADERFORMAT_MSL;
            info.code = reinterpret_cast<const Uint8 *>(msl);
            info.code_size = strlen(msl);
            info.entrypoint = "main0";
        } else {
            info.format = SDL_GPU_SHADERFORMAT_SPIRV;
            info.code = spirv;
            info.code_size = spirvSize;
            info.entrypoint = "main";
        }
        info.stage = stage;
        info.num_samplers = static_cast<Uint32>(samplers);
        info.num_uniform_buffers = static_cast<Uint32>(uniforms);
        return SDL_CreateGPUShader(device, &info);
    }

    struct Sizes {
        float size[2], scale[2];
    };
} // namespace

float PictureShape(int columns, int rows, Shape shape, float window) {
    switch (shape) {
    case Shape::SquarePixels: return static_cast<float>(columns) / rows;
    case Shape::Fill: return window;
    case Shape::Arcade: break;
    }
    // (4:3 for the board's 384 columns, and as much wider as a wider picture has more of them: 16:9 for 512)
    return 4.0f / 3.0f * columns / 384.0f;
}

SDL_FRect FitPicture(int width, int height, int columns, int rows, Shape shape, Scaling scaling) {
    const float ratio = PictureShape(columns, rows, shape, height > 0 ? static_cast<float>(width) / height : 1.0f);
    SDL_FRect where;
    if (scaling == Scaling::WholePixels) {
        // As many of the window's rows to one of the picture's as fits with the picture's width too (a
        // picture as wide as the window across, if it fills it); with none that fits, as large as fits
        for (int by = height / rows; by >= 1; by--) {
            where.h = static_cast<float>(by * rows);
            where.w = shape == Shape::Fill ? static_cast<float>(width) : std::round(where.h * ratio);
            if (where.w <= width) {
                where.x = std::floor((width - where.w) / 2);
                where.y = std::floor((height - where.h) / 2);
                return where;
            }
        }
    }
    where.w = std::min<float>(static_cast<float>(width), height * ratio);
    where.h = shape == Shape::Fill ? static_cast<float>(height) : where.w / ratio;
    where.x = (width - where.w) / 2;
    where.y = (height - where.h) / 2;
    return where;
}

Presenter::~Presenter() {
    Close();
}

bool Presenter::MakePipeline(SDL_GPUTextureFormat target, std::string &error) {
    SDL_GPUShader *vertex = MakeShader(m_device, SDL_GPU_SHADERSTAGE_VERTEX, k_present_vert_spirv, sizeof(k_present_vert_spirv),
                                       k_present_vert_msl, 0, 0);
    SDL_GPUShader *fragment = MakeShader(m_device, SDL_GPU_SHADERSTAGE_FRAGMENT, k_present_frag_spirv, sizeof(k_present_frag_spirv),
                                         k_present_frag_msl, 1, 1);
    if (vertex && fragment) {
        SDL_GPUColorTargetDescription colour = {};
        colour.format = target;
        SDL_GPUGraphicsPipelineCreateInfo info = {};
        info.vertex_shader = vertex;
        info.fragment_shader = fragment;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        info.target_info.num_color_targets = 1;
        info.target_info.color_target_descriptions = &colour;
        m_pipeline = SDL_CreateGPUGraphicsPipeline(m_device, &info);
    }
    if (vertex) {
        SDL_ReleaseGPUShader(m_device, vertex);
    }
    if (fragment) {
        SDL_ReleaseGPUShader(m_device, fragment);
    }
    SDL_GPUSamplerCreateInfo sampler = {};
    sampler.min_filter = sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler.address_mode_u = sampler.address_mode_v = sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    m_sampler = SDL_CreateGPUSampler(m_device, &sampler);
    if (!m_pipeline || !m_sampler) {
        error = SDL_GetError();
        return false;
    }
    return true;
}

bool Presenter::OpenGpu(SDL_Window *window, std::string &error) {
    m_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
    if (!m_device) {
        error = SDL_GetError();
        return false;
    }
    SDL_GPUTextureFormat target = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    if (window) {
        if (!SDL_ClaimWindowForGPUDevice(m_device, window)) {
            error = SDL_GetError();
            SDL_DestroyGPUDevice(m_device);
            m_device = nullptr;
            return false;
        }
        // Frames are paced by the port, to the board's rate: the picture is shown as soon as it is made, where
        // the window lets it be (no waiting for the screen, which would hold the game's controls back too)
        for (const SDL_GPUPresentMode mode : {SDL_GPU_PRESENTMODE_IMMEDIATE, SDL_GPU_PRESENTMODE_MAILBOX}) {
            if (SDL_WindowSupportsGPUPresentMode(m_device, window, mode)) {
                SDL_SetGPUSwapchainParameters(m_device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode);
                break;
            }
        }
        target = SDL_GetGPUSwapchainTextureFormat(m_device, window);
    }
    if (!MakePipeline(target, error)) {
        Close();
        return false;
    }
    return true;
}

bool Presenter::Open(SDL_Window *window, bool gpu, std::string &error) {
    m_window = window;
    if (gpu && OpenGpu(window, error)) {
        return true;
    }
    m_renderer = SDL_CreateRenderer(window, nullptr);
    if (!m_renderer) {
        error = SDL_GetError();
        return false;
    }
    SDL_SetRenderVSync(m_renderer, 0); // (frames are paced by the port, to the board's rate)
    return true;
}

void Presenter::Close() {
    if (m_device) {
        if (m_transfer) {
            SDL_ReleaseGPUTransferBuffer(m_device, m_transfer);
        }
        if (m_picture) {
            SDL_ReleaseGPUTexture(m_device, m_picture);
        }
        if (m_sampler) {
            SDL_ReleaseGPUSampler(m_device, m_sampler);
        }
        if (m_pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_device, m_pipeline);
        }
        if (m_window) {
            SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
        }
        SDL_DestroyGPUDevice(m_device);
    }
    m_device = nullptr;
    m_pipeline = nullptr;
    m_sampler = nullptr;
    m_picture = nullptr;
    m_transfer = nullptr;
    for (SDL_Texture *texture : {m_texture, m_enlarged}) {
        if (texture) {
            SDL_DestroyTexture(texture);
        }
    }
    m_texture = m_enlarged = nullptr;
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
    }
    m_renderer = nullptr;
    m_width = m_height = m_enlargedBy = 0;
}

void Presenter::Size(int &width, int &height) const {
    if (m_renderer) {
        SDL_GetRenderOutputSize(m_renderer, &width, &height);
    } else {
        SDL_GetWindowSizeInPixels(m_window, &width, &height);
    }
}

bool Presenter::Upload(SDL_GPUCommandBuffer *commands, const uint32_t *pixels, int width, int height) {
    if (width != m_width || height != m_height) {
        if (m_picture) {
            SDL_ReleaseGPUTexture(m_device, m_picture);
        }
        if (m_transfer) {
            SDL_ReleaseGPUTransferBuffer(m_device, m_transfer);
        }
        SDL_GPUTextureCreateInfo texture = {};
        texture.type = SDL_GPU_TEXTURETYPE_2D;
        texture.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        texture.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        texture.width = static_cast<Uint32>(width);
        texture.height = static_cast<Uint32>(height);
        texture.layer_count_or_depth = 1;
        texture.num_levels = 1;
        m_picture = SDL_CreateGPUTexture(m_device, &texture);
        SDL_GPUTransferBufferCreateInfo transfer = {};
        transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transfer.size = static_cast<Uint32>(width * height * 4);
        m_transfer = SDL_CreateGPUTransferBuffer(m_device, &transfer);
        m_width = width;
        m_height = height;
    }
    if (!m_picture || !m_transfer) {
        return false;
    }
    void *to = SDL_MapGPUTransferBuffer(m_device, m_transfer, true);
    if (!to) {
        return false;
    }
    memcpy(to, pixels, static_cast<size_t>(width) * height * 4);
    SDL_UnmapGPUTransferBuffer(m_device, m_transfer);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureTransferInfo from = {};
    from.transfer_buffer = m_transfer;
    from.pixels_per_row = static_cast<Uint32>(width);
    from.rows_per_layer = static_cast<Uint32>(height);
    SDL_GPUTextureRegion region = {};
    region.texture = m_picture;
    region.w = static_cast<Uint32>(width);
    region.h = static_cast<Uint32>(height);
    region.d = 1;
    SDL_UploadToGPUTexture(copy, &from, &region, true);
    SDL_EndGPUCopyPass(copy);
    return true;
}

void Presenter::Draw(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *target, const SDL_FRect &where) {
    SDL_GPUColorTargetInfo colour = {};
    colour.texture = target;
    colour.clear_color = {0, 0, 0, 1};
    colour.load_op = SDL_GPU_LOADOP_CLEAR;
    colour.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &colour, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(pass, m_pipeline);
    const SDL_GPUViewport viewport = {where.x, where.y, where.w, where.h, 0, 1};
    SDL_SetGPUViewport(pass, &viewport);
    const SDL_GPUTextureSamplerBinding binding = {m_picture, m_sampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    // (blended all over: as if each of the picture's pixels took one of the screen's, which the shader blends
    // from edge to edge)
    const bool smooth = m_scaling == Scaling::Smooth;
    const Sizes sizes = {{static_cast<float>(m_width), static_cast<float>(m_height)},
                         {smooth ? 1.0f : std::floor(where.w / m_width + 0.999f), smooth ? 1.0f : std::floor(where.h / m_height + 0.999f)}};
    SDL_PushGPUFragmentUniformData(commands, 0, &sizes, sizeof(sizes));
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

void Presenter::Show(const uint32_t *pixels, int width, int height, const SDL_FRect &where) {
    if (m_device) {
        SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(m_device);
        if (!commands) {
            return;
        }
        SDL_GPUTexture *screen = nullptr;
        // (not the call that waits for a free picture of the window's: a frame that cannot be shown is left out)
        if (SDL_AcquireGPUSwapchainTexture(commands, m_window, &screen, nullptr, nullptr) && screen && Upload(commands, pixels, width, height)) {
            Draw(commands, screen, where);
        }
        SDL_SubmitGPUCommandBuffer(commands);
        return;
    }
    if (!m_texture || width != m_width || height != m_height) {
        if (m_texture) {
            SDL_DestroyTexture(m_texture);
        }
        m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_XBGR8888, SDL_TEXTUREACCESS_STREAMING, width, height);
        SDL_SetTextureScaleMode(m_texture, SDL_SCALEMODE_NEAREST);
        m_width = width;
        m_height = height;
        m_enlargedBy = 0;
    }
    SDL_UpdateTexture(m_texture, nullptr, pixels, width * static_cast<int>(sizeof(uint32_t)));
    if (m_scaling == Scaling::Smooth) {
        // (blended all over: the picture as it is, fitted smoothly)
        SDL_SetTextureScaleMode(m_texture, SDL_SCALEMODE_LINEAR);
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
        SDL_RenderClear(m_renderer);
        SDL_RenderTexture(m_renderer, m_texture, nullptr, &where);
        SDL_RenderPresent(m_renderer);
        return;
    }
    SDL_SetTextureScaleMode(m_texture, SDL_SCALEMODE_NEAREST);
    // The picture's pixels are not square, so it never fits the window a whole number of times across. To keep
    // the pixels crisp and even it is first enlarged a whole number of times (each pixel a sharp block), and
    // that is then fitted to the window smoothly.
    const int by = std::clamp(static_cast<int>(where.h / height + 0.999f), 1, 8);
    if (by != m_enlargedBy) {
        if (m_enlarged) {
            SDL_DestroyTexture(m_enlarged);
        }
        m_enlarged = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_XBGR8888, SDL_TEXTUREACCESS_TARGET, width * by, height * by);
        if (m_enlarged) {
            SDL_SetTextureScaleMode(m_enlarged, SDL_SCALEMODE_LINEAR);
        }
        m_enlargedBy = by;
    }
    if (m_enlarged) {
        SDL_SetRenderTarget(m_renderer, m_enlarged);
        SDL_RenderTexture(m_renderer, m_texture, nullptr, nullptr);
        SDL_SetRenderTarget(m_renderer, nullptr);
    }
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    SDL_RenderTexture(m_renderer, m_enlarged ? m_enlarged : m_texture, nullptr, &where);
    SDL_RenderPresent(m_renderer);
}

bool Presenter::Render(const uint32_t *pixels, int width, int height, int outWidth, int outHeight, const SDL_FRect &where,
                       Scaling scaling, std::vector<uint32_t> &out, std::string &error) {
    Presenter p;
    p.m_scaling = scaling;
    if (!p.OpenGpu(nullptr, error)) {
        return false;
    }
    SDL_GPUDevice *device = p.m_device;
    SDL_GPUTextureCreateInfo texture = {};
    texture.type = SDL_GPU_TEXTURETYPE_2D;
    texture.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texture.width = static_cast<Uint32>(outWidth);
    texture.height = static_cast<Uint32>(outHeight);
    texture.layer_count_or_depth = 1;
    texture.num_levels = 1;
    SDL_GPUTexture *target = SDL_CreateGPUTexture(device, &texture);
    SDL_GPUTransferBufferCreateInfo transfer = {};
    transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    transfer.size = static_cast<Uint32>(outWidth * outHeight * 4);
    SDL_GPUTransferBuffer *download = SDL_CreateGPUTransferBuffer(device, &transfer);
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
    bool ok = target && download && commands && p.Upload(commands, pixels, width, height);
    if (ok) {
        p.Draw(commands, target, where);
        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
        SDL_GPUTextureRegion region = {};
        region.texture = target;
        region.w = static_cast<Uint32>(outWidth);
        region.h = static_cast<Uint32>(outHeight);
        region.d = 1;
        SDL_GPUTextureTransferInfo to = {};
        to.transfer_buffer = download;
        to.pixels_per_row = static_cast<Uint32>(outWidth);
        to.rows_per_layer = static_cast<Uint32>(outHeight);
        SDL_DownloadFromGPUTexture(copy, &region, &to);
        SDL_EndGPUCopyPass(copy);
        SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
        ok = fence && SDL_WaitForGPUFences(device, true, &fence, 1);
        if (fence) {
            SDL_ReleaseGPUFence(device, fence);
        }
        if (ok) {
            const void *from = SDL_MapGPUTransferBuffer(device, download, false);
            ok = from != nullptr;
            if (ok) {
                out.resize(static_cast<size_t>(outWidth) * outHeight);
                memcpy(out.data(), from, out.size() * 4);
                SDL_UnmapGPUTransferBuffer(device, download);
            }
        }
    } else if (commands) {
        SDL_SubmitGPUCommandBuffer(commands);
    }
    if (!ok) {
        error = SDL_GetError();
    }
    if (download) {
        SDL_ReleaseGPUTransferBuffer(device, download);
    }
    if (target) {
        SDL_ReleaseGPUTexture(device, target);
    }
    return ok;
}

} // namespace port
