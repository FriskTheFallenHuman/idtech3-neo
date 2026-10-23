// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from Hexen2 player bots; Doom combat and item rules.
#include "g_doombots_local.h"
#include "bg_doomitems.h"

static float ItemNeed(gentity_t *self, gentity_t *item) {
    const int index = item->s.frame;
    if (index < 0 || index >= doomitem::Count) {
        return 0;
    }
    const auto &definition = doomitem::definitions[index];
    if (definition.kind == doomitem::Key || definition.kind == doomitem::Map) {
        return 0;
    }
    const auto &ps = self->client->ps;
    playerState_t collected = ps;
    if (!doomitem::Collect(collected, index, level.time)) {
        return 0;
    }
    if (definition.kind == doomitem::Health || definition.kind == doomitem::HealthBonus ||
        definition.kind == doomitem::Soul) {
        return max(0, 100 - self->health) * 1.6f + 5;
    }
    if (definition.kind == doomitem::Armor || definition.kind == doomitem::ArmorBonus) {
        return max(0, collected.stats[STAT_ARMOR] - ps.stats[STAT_ARMOR]) * .6f + 5;
    }
    if (definition.kind == doomitem::Weapon) {
        const auto &pickup = doomweapon::pickups[definition.value];
        if (pickup.grantsWeapon && !doomweapon::Owned(ps, pickup.weapon)) {
            return 140;
        }
        if (pickup.thing == 8) {
            return 55;
        }
        const auto *weapon = doomweapon::Get(pickup.weapon);
        return weapon ? 10 + 55 * (1 - ps.ammo[weapon->ammo] /
                                           static_cast<float>(
                                               max(1, doomweapon::Maximum(ps, pickup.weapon))))
                      : 0;
    }
    return 40;
}
qboolean DoomBotFindItem(doomBot_t &bot, gentity_t *self, float range) {
    float best = 0;
    int chosen = -1;
    vec3_t destination;
    for (int i = level.maxclients; i < level.num_entities; ++i) {
        gentity_t *item = &g_entities[i];
        if (!item->inuse || !item->touch || !(item->r.contents & CONTENTS_TRIGGER) ||
            (item->s.eFlags & EF_NODRAW) || item->s.eType != ET_DOOM_PICKUP ||
            (i == bot.avoidGoalNum && level.time < bot.avoidUntil)) {
            continue;
        }
        float need = ItemNeed(self, item);
        if (need <= 0) {
            continue;
        }
        float distance = Distance(self->r.currentOrigin, item->r.currentOrigin);
        if (distance > range) {
            continue;
        }
        vec3_t goal;
        VectorCopy(item->r.currentOrigin, goal);
        doomAasRoute_t route{};
        if (trap_AAS_Initialized() && !DoomBotRoute(self, goal, route)) {
            continue;
        }
        float score = need / (1 + (route.travelTime ? route.travelTime * 3.0f : distance) / 400.0f);
        for (int recent = 0; recent < 8; ++recent) {
            if (bot.recentGoals[recent] == i && level.time - bot.recentTimes[recent] < 30000) {
                score *= .15f;
                break;
            }
        }
        if (score <= best) {
            continue;
        }
        best = score;
        chosen = i;
        VectorCopy(goal, destination);
    }
    if (chosen < 0) {
        return qfalse;
    }
    bot.hasGoal = qtrue;
    bot.goalType = DOOM_BOT_ITEM;
    bot.goalNum = chosen;
    bot.goalUntil = level.time + 10000;
    bot.nextRoute = level.time;
    VectorCopy(destination, bot.goal);
    return qtrue;
}
void DoomBotGoals(doomBot_t &bot, gentity_t *self) {
    const bool reachedRoam = bot.hasGoal && bot.goalType == DOOM_BOT_ROAM &&
                             Distance(self->r.currentOrigin, bot.goal) < 64;
    const bool finishedItem =
        bot.hasGoal && bot.goalType == DOOM_BOT_ITEM &&
        (!g_entities[bot.goalNum].inuse || !g_entities[bot.goalNum].r.contents ||
         ItemNeed(self, &g_entities[bot.goalNum]) <= 0);
    if (reachedRoam || finishedItem) {
        const int recent = bot.recentIndex++ % 8;
        bot.recentGoals[recent] = bot.goalNum;
        bot.recentTimes[recent] = level.time;
        bot.hasGoal = qfalse;
    }
    // Reaching the neighborhood of a pickup is not collecting it. Stay on the
    // item goal until the normal trigger touch succeeds, rather than turning
    // away at 64 units while still outside the pickup's trigger bounds.
    if (bot.hasGoal && bot.goalType == DOOM_BOT_ITEM && level.time < bot.goalUntil) {
        return;
    }
    // Seek equipment outside the current fight when still using starting gear.
    // Otherwise a visible opponent can keep an entire pistol-only group in one room.
    const bool startingGear = !doomweapon::Owned(self->client->ps, WP_DOOM_SHOTGUN) &&
                              !doomweapon::Owned(self->client->ps, WP_DOOM_CHAINGUN) &&
                              !doomweapon::Owned(self->client->ps, WP_DOOM_PLASMA) &&
                              !doomweapon::Owned(self->client->ps, WP_DOOM_ROCKET);
    if (startingGear && (!bot.visible || Distance(self->r.currentOrigin, bot.lastEnemy) > 300) &&
        DoomBotFindItem(bot, self, 4096)) {
        return;
    }
    // Low health can outweigh pursuit; combat still fires at visible enemies.
    if (bot.visible && !(bot.enemyNum == bot.avoidGoalNum && level.time < bot.avoidUntil) &&
        self->health >= self->client->ps.stats[STAT_MAX_HEALTH] / 3) {
        bot.hasGoal = qtrue;
        bot.goalType = DOOM_BOT_ENEMY;
        bot.goalNum = bot.enemyNum;
        VectorCopy(bot.lastEnemy, bot.goal);
        bot.goal[2] += self->r.mins[2];
        return;
    }
    if (bot.hasGoal && bot.goalType == DOOM_BOT_ITEM && level.time < bot.goalUntil) {
        gentity_t *item = &g_entities[bot.goalNum];
        if (item->inuse && item->touch && item->r.contents && ItemNeed(self, item) > 0) {
            return;
        }
        bot.hasGoal = qfalse;
    }
    if (bot.hasGoal && bot.goalType == DOOM_BOT_ROAM && level.time < bot.goalUntil &&
        Distance(self->r.currentOrigin, bot.goal) > 48) {
        return;
    }
    if (DoomBotFindItem(bot, self, 4096)) {
        return;
    }
    if (bot.enemyNum >= 0 && !(bot.enemyNum == bot.avoidGoalNum && level.time < bot.avoidUntil) &&
        level.time - bot.lastSeen < 2500) {
        bot.hasGoal = qtrue;
        bot.goalType = DOOM_BOT_ENEMY;
        bot.goalNum = bot.enemyNum;
        VectorCopy(bot.lastEnemy, bot.goal);
        bot.goal[2] += self->r.mins[2];
        return;
    }
    // Reservoir sampling reachable deathmatch starts gives exploration without
    // privileged knowledge of opponents hidden behind walls.
    float bestRoam = -1;
    gentity_t *chosen = NULL;
    for (int i = level.maxclients; i < level.num_entities; ++i) {
        gentity_t *spot = &g_entities[i];
        if (!spot->inuse || !spot->classname ||
            (Q_stricmp(spot->classname, "info_player_deathmatch") &&
             spot->s.eType != ET_DOOM_PICKUP) ||
            Distance(spot->r.currentOrigin, self->r.currentOrigin) < 96 ||
            (i == bot.avoidGoalNum && level.time < bot.avoidUntil)) {
            continue;
        }
        vec3_t goal;
        VectorCopy(spot->r.currentOrigin, goal);
        if (spot->s.eType != ET_DOOM_PICKUP) {
            goal[2] -= 25;
        }
        doomAasRoute_t route{};
        if (trap_AAS_Initialized() && !DoomBotRoute(self, goal, route)) {
            continue;
        }
        const float distance = Distance(self->r.currentOrigin, goal);
        float score = min(distance, 1600.0f) * (.5f + (DoomBotRandom(bot) & 255) / 255.0f);
        for (int recent = 0; recent < 8; ++recent) {
            if (bot.recentGoals[recent] == i && level.time - bot.recentTimes[recent] < 30000) {
                score *= .1f;
                break;
            }
        }
        if (score > bestRoam) {
            bestRoam = score;
            chosen = spot;
        }
    }
    bot.hasGoal = chosen ? qtrue : qfalse;
    bot.goalType = chosen ? DOOM_BOT_ROAM : DOOM_BOT_IDLE;
    if (chosen) {
        bot.goalNum = chosen->s.number;
        bot.goalUntil = level.time + 12000;
        bot.nextRoute = level.time;
        VectorCopy(chosen->r.currentOrigin, bot.goal);
        if (chosen->s.eType != ET_DOOM_PICKUP) {
            bot.goal[2] -= 25;
        }
    }
}
