// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

constexpr uint32_t DoomCompilerAPIVersion = 1;

// Borrowed data remains valid until the synchronous call returns. The caller
// supplies a 32 MiB worker stack and a fresh module instance for each invocation.
struct DoomCompilerRequest {
    uint32_t version;
    uint32_t size;
    const wchar_t *projectRoot;
    const wchar_t *logPath;
    int argumentCount;
    const char *const *arguments;
    bool (*cancelled)(void *context);
    void *context;
};
using DoomCompilerRun = int (*)(const DoomCompilerRequest *request);
