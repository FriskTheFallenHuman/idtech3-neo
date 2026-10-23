// SPDX-License-Identifier: GPL-3.0-or-later
#include "g_save_callbacks.h"
#include "g_local.h"
#include "bg_doomweapons.h"
#include "bg_doomblood.h"
#include "../qcommon/doom_bsp.h"

void G_ExplodeMissile(gentity_t *entity);

void DoomWeaponTouch(gentity_t *item, gentity_t *other, trace_t *trace) {
    G_DoomItemTouch(item, other, trace);
}
DOOM_SAVE_CALLBACK("DoomWeaponTouch", DoomWeaponTouch);

void G_DoomSpawnWeaponThings(const void *mapData) {
    G_DoomSpawnItemThings(mapData);
}
DOOM_SAVE_CALLBACK("G_DoomSpawnWeaponThings", G_DoomSpawnWeaponThings);

void G_DoomImpactBlood(gentity_t *impact, const trace_t *trace, const vec3_t travel) {
    VectorClear(impact->s.origin2);
    VectorClear(impact->s.angles);
    if (trace->entityNum < 0 || trace->entityNum >= ENTITYNUM_WORLD) {
        return;
    }
    const auto &victim = g_entities[trace->entityNum];
    if (!victim.client || !victim.takedamage) {
        return;
    }
    vec3_t minimum, maximum, exit;
    for (int axis = 0; axis < 3; ++axis) {
        minimum[axis] = victim.r.currentOrigin[axis] + victim.r.mins[axis];
        maximum[axis] = victim.r.currentOrigin[axis] + victim.r.maxs[axis];
    }
    if (doomblood::ExitPoint(trace->endpos, travel, minimum, maximum, exit)) {
        VectorNormalize2(travel, impact->s.origin2);
        VectorCopy(exit, impact->s.angles);
    }
}
DOOM_SAVE_CALLBACK("G_DoomImpactBlood", G_DoomImpactBlood);

void DoomWeaponRay(gentity_t *player, float yawSpread, float pitchSpread, float range, int damage,
                   int mod) {
    vec3_t start, angles, direction, end;
    VectorCopy(player->client->ps.origin, start);
    start[2] += player->client->ps.viewheight;
    VectorCopy(player->client->ps.viewangles, angles);
    angles[YAW] += yawSpread * crandom();
    angles[PITCH] += pitchSpread * crandom();
    AngleVectors(angles, direction, nullptr, nullptr);
    VectorMA(start, range, direction, end);
    trace_t trace;
    trap_Trace(&trace, start, nullptr, nullptr, end, player->s.number, MASK_SHOT);
    G_DoomTraceLine(player, start, trace.endpos, qtrue);
    const bool damageable = trace.entityNum >= 0 && trace.entityNum < ENTITYNUM_WORLD &&
                            g_entities[trace.entityNum].takedamage;
    const bool flesh = damageable && g_entities[trace.entityNum].client;
    if (trace.fraction < 1 && !(trace.surfaceFlags & SURF_NOIMPACT)) {
        auto *impact = G_TempEntity(trace.endpos, flesh ? EV_MISSILE_HIT : EV_MISSILE_MISS);
        impact->s.otherEntityNum = trace.entityNum;
        impact->s.effect2Time = trace.surfaceFlags;
        impact->s.effect1Time = trace.contents;
        impact->s.weapon = player->client->ps.weapon;
        impact->s.eventParm = DirToByte(trace.plane.normal);
        G_DoomImpactBlood(impact, &trace, direction);
    }
    if (damageable) {
        G_Damage(&g_entities[trace.entityNum], player, player, direction, trace.endpos, damage,
                 DAMAGE_NO_KNOCKBACK, mod);
    }
}
DOOM_SAVE_CALLBACK("DoomWeaponRay", DoomWeaponRay);

void G_DoomBfgSpray(gentity_t *missile) {
    if (missile->s.weapon != WP_DOOM_BFG || !missile->parent || !missile->parent->inuse) {
        return;
    }
    // The spray originates at the shooter and spans the launch direction's 90-degree cone.
    gentity_t *player = missile->parent;
    for (int ray = 0; ray < 40; ++ray) {
        vec3_t angles, direction, start, end;
        VectorCopy(missile->s.angles, angles);
        angles[YAW] += -45.0f + 90.0f * ray / 40.0f;
        AngleVectors(angles, direction, nullptr, nullptr);
        VectorCopy(player->r.currentOrigin, start);
        start[2] += player->client ? player->client->ps.viewheight : 32;
        VectorMA(start, 1024, direction, end);
        trace_t trace;
        trap_Trace(&trace, start, nullptr, nullptr, end, player->s.number, MASK_SHOT);
        if (trace.entityNum >= 0 && trace.entityNum < ENTITYNUM_WORLD &&
            g_entities[trace.entityNum].takedamage) {
            auto *impact =
                G_TempEntity(trace.endpos,
                             g_entities[trace.entityNum].client ? EV_MISSILE_HIT : EV_MISSILE_MISS);
            impact->s.weapon = WP_DOOM_BFG;
            impact->s.dmgFlags = doomweapon::ImpactBfgSpray;
            impact->s.otherEntityNum = trace.entityNum;
            impact->s.eventParm = DirToByte(trace.plane.normal);
            impact->s.effect2Time = trace.surfaceFlags;
            impact->s.effect1Time = trace.contents;
            G_DoomImpactBlood(impact, &trace, direction);
            int damage = 0;
            for (int roll = 0; roll < 15; ++roll) {
                damage += 1 + (rand() & 7);
            }
            G_Damage(&g_entities[trace.entityNum], missile, player, direction, trace.endpos, damage,
                     DAMAGE_NO_KNOCKBACK, MOD_BFG_SPLASH);
        }
    }
}
DOOM_SAVE_CALLBACK("G_DoomBfgSpray", G_DoomBfgSpray);

qboolean G_DoomFireWeapon(gentity_t *player) {
    G_DoomAIHear(player->s.number, player->r.currentOrigin, 2048);
    const int weapon = player->client->ps.weapon;
    const auto *definition = doomweapon::Get(weapon);
    if (!definition || !player->client->ps.stats[STAT_DOOM_WEAPONS]) {
        return qfalse;
    }
    if (weapon == WP_DOOM_FIST || weapon == WP_DOOM_CHAINSAW) {
        DoomWeaponRay(
            player, 5.625f, 0, weapon == WP_DOOM_FIST ? 64.0f : 65.0f,
            2 * (1 + rand() % 10) *
                (weapon == WP_DOOM_FIST && player->client->ps.stats[STAT_DOOM_BERSERK] ? 10 : 1),
            MOD_GAUNTLET);
    } else if (weapon == WP_DOOM_PISTOL || weapon == WP_DOOM_CHAINGUN ||
               weapon == WP_DOOM_SHOTGUN) {
        const int pellets = weapon == WP_DOOM_SHOTGUN ? 7 : 1;
        for (int pellet = 0; pellet < pellets; ++pellet) {
            const float spread = weapon == WP_DOOM_SHOTGUN                    ? 5.625f
                                 : player->client->ps.stats[STAT_DOOM_REFIRE] ? 2.8125f
                                                                              : 0;
            DoomWeaponRay(player, spread, 0, 2048, 5 * (1 + rand() % 3),
                          weapon == WP_DOOM_SHOTGUN ? MOD_SHOTGUN : MOD_MACHINEGUN);
        }
    } else {
        vec3_t start, direction;
        VectorCopy(player->client->ps.origin, start);
        start[2] += player->client->ps.viewheight;
        AngleVectors(player->client->ps.viewangles, direction, nullptr, nullptr);
        gentity_t *bolt = G_Spawn();
        bolt->classname = "doom_projectile";
        bolt->s.eType = ET_MISSILE;
        bolt->s.weapon = weapon;
        bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
        bolt->r.ownerNum = player->s.number;
        bolt->parent = player;
        bolt->clipmask = MASK_MISSILESHOT;
        bolt->nextthink = level.time + 10000;
        bolt->think = G_ExplodeMissile;
        bolt->damage = weapon == WP_DOOM_ROCKET   ? 20 * (1 + rand() % 8)
                       : weapon == WP_DOOM_PLASMA ? 5 * (1 + rand() % 8)
                                                  : 100 * (1 + rand() % 8);
        bolt->splashDamage = weapon == WP_DOOM_ROCKET ? 128 : 0;
        bolt->splashRadius = weapon == WP_DOOM_ROCKET ? 128 : 0;
        bolt->methodOfDeath = weapon == WP_DOOM_BFG ? MOD_BFG : MOD_ROCKET;
        bolt->splashMethodOfDeath = MOD_ROCKET_SPLASH;
        bolt->s.pos.trType = TR_LINEAR;
        bolt->s.pos.trTime = level.time;
        VectorCopy(start, bolt->s.pos.trBase);
        VectorCopy(start, bolt->r.currentOrigin);
        VectorCopy(player->client->ps.viewangles, bolt->s.angles);
        const float speed = weapon == WP_DOOM_ROCKET   ? 700.0f
                            : weapon == WP_DOOM_PLASMA ? 875.0f
                                                       : 875.0f;
        VectorScale(direction, speed, bolt->s.pos.trDelta);
        VectorSet(bolt->r.mins, -3, -3, -3);
        VectorSet(bolt->r.maxs, 3, 3, 3);
        trap_LinkEntity(bolt);
    }
    return qtrue;
}
DOOM_SAVE_CALLBACK("G_DoomFireWeapon", G_DoomFireWeapon);

void G_DoomDropMonsterPickup(gentity_t *entity, int type) {
    auto *item = G_Spawn();
    item->classname = "doom_weapon_pickup";
    item->s.eType = ET_DOOM_PICKUP;
    item->s.frame = type == doommonster::ShotgunGuy ? 0 : 6;
    item->touch = DoomWeaponTouch;
    item->count = 1;
    item->r.contents = CONTENTS_TRIGGER;
    VectorSet(item->r.mins, -12, -12, 0);
    VectorSet(item->r.maxs, 12, 12, 24);
    G_SetOrigin(item, entity->r.currentOrigin);
    trap_LinkEntity(item);
}
DOOM_SAVE_CALLBACK("G_DoomDropMonsterPickup", G_DoomDropMonsterPickup);

void G_DoomDropPlayerWeapon(gentity_t *player) {
    const int weapon = player->client->ps.weapon;
    for (int index = 0; index < 6; ++index) {
        if (doomweapon::pickups[index].weapon != weapon) {
            continue;
        }
        auto *item = G_Spawn();
        item->classname = "doom_dropped_weapon";
        item->s.eType = ET_DOOM_PICKUP;
        item->s.frame = index;
        item->count = 1;
        item->touch = DoomWeaponTouch;
        item->think = G_FreeEntity;
        item->nextthink = level.time + 30000;
        item->r.contents = CONTENTS_TRIGGER;
        VectorSet(item->r.mins, -12, -12, -8);
        VectorSet(item->r.maxs, 12, 12, 24);
        vec3_t origin;
        VectorCopy(player->r.currentOrigin, origin);
        origin[2] += player->r.mins[2];
        G_SetOrigin(item, origin);
        trap_LinkEntity(item);
        return;
    }
}
DOOM_SAVE_CALLBACK("G_DoomDropPlayerWeapon", G_DoomDropPlayerWeapon);
