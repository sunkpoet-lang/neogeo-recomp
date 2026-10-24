// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The build without translated code: everything runs in the interpreter.
#include "native.h"

namespace cps2recomp {
const NativeCode kNativeCode = {nullptr, 0, nullptr, 0, nullptr, 0, "cps2recomp: built with no translated code"};
} // namespace cps2recomp
