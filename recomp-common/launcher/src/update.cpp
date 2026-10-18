// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#include "update.h"

#include <zlib.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace fs = std::filesystem;

namespace launcher {
namespace {

// ---- SHA-256 (FIPS 180-4) --------------------------------------------------------------------

class Sha256 {
public:
    void add(const void* data, size_t size) {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        length_ += size;
        while (size > 0) {
            size_t take = std::min(size, sizeof block_ - used_);
            std::memcpy(block_ + used_, bytes, take);
            used_ += take, bytes += take, size -= take;
            if (used_ == sizeof block_) {
                compress(block_);
                used_ = 0;
            }
        }
    }
    std::string hex() {
        uint64_t bits = length_ * 8;
        uint8_t pad = 0x80;
        add(&pad, 1);
        pad = 0;
        while (used_ != 56) {
            add(&pad, 1);
        }
        uint8_t tail[8];
        for (int i = 0; i < 8; i++) {
            tail[i] = static_cast<uint8_t>(bits >> (56 - 8 * i));
        }
        add(tail, 8);
        static const char kDigits[] = "0123456789abcdef";
        std::string out;
        for (uint32_t word : h_) {
            for (int shift = 28; shift >= 0; shift -= 4) {
                out += kDigits[(word >> shift) & 15];
            }
        }
        return out;
    }

private:
    static uint32_t rotate(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
    void compress(const uint8_t* p) {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
            0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
            0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
            0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
            0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
            0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = uint32_t(p[4 * i]) << 24 | uint32_t(p[4 * i + 1]) << 16 | uint32_t(p[4 * i + 2]) << 8 | p[4 * i + 3];
        }
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = rotate(w[i - 15], 7) ^ rotate(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotate(w[i - 2], 17) ^ rotate(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = h + (rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            uint32_t t2 = (rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            h = g, g = f, f = e, e = d + t1, d = c, c = b, b = a, a = t1 + t2;
        }
        h_[0] += a, h_[1] += b, h_[2] += c, h_[3] += d, h_[4] += e, h_[5] += f, h_[6] += g, h_[7] += h;
    }
    uint32_t h_[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint8_t block_[64] = {};
    size_t used_ = 0;
    uint64_t length_ = 0;
};

// ---- JSON, as much as the list of releases needs -------------------------------------------------

struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    bool boolean = false;
    std::string text;  // a string's, or a number as written
    std::vector<Json> items;
    std::vector<std::pair<std::string, Json>> fields;

    const Json& operator[](const char* key) const {
        static const Json kNone;
        for (const auto& [name, value] : fields) {
            if (name == key) {
                return value;
            }
        }
        return kNone;
    }
};

class JsonReader {
public:
    explicit JsonReader(const std::string& text) : at_(text.data()), end_(text.data() + text.size()) {}
    bool read(Json& out) {
        out = value(0);
        space();
        return ok_ && at_ == end_;
    }

private:
    void space() {
        while (at_ < end_ && (*at_ == ' ' || *at_ == '\n' || *at_ == '\r' || *at_ == '\t')) {
            at_++;
        }
    }
    bool take(char c) {
        space();
        if (at_ < end_ && *at_ == c) {
            at_++;
            return true;
        }
        return false;
    }
    bool word(const char* w) {
        size_t n = std::strlen(w);
        if (static_cast<size_t>(end_ - at_) >= n && std::memcmp(at_, w, n) == 0) {
            at_ += n;
            return true;
        }
        return ok_ = false;
    }
    static void utf8(std::string& out, uint32_t c) {
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xc0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3f));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xe0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (c & 0x3f));
        } else {
            out += static_cast<char>(0xf0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3f));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (c & 0x3f));
        }
    }
    uint32_t hex4() {
        if (end_ - at_ < 4) {
            ok_ = false;
            return 0;
        }
        uint32_t c = 0;
        for (int i = 0; i < 4; i++, at_++) {
            char h = *at_;
            c = c * 16 + (h >= '0' && h <= '9' ? h - '0' : h >= 'a' && h <= 'f' ? h - 'a' + 10 : h >= 'A' && h <= 'F' ? h - 'A' + 10 : (ok_ = false, 0));
        }
        return c;
    }
    std::string string() {
        std::string out;
        if (!take('"')) {
            ok_ = false;
            return out;
        }
        while (ok_ && at_ < end_ && *at_ != '"') {
            char c = *at_++;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (at_ == end_) {
                break;
            }
            switch (char e = *at_++) {
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                uint32_t code = hex4();
                if (code >= 0xd800 && code < 0xdc00 && end_ - at_ >= 2 && at_[0] == '\\' && at_[1] == 'u') {
                    at_ += 2;
                    code = 0x10000 + ((code - 0xd800) << 10) + (hex4() - 0xdc00);
                }
                utf8(out, code);
                break;
            }
            default: out += e; break;
            }
        }
        if (at_ == end_) {
            ok_ = false;
        } else {
            at_++;
        }
        return out;
    }
    Json value(int depth) {
        Json v;
        space();
        if (!ok_ || at_ == end_ || depth > 64) {
            ok_ = false;
            return v;
        }
        char c = *at_;
        if (c == '{') {
            at_++;
            v.type = Json::Type::Object;
            if (take('}')) {
                return v;
            }
            do {
                std::string name = string();
                if (!take(':')) {
                    ok_ = false;
                    return v;
                }
                v.fields.emplace_back(std::move(name), value(depth + 1));
            } while (ok_ && take(','));
            if (!take('}')) {
                ok_ = false;
            }
        } else if (c == '[') {
            at_++;
            v.type = Json::Type::Array;
            if (take(']')) {
                return v;
            }
            do {
                v.items.push_back(value(depth + 1));
            } while (ok_ && take(','));
            if (!take(']')) {
                ok_ = false;
            }
        } else if (c == '"') {
            v.type = Json::Type::String;
            v.text = string();
        } else if (c == 't' || c == 'f') {
            v.type = Json::Type::Bool;
            v.boolean = c == 't';
            word(v.boolean ? "true" : "false");
        } else if (c == 'n') {
            word("null");
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            v.type = Json::Type::Number;
            while (at_ < end_ && std::strchr("+-0123456789.eE", *at_)) {
                v.text += *at_++;
            }
        } else {
            ok_ = false;
        }
        return v;
    }

    const char* at_;
    const char* end_;
    bool ok_ = true;
};

// ---- running curl -----------------------------------------------------------------------------

#ifdef _WIN32
std::wstring wide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), size);
    return out;
}
// An argument as CommandLineToArgvW reads it back: in quotes, a quote with a backslash before it, and backslashes
// doubled where a quote follows them
std::wstring quoted(const std::wstring& arg) {
    std::wstring out = L"\"";
    size_t backslashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') {
            backslashes++;
            continue;
        }
        out.append(c == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
        backslashes = 0;
        out += c;
    }
    out.append(backslashes * 2, L'\\');
    return out + L"\"";
}
std::wstring command_line(const fs::path& program, const std::vector<std::string>& args) {
    std::wstring line = quoted(program.wstring());
    for (const std::string& arg : args) {
        line += L" " + quoted(wide(arg));
    }
    return line;
}
#endif

// Runs a program (no shell) and waits for it: its exit code (-1: it did not start), and its output if asked
int run_program(const fs::path& program, const std::vector<std::string>& args, std::string* output) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES security{sizeof security, nullptr, TRUE};
    HANDLE read = nullptr, write = nullptr;
    if (!CreatePipe(&read, &write, &security, 0)) {
        return -1;
    }
    SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = write;
    startup.hStdError = write;
    startup.hStdInput = nullptr;
    PROCESS_INFORMATION process{};
    std::wstring line = command_line(program, args);
    if (!CreateProcessW(program.wstring().c_str(), line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        CloseHandle(write);
        CloseHandle(read);
        return -1;
    }
    CloseHandle(write);
    char buffer[16384];
    DWORD got = 0;
    std::string all;
    while (ReadFile(read, buffer, sizeof buffer, &got, nullptr) && got > 0) {
        all.append(buffer, got);
    }
    CloseHandle(read);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    if (output) {
        *output = std::move(all);
    }
    return static_cast<int>(code);
#else
    int pipe_ends[2];
    if (pipe(pipe_ends) != 0) {
        return -1;
    }
    std::vector<std::string> owned{program.string()};
    owned.insert(owned.end(), args.begin(), args.end());
    std::vector<char*> argv;
    for (std::string& arg : owned) {
        argv.push_back(arg.data());
    }
    argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipe_ends[1], 1);
    posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addclose(&actions, pipe_ends[0]);
    pid_t pid = 0;
    int result = posix_spawnp(&pid, owned[0].c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipe_ends[1]);
    std::string all;
    if (result == 0) {
        char buffer[16384];
        ssize_t got;
        while ((got = read(pipe_ends[0], buffer, sizeof buffer)) > 0) {
            all.append(buffer, static_cast<size_t>(got));
        }
    }
    close(pipe_ends[0]);
    if (result != 0) {
        return -1;
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (output) {
        *output = std::move(all);
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

fs::path curl_program() {
#ifdef _WIN32
    wchar_t system[MAX_PATH];
    UINT n = GetSystemDirectoryW(system, MAX_PATH);
    return n > 0 && n < MAX_PATH ? fs::path(system) / L"curl.exe" : fs::path(L"curl.exe");
#else
    std::error_code error;
    return fs::exists("/usr/bin/curl", error) ? fs::path("/usr/bin/curl") : fs::path("curl");
#endif
}

// What curl's exit code means, for the player
std::string curl_problem(int code) {
    switch (code) {
    case -1: return "curl, which downloads the update, could not be started";
    case 6:
    case 7: return "no connection to the internet (or to GitHub)";
    case 22: return "the file was not found on the server";
    case 28: return "the connection timed out";
    default: return "the download failed (curl " + std::to_string(code) + ")";
    }
}

// A URL is only ever a release's on GitHub, or a file:// one when the variable for tests names one
bool url_allowed(const std::string& url) {
    if (url.find_first_of("\"' \n\r\t") != std::string::npos) {
        return false;
    }
    return url.rfind("https://", 0) == 0 || (std::getenv("RECOMP_UPDATE_RELEASES") && url.rfind("file://", 0) == 0);
}

int fetch_text(const std::string& url, std::string& text) {
    if (!url_allowed(url)) {
        return 22;
    }
    return run_program(curl_program(),
                       {"-fsSL", "--connect-timeout", "15", "--max-time", "60", "-H", "Accept: application/vnd.github+json", "-A", "recomp-launcher", url},
                       &text);
}

int fetch_file(const std::string& url, const fs::path& to) {
    if (!url_allowed(url)) {
        return 22;
    }
    return run_program(curl_program(),
                       {"-fsSL", "--connect-timeout", "15", "--speed-limit", "1024", "--speed-time", "60", "--retry", "2", "-A", "recomp-launcher", "-o",
                        to.string(), url},
                       nullptr);
}

// A gzipped file unpacked to `to`, checked against its size and SHA-256 as it is written
bool unpack(const fs::path& from, const fs::path& to, const Updater::Entry& entry) {
#ifdef _WIN32
    gzFile in = gzopen_w(from.wstring().c_str(), "rb");
#else
    gzFile in = gzopen(from.string().c_str(), "rb");
#endif
    if (!in) {
        return false;
    }
    std::ofstream out(to, std::ios::binary | std::ios::trunc);
    Sha256 sha;
    uint64_t size = 0;
    char buffer[1 << 16];
    int got;
    while ((got = gzread(in, buffer, sizeof buffer)) > 0) {
        sha.add(buffer, static_cast<size_t>(got));
        out.write(buffer, got);
        size += static_cast<uint64_t>(got);
    }
    bool clean = got == 0;
    gzclose(in);
    out.close();
    return clean && out && size == entry.size && sha.hex() == entry.sha256;
}

// A manifest path is relative, with no .. and no empty part: nothing outside the top can be named
bool safe_path(const std::string& path) {
    if (path.empty() || path[0] == '/' || path.find('\\') != std::string::npos || path.find(':') != std::string::npos) {
        return false;
    }
    std::stringstream parts(path);
    std::string part;
    while (std::getline(parts, part, '/')) {
        if (part.empty() || part == "." || part == "..") {
            return false;
        }
    }
    return path.back() != '/';
}

std::string megabytes(uint64_t bytes) {
    char text[32];
    if (bytes < 1000000) {
        std::snprintf(text, sizeof text, "%llu KB", static_cast<unsigned long long>((bytes + 999) / 1000));
    } else {
        std::snprintf(text, sizeof text, "%.1f MB", static_cast<double>(bytes) / 1e6);
    }
    return text;
}

std::string plain_version(std::string tag) {
    return !tag.empty() && (tag[0] == 'v' || tag[0] == 'V') ? tag.substr(1) : tag;
}

}  // namespace

// ---- the parts the tests see too ---------------------------------------------------------------

const char* Updater::platform() {
#if defined(__APPLE__)
    return "macos";
#elif defined(_WIN32)
    return "windows-x64";
#elif defined(__linux__) && defined(__aarch64__)
    return "linux-arm64";
#elif defined(__linux__)
    return "linux-x86_64";
#else
    return "";
#endif
}

bool Updater::parse_manifest(const std::string& text, Manifest& manifest) {
    manifest = {};
    std::stringstream lines(text);
    std::string line;
    bool header = false;
    std::set<std::string> seen;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line == "recomp-update 1") {
            header = true;
        } else if (line.rfind("version ", 0) == 0) {
            manifest.version = line.substr(8);
        } else if (line.rfind("file ", 0) == 0) {
            std::istringstream fields(line.substr(5));
            Entry entry;
            std::string mode;
            if (!(fields >> entry.sha256 >> entry.size >> entry.packed >> mode)) {
                return false;
            }
            std::getline(fields >> std::ws, entry.path);
            entry.executable = mode == "x";
            if (entry.sha256.size() != 64 || entry.sha256.find_first_not_of("0123456789abcdef") != std::string::npos || (mode != "x" && mode != "-") ||
                !safe_path(entry.path) || !seen.insert(entry.path).second) {
                return false;
            }
            manifest.files.push_back(entry);
        } else {
            return false;
        }
    }
    return header && !manifest.version.empty() && !manifest.files.empty();
}

int Updater::compare_versions(const std::string& a, const std::string& b) {
    auto split = [](const std::string& text, char by) {
        std::vector<std::string> parts;
        std::stringstream in(text);
        std::string part;
        while (std::getline(in, part, by)) {
            parts.push_back(part);
        }
        return parts;
    };
    auto numeric = [](const std::string& s) { return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos; };
    auto compare_part = [&](const std::string& x, const std::string& y) {
        if (numeric(x) && numeric(y)) {
            unsigned long long nx = std::strtoull(x.c_str(), nullptr, 10), ny = std::strtoull(y.c_str(), nullptr, 10);
            return nx < ny ? -1 : nx > ny ? 1 : 0;
        }
        if (numeric(x) != numeric(y)) {
            return numeric(x) ? -1 : 1;  // (a number before a word, as semver has it)
        }
        return x < y ? -1 : x > y ? 1 : 0;
    };
    std::string va = plain_version(a), vb = plain_version(b);
    size_t da = va.find('-'), db = vb.find('-');
    std::vector<std::string> ca = split(va.substr(0, da), '.'), cb = split(vb.substr(0, db), '.');
    for (size_t i = 0; i < std::max(ca.size(), cb.size()); i++) {
        int c = compare_part(i < ca.size() ? ca[i] : "0", i < cb.size() ? cb[i] : "0");
        if (c != 0) {
            return c;
        }
    }
    // A pre-release comes before its version
    if ((da == std::string::npos) != (db == std::string::npos)) {
        return da == std::string::npos ? 1 : -1;
    }
    if (da == std::string::npos) {
        return 0;
    }
    std::vector<std::string> pa = split(va.substr(da + 1), '.'), pb = split(vb.substr(db + 1), '.');
    for (size_t i = 0; i < std::min(pa.size(), pb.size()); i++) {
        if (int c = compare_part(pa[i], pb[i])) {
            return c;
        }
    }
    return pa.size() < pb.size() ? -1 : pa.size() > pb.size() ? 1 : 0;
}

std::string Updater::sha256_of_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return "";
    }
    Sha256 sha;
    char buffer[1 << 16];
    while (in.read(buffer, sizeof buffer) || in.gcount() > 0) {
        sha.add(buffer, static_cast<size_t>(in.gcount()));
    }
    return sha.hex();
}

// ---- Updater -------------------------------------------------------------------------------------

Updater::Updater(fs::path program_folder, std::string launcher_name, std::string version, std::string repo, std::function<void()> wake)
    : program_folder_(std::move(program_folder)),
      launcher_name_(std::move(launcher_name)),
      version_(std::move(version)),
      repo_(std::move(repo)),
      wake_(std::move(wake)) {
    if (program_folder_.filename().empty()) {
        program_folder_ = program_folder_.parent_path();  // (a folder given with / at its end)
    }
    top_ = program_folder_;
    if (program_folder_.filename() == "MacOS" && program_folder_.parent_path().filename() == "Contents") {
        top_ = program_folder_.parent_path().parent_path();  // the app
    }
    // What the last install left (on Windows, the programs that were running then)
    std::error_code error;
    if (fs::is_directory(top_ / ".update", error)) {
        fs::remove_all(top_ / ".update", error);
    }
}

Updater::~Updater() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

Updater::Status Updater::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

bool Updater::busy() const {
    return busy_;
}

void Updater::set(State state, const std::string& message) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.state = state;
        status_.message = message;
    }
    if (wake_) {
        wake_();
    }
}

void Updater::run(void (Updater::*work)()) {
    if (busy_.exchange(true)) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    worker_ = std::thread([this, work] {
        (this->*work)();
        busy_ = false;
        if (wake_) {
            wake_();
        }
    });
}

void Updater::check() {
    run(&Updater::check_work);
}

void Updater::install() {
    if (status().state == State::Available) {
        run(&Updater::install_work);
    }
}

void Updater::check_work() {
    set(State::Checking, "Checking for a new version…");
    do_check();
}

void Updater::install_work() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.fraction = 0.0;
    }
    set(State::Installing, "Downloading…");
    do_install();
}

bool Updater::do_check() {
    const std::string manifest_name = std::string("update-") + platform() + ".txt";
    const char* other = std::getenv("RECOMP_UPDATE_RELEASES");
    const std::string list_url = other && *other ? other : "https://api.github.com/repos/" + repo_ + "/releases?per_page=30";
    std::string text;
    if (int code = fetch_text(list_url, text); code != 0) {
        set(State::Failed, "Couldn't check for a new version: " + curl_problem(code) + ".");
        return false;
    }
    Json releases;
    if (!JsonReader(text).read(releases) || releases.type != Json::Type::Array) {
        set(State::Failed, "Couldn't check for a new version: GitHub's answer could not be read.");
        return false;
    }
    // The newest release with a manifest for this system (a beta only while this is a beta), and this one's own
    const bool beta = version_.find('-') != std::string::npos;
    const Json* best = nullptr;
    std::string best_version, best_manifest, current_manifest;
    for (const Json& release : releases.items) {
        if (release["draft"].boolean) {
            continue;
        }
        std::string url;
        for (const Json& asset : release["assets"].items) {
            if (asset["name"].text == manifest_name) {
                url = asset["browser_download_url"].text;
            }
        }
        if (url.empty()) {
            continue;
        }
        const std::string version = plain_version(release["tag_name"].text);
        if (compare_versions(version, version_) == 0) {
            current_manifest = url;
        }
        if ((beta || !release["prerelease"].boolean) && (!best || compare_versions(version, best_version) > 0)) {
            best = &release;
            best_version = version;
            best_manifest = url;
        }
    }
    if (!best || compare_versions(best_version, version_) <= 0) {
        set(State::UpToDate, "This is the newest version.");
        return true;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.version = best_version;
        status_.notes_url = (*best)["html_url"].text;
    }
    if (int code = fetch_text(best_manifest, text); code != 0 || !parse_manifest(text, next_) || compare_versions(next_.version, best_version) != 0) {
        set(State::Failed, "Version " + best_version + " is out, but its list of files " +
                               (code != 0 ? "couldn't be downloaded: " + curl_problem(code) + "." : "could not be read."));
        return false;
    }
    blob_base_ = best_manifest.substr(0, best_manifest.rfind('/') + 1);
    // (the files this version had, so that those the new one has not can go; without its manifest none go)
    current_ = {};
    if (!current_manifest.empty() && fetch_text(current_manifest, text) == 0) {
        parse_manifest(text, current_);
    }
    // This copy is a release's download only if the launcher is where the manifest has it
    std::error_code error;
    const fs::path self = fs::relative(program_folder_ / launcher_name_, top_, error);
    const bool installed = !error && std::any_of(next_.files.begin(), next_.files.end(), [&](const Entry& e) { return e.path == self.generic_string(); });
    if (!installed) {
        set(State::NotInstalled, "This copy (" + version_ + ") is not one from a release's download, so it can't update itself: download the new " +
                                     "version from the releases page.");
        return true;
    }
    needed_.clear();
    needed_bytes_ = 0;
    for (size_t i = 0; i < next_.files.size(); i++) {
        const Entry& entry = next_.files[i];
        const fs::path here = top_ / fs::u8path(entry.path);
        if (!fs::is_regular_file(here, error) || fs::file_size(here, error) != entry.size || sha256_of_file(here) != entry.sha256) {
            needed_.push_back(i);
            needed_bytes_ += entry.packed;
        }
    }
    const size_t n = needed_.size();
    set(State::Available, "You have " + version_ + ". Only what changed is downloaded: " + std::to_string(n) + (n == 1 ? " file" : " files") + ", " +
                              megabytes(needed_bytes_) + ".");
    return true;
}

bool Updater::do_install() {
    std::error_code error;
    const fs::path staging = top_ / ".update";
    fs::remove_all(staging, error);
    if (!fs::create_directories(staging / "old", error) || error) {
        set(State::Failed, "Couldn't update: the launcher can't write to its folder (" + top_.string() +
                               "). Move it to a folder of your own, or download the new version from the releases page.");
        return false;
    }
    auto fail = [&](const std::string& why) {
        fs::remove_all(staging, error);
        set(State::Failed, "Couldn't update: " + why + " Nothing was changed.");
        return false;
    };
    // 1. Every file that differs, downloaded and checked
    uint64_t done = 0;
    size_t count = 0;
    for (size_t index : needed_) {
        const Entry& entry = next_.files[index];
        count++;
        set(State::Installing, "Downloading " + std::to_string(count) + " of " + std::to_string(needed_.size()) + " (" + megabytes(done) + " of " +
                                   megabytes(needed_bytes_) + ")…");
        const fs::path packed = staging / (entry.sha256 + ".gz"), file = staging / entry.sha256;
        if (!fs::exists(file, error)) {  // (the same file twice in the manifest is fetched once)
            if (int code = fetch_file(blob_base_ + entry.sha256 + ".gz", packed); code != 0) {
                return fail(curl_problem(code) + ".");
            }
            if (!unpack(packed, file, entry)) {
                return fail(entry.path + " was not what the release says it is.");
            }
            fs::remove(packed, error);
        }
        done += entry.packed;
        std::lock_guard<std::mutex> lock(mutex_);
        status_.fraction = needed_bytes_ ? static_cast<double>(done) / static_cast<double>(needed_bytes_) : 1.0;
    }
    // 2. All of them put in place: what was there is moved aside first (a running program can be moved, on Windows
    // too, but not written over), and moved back if any of them can't be
    set(State::Installing, "Putting the new files in place…");
    std::vector<std::pair<fs::path, fs::path>> moved;  // (where it was, where it is now) of the files that were there
    std::vector<fs::path> placed;
    auto undo = [&] {
        for (const fs::path& path : placed) {
            fs::remove(path, error);
        }
        for (auto it = moved.rbegin(); it != moved.rend(); ++it) {
            fs::rename(it->second, it->first, error);
        }
    };
    size_t aside = 0;
    auto move_aside = [&](const fs::path& target) {
        if (!fs::exists(target, error)) {
            return true;
        }
        const fs::path to = staging / "old" / std::to_string(aside++);
        fs::rename(target, to, error);
        if (error) {
            return false;
        }
        moved.emplace_back(target, to);
        return true;
    };
    std::map<std::string, size_t> uses;  // (staged files placed more than once are copied for all but the last)
    for (size_t index : needed_) {
        uses[next_.files[index].sha256]++;
    }
    for (size_t index : needed_) {
        const Entry& entry = next_.files[index];
        const fs::path target = top_ / fs::u8path(entry.path), file = staging / entry.sha256;
        fs::create_directories(target.parent_path(), error);
        if (!move_aside(target)) {
            undo();
            return fail(entry.path + " couldn't be moved aside.");
        }
        error.clear();
        if (--uses[entry.sha256] > 0) {
            fs::copy_file(file, target, error);
        } else {
            fs::rename(file, target, error);
        }
        if (error) {
            undo();
            return fail(entry.path + " couldn't be put in place.");
        }
        placed.push_back(target);
        fs::permissions(target,
                        entry.executable ? fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec | fs::perms::others_read |
                                               fs::perms::others_exec
                                         : fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read | fs::perms::others_read,
                        error);
    }
    // 3. What the running version had and the new one has not (only if it is still as that version had it)
    std::set<std::string> kept;
    for (const Entry& entry : next_.files) {
        kept.insert(entry.path);
    }
    for (const Entry& entry : current_.files) {
        const fs::path old = top_ / fs::u8path(entry.path);
        if (!kept.count(entry.path) && fs::is_regular_file(old, error) && sha256_of_file(old) == entry.sha256) {
            move_aside(old);
        }
    }
    fs::remove_all(staging, error);  // (on Windows what was running stays until the next start)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.fraction = 1.0;
    }
    set(State::Installed, "Starting the new version…");
    return true;
}

bool Updater::relaunch(const std::vector<std::string>& args) const {
    const fs::path program = program_folder_ / launcher_name_;
#ifdef _WIN32
    std::wstring line = command_line(program, args);
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(program.wstring().c_str(), line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
#else
    std::vector<std::string> owned{program.string()};
    owned.insert(owned.end(), args.begin(), args.end());
    std::vector<char*> argv;
    for (std::string& arg : owned) {
        argv.push_back(arg.data());
    }
    argv.push_back(nullptr);
    pid_t pid = 0;
    return posix_spawn(&pid, owned[0].c_str(), nullptr, nullptr, argv.data(), environ) == 0;
#endif
}

}  // namespace launcher
