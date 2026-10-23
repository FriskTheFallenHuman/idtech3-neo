// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
void CG_DoomMapInit();
void CG_DoomMapUpdate();
void CG_DrawDoomMap();
qboolean CG_DoomMapVisible();
void trap_R_DrawDoomMap(int x, int y, int width, int height, const byte *pixels);
