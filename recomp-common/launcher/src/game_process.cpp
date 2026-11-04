// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
// (Quoting an argument for Windows follows the launcher of wiiparty-recomp by arelkair, GPL-3.0-or-later:
// https://github.com/arelkair/wiiparty-recomp, launcher/src/subrun_program.cpp; see THIRD_PARTY_NOTICES.md.)
#include "game_process.h"

#include <SDL3/SDL.h>

#include <cstdio>

#if defined(__ANDROID__)
#include <dlfcn.h>
#include <jni.h>

#include <cstdlib>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
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

#if defined(__ANDROID__)

// Android: a game is a library (lib<id>.so beside the launcher's), started in a process of its own by the app's
// activity, which has these (in the port's Java):
//   boolean startGame(String library, String[] arguments, String folder, String log)   lib<library>.so's main
//   int gameResult()   what its main returned (-1: it ended another way), or Integer.MIN_VALUE while it runs
namespace {

constexpr jint kStillRunning = static_cast<jint>(0x80000000u);

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
    std::string library = program.stem().string();  // (libsfa2 of libsfa2.so)
    if (library.rfind("lib", 0) == 0) {
        library.erase(0, 3);
    }
    JNIEnv* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) {
        error = "The game could not be started (no activity).";
        return false;
    }
    jclass activity_class = env->GetObjectClass(activity);
    jmethodID start_game = env->GetMethodID(activity_class, "startGame", "(Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z");
    jclass string_class = env->FindClass("java/lang/String");
    jobjectArray arguments = env->NewObjectArray(static_cast<jsize>(args.size()), string_class, nullptr);
    for (size_t i = 0; i < args.size(); i++) {
        jstring arg = env->NewStringUTF(args[i].c_str());
        env->SetObjectArrayElement(arguments, static_cast<jsize>(i), arg);
        env->DeleteLocalRef(arg);
    }
    jstring library_text = env->NewStringUTF(library.c_str());
    jstring folder_text = env->NewStringUTF(folder.string().c_str());
    jstring log_text = env->NewStringUTF(log.string().c_str());
    bool started = start_game && env->CallBooleanMethod(activity, start_game, library_text, arguments, folder_text, log_text);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        started = false;
    }
    env->DeleteLocalRef(log_text);
    env->DeleteLocalRef(folder_text);
    env->DeleteLocalRef(library_text);
    env->DeleteLocalRef(arguments);
    env->DeleteLocalRef(string_class);
    env->DeleteLocalRef(activity_class);
    env->DeleteLocalRef(activity);
    if (!started) {
        error = "The game could not be started.";
        return false;
    }
    running_ = true;
    waiter_ = std::thread([this] {
        JNIEnv* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());  // (SDL attaches the thread)
        jint result = kStillRunning;
        while (result == kStillRunning) {
            SDL_Delay(100);
            jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
            if (!activity) {
                result = -1;
                break;
            }
            jclass activity_class = env->GetObjectClass(activity);
            jmethodID game_result = env->GetMethodID(activity_class, "gameResult", "()I");
            result = game_result ? env->CallIntMethod(activity, game_result) : -1;
            env->DeleteLocalRef(activity_class);
            env->DeleteLocalRef(activity);
        }
        exit_code_ = result;
        running_ = false;
        SDL_Event event{};
        event.type = SDL_EVENT_USER;
        SDL_PushEvent(&event);  // redraw the launcher
    });
    return true;
}

// No program to run: --list-features is asked of the game's library itself, which has
//   extern "C" char* RecompListFeatures(void)   the text --list-features writes, for the caller to free()
std::string GameProcess::output_of(const std::filesystem::path& program, const std::vector<std::string>& args) {
    if (args != std::vector<std::string>{"--list-features"}) {
        return "";
    }
    void* library = dlopen(program.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        SDL_Log("%s", dlerror());
        return "";
    }
    std::string output;
    if (auto list = reinterpret_cast<char* (*)()>(dlsym(library, "RecompListFeatures"))) {
        char* text = list();
        output = text ? text : "";
        std::free(text);
    }
    dlclose(library);
    return output;
}

#elif !defined(_WIN32)

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
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pid_ = pid;
    }
    waiter_ = std::thread([this, pid] {
        int status = 0;
        waitpid(pid, &status, 0);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pid_ = 0;
        }
        exit_code_ = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        running_ = false;
        SDL_Event event{};
        event.type = SDL_EVENT_USER;
        SDL_PushEvent(&event);  // redraw the launcher
    });
    return true;
}

void GameProcess::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pid_ > 0) {
        kill(pid_, SIGTERM);
    }
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
    // (with its output in a log, a console program such as the online tunnel needs no window of its own; a game, a
    // windowed program, is not affected)
    DWORD flags = output != INVALID_HANDLE_VALUE ? CREATE_NO_WINDOW : 0;
    BOOL started = CreateProcessW(program.wstring().c_str(), line.data(), nullptr, nullptr, output != INVALID_HANDLE_VALUE, flags, nullptr, directory.c_str(),
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
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handle_ = handle;
    }
    waiter_ = std::thread([this, handle] {
        WaitForSingleObject(handle, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(handle, &code);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            handle_ = nullptr;
        }
        CloseHandle(handle);
        exit_code_ = static_cast<int>(code);
        running_ = false;
        SDL_Event event{};
        event.type = SDL_EVENT_USER;
        SDL_PushEvent(&event);  // redraw the launcher
    });
    return true;
}

void GameProcess::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle_) {
        TerminateProcess(static_cast<HANDLE>(handle_), 1);
    }
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
