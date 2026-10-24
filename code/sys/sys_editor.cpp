// SPDX-License-Identifier: GPL-3.0-or-later
#include "../client/client.h"
#include "sys_local.h"
#include "sys_editor.h"
#include "../tools/editor/EditorAPI.h"

namespace {
SDL_SharedObject *editorModule = nullptr;
const DoomEditorAPI *editor = nullptr;
char projectRoot[32768]{};
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
    // The editor DLL sits next to the executable, like the old build installed it.
    const char *base = SDL_GetBasePath();
    if (!base) {
        Com_Printf("Cannot locate editor: executable path is unavailable\n");
        return false;
    }
    Q_strncpyz(projectRoot, base, sizeof(projectRoot));
    char modulePath[32768];
    const int needed = SDL_snprintf(modulePath, sizeof(modulePath), "%stoolsx86.dll", projectRoot);
    SDL_free((void *)base);
    if (needed < 0 || needed >= static_cast<int>(sizeof(modulePath))) {
        return false;
    }
    editorModule = SDL_LoadObject(modulePath);
    if (!editorModule) {
        Com_Printf("Cannot load toolsx86.dll (%s). Build DoomEditor.\n", SDL_GetError());
        return false;
    }
    const auto getAPI = reinterpret_cast<DoomEditorGetAPI>(
        SDL_LoadFunction(editorModule, "DoomEditor_GetAPI"));
    editor = getAPI ? getAPI(DoomEditorAPIVersion) : nullptr;
    if (!editor || editor->version != DoomEditorAPIVersion ||
        editor->size != sizeof(DoomEditorAPI) || !editor->initialize || !editor->shutdown ||
        !editor->show || !editor->frame || !editor->visible || !editor->build ||
        !editor->lastError || !editor->draw || !editor->resetGraphics || !editor->sdlevent) {
        Com_Printf("Editor DLL has an incompatible API; rebuild the engine and editor together\n");
        SDL_UnloadObject(editorModule);
        editorModule = nullptr;
        editor = nullptr;
        return false;
    }
    const DoomEditorHost host{DoomEditorAPIVersion,
                              sizeof(DoomEditorHost),
                              glw_state.window,
                              glw_state.context,
                              projectRoot,
                              copyMessage,
                              requestReturn,
                              requestPlay,
                              re.editor};
    const bool initialized = editor->initialize(&host);
    flushMessages();
    if (!initialized) {
        Com_Printf("Cannot initialize editor: %s\n", editor->lastError());
        editor->shutdown();
        SDL_UnloadObject(editorModule);
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
        SDL_Window *window = glw_state.window;
        if (window && !Cvar_VariableIntegerValue("r_hiddenWindow")) {
            if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
                SDL_RestoreWindow(window);
            }
            SDL_RaiseWindow(window);
            // The editor raised the minimum size while it was shown; the
            // engine window goes back to the size the game allows.
            SDL_SetWindowMinimumSize(window, 640, 480);
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
    if (!editor || !editor->visible()) {
        return;
    }
    SDL_Window *window = glw_state.window;
    if (!window || (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)) {
        return;
    }
    // The caller only knows the size of the last frame, so ask the window.
    SDL_GetWindowSize(window, &width, &height);
    editor->draw(width, height);
}
void Sys_EditorRendererReset() {
    if (editor) {
        editor->resetGraphics();
    }
}
bool Sys_EditorProcessEvent(const SDL_Event *event) {
    return editor && editor->sdlevent(event);
}
void Sys_ShutdownEditor() {
    if (editor) {
        editor->shutdown();
        flushMessages();
        editor = nullptr;
    }
    if (editorModule) {
        SDL_UnloadObject(editorModule);
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
