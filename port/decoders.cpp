// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// (the decoders themselves are third_party/'s, public domain or MIT-0, compiled here unchanged)
#include "decoders.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#define STB_VORBIS_NO_PUSHDATA_API
#include "../third_party/stb_vorbis.c"
#define DR_FLAC_IMPLEMENTATION
#include "../third_party/dr_flac.h"
#define DR_MP3_IMPLEMENTATION
#include "../third_party/dr_mp3.h"
#define DR_WAV_IMPLEMENTATION
#include "../third_party/dr_wav.h"
// (stb_vorbis leaves these defined)
#undef L
#undef C
#undef R

namespace port {

namespace {
    std::filesystem::path PathOf(const std::string &path) {
        return std::filesystem::path(std::u8string(path.begin(), path.end()));
    }

    FILE *OpenRead(const std::string &path) {
#ifdef _WIN32
        return _wfopen(PathOf(path).wstring().c_str(), L"rb");
#else
        return fopen(path.c_str(), "rb");
#endif
    }

    // A loop tag ("LOOPSTART=12345"), as RPG Maker and many soundtrack packs write them
    void LoopTag(Decoder &decoder, const char *comment, size_t size) {
        std::string text(comment, size);
        const size_t eq = text.find('=');
        if (eq == std::string::npos) {
            return;
        }
        std::string key = text.substr(0, eq);
        for (char &c : key) {
            c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        }
        const long long value = atoll(text.c_str() + eq + 1);
        if (key == "LOOPSTART") {
            decoder.loopStart = value;
        } else if (key == "LOOPEND") {
            decoder.loopEnd = value;
        } else if (key == "LOOPLENGTH") {
            decoder.loopEnd = -2 - value; // (made an end once the start is known)
        }
    }
    void LoopLength(Decoder &decoder) {
        if (decoder.loopEnd <= -2) {
            decoder.loopEnd = decoder.loopStart >= 0 ? decoder.loopStart + (-2 - decoder.loopEnd) : -1;
        }
    }

    struct Vorbis final : Decoder {
        stb_vorbis *v = nullptr;
        ~Vorbis() override {
            stb_vorbis_close(v);
        }
        bool Seek(uint64_t sample) override {
            return stb_vorbis_seek(v, static_cast<unsigned>(sample)) != 0;
        }
        size_t Read(float *out, size_t samples) override {
            return static_cast<size_t>(stb_vorbis_get_samples_float_interleaved(v, channels, out, static_cast<int>(samples * channels)));
        }
    };

    struct Flac final : Decoder {
        drflac *f = nullptr;
        ~Flac() override {
            drflac_close(f);
        }
        bool Seek(uint64_t sample) override {
            return drflac_seek_to_pcm_frame(f, sample);
        }
        size_t Read(float *out, size_t samples) override {
            return static_cast<size_t>(drflac_read_pcm_frames_f32(f, samples, out));
        }
    };

    struct Mp3 final : Decoder {
        drmp3 m{};
        bool open = false;
        ~Mp3() override {
            if (open) {
                drmp3_uninit(&m);
            }
        }
        bool Seek(uint64_t sample) override {
            return drmp3_seek_to_pcm_frame(&m, sample);
        }
        size_t Read(float *out, size_t samples) override {
            return static_cast<size_t>(drmp3_read_pcm_frames_f32(&m, samples, out));
        }
    };

    struct Wav final : Decoder {
        drwav w{};
        bool open = false;
        ~Wav() override {
            if (open) {
                drwav_uninit(&w);
            }
        }
        bool Seek(uint64_t sample) override {
            return drwav_seek_to_pcm_frame(&w, sample);
        }
        size_t Read(float *out, size_t samples) override {
            return static_cast<size_t>(drwav_read_pcm_frames_f32(&w, samples, out));
        }
    };

    std::unique_ptr<Decoder> OpenVorbis(const std::string &path) {
        FILE *file = OpenRead(path);
        if (!file) {
            return nullptr;
        }
        int error = 0;
        auto d = std::make_unique<Vorbis>();
        d->v = stb_vorbis_open_file(file, 1, &error, nullptr);
        if (!d->v) {
            fclose(file);
            return nullptr;
        }
        const stb_vorbis_info info = stb_vorbis_get_info(d->v);
        d->rate = static_cast<int>(info.sample_rate);
        d->channels = info.channels;
        d->length = stb_vorbis_stream_length_in_samples(d->v);
        const stb_vorbis_comment comments = stb_vorbis_get_comment(d->v);
        for (int n = 0; n < comments.comment_list_length; n++) {
            LoopTag(*d, comments.comment_list[n], strlen(comments.comment_list[n]));
        }
        LoopLength(*d);
        return d;
    }

    std::unique_ptr<Decoder> OpenFlac(const std::string &path) {
        auto d = std::make_unique<Flac>();
        auto meta = [](void *self, drflac_metadata *block) {
            if (block->type == DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT) {
                drflac_vorbis_comment_iterator each;
                drflac_init_vorbis_comment_iterator(&each, block->data.vorbis_comment.commentCount, block->data.vorbis_comment.pComments);
                drflac_uint32 size = 0;
                while (const char *comment = drflac_next_vorbis_comment(&each, &size)) {
                    LoopTag(*static_cast<Decoder *>(self), comment, size);
                }
            }
        };
#ifdef _WIN32
        d->f = drflac_open_file_with_metadata_w(PathOf(path).wstring().c_str(), meta, d.get(), nullptr);
#else
        d->f = drflac_open_file_with_metadata(path.c_str(), meta, d.get(), nullptr);
#endif
        if (!d->f) {
            return nullptr;
        }
        d->rate = static_cast<int>(d->f->sampleRate);
        d->channels = d->f->channels;
        d->length = d->f->totalPCMFrameCount;
        LoopLength(*d);
        return d;
    }

    std::unique_ptr<Decoder> OpenMp3(const std::string &path) {
        auto d = std::make_unique<Mp3>();
#ifdef _WIN32
        d->open = drmp3_init_file_w(&d->m, PathOf(path).wstring().c_str(), nullptr);
#else
        d->open = drmp3_init_file(&d->m, path.c_str(), nullptr);
#endif
        if (!d->open) {
            return nullptr;
        }
        d->rate = static_cast<int>(d->m.sampleRate);
        d->channels = static_cast<int>(d->m.channels);
        return d;
    }

    std::unique_ptr<Decoder> OpenWav(const std::string &path) {
        auto d = std::make_unique<Wav>();
#ifdef _WIN32
        d->open = drwav_init_file_w(&d->w, PathOf(path).wstring().c_str(), nullptr);
#else
        d->open = drwav_init_file(&d->w, path.c_str(), nullptr);
#endif
        if (!d->open) {
            return nullptr;
        }
        d->rate = static_cast<int>(d->w.sampleRate);
        d->channels = d->w.channels;
        d->length = d->w.totalPCMFrameCount;
        return d;
    }
} // namespace

std::unique_ptr<Decoder> OpenDecoder(const std::string &path) {
    unsigned char head[12] = {};
    if (FILE *file = OpenRead(path)) {
        const size_t got = fread(head, 1, sizeof(head), file);
        fclose(file);
        if (got < 4) {
            return nullptr;
        }
    } else {
        return nullptr;
    }
    std::unique_ptr<Decoder> d;
    if (memcmp(head, "OggS", 4) == 0) {
        d = OpenVorbis(path);
    } else if (memcmp(head, "fLaC", 4) == 0) {
        d = OpenFlac(path);
    } else if (memcmp(head, "RIFF", 4) == 0 || memcmp(head, "RF64", 4) == 0) {
        d = OpenWav(path);
    } else {
        d = OpenMp3(path); // (an MP3 starts with a frame or an ID3 tag)
    }
    if (d && (d->rate <= 0 || d->channels <= 0)) {
        d.reset();
    }
    return d;
}

} // namespace port
