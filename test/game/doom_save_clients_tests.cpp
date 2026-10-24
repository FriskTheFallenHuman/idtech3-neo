// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/game/g_local.h"
#include "game/game/g_doomsave.h"
#include <cstdio>
#include <stdexcept>
#include <string>

level_locals_t level{};
gentity_t g_entities[MAX_GENTITIES]{};
namespace {
gclient_t clients[MAX_CLIENTS]{};
bool allocated[MAX_CLIENTS]{};
std::string userinfos[MAX_CLIENTS], configstrings[MAX_CLIENTS];
bool records[MAX_CLIENTS]{};

void Check(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
template <typename Function> void Reject(Function function) {
    try {
        function();
    } catch (const std::runtime_error &error) {
        Check(strstr(error.what(), "G_LoadGame:") != nullptr, "Expected a contextual loader error");
        return;
    }
    throw std::runtime_error("Malformed saved client accepted");
}
void Actor(int slot, int species) {
    auto &entity = g_entities[slot];
    entity.inuse = qtrue;
    entity.s.number = entity.s.clientNum = slot;
    entity.s.eType = ET_PLAYER;
    entity.r.svFlags = SVF_AI | SVF_BOT;
    entity.client = &clients[slot];
    entity.doomNpc.type = species;
}
void SavedClient(int slot) {
    auto &client = clients[slot];
    client.ps.clientNum = slot;
    client.pers.connected = CON_CONNECTED;
    client.ps.stats[STAT_DOOM_MONSTER] = g_entities[slot].doomNpc.type;
    records[slot] = true;
}
} // namespace

void QDECL G_Error(const char *format, ...) {
    char message[1024];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    throw std::runtime_error(message);
}
void QDECL Com_Error(int, const char *message, ...) {
    throw std::runtime_error(message);
}
void QDECL Com_Printf(const char *, ...) {
}
void trap_UnlinkEntity(gentity_t *entity) {
    entity->r.linked = qfalse;
}
void trap_BotFreeClient(int slot) {
    Check(slot > 0 && slot < MAX_CLIENTS, "Human or out-of-range slot freed");
    allocated[slot] = false;
    g_entities[slot].r.svFlags &= ~SVF_BOT;
}
int trap_BotAllocateClient() {
    for (int slot = 1; slot < MAX_CLIENTS; ++slot) {
        if (!allocated[slot]) {
            allocated[slot] = true;
            return slot;
        }
    }
    return -1;
}
void trap_SetUserinfo(int slot, const char *text) {
    userinfos[slot] = text;
}
void trap_SetConfigstring(int index, const char *text) {
    Check(index >= CS_PLAYERS && index < CS_PLAYERS + MAX_CLIENTS, "Invalid player configstring");
    configstrings[index - CS_PLAYERS] = text;
}

int main() {
    try {
        level.clients = clients;
        level.maxclients = MAX_CLIENTS;
        g_entities[0].inuse = qtrue;
        g_entities[0].client = &clients[0];
        SavedClient(0);
        Actor(1, doommonster::Imp);
        Actor(3, doommonster::Demon);
        g_entities[1].r.linked = qtrue;
        allocated[1] = allocated[3] = allocated[11] = true;
        clients[11].pers.connected = CON_CONNECTED; // Orphan from an older load.
        G_DoomSaveBeginRestore();
        Check(clients[0].pers.connected == CON_CONNECTED && records[0], "Player preserved");
        for (int slot = 1; slot < MAX_CLIENTS; ++slot) {
            Check(!allocated[slot] && !g_entities[slot].inuse &&
                      clients[slot].pers.connected == CON_DISCONNECTED,
                  "Fresh map and orphan slots must be discarded before decoding");
        }
        Check(!g_entities[1].r.linked, "Discarded actors must be unlinked");

        Actor(2, doommonster::Baron);
        Actor(7, doommonster::LostSoul);
        Actor(MAX_CLIENTS - 1, doommonster::SpiderMastermind);
        Check(G_DoomSavePrepareClients() == 3, "Sparse saved roster reconstructed");
        for (int slot = 1; slot < MAX_CLIENTS; ++slot) {
            Check(allocated[slot] == (slot == 2 || slot == 7 || slot == MAX_CLIENTS - 1),
                  "Only exact saved actor slots remain allocated");
        }
        SavedClient(2);
        SavedClient(7);
        SavedClient(MAX_CLIENTS - 1);
        G_DoomSaveFinishRestore(records);
        Check(userinfos[2].find("baronofhell") != std::string::npos &&
                  configstrings[7].find("doomnpc\\6") != std::string::npos &&
                  configstrings[1].empty(),
              "Saved species and empty slots must be republished to clients");

        clients[2].ps.clientNum = MAX_CLIENTS;
        Reject([] { G_DoomSaveValidateClient(2, clients[2]); });
        clients[2].ps.clientNum = 2;
        clients[2].ps.stats[STAT_DOOM_MONSTER] = doommonster::Imp;
        Reject([] { G_DoomSaveValidateClient(2, clients[2]); });
        clients[2].ps.stats[STAT_DOOM_MONSTER] = doommonster::Baron;
        records[7] = false;
        Reject([] { G_DoomSaveFinishRestore(records); });
        g_entities[2].doomNpc.type = doommonster::SpeciesCount;
        Reject([] { G_DoomSavePrepareClients(); });
        g_entities[2].doomNpc.type = doommonster::Baron;
        g_entities[2].client = &clients[3];
        Reject([] { G_DoomSavePrepareClients(); });
        std::puts("Saved actor roster: exact sparse slots, orphan cleanup, species publication "
                  "and malformed identities passed.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
