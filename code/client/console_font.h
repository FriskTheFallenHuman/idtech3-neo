// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
void Con_FontInit();
void Con_FontUpdate();
int Con_FontWidth();
int Con_FontHeight();
void Con_FontDrawChar(int x, int y, int character);
void Con_FontDrawString(int x, int y, const char *text, const float *color);
