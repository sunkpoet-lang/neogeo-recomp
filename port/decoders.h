// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Sound files read as samples, for a custom soundtrack (soundtrack.h): Ogg Vorbis, FLAC, MP3 and WAV, by the
// single-file decoders in third_party/ (stb_vorbis, dr_flac, dr_mp3, dr_wav; THIRD_PARTY_NOTICES.md).
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace port {

struct Decoder {
    virtual ~Decoder() = default;
    int rate = 0, channels = 0;
    uint64_t length = 0;                // samples a channel (0: not known until the end is read)
    int64_t loopStart = -1, loopEnd = -1; // from the file's tags (LOOPSTART, LOOPEND or LOOPLENGTH), in samples
    virtual bool Seek(uint64_t sample) = 0;
    // Up to `samples` a channel, `channels` to a sample, into `out`; fewer at the end of the file
    virtual size_t Read(float *out, size_t samples) = 0;
};

// The file's decoder by what it starts with (null: not a file it reads)
std::unique_ptr<Decoder> OpenDecoder(const std::string &path);

} // namespace port
