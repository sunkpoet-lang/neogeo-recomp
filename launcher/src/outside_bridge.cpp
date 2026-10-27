// SPDX-License-Identifier: GPL-3.0-only
// Part of cps2recomp (the user's fork).
//
// The collection's program for a game that plays in a program of its own (description.cpp, OutsideGame: UltraCombo for
// Killer Instinct, FBNeo for 3rd Strike): built as the game's id beside the launcher (kinst, kinst2, sfiii3), it starts the
// program the launcher gives it (--rom) with BRIDGE_ARGS ("--game ki1", "sfiii3nr1 -w"), in the program's own folder, and
// ends as it ends. It has no features to list (--list-features: none), and the launcher's other arguments are not the
// program's: it leaves them out.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifndef BRIDGE_ARGS
#error "BRIDGE_ARGS (the program's arguments, \"--game ki1\" say) is not defined"
#endif

int main(int argc, char** argv) {
    std::string program;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--list-features") == 0) {
            return 0;
        }
        if (std::strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
            program = argv[++i];
        }
    }
    if (program.empty()) {
        std::fprintf(stderr, "The game's program isn't chosen: open the launcher and choose it.\n");
        return 1;
    }
    const std::filesystem::path folder = std::filesystem::path(program).parent_path();
#ifdef _WIN32
    // (the path as the launcher wrote it: UTF-8)
    auto wide = [](const std::string& text) {
        int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        std::wstring out(static_cast<size_t>(size > 0 ? size - 1 : 0), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), size);
        return out;
    };
    const std::wstring exe = wide(program);
    const std::wstring directory = wide(folder.string());
    std::wstring line = L"\"" + exe + L"\" " + wide(BRIDGE_ARGS);
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(exe.c_str(), line.data(), nullptr, nullptr, FALSE, 0, nullptr, directory.c_str(), &startup, &process)) {
        std::fprintf(stderr, "%s could not be started (error %lu).\n", program.c_str(), GetLastError());
        return 1;
    }
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    return static_cast<int>(code);
#else
    std::vector<std::string> args = {program};
    std::istringstream words(BRIDGE_ARGS);
    for (std::string word; words >> word;) {
        args.push_back(word);
    }
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
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) < 0) {
        std::fprintf(stderr, "%s could not be started.\n", program.c_str());
        return 1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}
