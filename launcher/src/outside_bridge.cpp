// SPDX-License-Identifier: GPL-3.0-only
// Part of cps2recomp (the user's fork).
//
// The collection's program for a game that plays in a program of its own (description.cpp, OutsideGame: UltraCombo for
// Killer Instinct, FBNeo for 3rd Strike): built as the game's id beside the launcher (kinst, kinst2, sfiii3), it starts the
// program the launcher gives it (--rom) with BRIDGE_ARGS ("--game ki1", "sfiii3nr1 -w"), in the program's own folder, and
// ends as it ends. It has no features to list (--list-features: none), and the launcher's other arguments are not the
// program's: it leaves them out, except those after "--", which go after BRIDGE_ARGS (online play: "--host 7845",
// "--join 127.0.0.1:PORT"). If this ends (the launcher stops it: a room left), the program ends with it. Opened by itself
// (no --rom; Windows), it asks the launcher beside it to start the game.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifndef BRIDGE_ARGS
#error "BRIDGE_ARGS (the program's arguments, \"--game ki1\" say) is not defined"
#endif

#ifndef _WIN32
static pid_t child = 0;
static void end_child(int) {
    if (child > 0) {
        kill(child, SIGTERM);
    }
    _exit(1);
}
#endif

int main(int argc, char** argv) {
    std::string program;
    std::vector<std::string> extra;  // (after "--": the program's own)
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--") == 0) {
            extra.assign(argv + i + 1, argv + argc);
            break;
        }
        if (std::strcmp(argv[i], "--list-features") == 0) {
            return 0;
        }
        if (std::strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
            program = argv[++i];
        }
    }
#ifdef _WIN32
    // (the path as the launcher wrote it: UTF-8)
    auto wide = [](const std::string& text) {
        int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        std::wstring out(static_cast<size_t>(size > 0 ? size - 1 : 0), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), size);
        return out;
    };
    if (program.empty()) {
        // Opened by itself (a double click): the launcher beside it starts the game as its Play does, with the program
        // it found or was given (--play ID), and starts this again with --rom
        wchar_t self[MAX_PATH * 4];
        DWORD length = GetModuleFileNameW(nullptr, self, MAX_PATH * 4);
        std::filesystem::path here = std::filesystem::path(std::wstring(self, length)).parent_path();
        std::wstring launcher = (here / BRIDGE_LAUNCHER).wstring();
        std::wstring play = L"\"" + launcher + L"\" --play " + wide(BRIDGE_ID);
        STARTUPINFOW startup{};
        startup.cb = sizeof startup;
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(launcher.c_str(), play.data(), nullptr, nullptr, FALSE, 0, nullptr, here.wstring().c_str(), &startup, &process)) {
            MessageBoxW(nullptr, (L"Open " + wide(BRIDGE_LAUNCHER) + L" and press Play.").c_str(), wide(BRIDGE_ID).c_str(), MB_ICONINFORMATION);
            return 1;
        }
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return 0;
    }
    const std::filesystem::path folder = std::filesystem::path(program).parent_path();
    const std::wstring exe = wide(program);
    const std::wstring directory = wide(folder.string());
    std::wstring line = L"\"" + exe + L"\" " + wide(BRIDGE_ARGS);
    for (const std::string& arg : extra) {
        line += L" \"" + wide(arg) + L"\"";
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(exe.c_str(), line.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, directory.c_str(), &startup, &process)) {
        std::fprintf(stderr, "%s could not be started (error %lu).\n", program.c_str(), GetLastError());
        return 1;
    }
    // (in a job that ends it when this ends, before it runs: stopping this, as the launcher does when a room is left,
    // closes the game too)
    if (HANDLE job = CreateJobObjectW(nullptr, nullptr)) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof limits);
        AssignProcessToJobObject(job, process.hProcess);
    }
    ResumeThread(process.hThread);
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    return static_cast<int>(code);
#else
    if (program.empty()) {
        std::fprintf(stderr, "The game's program isn't chosen: open the launcher and choose it.\n");
        return 1;
    }
    const std::filesystem::path folder = std::filesystem::path(program).parent_path();
    std::vector<std::string> args = {program};
    std::istringstream words(BRIDGE_ARGS);
    for (std::string word; words >> word;) {
        args.push_back(word);
    }
    args.insert(args.end(), extra.begin(), extra.end());
    std::vector<char*> argv2;
    for (std::string& arg : args) {
        argv2.push_back(arg.data());
    }
    argv2.push_back(nullptr);
    pid_t pid = fork();
    if (pid == 0) {
        if (chdir(folder.c_str()) == 0) {
            execv(program.c_str(), argv2.data());
        }
        _exit(127);
    }
    child = pid;
    signal(SIGTERM, end_child);
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) < 0) {
        std::fprintf(stderr, "%s could not be started.\n", program.c_str());
        return 1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}
