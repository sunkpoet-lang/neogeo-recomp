// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
//
// Updates: the launcher brings its own folder (the app, or the folder the download was unpacked to) up to the
// newest release, downloading only the files that changed. Built in when the port's launcher/CMakeLists.txt sets
// LAUNCHER_UPDATE_REPO (owner/name on GitHub) and LAUNCHER_VERSION (launcher.cmake).
//
// What a release carries for it, beside its archives (a port's release script writes them):
//   update-<platform>.txt   the manifest: every file of the download for that system, as unpacked
//   <sha256>.gz             each of those files, gzipped, named by the SHA-256 of the file itself
// Platforms: macos, linux-x86_64, linux-arm64, windows-x64. The manifest's lines:
//   recomp-update 1
//   version 0.5.0
//   file <sha256> <size> <size gzipped> <x or -> <path, from the top of the download, with / between folders>
// On macOS the top is the app (paths start with Contents/), elsewhere the folder the launcher is in.
//
// Checking reads the repository's releases (api.github.com), takes the newest with a manifest for this system
// (a beta only while this is a beta), and compares the files listed with the ones here. Installing downloads the
// ones that differ into <top>/.update, checks each against its SHA-256, and only then puts them all in place
// (a file a game or the launcher has open is moved aside, not written over), and takes away the files that the
// running version's manifest had and the new one has not. If any step fails nothing is changed. Downloads are
// made by the system's curl (macOS, Windows 10 and later, and Linux distributions have it).
//
// The variable RECOMP_UPDATE_RELEASES names another list of releases (a file:// URL, for tests).
#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace launcher {

class Updater {
public:
    enum class State {
        Idle,          // not checked yet
        Checking,
        UpToDate,
        Available,     // a newer version: install() gets it
        NotInstalled,  // a newer version, but this copy is not a release's download (a build), so it is not changed
        Installing,
        Installed,     // the new version is in place: start the launcher again
        Failed,        // the check or the install did not work (message): nothing was changed
    };
    struct Status {
        State state = State::Idle;
        std::string version;    // the newer version's (Available and after)
        std::string message;    // for the player
        std::string notes_url;  // the release's page
        double fraction = 0.0;  // of the download (Installing)
    };

    // `program_folder`: where the launcher is; `launcher_name`: its program's file name there; `version`: this
    // one's; `repo`: owner/name. `wake` is called (from another thread) when the status changes.
    Updater(std::filesystem::path program_folder, std::string launcher_name, std::string version, std::string repo,
            std::function<void()> wake = {});
    ~Updater();
    void check();    // on a thread
    void install();  // on a thread (Available only; the caller sees to it that no game is running)
    Status status() const;
    bool busy() const;  // checking or installing
    // Starts the launcher again (the new one) with these arguments; the caller then quits. False if it did not start.
    bool relaunch(const std::vector<std::string>& args = {}) const;
    // The folder the download was unpacked to (the app on macOS)
    const std::filesystem::path& top() const { return top_; }

    // (used inside, and by the tests)
    struct Entry {
        std::string sha256, path;
        uint64_t size = 0, packed = 0;
        bool executable = false;
    };
    struct Manifest {
        std::string version;
        std::vector<Entry> files;
    };
    static bool parse_manifest(const std::string& text, Manifest& manifest);
    // <0, 0, >0 as version a is older than, the same as, newer than b ("0.4.0-beta.1" < "0.4.0-beta.2" < "0.4.0")
    static int compare_versions(const std::string& a, const std::string& b);
    static std::string sha256_of_file(const std::filesystem::path& path);
    static const char* platform();

private:
    void set(State state, const std::string& message);
    bool do_check();
    bool do_install();
    void run(void (Updater::*work)());
    void check_work();
    void install_work();

    std::filesystem::path program_folder_, top_;
    std::string launcher_name_, version_, repo_;
    std::function<void()> wake_;
    mutable std::mutex mutex_;
    Status status_;
    std::thread worker_;
    std::atomic<bool> busy_{false};
    // What the check found (read by the install, on the same thread one after the other)
    Manifest next_, current_;
    std::string blob_base_;              // the URL the new release's files are under (ends with /)
    std::vector<size_t> needed_;         // next_.files that differ here
    uint64_t needed_bytes_ = 0;          // their gzipped sizes, summed
};

}  // namespace launcher
