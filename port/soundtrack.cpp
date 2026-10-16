// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#include "soundtrack.h"

#include "../machine/machine.h"
#include "../runtime/game.h"
#include "decoders.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>

namespace port {

using cps2recomp::Soundtrack;
using cps2recomp::TheGame;

namespace {
    const cps2recomp::Song *SongNamed(uint16_t number) {
        for (const cps2recomp::Song &song : TheGame().songs) {
            if (song.number == number) {
                return &song;
            }
        }
        return nullptr;
    }

    std::string Lower(std::string text) {
        for (char &c : text) {
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }
        return text;
    }

    std::string Trim(const std::string &text) {
        const size_t from = text.find_first_not_of(" \t\r\n");
        if (from == std::string::npos) {
            return "";
        }
        return text.substr(from, text.find_last_not_of(" \t\r\n") - from + 1);
    }

    std::string Hex(uint16_t number) {
        char text[8];
        snprintf(text, sizeof(text), "%02X", number);
        return text;
    }

    // The names a song goes by in the folder: its name, and its number in hexadecimal (2a, 02a, 0x2a)
    std::vector<std::string> NamesOf(uint16_t number) {
        std::vector<std::string> names;
        if (const cps2recomp::Song *song = SongNamed(number)) {
            names.push_back(Lower(song->name));
        }
        const std::string hex = Lower(Hex(number));
        names.push_back(hex);
        names.push_back("0" + hex);
        names.push_back("0x" + hex);
        return names;
    }

    // "1:02.5", "62.5": seconds
    double Seconds(const std::string &text) {
        double seconds = 0;
        size_t at = 0;
        while (true) {
            const size_t colon = text.find(':', at);
            const double part = atof(text.substr(at, colon == std::string::npos ? std::string::npos : colon - at).c_str());
            seconds = seconds * 60 + part;
            if (colon == std::string::npos) {
                break;
            }
            at = colon + 1;
        }
        return seconds;
    }

    // soundtrack.ini: [section] lines and key = value lines. A line that starts with ';' or '#' is a remark, and so is
    // the rest of a line from one, but for a file's name (which may have them)
    std::map<std::string, std::map<std::string, std::string>> ReadIni(const std::filesystem::path &path) {
        std::map<std::string, std::map<std::string, std::string>> sections;
        std::ifstream file(path);
        std::string section, line;
        while (std::getline(file, line)) {
            line = Trim(line);
            if (line.empty() || line[0] == ';' || line[0] == '#') {
                continue;
            }
            if (line.front() == '[') {
                const size_t end = line.find(']');
                if (end != std::string::npos) {
                    section = Lower(Trim(line.substr(1, end - 1)));
                }
                continue;
            }
            const size_t eq = line.find('=');
            if (eq == std::string::npos) {
                continue;
            }
            const std::string key = Lower(Trim(line.substr(0, eq)));
            std::string value = line.substr(eq + 1);
            if (key != "file") {
                value = value.substr(0, value.find_first_of(";#"));
            }
            sections[section][key] = Trim(value);
        }
        return sections;
    }

    constexpr size_t kChunk = 1024;
} // namespace

SoundtrackPlayer::SoundtrackPlayer() = default;
SoundtrackPlayer::~SoundtrackPlayer() = default;

int SoundtrackPlayer::Open(const std::string &folder, Soundtrack &tap) {
    m_files.clear();
    m_decoder.reset();
    m_song = -1;
    std::error_code error;
    const std::filesystem::path dir = TextPath(folder);
    // The folder's sound files by their names without the ending, in small letters
    std::map<std::string, std::string> stems;
    for (std::filesystem::directory_iterator each(dir, error); !error && each != std::filesystem::directory_iterator(); each.increment(error)) {
        const std::string ending = Lower(PathText(each->path().extension()));
        if (ending == ".ogg" || ending == ".oga" || ending == ".flac" || ending == ".mp3" || ending == ".wav") {
            stems.emplace(Lower(PathText(each->path().stem())), PathText(each->path()));
        }
    }
    auto ini = ReadIni(dir / "soundtrack.ini");
    if (ini[""].count("volume")) {
        volume = std::clamp(atoi(ini[""]["volume"].c_str()), 0, 400);
    }
    int found = 0;
    std::vector<bool> replaced;
    for (uint16_t number : tap.MusicSongs()) {
        File file;
        const cps2recomp::Song *song = SongNamed(number);
        file.once = song && song->once;
        const std::map<std::string, std::string> *own = nullptr;
        for (const std::string &name : NamesOf(number)) {
            if (!own && ini.count(name)) {
                own = &ini[name];
            }
            if (file.path.empty() && stems.count(name)) {
                file.path = stems[name];
            }
        }
        if (own) {
            if (own->count("file")) {
                const std::filesystem::path named = TextPath(own->at("file"));
                file.path = PathText(named.is_absolute() ? named : dir / named);
            }
            if (own->count("volume")) {
                file.volume = std::clamp(atoi(own->at("volume").c_str()), 0, 400);
            }
            if (own->count("loop")) {
                const std::string loop = Lower(own->at("loop"));
                const size_t dash = loop.find('-');
                if (loop == "no" || loop == "off" || loop == "once") {
                    file.once = true;
                } else if (loop == "yes" || loop == "on") {
                    file.once = false;
                } else if (dash != std::string::npos) {
                    file.once = false;
                    file.loopStart = Seconds(Trim(loop.substr(0, dash)));
                    file.loopEnd = Seconds(Trim(loop.substr(dash + 1)));
                    if (file.loopEnd <= file.loopStart) {
                        file.loopEnd = -1;
                    }
                }
            }
        }
        if (file.path.empty() || !std::filesystem::is_regular_file(TextPath(file.path), error)) {
            continue;
        }
        if (m_files.size() <= number) {
            m_files.resize(number + 1u);
            replaced.resize(number + 1u);
        }
        m_files[number] = file;
        replaced[number] = true;
        found++;
    }
    if (!found) {
        m_files.clear();
    }
    tap.Replace(std::move(replaced));
    return found;
}

std::string SoundtrackPlayer::FileFor(uint16_t song) const {
    return song < m_files.size() ? m_files[song].path : std::string();
}

void SoundtrackPlayer::Start(int32_t song) {
    m_song = song;
    m_decoder.reset();
    m_made = m_at = 0;
    m_ended = false;
    m_gain = 0;
    if (song < 0 || static_cast<size_t>(song) >= m_files.size() || m_files[song].path.empty()) {
        return;
    }
    const File &file = m_files[song];
    m_decoder = OpenDecoder(file.path);
    if (!m_decoder) {
        fprintf(stderr, "soundtrack: %s is not a sound file this reads (Ogg Vorbis, FLAC, MP3, WAV)\n", file.path.c_str());
        return;
    }
    m_rate = m_decoder->rate;
    m_once = file.once;
    m_loopStart = 0;
    m_loopEnd = 0;
    if (file.loopStart >= 0 && file.loopEnd > file.loopStart) {
        m_loopStart = static_cast<uint64_t>(file.loopStart * m_rate);
        m_loopEnd = static_cast<uint64_t>(file.loopEnd * m_rate);
    } else if (m_decoder->loopStart >= 0) {
        m_loopStart = static_cast<uint64_t>(m_decoder->loopStart);
        m_loopEnd = m_decoder->loopEnd > m_decoder->loopStart ? static_cast<uint64_t>(m_decoder->loopEnd) : 0;
    }
    if (m_decoder->length && (m_loopEnd == 0 || m_loopEnd > m_decoder->length)) {
        m_loopEnd = m_decoder->length;
    }
    if (m_loopEnd && m_loopStart >= m_loopEnd) {
        m_loopStart = 0;
    }
}

uint64_t SoundtrackPlayer::Place(uint64_t made) const {
    if (m_once || m_loopEnd == 0 || made < m_loopEnd) {
        return made;
    }
    return m_loopStart + (made - m_loopStart) % (m_loopEnd - m_loopStart);
}

void SoundtrackPlayer::SeekTo(uint64_t made) {
    m_made = made;
    m_at = Place(made);
    m_ended = (m_once && m_loopEnd && made >= m_loopEnd) || !m_decoder->Seek(m_at);
}

void SoundtrackPlayer::Make(const Soundtrack::State &state, bool heard, std::vector<float> &out) {
    out.clear();
    if (state.song != m_song || state.serial != m_serial) {
        m_serial = state.serial;
        Start(state.song);
    }
    if (!m_decoder) {
        return;
    }
    // Where the game is in the song: its frames since the start, in samples
    const uint64_t due = static_cast<uint64_t>(state.frames * (static_cast<double>(m_rate) / cps2recomp::Machine::kFramesPerSecond));
    if (!heard) {
        m_made = UINT64_MAX; // (the next call starts where the game is then)
        return;
    }
    // Far from where the samples given before ended (a state loaded, a fast-forward, the menu): there
    if (m_made == UINT64_MAX || due < m_made || due - m_made > static_cast<uint64_t>(m_rate / 4)) {
        SeekTo(due);
        m_gain = 0;
        return;
    }
    size_t wanted = static_cast<size_t>(due - m_made);
    m_made = due;
    const float target = static_cast<float>(volume / 100.0 * m_files[m_song].volume / 100.0 * (state.level / double(Soundtrack::kFull)) *
                                            (state.allLevel / double(Soundtrack::kFull)));
    const size_t total = wanted;
    const float from = m_gain;
    float buffer[kChunk * 8];
    const int channels = std::min(m_decoder->channels, 8);
    out.reserve(total * 2);
    while (wanted && !m_ended) {
        size_t ask = std::min(wanted, kChunk);
        if (!m_once && m_loopEnd && m_at < m_loopEnd) {
            ask = std::min<uint64_t>(ask, m_loopEnd - m_at);
        }
        const size_t got = channels == m_decoder->channels ? m_decoder->Read(buffer, ask) : 0;
        for (size_t n = 0; n < got; n++) {
            const float g = from + (target - from) * static_cast<float>(out.size() / 2 + 1) / static_cast<float>(total);
            const float left = buffer[n * channels];
            const float right = channels > 1 ? buffer[n * channels + 1] : left;
            out.push_back(left * g);
            out.push_back(right * g);
        }
        m_at += got;
        wanted -= got;
        if (got < ask) { // the end of the file
            if (m_loopEnd == 0 || m_loopEnd > m_at) {
                m_loopEnd = m_at;
            }
            if (m_once || m_loopEnd <= m_loopStart || (got == 0 && m_at == m_loopStart)) {
                m_ended = true;
                break;
            }
            m_at = m_loopStart;
            m_ended = !m_decoder->Seek(m_at);
        } else if (!m_once && m_loopEnd && m_at >= m_loopEnd) {
            m_at = m_loopStart;
            m_ended = !m_decoder->Seek(m_at);
        }
    }
    m_gain = target;
}

std::string SongList(const Soundtrack &tap) {
    std::string text;
    for (uint16_t number : tap.MusicSongs()) {
        const cps2recomp::Song *song = SongNamed(number);
        char line[256];
        snprintf(line, sizeof(line), "%-22s %-4s %s%s\n", song ? song->name : "", Hex(number).c_str(), song ? song->title : "",
                 song && song->once ? " (played once)" : "");
        text += line;
    }
    return text;
}

} // namespace port
