// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// The words of the launcher and the in-game menu in the player's language.
//
// Every word is written in English where it is used, and looked up (Tr) where it is drawn: a table for each
// language says what each English text is in it. A text a table does not have stays in English, so a table
// may be short, and a port adds its own (Add) for its own words. recomp-common's tables (i18n_pt_br.h,
// i18n_es.h) are taken in the first time a text is looked up.
//
// The language: the launcher's choice (its [launcher] language, else the system's), handed to the games it
// starts in RECOMP_LANGUAGE; a game started on its own takes that or the system's (FromEnvironment,
// Resolve).
//
// RECOMP_I18N_MISSING=FILE writes each text looked up that the language's tables do not have, once, to FILE:
// what a table still needs.
//
// Header only, C++17, no dependencies.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace recomp::i18n {

struct Entry {
    const char *english, *translated;
};

struct Language {
    const char *code; // as RECOMP_LANGUAGE and [launcher] language have it
    const char *name; // in its own words
};
// The languages there are tables for, English first
inline constexpr Language kLanguages[] = {{"en", "English"}, {"pt-BR", "Português (Brasil)"}, {"es", "Español"}};

namespace detail {
    struct State {
        std::mutex lock;
        std::string language = "en";
        std::unordered_map<std::string, std::unordered_map<std::string_view, const char *>> tables;
        bool builtIn = false;
        std::set<std::string> missing; // (written already)
    };
    inline State &The() {
        static State state;
        return state;
    }
    inline void AddLocked(State &state, const char *language, const Entry *entries, size_t count) {
        auto &table = state.tables[language];
        for (size_t i = 0; i < count; i++) {
            if (entries[i].english && entries[i].translated && *entries[i].translated) {
                table[entries[i].english] = entries[i].translated;
            }
        }
    }
    void AddBuiltIn(State &state); // (below, after the tables)
    inline void EnsureBuiltIn(State &state) {
        if (!state.builtIn) {
            state.builtIn = true;
            AddBuiltIn(state);
        }
    }
} // namespace detail

// A table of a language's words (as long as the program runs: the entries are not copied). A later table's text
// goes before an earlier one's (and a port's before recomp-common's).
inline void Add(const char *language, const Entry *entries, size_t count) {
    detail::State &state = detail::The();
    std::lock_guard<std::mutex> hold(state.lock);
    detail::EnsureBuiltIn(state);
    detail::AddLocked(state, language, entries, count);
}
template <size_t N> inline void Add(const char *language, const Entry (&entries)[N]) {
    Add(language, entries, N);
}

// A language's code as written ("pt_BR.UTF-8", "es-MX", "PT") as one of kLanguages' ("pt-BR", "es"); "" if none
inline std::string Known(std::string code) {
    const size_t dot = code.find_first_of(".@");
    code = code.substr(0, dot);
    for (char &c : code) {
        c = c == '_' ? '-' : static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    }
    const std::string primary = code.substr(0, code.find('-'));
    if (primary == "en" || code == "c" || code == "posix") {
        return "en";
    }
    if (primary == "pt") {
        return "pt-BR"; // (Portugal's Portuguese too: the one table there is)
    }
    if (primary == "es") {
        return "es";
    }
    return "";
}

// The language to use: the one chosen ("" or "auto": the system's), else the first of the system's preferred
// (most wanted first) that there is a table for, else English
inline std::string Resolve(const std::string &chosen, const std::vector<std::string> &preferred) {
    if (!chosen.empty() && chosen != "auto") {
        const std::string known = Known(chosen);
        if (!known.empty()) {
            return known;
        }
    }
    for (const std::string &one : preferred) {
        const std::string known = Known(one);
        if (!known.empty()) {
            return known;
        }
    }
    return "en";
}

// The system's preferred languages as the environment says them (RECOMP_LANGUAGE first: what the launcher chose)
inline std::vector<std::string> FromEnvironment() {
    std::vector<std::string> preferred;
    for (const char *name : {"RECOMP_LANGUAGE", "LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG"}) {
        const char *value = std::getenv(name);
        if (value && *value) {
            // (LANGUAGE may be a list, "pt_BR:en")
            std::string list = value;
            for (size_t at = 0; at <= list.size();) {
                const size_t colon = std::min(list.find(':', at), list.size());
                if (colon > at) {
                    preferred.push_back(list.substr(at, colon - at));
                }
                at = colon + 1;
            }
        }
    }
    return preferred;
}

inline void SetLanguage(const std::string &code) {
    detail::State &state = detail::The();
    std::lock_guard<std::mutex> hold(state.lock);
    const std::string known = Known(code);
    state.language = known.empty() ? "en" : known;
}

inline std::string Current() {
    detail::State &state = detail::The();
    std::lock_guard<std::mutex> hold(state.lock);
    return state.language;
}

// The text in the language chosen (as it is, if its table does not have it). The pointer lives as long as the
// program (a table's) or as `english` does.
inline const char *Tr(const char *english) {
    if (!english || !*english) {
        return english;
    }
    detail::State &state = detail::The();
    std::lock_guard<std::mutex> hold(state.lock);
    if (state.language == "en") {
        return english;
    }
    detail::EnsureBuiltIn(state);
    const auto table = state.tables.find(state.language);
    if (table != state.tables.end()) {
        const auto found = table->second.find(english);
        if (found != table->second.end()) {
            return found->second;
        }
    }
    // (a text with no letters in it, a number or a path, is not missing)
    static const char *const missingFile = std::getenv("RECOMP_I18N_MISSING");
    bool letters = false;
    for (const char *c = english; *c && !letters; c++) {
        letters = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z');
    }
    if (missingFile && *missingFile && letters && state.missing.insert(english).second) {
        if (FILE *file = std::fopen(missingFile, "a")) {
            // (a line each: a line break in the text as \n)
            for (const char *c = english; *c; c++) {
                if (*c == '\n') {
                    std::fputs("\\n", file);
                } else {
                    std::fputc(*c, file);
                }
            }
            std::fputc('\n', file);
            std::fclose(file);
        }
    }
    return english;
}
inline std::string Tr(const std::string &english) {
    return Tr(english.c_str());
}

// A pattern's {} filled with the texts in turn ({1}, {2}...: by their place, for a language whose words
// come in another order)
inline std::string Format(const std::string &pattern, std::initializer_list<std::string> args) {
    const std::vector<std::string> list(args);
    std::string out;
    size_t next = 0;
    for (size_t i = 0; i < pattern.size(); i++) {
        if (pattern[i] == '{') {
            const size_t close = pattern.find('}', i);
            if (close != std::string::npos) {
                const std::string inside = pattern.substr(i + 1, close - i - 1);
                size_t which = next;
                bool number = !inside.empty() && inside.size() < 3 && inside.find_first_not_of("0123456789") == std::string::npos;
                if (inside.empty() || number) {
                    which = number ? static_cast<size_t>(std::atoi(inside.c_str())) - 1 : next++;
                    if (which < list.size()) {
                        out += list[which];
                    }
                    i = close;
                    continue;
                }
            }
        }
        out += pattern[i];
    }
    return out;
}
// ... the pattern looked up first
inline std::string TrFormat(const char *pattern, std::initializer_list<std::string> args) {
    return Format(Tr(pattern), args);
}

} // namespace recomp::i18n

#include "i18n_es.h"
#include "i18n_pt_br.h"

inline void recomp::i18n::detail::AddBuiltIn(State &state) {
    AddLocked(state, "pt-BR", kPortuguese, sizeof(kPortuguese) / sizeof(kPortuguese[0]));
    AddLocked(state, "es", kSpanish, sizeof(kSpanish) / sizeof(kSpanish[0]));
}

namespace recomp {
using i18n::Tr;
} // namespace recomp
