// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include "../../renderer/editor_render.h"

// The editor owns all allocations and catches exceptions before returning to the
// engine. Callbacks only copy transient strings; they never enter Com_Error or a VM.
constexpr uint32_t DoomEditorAPIVersion = 2;
struct DoomEditorHost {
    uint32_t version;
    uint32_t size;
    void *gameWindow;
    const wchar_t *projectRoot;
    void (*print)(const char *text);
    void (*returnToGame)();
    void (*playMap)(const char *mapName);
    EditorRenderAPI renderer;
};
struct DoomEditorAPI {
    uint32_t version;
    uint32_t size;
    bool (*initialize)(const DoomEditorHost *host);
    void (*shutdown)();
    bool (*show)(const char *mapName);
    void (*frame)();
    bool (*visible)();
    bool (*build)(const char *mapName, bool navigationOnly);
    const char *(*lastError)();
    void (*draw)(int width, int height);
    void (*resetGraphics)();
    bool (*windowMessage)(void *window, uint32_t message, uintptr_t wparam, intptr_t lparam);
};
using DoomEditorGetAPI = const DoomEditorAPI *(__cdecl *)(uint32_t version);
