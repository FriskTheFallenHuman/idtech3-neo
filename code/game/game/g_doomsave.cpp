// SPDX-License-Identifier: GPL-3.0-or-later
#include "g_local.h"
#include "g_doomsave.h"

namespace {
bool IsSavedActor(int slot) {
    const auto &entity = g_entities[slot];
    return entity.inuse && entity.doomNpc.type >= doommonster::Zombie &&
           entity.doomNpc.type <= doommonster::SpiderMastermind &&
           (entity.r.svFlags & (SVF_AI | SVF_BOT)) == (SVF_AI | SVF_BOT) &&
           entity.s.eType == ET_PLAYER && entity.s.number == slot && entity.s.clientNum == slot &&
           entity.client == &level.clients[slot];
}
} // namespace

void G_DoomSaveBeginRestore() {
    // Single-player reserves slot zero for the human. Release all other server
    // slots before decoding, including orphan slots left by an older loader.
    for (int slot = 1; slot < level.maxclients; ++slot) {
        auto &entity = g_entities[slot];
        if (entity.r.linked) {
            trap_UnlinkEntity(&entity);
        }
        trap_BotFreeClient(slot);
        entity.inuse = qfalse;
        memset(&level.clients[slot], 0, sizeof(gclient_t));
    }
}

int G_DoomSavePrepareClients() {
    bool temporarySlots[MAX_CLIENTS]{};
    int count = 0;
    for (int slot = 1; slot < level.maxclients; ++slot) {
        if (!g_entities[slot].inuse) {
            continue;
        }
        if (!IsSavedActor(slot)) {
            G_Error("G_LoadGame: invalid saved monster entity in client slot %d", slot);
        }
        // The existing allocator chooses the first free slot. Hold gaps until
        // all saved indices are reserved, preserving references in sparse saves.
        for (;;) {
            const int allocated = trap_BotAllocateClient();
            if (allocated < 1 || allocated > slot) {
                G_Error("G_LoadGame: cannot reserve saved monster slot %d (allocated %d)", slot,
                        allocated);
            }
            if (allocated == slot) {
                break;
            }
            temporarySlots[allocated] = true;
        }
        ++count;
    }
    for (int slot = 1; slot < level.maxclients; ++slot) {
        if (temporarySlots[slot]) {
            trap_BotFreeClient(slot);
        }
    }
    return count;
}

void G_DoomSaveValidateClient(int slot, const gclient_t &client) {
    if (slot < 0 || slot >= level.maxclients || client.ps.clientNum != slot ||
        client.pers.connected != CON_CONNECTED || !g_entities[slot].inuse ||
        g_entities[slot].client != &level.clients[slot]) {
        G_Error("G_LoadGame: invalid saved client identity in slot %d", slot);
    }
    if ((slot == 0 &&
         (client.ps.stats[STAT_DOOM_MONSTER] != 0 || g_entities[slot].doomNpc.type != 0 ||
          (g_entities[slot].r.svFlags & (SVF_AI | SVF_BOT)))) ||
        (slot > 0 && (!IsSavedActor(slot) ||
                      client.ps.stats[STAT_DOOM_MONSTER] != g_entities[slot].doomNpc.type))) {
        G_Error("G_LoadGame: saved client/monster species mismatch in slot %d", slot);
    }
}

void G_DoomSaveFinishRestore(const bool *clientRecords) {
    if (!clientRecords[0]) {
        G_Error("G_LoadGame: missing saved player client");
    }
    for (int slot = 1; slot < level.maxclients; ++slot) {
        if (!g_entities[slot].inuse) {
            trap_SetConfigstring(CS_PLAYERS + slot, "");
            trap_SetUserinfo(slot, "");
            continue;
        }
        if (!clientRecords[slot]) {
            G_Error("G_LoadGame: missing saved monster client in slot %d", slot);
        }
        G_DoomSaveValidateClient(slot, level.clients[slot]);
        const int type = g_entities[slot].doomNpc.type;
        const char *name = doommonster::definitions[type].name;
        char userinfo[MAX_INFO_STRING] = "";
        Info_SetValueForKey(userinfo, "name", name);
        Info_SetValueForKey(userinfo, "doomnpc", va("%d", type));
        trap_SetUserinfo(slot, userinfo);
        trap_SetConfigstring(CS_PLAYERS + slot, va("n\\%s\\t\\0\\doomnpc\\%d", name, type));
    }
}
