// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the real combat decisions against independent visibility/team boundaries.
#include "game/g_doombots_combat.cpp"
#include "game/g_doombots_goals.cpp"
#include <cstdio>
#include <stdexcept>

level_locals_t level{};
gentity_t g_entities[MAX_GENTITIES]{};
static gclient_t clients[4];
static int hit = 3;
static bool pvs = true;
void QDECL Com_Printf(const char *, ...) {
}
void QDECL Com_Error(int, const char *why, ...) {
    throw std::runtime_error(why);
}
int trap_AAS_Initialized() {
    return 1;
}
qboolean DoomBotRoute(gentity_t *, const vec3_t goal, doomAasRoute_t &route) {
    route = {};
    VectorCopy(goal, route.point);
    route.travelTime = 100;
    return qtrue;
}
static void Touch(gentity_t *, gentity_t *, trace_t *) {
}

qboolean OnSameTeam(gentity_t *a, gentity_t *b) {
    return a->client && b->client && a->client->sess.sessionTeam == b->client->sess.sessionTeam
               ? qtrue
               : qfalse;
}
qboolean trap_InPVS(const vec3_t, const vec3_t) {
    return pvs ? qtrue : qfalse;
}
void trap_Trace(trace_t *trace, const vec3_t, const vec3_t, const vec3_t, const vec3_t end, int,
                int) {
    *trace = {};
    trace->fraction = .9f;
    trace->entityNum = hit;
    VectorCopy(end, trace->endpos);
}
unsigned int DoomBotRandom(doomBot_t &) {
    return 1;
}
qboolean DoomBotSafeStep(gentity_t *, const vec3_t, qboolean, qboolean) {
    return qtrue;
}
void DoomBotView(gentity_t *self, usercmd_t &cmd, const vec3_t desired, float) {
    for (int axis = 0; axis < 3; ++axis) {
        cmd.angles[axis] = ANGLE2SHORT(desired[axis]) - self->client->ps.delta_angles[axis];
    }
}
static void Check(bool okay, const char *why) {
    if (!okay) {
        throw std::runtime_error(why);
    }
}
static void Grant(playerState_t &ps, int weapon, int ammo) {
    ps.weapons[weapon / 32] |= 1u << (weapon % 32);
    const auto *definition = doomweapon::Get(weapon);
    if (definition->cost) {
        ps.ammo[definition->ammo] = ammo;
    }
}

int main() {
    try {
        level.time = 1000;
        level.maxclients = 4;
        for (int i = 1; i < 4; ++i) {
            auto &entity = g_entities[i];
            entity.inuse = entity.takedamage = qtrue;
            entity.health = 100;
            entity.s.number = i;
            entity.client = &clients[i];
            entity.client->pers.connected = CON_CONNECTED;
            entity.client->sess.sessionTeam = i == 3 ? TEAM_RED : TEAM_BLUE;
            entity.client->ps.stats[STAT_HEALTH] = 100;
            entity.client->ps.viewheight = 17;
            doomweapon::Initialize(entity.client->ps);
        }
        auto *self = &g_entities[1];
        auto &ps = self->client->ps;
        VectorSet(g_entities[3].r.currentOrigin, 300, 0, 0);
        VectorSet(g_entities[2].r.currentOrigin, -300, 0, 0);
        Check(DoomBotEnemy(self, &g_entities[3]) && !DoomBotEnemy(self, &g_entities[2]),
              "Team bots must target opponents and reject teammates");
        g_entities[3].client->sess.sessionTeam = TEAM_SPECTATOR;
        Check(!DoomBotEnemy(self, &g_entities[3]), "Spectators cannot be targets");
        g_entities[3].client->sess.sessionTeam = TEAM_RED;
        pvs = false;
        Check(!DoomBotVisible(self, &g_entities[3]), "No targeting through hidden map areas");
        pvs = true;
        Grant(ps, WP_DOOM_ROCKET, 5);
        Check(ChooseWeapon(&ps, 80) == WP_DOOM_PISTOL && ChooseWeapon(&ps, 500) == WP_DOOM_ROCKET,
              "Rocket selection must respect self-splash distance");
        Grant(ps, WP_DOOM_PLASMA, 39);
        Grant(ps, WP_DOOM_BFG, 39);
        Check(ChooseWeapon(&ps, 400) == WP_DOOM_PLASMA,
              "BFG needs forty cells while plasma can use the remaining cells");
        ps.ammo[WP_DOOM_PLASMA] = 80;
        Check(ChooseWeapon(&ps, 400) == WP_DOOM_BFG, "Loaded BFG is a mid-range weapon");
        Grant(ps, WP_DOOM_CHAINSAW, 0);
        Check(ChooseWeapon(&ps, 40) == WP_DOOM_CHAINSAW, "Chainsaw is a close-range choice");
        doomweapon::Initialize(ps);
        doomBot_t bot{};
        bot.clientNum = 1;
        bot.enemyNum = 3;
        bot.visible = qtrue;
        bot.skill = 5;
        bot.goalType = DOOM_BOT_ENEMY;
        usercmd_t command{};
        DoomBotCombat(bot, self, command, 50);
        Check(command.weapon == WP_DOOM_PISTOL && (command.buttons & BUTTON_ATTACK),
              "Visible opponent receives ordinary Doom attack input");
        Grant(ps, WP_DOOM_ROCKET, 4);
        ps.weapon = WP_DOOM_ROCKET;
        VectorSet(g_entities[2].r.currentOrigin, 295, 0, 0);
        command = {};
        DoomBotCombat(bot, self, command, 50);
        Check(!(command.buttons & BUTTON_ATTACK), "Rocket splash must avoid teammates");
        VectorSet(g_entities[2].r.currentOrigin, -300, 0, 0);
        command = {};
        DoomBotCombat(bot, self, command, 50);
        Check(command.buttons & BUTTON_ATTACK, "Clear rocket shot must be allowed");
        doomweapon::Initialize(ps);
        ps.stats[STAT_HEALTH] = 100;
        ps.ammo[WP_DOOM_PISTOL] = 10;
        level.num_entities = MAX_CLIENTS + 2;
        auto &shotgun = g_entities[MAX_CLIENTS];
        shotgun.inuse = qtrue;
        shotgun.s.eType = ET_DOOM_PICKUP;
        shotgun.s.frame = doomitem::ForThing(2001);
        shotgun.r.contents = CONTENTS_TRIGGER;
        shotgun.touch = Touch;
        VectorSet(shotgun.r.currentOrigin, 100, 0, 0);
        auto &ammo = g_entities[MAX_CLIENTS + 1];
        ammo = shotgun;
        ammo.s.frame = doomitem::ForThing(2048);
        VectorSet(ammo.r.currentOrigin, 200, 0, 0);
        VectorSet(self->r.currentOrigin, 50, 0, 24);
        bot = {};
        bot.hasGoal = qtrue;
        bot.goalType = DOOM_BOT_ITEM;
        bot.goalNum = MAX_CLIENTS;
        bot.goalUntil = level.time + 10000;
        VectorCopy(shotgun.r.currentOrigin, bot.goal);
        DoomBotGoals(bot, self);
        Check(bot.hasGoal && bot.goalNum == MAX_CLIENTS && bot.recentIndex == 0,
              "Bot must approach within pickup touch range rather than abandon it at 64 units");
        shotgun.r.contents = 0;
        DoomBotGoals(bot, self);
        Check(bot.goalNum == MAX_CLIENTS + 1,
              "Collected pickups must release their navigation goal");
        std::puts("Doom bot teams, visibility, ammo, weapon ranges and friendly splash passed.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
