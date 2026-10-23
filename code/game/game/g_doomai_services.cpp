// SPDX-License-Identifier: GPL-3.0-or-later
#include "g_save_callbacks.h"
#include "g_local.h"

doomProgress_t doomProgress;
qboolean saveGamePending;

gentity_t *G_FindActor(const char *name) {
    if (!name || !*name) {
        return nullptr;
    }
    for (int i = 0; i < level.num_entities; ++i) {
        auto *entity = &g_entities[i];
        if (entity->inuse &&
            ((!Q_stricmp(name, "player") && entity->client && !entity->doomNpc.type) ||
             (entity->targetname && !Q_stricmp(name, entity->targetname)) ||
             (entity->aiName && !Q_stricmp(name, entity->aiName)))) {
            return entity;
        }
    }
    return nullptr;
}
DOOM_SAVE_CALLBACK("G_FindActor", G_FindActor);

void G_ActorScriptEvent(gentity_t *entity, const char *event, const char *params) {
    if (entity) {
        G_Script_ScriptEvent(entity, const_cast<char *>(event), const_cast<char *>(params));
    }
}
DOOM_SAVE_CALLBACK("G_ActorScriptEvent", G_ActorScriptEvent);

void G_DoomAgePlayTime() {
    if (saveGamePending || g_reloading.integer) {
        return;
    }
    const int elapsed = level.time - doomProgress.lastLoadTime;
    if (elapsed > 1000) {
        doomProgress.totalPlayTime += elapsed;
        doomProgress.lastLoadTime = level.time;
        trap_Cvar_Set("g_totalPlayTime", va("%d", doomProgress.totalPlayTime));
    }
}
DOOM_SAVE_CALLBACK("G_DoomAgePlayTime", G_DoomAgePlayTime);

void G_DoomCheckLoadGame() {
    if (!saveGamePending || level.time - level.startTime < 500) {
        return;
    }
    auto *player = G_FindActor("player");
    if (!player || player->client->pers.connected != CON_CONNECTED) {
        return;
    }
    for (int i = MAX_CLIENTS; i < level.num_entities; ++i) {
        if (g_entities[i].inuse && g_entities[i].think == G_DoomAISpawnThink) {
            return;
        }
    }
    saveGamePending = qfalse;
    if (trap_Cvar_VariableIntegerValue("savegame_loading")) {
        trap_Cvar_Set("savegame_loading", "0");
        G_LoadGame(nullptr);
        level.reloadPauseTime = level.time + 1100;
        trap_SendServerCommand(-1, "snd_fade 1 0\n");
    } else {
        G_LoadPersistant();
        doomProgress.lastLoadTime = level.time;
        G_SaveGame(nullptr);
    }
    trap_Cvar_Set("cg_norender", "0");
    trap_Cvar_Set("g_reloading", "0");
    trap_SetConfigstring(CS_SCREENFADE, "0 0 0");
}
DOOM_SAVE_CALLBACK("G_DoomCheckLoadGame", G_DoomCheckLoadGame);

void G_SetAASBlockingEntity(gentity_t *entity, qboolean blocking) {
    entity->AASblocking = blocking;
    for (int world = 0; world < 4; ++world) {
        trap_AAS_SetCurrentWorld(world);
        if (trap_AAS_Initialized()) {
            trap_AAS_SetAASBlockingEntity(entity->r.absmin, entity->r.absmax, blocking);
        }
    }
    trap_AAS_SetCurrentWorld(0);
}
DOOM_SAVE_CALLBACK("G_SetAASBlockingEntity", G_SetAASBlockingEntity);

int G_DoomSkill() {
    return max(0, min(4, trap_Cvar_VariableIntegerValue("doom_skill")));
}
DOOM_SAVE_CALLBACK("G_DoomSkill", G_DoomSkill);

void G_DoomAIRestore(int savedTime) {
    const int delta = level.time - savedTime;
    for (int i = 0; i < level.maxclients; ++i) {
        auto &entity = g_entities[i];
        if (!entity.inuse || !entity.client || !entity.client->ps.stats[STAT_DOOM_WEAPONS]) {
            continue;
        }
        const int powers[] = {PW_INVULNERABLE, PW_INVIS, PW_BATTLESUIT, PW_QUAD};
        for (int power : powers) {
            int &expires = entity.client->ps.powerups[power];
            if (expires) {
                expires = max(level.time, expires + delta);
            }
        }
    }
    for (int i = 1; i < level.maxclients; ++i) {
        auto &entity = g_entities[i];
        if (!entity.inuse || !entity.doomNpc.type || !entity.client) {
            continue;
        }
        auto &npc = entity.doomNpc;
        npc.clipStart += delta;
        npc.nextDecision += delta;
        npc.nextRoute = level.time;
        npc.attackReady += delta;
        npc.lastSeen += delta;
        if (npc.charging) {
            npc.charging += delta;
        }
        entity.client->ps.commandTime = level.time;
        G_DoomAIReplicate(&entity);
        trap_LinkEntity(&entity);
    }
}
DOOM_SAVE_CALLBACK("G_DoomAIRestore", G_DoomAIRestore);

qboolean G_ActorVisibleFromPos(vec3_t source, int sourceNum, vec3_t target, int targetNum) {
    trace_t trace;
    trap_Trace(&trace, source, nullptr, nullptr, target, sourceNum, MASK_SHOT);
    return trace.fraction == 1 || trace.entityNum == targetNum ? qtrue : qfalse;
}
DOOM_SAVE_CALLBACK("G_ActorVisibleFromPos", G_ActorVisibleFromPos);

qboolean G_ActorCanAttackFromPos(int entityNum, int enemyNum, vec3_t origin) {
    if (entityNum < 0 || entityNum >= level.num_entities || enemyNum < 0 ||
        enemyNum >= level.num_entities) {
        return qfalse;
    }
    vec3_t source, target;
    VectorCopy(origin, source);
    source[2] += g_entities[entityNum].client ? g_entities[entityNum].client->ps.viewheight : 32;
    VectorCopy(g_entities[enemyNum].r.currentOrigin, target);
    target[2] += 24;
    return G_ActorVisibleFromPos(source, entityNum, target, enemyNum);
}
DOOM_SAVE_CALLBACK("G_ActorCanAttackFromPos", G_ActorCanAttackFromPos);
