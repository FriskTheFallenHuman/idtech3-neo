// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

bool DoomBotsActive();
void G_DoomBotsInit();
void G_DoomBotsShutdown();
void G_DoomBotsFrame();
void G_DoomBotsMakeRoom();
void G_DoomBotDisconnect(int clientNum);
qboolean G_DoomBotCommand(const char *command);
