// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#include "run_program.h"

#include <system_error>

#if defined(__ANDROID__)
#include <SDL3/SDL_system.h>
#include <jni.h>
#elif defined(_WIN32)
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

#ifdef _WIN32
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
}  // namespace

std::wstring command_line(const fs::path& program, const std::vector<std::string>& args) {
    std::wstring line = quoted(program.wstring());
    for (const std::string& arg : args) {
        line += L" " + quoted(wide(arg));
    }
    return line;
}
#endif

#ifdef __ANDROID__
namespace {
// Android has no curl, nor programs to run: the app's activity makes the request instead, from curl's arguments (in the
// port's Java: String curl(String[] arguments), what curl would write, or null where curl would fail)
int android_curl(const std::vector<std::string>& args, std::string* output) {
    JNIEnv* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) {
        return -1;
    }
    int code = -1;
    jclass activity_class = env->GetObjectClass(activity);
    jmethodID curl = env->GetMethodID(activity_class, "curl", "([Ljava/lang/String;)Ljava/lang/String;");
    if (curl) {
        jclass string_class = env->FindClass("java/lang/String");
        jobjectArray arguments = env->NewObjectArray(static_cast<jsize>(args.size()), string_class, nullptr);
        for (size_t i = 0; i < args.size(); i++) {
            jstring arg = env->NewStringUTF(args[i].c_str());
            env->SetObjectArrayElement(arguments, static_cast<jsize>(i), arg);
            env->DeleteLocalRef(arg);
        }
        jstring answer = static_cast<jstring>(env->CallObjectMethod(activity, curl, arguments));
        if (!env->ExceptionCheck() && answer) {
            const char* chars = env->GetStringUTFChars(answer, nullptr);
            if (output && chars) {
                *output = chars;
            }
            env->ReleaseStringUTFChars(answer, chars);
            env->DeleteLocalRef(answer);
            code = 0;
        } else {
            code = 7;  // (curl's "could not connect")
        }
        env->DeleteLocalRef(arguments);
        env->DeleteLocalRef(string_class);
    }
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    env->DeleteLocalRef(activity_class);
    env->DeleteLocalRef(activity);
    return code;
}
}  // namespace
#endif

// Runs a program (no shell) and waits for it: its exit code (-1: it did not start), and its output if asked
int run_program(const fs::path& program, const std::vector<std::string>& args, std::string* output) {
#if defined(__ANDROID__)
    return program == curl_program() ? android_curl(args, output) : -1;
#elif defined(_WIN32)
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
#elif defined(__ANDROID__)
    return "curl";  // (run_program's own: android_curl)
#else
    std::error_code error;
    return fs::exists("/usr/bin/curl", error) ? fs::path("/usr/bin/curl") : fs::path("curl");
#endif
}

}  // namespace launcher
