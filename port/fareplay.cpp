// SPDX-License-Identifier: GPL-3.0-only
// (this fork, ForkAde's native-replays) The .fareplay file: replay.h.
#include "fareplay.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <cmath>

#include <zlib.h>

namespace port {
namespace {

constexpr char kMagic[8] = {'F', 'A', 'R', 'E', 'P', 'L', 'A', 'Y'};
constexpr uint32_t kHeaderMost = 64 * 1024;          // (a header is a few hundred bytes)
constexpr uint32_t kPartMost = 8u * 1024u * 1024u;  // Neo Geo: bounded state/input parts, not ROM buffers.

void Put16(std::vector<uint8_t> &out, uint16_t v) { out.push_back(v & 0xff); out.push_back(v >> 8); }
void Put32(std::vector<uint8_t> &out, uint32_t v) { for (int i = 0; i < 4; i++) out.push_back((v >> (8 * i)) & 0xff); }
void Put64(std::vector<uint8_t> &out, uint64_t v) { for (int i = 0; i < 8; i++) out.push_back((v >> (8 * i)) & 0xff); }
void PutVarint(std::vector<uint8_t> &out, uint64_t v) {
    while (v >= 0x80) {
        out.push_back(static_cast<uint8_t>(v | 0x80));
        v >>= 7;
    }
    out.push_back(static_cast<uint8_t>(v));
}

std::vector<uint8_t> Compress(const std::vector<uint8_t> &raw) {
    uLongf size = compressBound(static_cast<uLong>(raw.size()));
    std::vector<uint8_t> out(size);
    if (compress2(out.data(), &size, raw.data(), static_cast<uLong>(raw.size()), 6) != Z_OK) {
        return {};
    }
    out.resize(size);
    return out;
}
bool Uncompress(const uint8_t *data, size_t size, uint32_t raw, std::vector<uint8_t> &out) {
    out.resize(raw);
    uLongf got = raw;
    if (raw == 0) {
        return size == 0 || uncompress(out.data(), &got, data, static_cast<uLong>(size)) == Z_OK;
    }
    return uncompress(out.data(), &got, data, static_cast<uLong>(size)) == Z_OK && got == raw;
}

// (a UTF-8 path: a player's folder or name may have accents)
FILE *OpenFile(const std::string &path, const char *mode) {
#ifdef _WIN32
    const std::filesystem::path p(std::u8string(reinterpret_cast<const char8_t *>(path.c_str())));
    const std::wstring wide(mode, mode + std::strlen(mode));
    return _wfopen(p.c_str(), wide.c_str());
#else
    return std::fopen(path.c_str(), mode);
#endif
}

// Reading the file's bytes in order, every read checked against the end
struct Reader {
    const std::vector<uint8_t> &b;
    size_t at = 0;
    bool ok = true;
    bool Need(size_t n) { ok = ok && at + n <= b.size() && at + n >= at; return ok; }
    uint16_t U16() { if (!Need(2)) return 0; uint16_t v = b[at] | (b[at + 1] << 8); at += 2; return v; }
    uint32_t U32() { if (!Need(4)) return 0; uint32_t v = 0; for (int i = 0; i < 4; i++) v |= static_cast<uint32_t>(b[at + i]) << (8 * i); at += 4; return v; }
    uint64_t U64() { if (!Need(8)) return 0; uint64_t v = 0; for (int i = 0; i < 8; i++) v |= static_cast<uint64_t>(b[at + i]) << (8 * i); at += 8; return v; }
    const uint8_t *Bytes(size_t n) { if (!Need(n)) return nullptr; const uint8_t *p = b.data() + at; at += n; return p; }
};

// --- The header's JSON: written from Info, and read back with a small reader for what it holds
std::string Quoted(const std::string &s) {
    std::string out = "\"";
    for (const unsigned char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c < 0x20) {
            char hex[8];
            std::snprintf(hex, sizeof(hex), "\\u%04x", c);
            out += hex;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out + "\"";
}

struct Json {
    const std::string &s;
    size_t at = 0;
    bool ok = true;
    void Space() { while (at < s.size() && (s[at] == ' ' || s[at] == '\n' || s[at] == '\r' || s[at] == '\t')) at++; }
    bool Eat(char c) { Space(); if (at < s.size() && s[at] == c) { at++; return true; } return false; }
    std::string String() {
        std::string out;
        if (!Eat('"')) { ok = false; return out; }
        while (at < s.size() && s[at] != '"') {
            char c = s[at++];
            if (c == '\\' && at < s.size()) {
                const char e = s[at++];
                if (e == 'u' && at + 4 <= s.size()) {
                    const unsigned v = static_cast<unsigned>(std::strtoul(s.substr(at, 4).c_str(), nullptr, 16));
                    at += 4;
                    if (v < 0x80) out += static_cast<char>(v);
                    else if (v < 0x800) { out += static_cast<char>(0xC0 | (v >> 6)); out += static_cast<char>(0x80 | (v & 0x3F)); }
                    else { out += static_cast<char>(0xE0 | (v >> 12)); out += static_cast<char>(0x80 | ((v >> 6) & 0x3F)); out += static_cast<char>(0x80 | (v & 0x3F)); }
                    continue;
                }
                c = e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e;
            }
            out += c;
        }
        if (!Eat('"')) ok = false;
        return out;
    }
    double Number() {
        Space();
        const size_t from = at;
        while (at < s.size() && (std::strchr("+-.eE0123456789", s[at]) != nullptr)) at++;
        if (from == at) { ok = false; return 0; }
        const double value=std::strtod(s.substr(from, at - from).c_str(), nullptr);
        if(!std::isfinite(value)||std::fabs(value)>1000000000.0){ok=false;return 0;}
        return value;
    }
    // (any value, skipped)
    void Skip(unsigned depth=0) {
        if(depth>16){ok=false;return;}
        Space();
        if (at >= s.size()) { ok = false; return; }
        const char c = s[at];
        if (c == '"') { String(); return; }
        if (c == '{' || c == '[') {
            const char end = c == '{' ? '}' : ']';
            at++;
            if (Eat(end)) return;
            do {
                if (c == '{') { String(); if (!Eat(':')) { ok = false; return; } }
                Skip(depth+1);
            } while (ok && Eat(','));
            if (!Eat(end)) ok = false;
            return;
        }
        if (s.compare(at, 4, "true") == 0 || s.compare(at, 4, "null") == 0) { at += 4; return; }
        if (s.compare(at, 5, "false") == 0) { at += 5; return; }
        Number();
    }
};

}  // namespace

std::string Replay::HeaderJson(const Info &info) {
    std::string j = "{\"format\":2,\"backend\":" + Quoted(info.backend) + ",\"game\":" + Quoted(info.game) + ",\"title\":" + Quoted(info.title) +
                    ",\"identity\":" + Quoted(info.identity) + ",\"build\":" + Quoted(info.build) + ",\"version\":" + Quoted(info.version) +
                    ",\"forkade\":" + Quoted(info.forkade) + ",\"inputs\":{\"players\":" + std::to_string(info.inputPlayers) +
                    ",\"bytes\":" + std::to_string(info.inputBytes) + "},\"mode\":" + Quoted(info.mode);
    if (!info.role.empty()) {
        j += ",\"role\":" + Quoted(info.role);
    }
    j += ",\"players\":[";
    for (size_t i = 0; i < info.players.size(); i++) {
        j += (i ? "," : "") + Quoted(info.players[i]);
    }
    j += "],\"started\":" + Quoted(info.started) + ",\"frames\":" + std::to_string(info.frames) +
         ",\"features\":" + Quoted(info.features) + ",\"online\":" + (info.online ? "true" : "false");
    if (info.result) {
        j += ",\"result\":{\"wins\":[" + std::to_string(info.wins[0]) + "," + std::to_string(info.wins[1]) + "]}";
    }
    return j + "}";
}

bool Replay::ParseHeader(const std::string &json, Info &info) {
    Json r{json};
    info = Info{};
    if (!r.Eat('{')) {
        return false;
    }
    if (r.Eat('}')) {
        return true;
    }
    do {
        const std::string key = r.String();
        if (!r.ok || !r.Eat(':')) {
            return false;
        }
        if (key == "game") info.game = r.String();
        else if (key == "title") info.title = r.String();
        else if (key == "build") info.build = r.String();
        else if (key == "backend") info.backend = r.String();
        else if (key == "identity") info.identity = r.String();
        else if (key == "version") info.version = r.String();
        else if (key == "inputs") {
            if (!r.Eat('{')) return false;
            if (!r.Eat('}')) {
                do {
                    const std::string inner = r.String();
                    if (!r.Eat(':')) return false;
                    if (inner == "players") info.inputPlayers = static_cast<int>(r.Number());
                    else if (inner == "bytes") info.inputBytes = static_cast<int>(r.Number());
                    else r.Skip();
                } while (r.ok && r.Eat(','));
                if (!r.Eat('}')) return false;
            }
        }
        else if (key == "forkade") info.forkade = r.String();
        else if (key == "mode") info.mode = r.String();
        else if (key == "role") info.role = r.String();
        else if (key == "started") info.started = r.String();
        else if (key == "frames") info.frames = static_cast<uint64_t>(r.Number());
        else if (key == "features") info.features = r.String();
        else if (key == "online") {
            r.Space();
            info.online = json.compare(r.at, 4, "true") == 0;
            r.Skip();
        }
        else if (key == "players") {
            if (!r.Eat('[')) return false;
            if (!r.Eat(']')) {
                do { info.players.push_back(r.String()); } while (r.ok && r.Eat(','));
                if (!r.Eat(']')) return false;
            }
        } else if (key == "result") {
            if (!r.Eat('{')) return false;
            if (!r.Eat('}')) {
                do {
                    const std::string inner = r.String();
                    if (!r.Eat(':')) return false;
                    if (inner == "wins" && r.Eat('[')) {
                        info.wins[0] = static_cast<int>(r.Number());
                        if (r.Eat(',')) info.wins[1] = static_cast<int>(r.Number());
                        if (!r.Eat(']')) return false;
                        info.result = true;
                    } else {
                        r.Skip();
                    }
                } while (r.ok && r.Eat(','));
                if (!r.Eat('}')) return false;
            }
        } else {
            r.Skip();  // (a later version's field: kept readable)
        }
    } while (r.ok && r.Eat(','));
    return r.ok && r.Eat('}');
}

std::vector<uint8_t> Replay::Bytes() const {
    std::vector<uint8_t> out(kMagic, kMagic + 8);
    Put16(out, kFormat);
    Put16(out, 0); // (format 2: the backend is named in the header)
    Info written = info;
    written.inputPlayers = players;
    if (written.inputBytes != 4) written.inputBytes = 2;
    const std::string header = HeaderJson(written);
    Put32(out, static_cast<uint32_t>(header.size()));
    out.insert(out.end(), header.begin(), header.end());
    const std::vector<uint8_t> packedState = Compress(state);
    Put32(out, static_cast<uint32_t>(state.size()));
    Put32(out, static_cast<uint32_t>(packedState.size()));
    out.insert(out.end(), packedState.begin(), packedState.end());
    std::vector<uint8_t> raw;
    raw.push_back(static_cast<uint8_t>(players));
    uint64_t last = 0;
    for (const auto &[frame, pads] : inputs) {
        PutVarint(raw, frame - last);
        last = frame;
        for (int n = 0; n < players; n++) {
            Put16(raw, pads[static_cast<size_t>(n)]);
            if (written.inputBytes == 4) Put16(raw, 0);
        }
    }
    const std::vector<uint8_t> packedInputs = Compress(raw);
    Put32(out, static_cast<uint32_t>(raw.size()));
    Put32(out, static_cast<uint32_t>(packedInputs.size()));
    out.insert(out.end(), packedInputs.begin(), packedInputs.end());
    Put32(out, static_cast<uint32_t>(checks.size()));
    for (const auto &[frame, hash] : checks) {
        Put64(out, frame);
        Put32(out, hash);
    }
    return out;
}

bool Replay::Read(const std::vector<uint8_t> &bytes, std::string &error) {
    Reader r{bytes};
    const uint8_t *magic = r.Bytes(8);
    if (!magic || std::memcmp(magic, kMagic, 8) != 0) {
        error = "not a ForkAde replay";
        return false;
    }
    const uint16_t format = r.U16();
    const uint16_t backendField = r.U16();
    if (!r.ok) {
        error = "the replay is cut short";
        return false;
    }
    if (format > kFormat || format == 0) {
        error = "a replay of a newer ForkAde (format " + std::to_string(format) + "): update ForkAde to play it";
        return false;
    }
    const uint32_t headerSize = r.U32();
    if (r.ok && headerSize > kHeaderMost) {
        error = "the replay's header is too long";
        return false;
    }
    const uint8_t *header = r.Bytes(headerSize);
    if (!header) {
        error = "the replay is cut short";
        return false;
    }
    if (!ParseHeader(std::string(reinterpret_cast<const char *>(header), headerSize), info)) {
        error = "the replay's header can't be read";
        return false;
    }
    info.format = format;
    if (format == 1) {
        // (0.1.2's: the CPS games, two bytes a player, no identity)
        if (backendField != kBackendCps) {
            error = "a replay of another kind of game (backend " + std::to_string(backendField) + ")";
            return false;
        }
        info.backend = "cps";
        info.identity.clear();
        info.inputBytes = 2;
    } else if (backendField != 0 || info.backend.empty() || (info.inputBytes != 2 && info.inputBytes != 4)) {
        error = "the replay's header can't be read";
        return false;
    }
    backend = info.backend == "cps" ? kBackendCps : 0;
    if(!info.frames||info.frames>72000){error="the replay duration is outside this backend's limits";return false;}
    // (the state, then the inputs: a raw length, a compressed length, the zlib bytes)
    auto part = [&](std::vector<uint8_t> &out) {
        const uint32_t raw = r.U32(), packed = r.U32();
        if (!r.ok) {
            error = "the replay is cut short";
            return false;
        }
        if (raw > kPartMost || packed > kPartMost) {
            error = "the replay's data is too large";
            return false;
        }
        const uint8_t *data = r.Bytes(packed);
        if (!data) {
            error = "the replay is cut short";
            return false;
        }
        if (!Uncompress(data, packed, raw, out)) {
            error = "the replay's data is damaged";
            return false;
        }
        return true;
    };
    std::vector<uint8_t> raw;
    if (!part(state) || !part(raw)) {
        return false;
    }
    inputs.clear();
    if (raw.empty() || raw[0] < 1 || raw[0] > 4) {
        error = "the replay's inputs are damaged";
        return false;
    }
    players = raw[0];
    uint64_t frame = 0;
    for (size_t at = 1; at < raw.size();) {
        uint64_t delta = 0;
        int shift = 0;
        bool ended=false;
        while (at < raw.size() && shift < 64) {
            const uint8_t b = raw[at++];
            if(shift==63&&(b&0xfe)){error="the replay input delta overflows";return false;}
            delta |= static_cast<uint64_t>(b & 0x7f) << shift;
            shift += 7;
            if (!(b & 0x80)) {
                ended=true;
                break;
            }
        }
        if(!ended||frame>=info.frames||delta>=info.frames-frame||(!inputs.empty()&&!delta)){error="the replay input positions are invalid";return false;}
        const size_t width = static_cast<size_t>(info.inputBytes);
        if (at + width * static_cast<size_t>(players) > raw.size()) {
            error = "the replay's inputs are damaged";
            return false;
        }
        frame += delta;
        Pads pads{};
        for (int n = 0; n < players; n++) {
            pads[static_cast<size_t>(n)] = static_cast<uint16_t>(raw[at] | (raw[at + 1] << 8));
            // (a 4-byte input whose upper half is used is not this game's: refused, never cut)
            if (width == 4 && (raw[at + 2] | raw[at + 3])) {
                error = "the replay's inputs are wider than this game's";
                return false;
            }
            at += width;
        }
        inputs[frame] = pads;
    }
    const uint32_t count = r.U32();
    if (!r.ok || count>1200 || static_cast<uint64_t>(count) * 12 > bytes.size()) {
        error = "the replay is cut short";
        return false;
    }
    checks.clear();
    for (uint32_t i = 0; i < count; i++) {
        const uint64_t at = r.U64();
        const uint32_t hash = r.U32();
        checks.emplace_back(at, hash);
    }
    if (!r.ok || r.at != bytes.size()) {
        error = "the replay is cut short or has trailing data";
        return false;
    }
    return true;
}

bool Replay::Save(const std::string &path, std::string &error) const {
    const std::vector<uint8_t> bytes = Bytes();
    FILE *f = OpenFile(path, "wb");
    if (!f) {
        error = "can't write " + path;
        return false;
    }
    const bool written = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    const bool closed = std::fclose(f) == 0;
    if (!written || !closed) {
        error = "can't write all of " + path + " (is the disk full?)";
        return false;
    }
    return true;
}

bool Replay::Load(const std::string &path, std::string &error) {
    FILE *f = OpenFile(path, "rb");
    if (!f) {
        error = "can't read " + path;
        return false;
    }
    std::vector<uint8_t> bytes;
    uint8_t chunk[65536];
    size_t got;
    while ((got = std::fread(chunk, 1, sizeof(chunk), f)) > 0 && bytes.size() < kPartMost * 2) {
        bytes.insert(bytes.end(), chunk, chunk + got);
    }
    std::fclose(f);
    return Read(bytes, error);
}

bool Replay::ReadInfo(const std::string &path, Info &info, std::string &error) {
    FILE *f = OpenFile(path, "rb");
    if (!f) {
        error = "can't read " + path;
        return false;
    }
    std::vector<uint8_t> start(16);
    const size_t got = std::fread(start.data(), 1, start.size(), f);
    if (got < 16 || std::memcmp(start.data(), kMagic, 8) != 0) {
        std::fclose(f);
        error = "not a ForkAde replay";
        return false;
    }
    const uint16_t format = static_cast<uint16_t>(start[8] | (start[9] << 8));
    const uint32_t size = start[12] | (start[13] << 8) | (start[14] << 16) | (static_cast<uint32_t>(start[15]) << 24);
    if (format == 0 || format > kFormat || size > kHeaderMost) { // (format 1 and 2: the header is in the same place)
        std::fclose(f);
        error = format > kFormat ? "a replay of a newer ForkAde" : "the replay's header can't be read";
        return false;
    }
    std::string header(size, '\0');
    const bool read = std::fread(header.data(), 1, size, f) == size;
    std::fclose(f);
    if (!read || !ParseHeader(header, info)) {
        error = "the replay's header can't be read";
        return false;
    }
    return true;
}

}  // namespace port
