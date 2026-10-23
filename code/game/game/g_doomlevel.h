// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
void G_DoomLevelInit(const void *data);
bool G_DoomBeginIntermission(gentity_t *player, bool secret);
bool G_DoomLevelFrame();
void G_DoomApplyCarry(gentity_t *player);
