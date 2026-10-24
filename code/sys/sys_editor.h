// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
typedef union SDL_Event SDL_Event;
void Sys_InitEditorCommands();
void Sys_ShutdownEditor();
void Sys_EditorFrame();
bool Sys_EditorVisible();
void Sys_EditorDraw(int width, int height);
void Sys_EditorRendererReset();
void Sys_UpdateWindowSize();
bool Sys_EditorProcessEvent(const SDL_Event *event);
