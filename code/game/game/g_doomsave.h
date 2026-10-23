// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

struct gclient_s;

// The save's entity/client records own the actor roster. Map spawns and the
// current difficulty must not decide which saved client slots can be restored.
void G_DoomSaveBeginRestore();
int G_DoomSavePrepareClients();
void G_DoomSaveValidateClient(int slot, const gclient_s &client);
void G_DoomSaveFinishRestore(const bool *clientRecords);
