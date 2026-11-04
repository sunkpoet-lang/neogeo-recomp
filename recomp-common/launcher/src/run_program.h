// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#pragma once
// Running another program and reading what it writes (with no console window on Windows): the system's curl, for
// the updates (update.h) and the Online card's list of open matches (online.h).

#include <filesystem>
#include <string>
#include <vector>

namespace launcher {

// Runs a program (no shell) and waits for it: its exit code (-1: it did not start), and its output if asked
int run_program(const std::filesystem::path& program, const std::vector<std::string>& args, std::string* output);
// The system's curl (macOS, Windows 10 and later and Linux have one)
std::filesystem::path curl_program();
#ifdef _WIN32
// A command line that CommandLineToArgvW reads back as `program` and `args`
std::wstring command_line(const std::filesystem::path& program, const std::vector<std::string>& args);
#endif

}  // namespace launcher
