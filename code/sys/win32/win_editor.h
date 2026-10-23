// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
void Sys_InitEditorCommands();
void Sys_ShutdownEditor();
void Sys_EditorFrame();
bool Sys_EditorVisible();
void Sys_EditorDraw(int width, int height);
void Sys_EditorRendererReset();
void Sys_UpdateWindowSize();
bool Sys_EditorWindowMessage(void *window, unsigned int message, uintptr_t wparam, intptr_t lparam);
