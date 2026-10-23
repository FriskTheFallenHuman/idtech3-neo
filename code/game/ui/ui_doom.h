// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
void UI_DoomInit();
qboolean UI_DoomOpen(uiMenuCommand_t menu);
qboolean UI_DoomActive();
qboolean UI_DoomFullscreen();
void UI_DoomDraw();
void UI_DoomKey(int key, qboolean down);
void UI_DoomMouse(int dx, int dy);
qboolean UI_DoomCommand(const char *command);

qboolean UI_DoomDrawLoading();
