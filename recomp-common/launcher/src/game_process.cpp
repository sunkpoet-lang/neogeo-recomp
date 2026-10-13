// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// (Quoting an argument for Windows follows the launcher of wiiparty-recomp by arelkair, GPL-3.0-or-later:
// https://github.com/arelkair/wiiparty-recomp, launcher/src/subprocess.cpp; see THIRD_PARTY_NOTICES.md.)
#include "game_process.h"

#include <SDL3/SDL.h>

#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

GameProcess::~GameProcess() {
    if (waiter_.joinable()) {
        waiter_.detach();
    }
}

#ifndef _WIN32

bool GameProcess::start(const std::filesystem::path& program, const std::vector<std::string>& args, const std::filesystem::path& folder, std::string& error,
                        const std::filesystem::path& log) {
    if (running_) {
        error = "The game is already running.";
        return false;
    }
    if (waiter_.joinable()) {
        waiter_.join();
    }
    std::vector<std::string> owned;
    owned.push_back(program.string());
    owned.insert(owned.end(), args.begin(), args.end());
    std::vector<char*> argv;
    for (std::string& arg : owned) {
        argv.push_back(arg.data());
    }
    argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
    posix_spawn_file_actions_addchdir_np(&actions, folder.string().c_str());  // the non-_np one needs macOS 26
#ifdef __clang__
#pragma clang diagnostic pop
#endif
    std::string log_path = log.string();
    if (!log.empty()) {
        posix_spawn_file_actions_addopen(&actions, 1, log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        posix_spawn_file_actions_adddup2(&actions, 1, 2);
    }
    pid_t pid = 0;
    int result = posix_spawn(&pid, owned[0].c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (result != 0) {
        error = "The game could not be started (" + std::to_string(result) + ").";
        return false;
    }
    running_ = true;
    waiter_ = std::thread([this, pid] {
        int status = 0;
        waitpid(pid, &status, 0);
        exit_code_ = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        running_ = false;
        SDL_Event event{};
        event.type = SDL_EVENT_USER;
        SDL_PushEvent(&event);  // redraw the launcher
    });
    return true;
}

std::string GameProcess::output_of(const std::filesystem::path& program, const std::vector<std::string>& args) {
    std::string command = "'" + program.string() + "'";
    for (const std::string& arg : args) {
        command += " '" + arg + "'";
    }
    std::string output;
    if (std::FILE* pipe = popen(command.c_str(), "r")) {
        char buffer[4096];
        size_t got;
        while ((got = std::fread(buffer, 1, sizeof buffer, pipe)) > 0) {
            output.append(buffer, got);
        }
        pclose(pipe);
    }
    return output;
}

#else

namespace {

std::wstring wide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

// One argument quoted for CommandLineToArgvW.
std::wstring quote(const std::wstring& arg) {
    std::wstring out = L"\"";
    size_t slashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') {
            slashes++;
            continue;
        }
        out.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, L'\\');
    out += L'"';
    return out;
}

std::wstring command_line(const std::filesystem::path& program, const std::vector<std::string>& args) {
    std::wstring line = quote(program.wstring());
    for (const std::string& arg : args) {
        line += L" " + quote(wide(arg));
    }
    return line;
}

}  // namespace

bool GameProcess::start(const std::filesystem::path& program, const std::vector<std::string>& args, const std::filesystem::path& folder, std::string& error,
                        const std::filesystem::path& log) {
    if (running_) {
        error = "The game is already running.";
        return false;
    }
    if (waiter_.joinable()) {
        waiter_.join();
    }
    std::wstring line = command_line(program, args);
    std::wstring directory = folder.wstring();
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    HANDLE output = INVALID_HANDLE_VALUE;
    if (!log.empty()) {
        SECURITY_ATTRIBUTES security{sizeof security, nullptr, TRUE};
        output = CreateFileW(log.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (output != INVALID_HANDLE_VALUE) {
            startup.dwFlags = STARTF_USESTDHANDLES;
            startup.hStdOutput = output;
            startup.hStdError = output;
            startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        }
    }
    PROCESS_INFORMATION process{};
    BOOL started = CreateProcessW(program.wstring().c_str(), line.data(), nullptr, nullptr, output != INVALID_HANDLE_VALUE, 0, nullptr, directory.c_str(),
                                  &startup, &process);
    if (output != INVALID_HANDLE_VALUE) {
        CloseHandle(output);
    }
    if (!started) {
        error = "The game could not be started (error " + std::to_string(GetLastError()) + ").";
        return false;
    }
    CloseHandle(process.hThread);
    running_ = true;
    HANDLE handle = process.hProcess;
    waiter_ = std::thread([this, handle] {
        WaitForSingleObject(handle, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(handle, &code);
        CloseHandle(handle);
        exit_code_ = static_cast<int>(code);
        running_ = false;
        SDL_Event event{};
        event.type = SDL_EVENT_USER;
        SDL_PushEvent(&event);  // redraw the launcher
    });
    return true;
}

std::string GameProcess::output_of(const std::filesystem::path& program, const std::vector<std::string>& args) {
    SECURITY_ATTRIBUTES security{sizeof security, nullptr, TRUE};
    HANDLE read = nullptr;
    HANDLE write = nullptr;
    if (!CreatePipe(&read, &write, &security, 0)) {
        return "";
    }
    SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = write;
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    std::wstring line = command_line(program, args);
    std::string output;
    if (CreateProcessW(program.wstring().c_str(), line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        CloseHandle(write);
        write = nullptr;
        char buffer[4096];
        DWORD got = 0;
        while (ReadFile(read, buffer, sizeof buffer, &got, nullptr) && got > 0) {
            output.append(buffer, got);
        }
        WaitForSingleObject(process.hProcess, INFINITE);
        CloseHandle(process.hProcess);
        CloseHandle(process.hThread);
    }
    if (write) {
        CloseHandle(write);
    }
    CloseHandle(read);
    return output;
}

#endif
