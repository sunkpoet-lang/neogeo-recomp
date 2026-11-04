// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
//
// The proxy Windows is set to use, for the curl a port starts (the online service, the launcher's updates). The
// system's curl.exe takes a proxy only from HTTPS_PROXY and HTTP_PROXY, not from the one set in Windows' proxy
// settings (Settings > Network > Proxy, or Internet Options), where proxy programs such as Clash and v2rayN put
// theirs: behind one, nothing curl asked for was reached. UseSystemProxy(), called once when the program starts,
// sets those two variables from Windows' setting for the programs this one starts, unless the player set one
// (or ALL_PROXY) already. Elsewhere (and with no proxy set) it does nothing. Nothing here knows of a game or SDL.
#pragma once

#include <cstdlib>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace recomp {

// curl's proxy from Windows' ProxyServer value: "host:port" for everything, or "http=host:port;https=host:port;
// socks=host:port" for each kind (https's taken first, then http's, then a SOCKS one). "" for none.
inline std::string ProxyFromSetting(const std::string &setting) {
    std::string any, http, https, socks;
    size_t at = 0;
    while (at <= setting.size()) {
        size_t end = setting.find_first_of("; ", at);
        if (end == std::string::npos) {
            end = setting.size();
        }
        const std::string part = setting.substr(at, end - at);
        at = end + 1;
        if (part.empty()) {
            continue;
        }
        const size_t equals = part.find('=');
        if (equals == std::string::npos) {
            any = part;
            continue;
        }
        const std::string kind = part.substr(0, equals), where = part.substr(equals + 1);
        if (kind == "https") {
            https = where;
        } else if (kind == "http") {
            http = where;
        } else if (kind == "socks") {
            socks = where;
        }
    }
    std::string proxy = !https.empty() ? https : !http.empty() ? http : !any.empty() ? any : "";
    if (proxy.empty() && !socks.empty()) {
        // (Windows' SOCKS proxy is version 4; the names are looked up by it)
        return socks.find("://") == std::string::npos ? "socks4a://" + socks : socks;
    }
    if (proxy.empty()) {
        return "";
    }
    return proxy.find("://") == std::string::npos ? "http://" + proxy : proxy;
}

inline void UseSystemProxy() {
#ifdef _WIN32
    for (const char *name : {"HTTPS_PROXY", "HTTP_PROXY", "ALL_PROXY"}) {
        if (const char *set = std::getenv(name); set && *set) {
            return;  // (the player's own)
        }
    }
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return;
    }
    DWORD enabled = 0, size = sizeof(enabled);
    char server[1024] = {};
    DWORD serverSize = sizeof(server) - 1;
    const bool on = RegQueryValueExA(key, "ProxyEnable", nullptr, nullptr, reinterpret_cast<BYTE *>(&enabled), &size) == ERROR_SUCCESS && enabled != 0 &&
                    RegQueryValueExA(key, "ProxyServer", nullptr, nullptr, reinterpret_cast<BYTE *>(server), &serverSize) == ERROR_SUCCESS;
    RegCloseKey(key);
    const std::string proxy = on ? ProxyFromSetting(server) : "";
    if (!proxy.empty()) {
        SetEnvironmentVariableA("HTTPS_PROXY", proxy.c_str());
        SetEnvironmentVariableA("HTTP_PROXY", proxy.c_str());
    }
#endif
}

}  // namespace recomp
