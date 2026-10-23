// SPDX-License-Identifier: GPL-3.0-or-later
#include "../client/client.h"
#include "win_local.h"
#include "win_editor.h"
#include "../tools/editor/EditorAPI.h"
#include <cwchar>

namespace {
HMODULE editorModule = nullptr;
const DoomEditorAPI *editor = nullptr;
wchar_t projectRoot[32768]{};
char pendingMap[64]{};
char messages[64][2048]{};
int messageCount = 0;
bool returnRequested = false, pausedByEditor = false;
int previousPause = 0;

void copyMessage(const char *text) {
    if (messageCount < static_cast<int>(sizeof(messages) / sizeof(messages[0]))) {
        Q_strncpyz(messages[messageCount++], text, sizeof(messages[0]));
    }
}
void flushMessages() {
    for (int i = 0; i < messageCount; ++i) {
        Com_Printf("%s\n", messages[i]);
    }
    messageCount = 0;
}
void requestReturn() {
    returnRequested = true;
}
bool validMapName(const char *name) {
    const size_t length = strlen(name);
    if (!length || length > 32) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        const char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_')) {
            return false;
        }
    }
    return true;
}
void requestPlay(const char *name) {
    if (validMapName(name)) {
        Q_strncpyz(pendingMap, name, sizeof(pendingMap));
    }
}
bool loadEditor() {
    if (editor) {
        return true;
    }
    const auto length = GetModuleFileNameW(nullptr, projectRoot, 32768);
    if (!length || length >= 32768) {
        Com_Printf("Cannot locate editor: executable path is unavailable\n");
        return false;
    }
    auto *separator = wcsrchr(projectRoot, L'\\');
    if (!separator) {
        return false;
    }
    *separator = 0;
    wchar_t modulePath[32768]{};
    if (swprintf_s(modulePath, L"%s\\toolsx86.dll", projectRoot) < 0) {
        return false;
    }
    editorModule = LoadLibraryExW(modulePath, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!editorModule) {
        Com_Printf("Cannot load toolsx86.dll (Windows error %lu). Build DoomEditor.\n",
                   GetLastError());
        return false;
    }
    const auto getAPI =
        reinterpret_cast<DoomEditorGetAPI>(GetProcAddress(editorModule, "DoomEditor_GetAPI"));
    editor = getAPI ? getAPI(DoomEditorAPIVersion) : nullptr;
    if (!editor || editor->version != DoomEditorAPIVersion ||
        editor->size != sizeof(DoomEditorAPI) || !editor->initialize || !editor->shutdown ||
        !editor->show || !editor->frame || !editor->visible || !editor->build ||
        !editor->lastError || !editor->draw || !editor->resetGraphics || !editor->windowMessage) {
        Com_Printf("Editor DLL has an incompatible API; rebuild the engine and editor together\n");
        FreeLibrary(editorModule);
        editorModule = nullptr;
        editor = nullptr;
        return false;
    }
    const DoomEditorHost host{
        DoomEditorAPIVersion, sizeof(DoomEditorHost), g_wv.hWnd,   projectRoot,
        copyMessage,          requestReturn,          requestPlay, re.editor};
    const bool initialized = editor->initialize(&host);
    flushMessages();
    if (!initialized) {
        Com_Printf("Cannot initialize editor: %s\n", editor->lastError());
        editor->shutdown();
        FreeLibrary(editorModule);
        editorModule = nullptr;
        editor = nullptr;
        return false;
    }
    return true;
}
bool validCommand(const char *usage, bool requireMap) {
    if (Cmd_Argc() > 2 || (requireMap && Cmd_Argc() != 2)) {
        Com_Printf("Usage: %s\n", usage);
        return false;
    }
    if (Cmd_Argc() == 2 && !validMapName(Cmd_Argv(1))) {
        Com_Printf(
            "Map names must contain 1..32 letters, digits or underscores; omit paths/extensions\n");
        return false;
    }
    return true;
}
void editorCommand() {
    if (!validCommand("editor [mapname]", false) || !loadEditor()) {
        return;
    }
    const bool opened = editor->show(Cmd_Argc() == 2 ? Cmd_Argv(1) : "");
    flushMessages();
    if (!opened) {
        Com_Printf("Cannot open editor: %s\n", editor->lastError());
        return;
    }
    if (!editor->visible()) {
        return;
    }
    Key_ClearStates();
    Con_Close();
    IN_Activate(qfalse);
    if (!pausedByEditor) {
        previousPause = Cvar_VariableIntegerValue("cl_paused");
    }
    pausedByEditor = true;
    Cvar_Set("cl_paused", "1");
    Cvar_Set("editor_active", "1");
    S_SetEditorPaused(qtrue);
}
void buildCommand(bool navigationOnly) {
    if (!validCommand(navigationOnly ? "aasbuild <mapname>" : "dmap [mapname]", navigationOnly) ||
        !loadEditor()) {
        return;
    }
    const bool started = editor->build(Cmd_Argc() == 2 ? Cmd_Argv(1) : "", navigationOnly);
    flushMessages();
    if (!started) {
        Com_Printf("Cannot start %s: %s\n", navigationOnly ? "aasbuild" : "dmap",
                   editor->lastError());
    }
}
void dmapCommand() {
    buildCommand(false);
}
void aasbuildCommand() {
    buildCommand(true);
}
} // namespace

bool Sys_EditorVisible() {
    return editor && editor->visible();
}
void Sys_InitEditorCommands() {
    Cvar_Get("editor_active", "0", CVAR_ROM);
    Cmd_AddCommand("editor", editorCommand);
    Cmd_AddCommand("dmap", dmapCommand);
    Cmd_AddCommand("aasbuild", aasbuildCommand);
}
void Sys_EditorFrame() {
    if (!editor) {
        return;
    }
    editor->frame();
    flushMessages();
    if (Sys_EditorVisible()) {
        Cvar_Set("cl_paused", "1");
        S_SetEditorPaused(qtrue);
    }
    if (returnRequested) {
        returnRequested = false;
        Key_ClearStates();
        Cvar_Set("editor_active", "0");
        if (pausedByEditor) {
            Cvar_Set("cl_paused", previousPause ? "1" : "0");
            S_SetEditorPaused(qfalse);
            pausedByEditor = false;
        }
        if (!Cvar_VariableIntegerValue("r_hiddenWindow")) {
            if (IsIconic(g_wv.hWnd)) {
                ShowWindow(g_wv.hWnd, SW_RESTORE);
            }
            SetForegroundWindow(g_wv.hWnd);
            SetFocus(g_wv.hWnd);
        }
        IN_Activate(qtrue);
        Com_Printf("Returned to game; editor document retained\n");
    }
    if (*pendingMap) {
        // Run only after the DLL has returned. VM errors may longjmp; no editor
        // allocations, stack objects or exception scopes span that boundary.
        char command[128];
        Com_sprintf(command, sizeof(command), "spdevmap %s\n", pendingMap);
        pendingMap[0] = 0;
        Cbuf_AddText(command);
    }
}
void Sys_EditorDraw(int width, int height) {
    if (editor && editor->visible() && !IsIconic(g_wv.hWnd)) {
        RECT client{};
        if (GetClientRect(g_wv.hWnd, &client)) {
            width = client.right - client.left;
            height = client.bottom - client.top;
        }
        editor->draw(width, height);
    }
}
void Sys_EditorRendererReset() {
    if (editor) {
        editor->resetGraphics();
    }
}
bool Sys_EditorWindowMessage(void *window, unsigned int message, uintptr_t wparam,
                             intptr_t lparam) {
    return editor && editor->windowMessage(window, message, wparam, lparam);
}
void Sys_ShutdownEditor() {
    if (editor) {
        editor->shutdown();
        flushMessages();
        editor = nullptr;
    }
    if (editorModule) {
        FreeLibrary(editorModule);
        editorModule = nullptr;
    }
    if (pausedByEditor) {
        Cvar_Set("cl_paused", previousPause ? "1" : "0");
        S_SetEditorPaused(qfalse);
    }
    pausedByEditor = returnRequested = false;
    pendingMap[0] = 0;
    Cvar_Set("editor_active", "0");
    Cmd_RemoveCommand("editor");
    Cmd_RemoveCommand("dmap");
    Cmd_RemoveCommand("aasbuild");
}
