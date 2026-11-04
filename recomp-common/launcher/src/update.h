// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
//
// Updates: the launcher brings its own folder (the app, or the folder the download was unpacked to) up to the
// newest release. Built in when the port's launcher/CMakeLists.txt sets LAUNCHER_UPDATE_REPO (owner/name on
// GitHub) and LAUNCHER_VERSION (launcher.cmake).
//
// What a release carries for it, beside its archives (a port's release script writes it), and nothing else:
//   update-<platform>.txt   the manifest: that system's archive, and every file in it
// Platforms: macos, linux-x86_64, linux-arm64, windows-x64. The manifest's lines:
//   recomp-update 2
//   version 0.5.0
//   archive <sha256> <size> <file name of the archive, among the release's files>
//   top <the folder at the top of the archive: the app on macOS, the folder the download is unpacked to elsewhere>
//   file <sha256> <size> <x or -> <path, from that top, with / between folders>
// On macOS the top is the app (paths start with Contents/), elsewhere the folder the launcher is in. (Manifests of
// the first kind, "recomp-update 1", named each file gzipped as a release file of its own; they are still read for
// the files the running version had.)
//
// Checking reads the repository's releases (api.github.com), takes the newest with a manifest for this system
// (a beta only while this is a beta), and compares the files listed with the ones here. Installing downloads the
// archive into <top>/.update, checks it against its SHA-256, unpacks it there (the system's ditto on macOS, tar
// elsewhere) and checks each file that differs; only then it puts them all in place (a file a game or the
// launcher has open is moved aside, not written over), and takes away the files that the running version's
// manifest had and the new one has not. If any step fails nothing is changed. Downloads are made by the system's
// curl (macOS, Windows 10 and later, and Linux distributions have it, and tar).
//
// Android: an app can't replace its own files, only the system's installer can. So there's no manifest: the check
// takes the newest release with an APK, and installing downloads that APK into the app's cache, checks it against the
// release's SHA256SUMS.txt, and hands it to the port's activity (installPackage(String path): 0 Android's installer
// opened, 1 Android's Settings opened first, for the player to allow the app to install apps; anything else: it
// couldn't). Android asks the player to confirm, and installs it over the app (the same signing key: settings, saves
// and ROM sets stay). A release without SHA256SUMS.txt: the update card's Download opens the APK in the browser.
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
        Downloaded,    // (Android) the new APK is downloaded and checked, and Android's installer asks the player to
                       // confirm; install() opens the installer again (the player said not now, or allowed it in Settings)
    };
    struct Status {
        State state = State::Idle;
        std::string version;    // the newer version's (Available and after)
        std::string message;    // for the player
        std::string notes_url;  // the release's page
        std::string download_url; // (Android) the newer release's APK, for the player to download and open when the
                                  // release has no SHA256SUMS.txt to check it with (NotInstalled)
        double fraction = 0.0;  // of the download (Installing)
    };

    // `program_folder`: where the launcher is; `launcher_name`: its program's file name there; `version`: this
    // one's; `repo`: owner/name. `wake` is called (from another thread) when the status changes.
    Updater(std::filesystem::path program_folder, std::string launcher_name, std::string version, std::string repo,
            std::function<void()> wake = {});
    ~Updater();
    void check();    // on a thread
    void install();  // on a thread (Available, or Downloaded on Android; the caller sees to it that no game is running)
    Status status() const;
    bool busy() const;  // checking or installing
    // Starts the launcher again (the new one) with these arguments; the caller then quits. False if it did not start.
    bool relaunch(const std::vector<std::string>& args = {}) const;
    // The folder the download was unpacked to (the app on macOS)
    const std::filesystem::path& top() const { return top_; }

    // (used inside, and by the tests)
    struct Entry {
        std::string sha256, path;
        uint64_t size = 0;
        bool executable = false;
    };
    struct Manifest {
        std::string version;
        std::string archive, archive_sha256, top;  // (none in a manifest of the first kind)
        uint64_t archive_size = 0;
        std::vector<Entry> files;
    };
    static bool parse_manifest(const std::string& text, Manifest& manifest);
    // <0, 0, >0 as version a is older than, the same as, newer than b ("0.4.0-beta.1" < "0.4.0-beta.2" < "0.4.0")
    static int compare_versions(const std::string& a, const std::string& b);
    static std::string sha256_of_file(const std::filesystem::path& path);
    // A file's SHA-256 in a SHA256SUMS.txt (lines of "<sha256>  <name>", sha256sum's and shasum's, "*" before a
    // name read in binary), or "" if it isn't there
    static std::string sum_in(const std::string& sums, const std::string& name);
    static const char* platform();

private:
    void set(State state, const std::string& message);
    bool do_check();
    bool do_install();
    bool do_install_apk();  // (Android)
    // Downloads url to a file, its progress shown against `size`: curl's exit code
    int download(const std::string& url, const std::filesystem::path& to, uint64_t size);
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
    std::string archive_url_;            // the new release's archive
    std::vector<size_t> needed_;         // next_.files that differ here
#ifdef __ANDROID__
    // (Android) the new release's APK and its SHA256SUMS.txt, and the APK once it is downloaded and checked
    std::string apk_url_, apk_name_, sums_url_;
    uint64_t apk_size_ = 0;
    std::filesystem::path apk_;
#endif
};

}  // namespace launcher
