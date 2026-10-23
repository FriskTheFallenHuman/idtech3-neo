// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bg_doommonsters.h"

// Saved with gentity_t. No heap pointers or client rendering handles.
struct doomNpc_t {
    int type, options, enemy, clip, clipStart, eventStep, nextDecision, nextRoute;
    int attackReady, lastSeen, charging, bossDone, shots, routes;
    int moveDirection, moveCount, justAttacked, justHit, targetThreshold;
    unsigned int randomState;
    vec3_t lastKnown, routePoint;
};
struct doomProgress_t {
    int totalPlayTime, lastLoadTime, attempts;
};
extern doomProgress_t doomProgress;
extern qboolean saveGamePending;
void G_DoomAIInit();
void G_DoomAIShutdown();
void G_DoomAIBeginFrame();
void G_DoomAISpawnThings(const void *mapData);
qboolean G_DoomAIConfigure(gentity_t *entity);
void G_DoomAISpawnThink(gentity_t *source);
void G_DoomAIRun(gentity_t *entity);
void G_DoomAIReplicate(gentity_t *entity);
void G_DoomAIHear(int sourceNum, const vec3_t origin, float radius);
bool G_DoomCanHear(const vec3_t source, const vec3_t listener);
bool G_DoomAIDamage(gentity_t *entity, gentity_t *attacker, int damage, int flags, int mod);
bool G_DoomAICommand(gentity_t *player, const char *command);
void G_DoomAIBossDeath(int type);
void G_DoomAIProjectileRun(gentity_t *entity);
void G_DoomCheckLoadGame();
void G_DoomAgePlayTime();
gentity_t *G_FindActor(const char *name);
void G_ActorScriptEvent(gentity_t *entity, const char *event, const char *params);
void G_SetAASBlockingEntity(gentity_t *entity, qboolean blocking);
qboolean trap_AAS_Route(vec3_t origin, vec3_t goal, int flags, vec3_t point);

int G_DoomSkill();
void G_DoomAIRestore(int savedTime);

qboolean G_ActorVisibleFromPos(vec3_t source, int sourceNum, vec3_t target, int targetNum);
qboolean G_ActorCanAttackFromPos(int entityNum, int enemyNum, vec3_t origin);
