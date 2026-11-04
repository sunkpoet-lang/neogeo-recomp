// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#include "online.h"

#include "run_program.h"

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <random>
#include <thread>

namespace launcher {

namespace {
// What an address is, by its range: private (a home network), 100.64.0.0/10 (Tailscale gives these; other carrier
// and VPN networks too), link-local and the loopback (left out: "-")
std::string Kind(uint32_t a, const std::string &name) {
    const uint8_t b0 = a >> 24, b1 = (a >> 16) & 0xFF;
    if (b0 == 127 || (b0 == 169 && b1 == 254) || a == 0) {
        return "-";
    }
    if (b0 == 100 && (b1 & 0xC0) == 64) {
        return name.find("tailscale") != std::string::npos || name.rfind("utun", 0) == 0 || name.find("Tailscale") != std::string::npos
                   ? "Tailscale"
                   : "VPN";
    }
    if (b0 == 10 || (b0 == 172 && (b1 & 0xF0) == 16) || (b0 == 192 && b1 == 168)) {
        return "home network";
    }
    return "";
}

// A network of this machine's own making (a virtual machine's or a container's): nobody else is on it
bool Virtual(const std::string &name) {
    for (const char *start : {"bridge", "docker", "br-", "veth", "virbr", "vmnet", "vboxnet", "vEthernet", "VirtualBox", "VMware", "Hyper-V"}) {
        if (name.rfind(start, 0) == 0) {
            return true;
        }
    }
    return false;
}

void Add(std::vector<LocalAddress> &list, uint32_t a, const std::string &name) {
    const std::string kind = Kind(a, name);
    if (kind == "-" || Virtual(name)) {
        return;
    }
    const std::string text = std::to_string(a >> 24) + "." + std::to_string((a >> 16) & 0xFF) + "." + std::to_string((a >> 8) & 0xFF) + "." +
                             std::to_string(a & 0xFF);
    if (std::none_of(list.begin(), list.end(), [&](const LocalAddress &l) { return l.address == text; })) {
        list.push_back({text, kind});
    }
}
}  // namespace

std::vector<LocalAddress> local_addresses() {
    std::vector<LocalAddress> list;
#ifdef _WIN32
    ULONG size = 16384;
    std::vector<uint8_t> buffer(size);
    auto *adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data());
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, adapters, &size) ==
        ERROR_BUFFER_OVERFLOW) {
        buffer.resize(size);
        adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data());
        if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, adapters, &size) != NO_ERROR) {
            return list;
        }
    }
    for (auto *adapter = adapters; adapter; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp) {
            continue;
        }
        std::string name;
        for (const wchar_t *c = adapter->Description; c && *c; c++) {
            name += static_cast<char>(*c < 128 ? *c : '?');
        }
        for (auto *unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
            const auto *in = reinterpret_cast<const sockaddr_in *>(unicast->Address.lpSockaddr);
            Add(list, ntohl(in->sin_addr.s_addr), name);
        }
    }
#else
    ifaddrs *all = nullptr;
    if (getifaddrs(&all) != 0) {
        return list;
    }
    for (const ifaddrs *i = all; i; i = i->ifa_next) {
        if (i->ifa_addr && i->ifa_addr->sa_family == AF_INET && (i->ifa_flags & IFF_UP)) {
            Add(list, ntohl(reinterpret_cast<const sockaddr_in *>(i->ifa_addr)->sin_addr.s_addr), i->ifa_name ? i->ifa_name : "");
        }
    }
    freeifaddrs(all);
#endif
    std::stable_sort(list.begin(), list.end(), [](const LocalAddress &a, const LocalAddress &b) {
        const auto rank = [](const std::string &kind) { return kind == "home network" ? 0 : kind == "Tailscale" ? 1 : kind == "VPN" ? 2 : 3; };
        return rank(a.kind) < rank(b.kind);
    });
    return list;
}

CodeLookup lookup_code(const std::string& url) {
    CodeLookup out;
    std::string body;
    if (run_program(curl_program(), {"-sS", "--max-time", "8", "-w", "\n%{http_code}", url}, &body) != 0) {
        return out;
    }
    const size_t line = body.rfind('\n');
    if (line == std::string::npos) {
        return out;
    }
    out.status = std::atoi(body.c_str() + line + 1);
    body.resize(line);
    const auto text = [&](const char* key) {
        std::string value;
        const size_t at = body.find(std::string("\"") + key + "\":\"");
        if (at == std::string::npos) {
            return value;
        }
        for (size_t i = at + std::strlen(key) + 4; i < body.size() && body[i] != '"'; i++) {
            if (body[i] == '\\' && i + 1 < body.size()) {
                i++;
            }
            value += body[i];
        }
        return value;
    };
    out.game = text("game");
    out.title = text("title");
    out.version = text("version");
    out.error = text("error");
    return out;
}

namespace {
// JSON text: quotes and backslashes escaped, control characters left out
std::string Quoted(const std::string& text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        if (static_cast<unsigned char>(c) >= 0x20) {
            out += c;
        }
    }
    return out + "\"";
}

std::string QuotedList(const std::vector<std::string>& list) {
    std::string out = "[";
    for (const std::string& item : list) {
        out += (out.size() > 1 ? "," : "") + Quoted(item);
    }
    return out + "]";
}

// A string's value in json[from, end) (its first "key":"..."), unescaped as far as quotes and backslashes go
std::string Text(const std::string& json, size_t from, size_t end, const char* key) {
    std::string out;
    const size_t at = json.find(std::string("\"") + key + "\":\"", from);
    if (at == std::string::npos || at >= end) {
        return out;
    }
    for (size_t i = at + std::strlen(key) + 4; i < end && json[i] != '"'; i++) {
        if (json[i] == '\\' && i + 1 < end) {
            i++;
        }
        out += json[i];
    }
    return out;
}

// The objects of the list "key":[{...}, ...] (none with objects or lists of objects inside), each as its [begin, end)
std::vector<std::pair<size_t, size_t>> Objects(const std::string& json, const char* key) {
    std::vector<std::pair<size_t, size_t>> out;
    size_t at = json.find(std::string("\"") + key + "\":[");
    if (at == std::string::npos) {
        return out;
    }
    at += std::strlen(key) + 4;
    while (at < json.size() && json[at] != ']') {
        if (json[at] == '{') {
            bool quoted = false;
            size_t end = at + 1;
            for (; end < json.size() && (quoted || json[end] != '}'); end++) {
                if (json[end] == '\\') {
                    end++;
                } else if (json[end] == '"') {
                    quoted = !quoted;
                }
            }
            out.push_back({at, end});
            at = end + 1;
        } else {
            at++;
        }
    }
    return out;
}

// The strings of "key":["...", ...] in json[from, end)
std::vector<std::string> Strings(const std::string& json, size_t from, size_t end, const char* key) {
    std::vector<std::string> out;
    size_t at = json.find(std::string("\"") + key + "\":[", from);
    if (at == std::string::npos || at >= end) {
        return out;
    }
    for (at += std::strlen(key) + 4; at < end && json[at] != ']'; at++) {
        if (json[at] == '"') {
            std::string item;
            for (at++; at < end && json[at] != '"'; at++) {
                if (json[at] == '\\' && at + 1 < end) {
                    at++;
                }
                item += json[at];
            }
            out.push_back(item);
        }
    }
    return out;
}

int Post(const std::string& url, const std::string& body, std::string* answer) {
    return run_program(curl_program(), {"-sS", "--max-time", "8", "-X", "POST", "-H", "content-type: application/json", "--data-binary", body, url}, answer);
}

// {"matches":[{"game","title","version","code","name","waiting"}, ...]}: the lobby's list in the service's answer (cps2recomp's
// online/), alone or in a waiting room check-in's
std::vector<OpenMatch> Matches(const std::string& json) {
    std::vector<OpenMatch> matches;
    for (const auto& [at, end] : Objects(json, "matches")) {
        OpenMatch match{Text(json, at, end, "game"), Text(json, at, end, "title"), Text(json, at, end, "version"),
                        Text(json, at, end, "code"), Text(json, at, end, "name")};
        const size_t waiting = json.find("\"waiting\":", at);
        match.waiting = waiting != std::string::npos && waiting < end ? std::atoi(json.c_str() + waiting + 10) : 0;
        if (!match.code.empty()) {
            matches.push_back(match);
        }
    }
    return matches;
}

// (sent on a thread of its own: the launcher doesn't wait for the answer)
void PostAway(const std::string& url, const std::string& body) {
    std::thread([url, body] { std::string ignored; Post(url, body, &ignored); }).detach();
}
}  // namespace

bool fetch_lobby(const std::string& url, std::vector<OpenMatch>& matches) {
    std::string json;
    if (run_program(curl_program(), {"-sS", "--max-time", "8", url}, &json) != 0 || json.find("\"matches\"") == std::string::npos) {
        return false;
    }
    matches = Matches(json);
    return true;
}

Room room_check_in(const std::string& url, const RoomCheckIn& me) {
    Room room;
    const std::string body = "{\"secret\":" + Quoted(me.secret) + ",\"name\":" + Quoted(me.name) + ",\"games\":" + QuotedList(me.games) +
                             ",\"protocol\":" + std::to_string(me.protocol) + ",\"version\":" + Quoted(me.version) +
                             ",\"invitable\":" + (me.invitable ? "true" : "false") + ",\"blocked\":" + QuotedList(me.blocked) +
                             (me.lobby ? ",\"lobby\":true}" : "}");
    std::string json;
    if (Post(url, body, &json) != 0 || json.find("\"players\"") == std::string::npos) {
        return room;
    }
    // {"id":"...","players":[{"id","name","games":[...]}],"invitations":[{"code","from","name","game","title","version"}]}:
    // the service's answer (cps2recomp's online/)
    room.ok = true;
    // (the lobby's list too, asked for: from a service that says it)
    room.listed = json.find("\"matches\":[") != std::string::npos;
    room.matches = Matches(json);
    const size_t players = json.find("\"players\":[");
    room.id = Text(json, 0, players, "id");
    for (const auto& [at, end] : Objects(json, "players")) {
        RoomPlayer player{Text(json, at, end, "id"), Text(json, at, end, "name"), Strings(json, at, end, "games")};
        if (!player.id.empty()) {
            room.players.push_back(player);
        }
    }
    for (const auto& [at, end] : Objects(json, "invitations")) {
        Invitation invitation{Text(json, at, end, "code"),  Text(json, at, end, "from"),  Text(json, at, end, "name"),
                              Text(json, at, end, "game"),  Text(json, at, end, "title"), Text(json, at, end, "version")};
        if (!invitation.code.empty()) {
            room.invitations.push_back(invitation);
        }
    }
    return room;
}

void room_leave(const std::string& url, const std::string& secret) {
    PostAway(url + "/leave", "{\"secret\":" + Quoted(secret) + "}");
}

void room_decline(const std::string& url, const std::string& secret, const std::string& code) {
    PostAway(url + "/decline", "{\"secret\":" + Quoted(secret) + ",\"code\":" + Quoted(code) + "}");
}

std::string new_room_secret() {
    std::random_device device;
    std::mt19937_64 random((static_cast<uint64_t>(device()) << 32) ^ device() ^
                           static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    static const char kHex[] = "0123456789abcdef";
    std::string secret;
    for (int i = 0; i < 32; i++) {
        secret += kHex[random() & 15];
    }
    return secret;
}

}  // namespace launcher
