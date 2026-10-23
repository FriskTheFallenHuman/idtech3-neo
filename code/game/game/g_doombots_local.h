// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from Hexen2 player bots; Doom combat and item rules.
#ifndef G_DOOMBOTS_LOCAL_H
#define G_DOOMBOTS_LOCAL_H
#include "g_local.h"
#include "g_doombots.h"
#include "bg_doomweapons.h"
#include "../qcommon/doom_aas_route.h"

enum { DOOM_MAX_BOTS = 16 };
enum doomBotGoal_t { DOOM_BOT_IDLE, DOOM_BOT_ITEM, DOOM_BOT_ROAM, DOOM_BOT_ENEMY };
struct doomBot_t {
    int clientNum, team, skill;
    char name[32];
    unsigned int randomState;
    int enemyNum, goalNum, avoidGoalNum;
    int nextDecision, nextRoute, goalUntil, avoidUntil, attackReady, lastSeen;
    int nextJump, progressTime, strafeUntil, strafeSign, spawnCount;
    int escapeUntil;
    int recentGoals[8], recentTimes[8], recentIndex;
    doomBotGoal_t goalType;
    vec3_t goal, lastEnemy, progressOrigin, progressPoint, escapeDirection;
    doomAasRoute_t route;
    qboolean hasGoal, visible, routed;
    unsigned int commands, routeAttempts, routes, attacks, recoveries;
    float moved;
};
extern doomBot_t doomBots[DOOM_MAX_BOTS];
unsigned int DoomBotRandom(doomBot_t &bot);
qboolean DoomBotEnemy(gentity_t *self, gentity_t *other);
qboolean DoomBotVisible(gentity_t *self, gentity_t *other);
void DoomBotSense(doomBot_t &bot, gentity_t *self);
void DoomBotCombat(doomBot_t &bot, gentity_t *self, usercmd_t &cmd, int msec);
void DoomBotGoals(doomBot_t &bot, gentity_t *self);
qboolean DoomBotFindItem(doomBot_t &bot, gentity_t *self, float range);
void DoomBotNavigate(doomBot_t &bot, gentity_t *self, usercmd_t &cmd, int msec);
void DoomBotView(gentity_t *self, usercmd_t &cmd, const vec3_t desired, float turn);
int DoomBotTravelFlags();
int DoomBotArea(const vec3_t point);
qboolean DoomBotRoute(gentity_t *self, const vec3_t goal, doomAasRoute_t &route);
qboolean DoomBotSafeStep(gentity_t *self, const vec3_t direction, qboolean crossing,
                         qboolean water);
qboolean trap_AAS_DoomPlayerRoute(vec3_t origin, vec3_t goal, int flags, doomAasRoute_t *route);
#endif
