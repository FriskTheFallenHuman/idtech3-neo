// SPDX-License-Identifier: GPL-3.0-or-later
// Chase decisions adapted from id Software's p_enemy.cpp in the original Doom
// source (Copyright (C) 1993-1996 id Software, Inc.). P_NewChaseDir, A_Chase and
// P_CheckMissileRange supply the direction ordering, movement counters and attack
// rules. Collision, movement and snapshots use this engine's server interfaces.
#include "g_save_callbacks.h"
#include "g_local.h"
#include "be_aas.h"
#include "../qcommon/doom_bsp.h"

namespace {
using namespace doommonster;
SpriteSet spriteSets[SpeciesCount];
bool spriteLoaded[SpeciesCount];
bool navigationSetup;
int navigationQueries[4], navigationRoutes[4];
constexpr int NoDirection = 8;
constexpr int MonsterSolid = MASK_PLAYERSOLID | CONTENTS_MONSTERCLIP;

const SpriteSet &Sprites(int type) {
    if (!spriteLoaded[type]) {
        char text[60000];
        fileHandle_t file = 0;
        const char *path = va("models/players/%s/animation.cfg", definitions[type].name);
        const int length = trap_FS_FOpenFile(path, &file, FS_READ);
        if (!file || length <= 0 || length >= static_cast<int>(sizeof(text))) {
            if (file) {
                trap_FS_FCloseFile(file);
            }
            G_Error("Missing or oversized Doom animation: %s", path);
        }
        trap_FS_Read(text, length, file);
        trap_FS_FCloseFile(file);
        text[length] = 0;
        if (!Parse(text, spriteSets[type])) {
            G_Error("Invalid Doom animation: %s", path);
        }
        spriteLoaded[type] = true;
    }
    return spriteSets[type];
}

unsigned int Random(gentity_t *entity) {
    auto &state = entity->doomNpc.randomState;
    state = state * 1664525u + 1013904223u;
    return (state >> 16) & 255;
}

void Sound(gentity_t *entity, const char *sound) {
    if (sound && *sound) {
        G_AddEvent(entity, EV_GENERAL_SOUND, G_SoundIndex(va("sound/doom/monsters/%s.wav", sound)));
    }
}

void Animate(gentity_t *entity, int clip, bool restart = false) {
    auto &npc = entity->doomNpc;
    if (npc.clip != clip || restart) {
        npc.clip = clip;
        npc.clipStart = level.time;
        npc.eventStep = 0;
    }
}

bool Alive(const gentity_t *entity) {
    return entity && entity->inuse && entity->client && entity->health > 0 &&
           !(entity->flags & FL_NOTARGET) && entity->client->pers.connected == CON_CONNECTED &&
           entity->client->sess.sessionTeam != TEAM_SPECTATOR;
}

bool Visible(gentity_t *entity, gentity_t *enemy) {
    vec3_t start, end;
    VectorCopy(entity->client->ps.origin, start);
    start[2] += entity->client->ps.viewheight;
    VectorCopy(enemy->r.currentOrigin, end);
    end[2] += enemy->client ? enemy->client->ps.viewheight : 32;
    trace_t trace;
    trap_Trace(&trace, start, nullptr, nullptr, end, entity->s.number, MASK_SHOT);
    return trace.fraction == 1 || trace.entityNum == enemy->s.number;
}

void Face(gentity_t *entity, const vec3_t point) {
    vec3_t direction;
    VectorSubtract(point, entity->client->ps.origin, direction);
    entity->client->ps.viewangles[YAW] = vectoyaw(direction);
}

void FindEnemy(gentity_t *entity) {
    auto &npc = entity->doomNpc;
    if (npc.enemy >= 0 && npc.enemy < MAX_CLIENTS && Alive(&g_entities[npc.enemy])) {
        return;
    }
    npc.enemy = -1;
    float nearest = 4096;
    for (int i = 0; i < level.maxclients; ++i) {
        auto *candidate = &g_entities[i];
        if (candidate->doomNpc.type || !Alive(candidate)) {
            continue;
        }
        const float distance = Distance(entity->r.currentOrigin, candidate->r.currentOrigin);
        vec3_t direction;
        VectorSubtract(candidate->r.currentOrigin, entity->r.currentOrigin, direction);
        const float delta =
            fabs(AngleSubtract(vectoyaw(direction), entity->client->ps.viewangles[YAW]));
        if (distance < nearest && (delta < 90 || distance < 128) && Visible(entity, candidate)) {
            nearest = distance;
            npc.enemy = i;
        }
    }
    if (npc.enemy >= 0) {
        Sound(entity, definitions[npc.type].seeSound);
        npc.attackReady = level.time + 8 * 1000 / 35;
        npc.moveCount = 0;
    }
}

bool Walkable(gentity_t *entity, const vec3_t end) {
    trace_t trace;
    vec3_t start, candidate, down;
    for (int lift = 0; lift <= 24; lift += 4) {
        VectorCopy(entity->client->ps.origin, start);
        VectorCopy(end, candidate);
        start[2] += lift + 0.125f;
        candidate[2] = start[2];
        trap_Trace(&trace, start, entity->r.mins, entity->r.maxs, candidate, entity->s.number,
                   MonsterSolid);
        if (trace.startsolid || trace.allsolid || trace.fraction < 1) {
            continue;
        }
        VectorCopy(candidate, down);
        down[2] = entity->client->ps.origin[2] - 24;
        trap_Trace(&trace, candidate, entity->r.mins, entity->r.maxs, down, entity->s.number,
                   MonsterSolid);
        if (!trace.startsolid && !trace.allsolid && trace.fraction < 1 &&
            trace.plane.normal[2] >= 0.7f) {
            return !(trap_PointContents(trace.endpos, entity->s.number) &
                     (CONTENTS_LAVA | CONTENTS_SLIME));
        }
    }
    G_DoomTraceLine(entity, entity->r.currentOrigin, end, qfalse);
    return false;
}

bool ClearSpawn(gentity_t *entity, vec3_t point) {
    trace_t trace;
    trap_Trace(&trace, point, entity->r.mins, entity->r.maxs, point, entity->s.number,
               MonsterSolid);
    return !trace.startsolid && !trace.allsolid;
}

void ResolveSpawn(gentity_t *entity) {
    auto &ps = entity->client->ps;
    if (ClearSpawn(entity, ps.origin)) {
        return;
    }
    // Original Doom tolerates an initially overlapping thing while it moves
    // out. Pmove requires a clear starting hull. Find the closest clear point
    // at the original floor height without changing the map's source things.
    const int limit = max(32, definitions[entity->doomNpc.type].radius * 2);
    for (int radius = 0; radius <= limit; radius += 4) {
        for (int side = 0; side < 16; ++side) {
            const float angle = side * static_cast<float>(M_PI / 8);
            vec3_t point;
            VectorCopy(ps.origin, point);
            point[0] += cos(angle) * radius;
            point[1] += sin(angle) * radius;
            for (int lift = 0; lift <= 24; lift += 4) {
                point[2] = ps.origin[2] + lift + HullInset;
                if (ClearSpawn(entity, point)) {
                    VectorCopy(point, ps.origin);
                    G_Printf("Doom spawn clearance: actor=%d offset=%d lift=%d\n", entity->s.number,
                             radius, lift);
                    return;
                }
            }
        }
    }
    trace_t worldTrace;
    trap_Trace(&worldTrace, ps.origin, ps.mins, ps.maxs, ps.origin, entity->s.number, MASK_SOLID);
    // Packed original things and floating things against a lower neighboring
    // ceiling can require clearance in the other direction. Keep this fallback
    // local and deterministic rather than leaving an actor permanently solid.
    const int heights[] = {0, -4, 4, -8, 8, -16, 16, -24, 24, -32, 32, -48, 48, -64, 64};
    for (int radius = 0; radius <= max(128, limit); radius += 8) {
        for (int side = 0; side < 16; ++side) {
            const float angle = side * static_cast<float>(M_PI / 8);
            for (int height : heights) {
                vec3_t point = {ps.origin[0] + static_cast<float>(cos(angle) * radius),
                                ps.origin[1] + static_cast<float>(sin(angle) * radius),
                                ps.origin[2] + height};
                if (ClearSpawn(entity, point)) {
                    VectorCopy(point, ps.origin);
                    G_Printf("Doom spawn clearance: actor=%d offset=%d height=%d\n",
                             entity->s.number, radius, height);
                    return;
                }
            }
        }
    }
    G_Printf("WARNING: No clear Doom spawn hull for actor %d (%s) world=%d at=%.1f %.1f %.1f\n",
             entity->s.number, definitions[entity->doomNpc.type].name,
             worldTrace.startsolid || worldTrace.allsolid, ps.origin[0], ps.origin[1],
             ps.origin[2]);
}

int ChaseInterval(gentity_t *entity) {
    const int tics = Sprites(entity->doomNpc.type).clips[Chase].steps[0].tics;
    const bool fast =
        G_DoomSkill() >= 4 && (entity->doomNpc.type == Demon || entity->doomNpc.type == Spectre);
    return max(1, ((fast ? max(1, tics / 2) : tics) * 1000 + 34) / 35);
}

void RouteGoal(gentity_t *entity) {
    auto &npc = entity->doomNpc;
    if (level.time < npc.nextRoute) {
        return;
    }
    npc.nextRoute = level.time + 250;
    VectorCopy(npc.lastKnown, npc.routePoint);
    const auto &definition = definitions[npc.type];
    if (definition.flying || !navigationSetup) {
        return;
    }
    trap_AAS_SetCurrentWorld(definition.world);
    if (trap_AAS_Initialized()) {
        vec3_t origin, goal;
        VectorCopy(entity->client->ps.origin, origin);
        VectorCopy(npc.lastKnown, goal);
        origin[2] += 1;
        goal[2] += 1;
        ++navigationQueries[definition.world];
        if (trap_AAS_Route(origin, goal, TFL_WALK | TFL_WALKOFFLEDGE | TFL_AIR, npc.routePoint)) {
            ++navigationRoutes[definition.world];
            ++npc.routes;
        }
    }
    trap_AAS_SetCurrentWorld(0);
}

void DirectionVector(int direction, vec3_t vector) {
    const float angle = direction * static_cast<float>(M_PI / 4);
    VectorSet(vector, cos(angle), sin(angle), 0);
}

bool CanMove(gentity_t *entity, int direction) {
    if (direction < 0 || direction >= NoDirection) {
        return false;
    }
    vec3_t vector, end;
    DirectionVector(direction, vector);
    // Doom tests one movement step, not a full body width ahead. A 32-unit
    // probe incorrectly rejects narrow landings and approaching closed doors.
    const float step =
        max(8.0f, definitions[entity->doomNpc.type].speed * ChaseInterval(entity) / 1000.0f);
    VectorMA(entity->client->ps.origin, step, vector, end);
    if (definitions[entity->doomNpc.type].flying) {
        trace_t trace;
        trap_Trace(&trace, entity->client->ps.origin, entity->r.mins, entity->r.maxs, end,
                   entity->s.number, MonsterSolid);
        return !trace.startsolid && !trace.allsolid && trace.fraction == 1;
    }
    if (Walkable(entity, end)) {
        return true;
    }
    // P_Move's special-line test includes the actor's radius. A center ray of
    // only one movement step cannot reach a door touched by the front of its hull.
    VectorMA(entity->client->ps.origin, step + definitions[entity->doomNpc.type].radius + 2, vector,
             end);
    G_DoomTraceLine(entity, entity->client->ps.origin, end, qfalse);
    return false;
}

bool TryWalk(gentity_t *entity, int direction) {
    if (!CanMove(entity, direction)) {
        return false;
    }
    entity->doomNpc.moveDirection = direction;
    entity->doomNpc.moveCount = Random(entity) & 15;
    return true;
}

void NewChaseDirection(gentity_t *entity) {
    auto &npc = entity->doomNpc;
    RouteGoal(entity);
    const int oldDirection = npc.moveDirection;
    const int turnaround = oldDirection < NoDirection ? (oldDirection + 4) & 7 : NoDirection;
    const float dx = npc.routePoint[0] - entity->client->ps.origin[0];
    const float dy = npc.routePoint[1] - entity->client->ps.origin[1];
    int horizontal = dx > 10 ? 0 : dx < -10 ? 4 : NoDirection;
    int vertical = dy > 10 ? 2 : dy < -10 ? 6 : NoDirection;
    if (horizontal != NoDirection && vertical != NoDirection) {
        const int diagonal = dy < 0 ? (dx > 0 ? 7 : 5) : (dx > 0 ? 1 : 3);
        if (diagonal != turnaround && TryWalk(entity, diagonal)) {
            return;
        }
    }
    if (Random(entity) > 200 || fabs(dy) > fabs(dx)) {
        const int temporary = horizontal;
        horizontal = vertical;
        vertical = temporary;
    }
    if (horizontal != turnaround && TryWalk(entity, horizontal)) {
        return;
    }
    if (vertical != turnaround && TryWalk(entity, vertical)) {
        return;
    }
    if (TryWalk(entity, oldDirection)) {
        return;
    }
    const bool reverse = (Random(entity) & 1) != 0;
    for (int i = 0; i < NoDirection; ++i) {
        const int candidate = reverse ? 7 - i : i;
        if (candidate != turnaround && TryWalk(entity, candidate)) {
            return;
        }
    }
    if (!TryWalk(entity, turnaround)) {
        npc.moveDirection = NoDirection;
        npc.moveCount = 0;
    }
}

float ApproximateDistance(const vec3_t source, const vec3_t target) {
    const float dx = fabs(target[0] - source[0]);
    const float dy = fabs(target[1] - source[1]);
    return max(dx, dy) + min(dx, dy) * 0.5f;
}

bool HasMelee(int type) {
    return type == Imp || type == Demon || type == Spectre || type == Baron || type == Cacodemon;
}

bool MeleeRange(gentity_t *entity, gentity_t *enemy) {
    const auto &definition = definitions[entity->doomNpc.type];
    const float enemyHeight = enemy->r.maxs[2] - enemy->r.mins[2];
    const float enemyFoot = enemy->r.currentOrigin[2] + enemy->r.mins[2];
    const float foot = entity->client->ps.origin[2];
    const float radius = enemy->doomNpc.type ? definitions[enemy->doomNpc.type].radius : 16;
    return HasMelee(entity->doomNpc.type) &&
           ApproximateDistance(entity->r.currentOrigin, enemy->r.currentOrigin) < 44 + radius &&
           foot < enemyFoot + enemyHeight && foot + definition.height > enemyFoot &&
           Visible(entity, enemy);
}

bool MissileRange(gentity_t *entity, bool visible) {
    auto &npc = entity->doomNpc;
    if (!visible || definitions[npc.type].attack == Bite) {
        return false;
    }
    if (npc.justHit) {
        npc.justHit = 0;
        return true;
    }
    if (level.time < npc.attackReady) {
        return false;
    }
    float distance = ApproximateDistance(entity->r.currentOrigin, npc.lastKnown) - 64;
    if (!HasMelee(npc.type)) {
        distance -= 128;
    }
    if (npc.type == Cyberdemon || npc.type == SpiderMastermind || npc.type == LostSoul) {
        distance *= 0.5f;
    }
    const int threshold = min(npc.type == Cyberdemon ? 160 : 200, static_cast<int>(distance));
    return static_cast<int>(Random(entity)) >= threshold;
}

void ChaseDecision(gentity_t *entity) {
    auto &npc = entity->doomNpc;
    FindEnemy(entity);
    if (npc.enemy < 0) {
        npc.moveDirection = NoDirection;
        Animate(entity, Idle);
        return;
    }
    auto *enemy = &g_entities[npc.enemy];
    // Like the original A_Chase, an alerted monster retains its target behind
    // walls. AAS supplies a physical waypoint instead of losing it after 10 s.
    VectorCopy(enemy->r.currentOrigin, npc.lastKnown);
    npc.lastKnown[2] += enemy->r.mins[2];
    const bool visible = Visible(entity, enemy);
    if (visible) {
        npc.lastSeen = level.time;
    }
    if (npc.targetThreshold > 0) {
        --npc.targetThreshold;
    }
    Animate(entity, Chase);
    if (npc.justAttacked) {
        npc.justAttacked = 0;
        if (G_DoomSkill() < 4) {
            NewChaseDirection(entity);
            return;
        }
    }
    if (MeleeRange(entity, enemy)) {
        Face(entity, enemy->r.currentOrigin);
        Animate(entity, Melee, true);
        return;
    }
    if ((npc.moveCount <= 0 || G_DoomSkill() >= 4) && MissileRange(entity, visible)) {
        Face(entity, enemy->r.currentOrigin);
        Animate(entity, Missile, true);
        npc.justAttacked = 1;
        return;
    }
    if (--npc.moveCount < 0 || !CanMove(entity, npc.moveDirection)) {
        NewChaseDirection(entity);
    }
}

bool MoveDirection(gentity_t *entity, vec3_t direction) {
    auto &npc = entity->doomNpc;
    if (npc.moveDirection >= NoDirection) {
        return false;
    }
    DirectionVector(npc.moveDirection, direction);
    if (definitions[npc.type].flying) {
        // Float toward the target's body height independently of walking. The
        // original float rule avoids a dive toward the player's floor origin.
        const float dz = npc.lastKnown[2] + 24 - entity->client->ps.origin[2];
        const float distance = ApproximateDistance(entity->client->ps.origin, npc.lastKnown);
        direction[2] = fabs(dz) > 8 ? max(-0.7f, min(0.7f, dz / max(32.0f, distance))) : 0;
        VectorNormalize(direction);
    }
    return true;
}

void Projectile(gentity_t *entity, gentity_t *enemy) {
    const auto &definition = definitions[entity->doomNpc.type];
    auto *bolt = G_Spawn();
    bolt->classname = "doom_monster_projectile";
    bolt->s.eType = ET_MISSILE;
    const int type = definition.attack == Fireball    ? ImpFireball
                     : definition.attack == CacoBall  ? CacoFireball
                     : definition.attack == BaronBall ? BaronFireball
                                                      : DoomRocket;
    bolt->s.density = NetworkBase + type;
    bolt->s.legsAnim = Idle;
    bolt->s.effect1Time = level.time;
    bolt->s.time2 = level.time + 10000;
    bolt->parent = entity;
    bolt->r.ownerNum = entity->s.number;
    bolt->clipmask = MASK_MISSILESHOT;
    bolt->damage = definition.damage * (1 + Random(entity) % 8);
    bolt->splashDamage = type == DoomRocket ? 128 : 0;
    bolt->splashRadius = bolt->splashDamage;
    vec3_t start, target, direction;
    VectorCopy(entity->client->ps.origin, start);
    start[2] += definition.height * 0.6f;
    VectorCopy(enemy->r.currentOrigin, target);
    target[2] += enemy->client ? enemy->client->ps.viewheight * 0.5f : 28;
    VectorSubtract(target, start, direction);
    VectorNormalize(direction);
    if (enemy->client && enemy->client->ps.powerups[PW_INVIS] > level.time) {
        vec3_t angles;
        vectoangles(direction, angles);
        angles[YAW] +=
            (static_cast<int>(Random(entity) & 255) - static_cast<int>(Random(entity) & 255)) *
            (45.0f / 256.0f);
        AngleVectors(angles, direction, nullptr, nullptr);
    }
    vectoangles(direction, bolt->s.angles);
    bolt->s.pos.trType = TR_LINEAR;
    bolt->s.pos.trTime = level.time;
    VectorCopy(start, bolt->s.pos.trBase);
    VectorCopy(start, bolt->r.currentOrigin);
    VectorScale(direction,
                type == DoomRocket    ? 700.0f
                : type == ImpFireball ? 350.0f
                                      : 525.0f,
                bolt->s.pos.trDelta);
    VectorSet(bolt->r.mins, -6, -6, -8);
    VectorSet(bolt->r.maxs, 6, 6, 8);
    Sprites(type);
    trap_LinkEntity(bolt);
    Sound(entity, type == DoomRocket ? "rlaunc" : "firsht");
}

void AttackEnemy(gentity_t *entity) {
    auto &npc = entity->doomNpc;
    if (npc.enemy < 0 || npc.enemy >= MAX_CLIENTS || !Alive(&g_entities[npc.enemy])) {
        return;
    }
    auto *enemy = &g_entities[npc.enemy];
    const auto &definition = definitions[npc.type];
    Face(entity, enemy->r.currentOrigin);
    ++npc.shots;
    if (definition.attack == Charge) {
        vec3_t target;
        VectorCopy(enemy->r.currentOrigin, target);
        target[2] += 24;
        VectorSubtract(target, entity->client->ps.origin, entity->client->ps.velocity);
        VectorNormalize(entity->client->ps.velocity);
        VectorScale(entity->client->ps.velocity, 700, entity->client->ps.velocity);
        npc.charging = level.time + 2500;
        Sound(entity, definition.attackSound);
        return;
    }
    if (npc.clip == Melee || definition.attack == Bite) {
        if (MeleeRange(entity, enemy)) {
            const int dice = npc.type == Demon || npc.type == Spectre ? 10
                             : npc.type == Cacodemon                  ? 6
                                                                      : 8;
            const int multiplier =
                npc.type == Baron || npc.type == Cacodemon ? 10 : definition.damage;
            const int damage = multiplier * (1 + Random(entity) % dice);
            G_Damage(enemy, entity, entity, nullptr, nullptr, damage, DAMAGE_NO_KNOCKBACK,
                     MOD_GAUNTLET);
            Sound(entity, definition.attackSound);
        }
        return;
    }
    if (!Visible(entity, enemy)) {
        return;
    }
    if (definition.attack == Bullet || definition.attack == Shotgun ||
        definition.attack == Chaingun) {
        const int pellets = definition.attack == Bullet ? 1 : 3;
        Sound(entity, definition.attackSound);
        for (int i = 0; i < pellets; ++i) {
            vec3_t start, target, angles, direction, end;
            VectorCopy(entity->client->ps.origin, start);
            start[2] += entity->client->ps.viewheight;
            VectorCopy(enemy->r.currentOrigin, target);
            target[2] += enemy->client->ps.viewheight * 0.5f;
            VectorSubtract(target, start, direction);
            vectoangles(direction, angles);
            if (enemy->client->ps.powerups[PW_INVIS] > level.time) {
                angles[YAW] += (static_cast<int>(Random(entity) & 255) -
                                static_cast<int>(Random(entity) & 255)) *
                               (45.0f / 256.0f);
            }
            angles[YAW] += (static_cast<int>(Random(entity) % 256) - 128) * (22.5f / 256);
            AngleVectors(angles, direction, nullptr, nullptr);
            VectorMA(start, 2048, direction, end);
            trace_t trace;
            trap_Trace(&trace, start, nullptr, nullptr, end, entity->s.number, MASK_SHOT);
            if (trace.entityNum < ENTITYNUM_WORLD && g_entities[trace.entityNum].takedamage) {
                G_Damage(&g_entities[trace.entityNum], entity, entity, direction, trace.endpos,
                         definition.damage * (1 + Random(entity) % 5), DAMAGE_NO_KNOCKBACK,
                         MOD_MACHINEGUN);
            }
        }
    } else {
        Projectile(entity, enemy);
    }
}
} // namespace

void G_DoomAIInit() {
    memset(&doomProgress, 0, sizeof(doomProgress));
    memset(spriteLoaded, 0, sizeof(spriteLoaded));
    memset(navigationQueries, 0, sizeof(navigationQueries));
    memset(navigationRoutes, 0, sizeof(navigationRoutes));
    navigationSetup = false;
    vmCvar_t skill;
    trap_Cvar_Register(&skill, "doom_skill", "2", CVAR_SERVERINFO | CVAR_ARCHIVE);
    saveGamePending = g_gametype.integer == GT_SINGLE_PLAYER ? qtrue : qfalse;
    trap_Cvar_Set("g_reloading", "0");
}
DOOM_SAVE_CALLBACK("G_DoomAIInit", G_DoomAIInit);

void G_DoomAIShutdown() {
    // These slots belong to map actors, not persistent player bots. Release them
    // before a VM restart clears their identifying flags and respawns the map.
    for (int i = 1; i < level.maxclients; ++i) {
        const auto &entity = g_entities[i];
        if (entity.inuse && entity.doomNpc.type && (entity.r.svFlags & SVF_AI)) {
            trap_BotFreeClient(i);
        }
    }
    if (navigationSetup) {
        trap_BotLibShutdown();
    }
    navigationSetup = false;
}
DOOM_SAVE_CALLBACK("G_DoomAIShutdown", G_DoomAIShutdown);

void G_DoomAIBeginFrame() {
    if (navigationSetup) {
        trap_BotLibStartFrame(level.time / 1000.0f);
    }
}
DOOM_SAVE_CALLBACK("G_DoomAIBeginFrame", G_DoomAIBeginFrame);

qboolean G_DoomAIConfigure(gentity_t *entity) {
    const int type = TypeForClass(entity->classname);
    if (!type) {
        return qfalse;
    }
    entity->doomNpc.type = type;
    int options;
    G_SpawnInt("spawnflags", "7", &options);
    G_SpawnInt("doom_options", va("%d", options), &entity->doomNpc.options);
    entity->think = G_DoomAISpawnThink;
    entity->nextthink = level.time + 400;
    G_SetOrigin(entity, entity->s.origin);
    return qtrue;
}
DOOM_SAVE_CALLBACK("G_DoomAIConfigure", G_DoomAIConfigure);

// Barrels are replicated props, not client-backed monsters. Damage is delayed
// until Doom's third explosion frame so adjacent barrels chain in sequence.
void DoomBarrelExplode(gentity_t *barrel) {
    barrel->r.contents = 0;
    trap_LinkEntity(barrel);
    G_RadiusDamage(barrel->r.currentOrigin, barrel->parent ? barrel->parent : barrel, 128, 128,
                   barrel, MOD_EXPLOSIVE);
    barrel->think = G_FreeEntity;
    barrel->nextthink = level.time + 715;
    G_Printf("Doom barrel exploded: %d\n", barrel->s.number);
}
DOOM_SAVE_CALLBACK("DoomBarrelExplode", DoomBarrelExplode);

void DoomBarrelDie(gentity_t *barrel, gentity_t *, gentity_t *attacker, int, int) {
    if (!barrel->takedamage) {
        return;
    }
    barrel->takedamage = qfalse;
    barrel->parent = attacker;
    barrel->s.legsAnim = Death;
    barrel->s.effect1Time = level.time;
    Sound(barrel, "barexp");
    barrel->think = DoomBarrelExplode;
    barrel->nextthink = level.time + 286;
}
DOOM_SAVE_CALLBACK("DoomBarrelDie", DoomBarrelDie);

void G_DoomAISpawnThings(const void *mapData) {
    trap_BotLibVarSet("maxclients", va("%d", level.maxclients));
    trap_BotLibVarSet("maxentities", va("%d", MAX_GENTITIES));
    trap_BotLibVarSet("doom_navonly", "1");
    trap_BotLibVarSet("sv_mapChecksum", va("%d", trap_Cvar_VariableIntegerValue("sv_mapChecksum")));
    navigationSetup = trap_BotLibSetup() == 0;
    if (navigationSetup) {
        char map[MAX_QPATH];
        trap_Cvar_VariableStringBuffer("mapname", map, sizeof(map));
        const int result = trap_BotLibLoadMap(map);
        G_Printf("Doom navigation: %s result=%d\n", map, result);
    }
    const auto *header = static_cast<const dbsp::Header *>(mapData);
    const auto *base = static_cast<const byte *>(mapData);
    const auto *things =
        reinterpret_cast<const dbsp::Thing *>(base + header->lumps[dbsp::Things].offset);
    const auto *sectors =
        reinterpret_cast<const dbsp::Sector *>(base + header->lumps[dbsp::Sectors].offset);
    const int count = header->lumps[dbsp::Things].length / sizeof(dbsp::Thing);
    int spawned = 0, props = 0, barrels = 0;
    for (int i = 0; i < count; ++i) {
        const auto &thing = things[i];
        const int type = TypeForThing(thing.type);
        if (g_gametype.integer != GT_SINGLE_PLAYER && type >= Zombie && type <= SpiderMastermind) {
            continue;
        }
        if (!type || thing.sector < 0 ||
            !SpawnAllowed(thing.options, G_DoomSkill(), g_gametype.integer != GT_SINGLE_PLAYER)) {
            continue;
        }
        auto *source = G_Spawn();
        source->classname = const_cast<char *>(definitions[type].classname);
        source->doomNpc.type = type;
        source->doomNpc.options = thing.options;
        source->think = G_DoomAISpawnThink;
        source->nextthink = level.time + 400;
        vec3_t origin = {static_cast<float>(thing.x), static_cast<float>(thing.y),
                         static_cast<float>(sectors[thing.sector].floor) + HullInset};
        if (definitions[type].flying) {
            const float clearance =
                static_cast<float>(sectors[thing.sector].ceiling - sectors[thing.sector].floor -
                                   definitions[type].height);
            origin[2] += max(0.0f, min(24.0f, clearance - 2 * HullInset));
        }
        G_SetOrigin(source, origin);
        source->s.angles[YAW] = static_cast<float>(thing.angle);
        if (type == TechPillar || type == Barrel) {
            Sprites(type);
            source->doomNpc.type = 0;
            source->s.eType = ET_GENERAL;
            source->s.density = NetworkBase + type;
            source->s.legsAnim = Idle;
            source->s.effect1Time = level.time;
            source->think = nullptr;
            source->nextthink = 0;
            if (type == Barrel) {
                source->health = definitions[type].health;
                source->takedamage = qtrue;
                source->die = DoomBarrelDie;
            }
            source->r.contents = CONTENTS_SOLID;
            VectorSet(source->r.mins, -definitions[type].radius, -definitions[type].radius, 0);
            VectorSet(source->r.maxs, definitions[type].radius, definitions[type].radius,
                      definitions[type].height);
            trap_LinkEntity(source);
            if (type == Barrel) {
                ++barrels;
            } else {
                ++props;
            }
            continue;
        }
        ++spawned;
    }
    G_Printf(
        "Doom monsters: %d spawn anchors, skill=%d, capacity=%d; %d tech pillars; %d barrels\n",
        spawned, G_DoomSkill(), MAX_CLIENTS - 1, props, barrels);
}
DOOM_SAVE_CALLBACK("G_DoomAISpawnThings", G_DoomAISpawnThings);

void G_DoomAISpawnThink(gentity_t *source) {
    const int type = source->doomNpc.type;
    if (!SpawnAllowed(source->doomNpc.options, G_DoomSkill(),
                      g_gametype.integer != GT_SINGLE_PLAYER)) {
        G_FreeEntity(source);
        return;
    }
    const int slot = trap_BotAllocateClient();
    if (slot < 1 || slot >= level.maxclients) {
        G_Error("Doom monster capacity exhausted spawning %s (maximum %d)", source->classname,
                level.maxclients - 1);
        return;
    }
    const auto &definition = definitions[type];
    Sprites(type);
    auto *entity = &g_entities[slot];
    *entity = *source;
    entity->s.number = slot;
    entity->s.eType = ET_PLAYER;
    entity->s.clientNum = slot;
    entity->s.density = NetworkBase + type;
    entity->think = nullptr;
    entity->nextthink = 0;
    entity->client = &level.clients[slot];
    memset(entity->client, 0, sizeof(*entity->client));
    entity->r.svFlags = SVF_BOT | SVF_AI;
    entity->r.linked = qfalse;
    entity->r.contents = CONTENTS_BODY;
    entity->clipmask = MonsterSolid;
    entity->takedamage = qtrue;
    entity->health = definition.health;
    entity->doomNpc.enemy = -1;
    entity->doomNpc.moveDirection = NoDirection;
    entity->doomNpc.randomState = source->s.number * 7919u + 17;
    entity->doomNpc.nextDecision = level.time + slot % 4 * 25;
    Animate(entity, Idle, true);
    auto &client = *entity->client;
    client.pers.connected = CON_CONNECTED;
    client.sess.sessionTeam = TEAM_FREE;
    auto &ps = client.ps;
    ps.clientNum = slot;
    ps.commandTime = level.time;
    ps.groundEntityNum = ENTITYNUM_NONE;
    ps.pm_type = PM_NORMAL;
    ps.gravity = 800;
    ps.friction = 1;
    ps.speed = definition.speed;
    ps.runSpeedScale = ps.sprintSpeedScale = ps.crouchSpeedScale = 1;
    ps.stats[STAT_HEALTH] = ps.stats[STAT_MAX_HEALTH] = entity->health;
    ps.stats[STAT_DOOM_MONSTER] = type;
    client.pers.maxHealth = entity->health;
    VectorSet(ps.mins, -definition.radius + HullInset, -definition.radius + HullInset, 0);
    VectorSet(ps.maxs, definition.radius - HullInset, definition.radius - HullInset,
              definition.height - 2 * HullInset);
    VectorCopy(ps.mins, entity->r.mins);
    VectorCopy(ps.maxs, entity->r.maxs);
    ps.crouchMaxZ = ps.maxs[2];
    ps.standViewHeight = ps.crouchViewHeight = definition.height * 3 / 4;
    ps.viewheight = ps.standViewHeight;
    ps.deadViewHeight = 4;
    VectorCopy(source->r.currentOrigin, ps.origin);
    VectorCopy(source->s.angles, ps.viewangles);
    ResolveSpawn(entity);
    if (definition.flying) {
        ps.powerups[PW_FLIGHT] = INT_MAX;
    } else {
        vec3_t down;
        VectorCopy(ps.origin, down);
        down[2] -= 256;
        trace_t trace;
        trap_Trace(&trace, ps.origin, ps.mins, ps.maxs, down, slot, MonsterSolid);
        if (!trace.startsolid && !trace.allsolid && trace.fraction < 1) {
            VectorCopy(trace.endpos, ps.origin);
        }
    }
    G_SetOrigin(entity, ps.origin);
    char userinfo[MAX_INFO_STRING] = "";
    Info_SetValueForKey(userinfo, "name", definition.name);
    Info_SetValueForKey(userinfo, "doomnpc", va("%d", type));
    trap_SetUserinfo(slot, userinfo);
    trap_SetConfigstring(CS_PLAYERS + slot, va("n\\%s\\t\\0\\doomnpc\\%d", definition.name, type));
    G_DoomAIReplicate(entity);
    trap_LinkEntity(entity);
    G_FreeEntity(source);
    G_Printf("Doom AI spawn: %d %s health=%d\n", slot, definition.name, entity->health);
}
DOOM_SAVE_CALLBACK("G_DoomAISpawnThink", G_DoomAISpawnThink);

void G_DoomAIReplicate(gentity_t *entity) {
    BG_PlayerStateToEntityState(&entity->client->ps, &entity->s, qfalse);
    entity->s.eType = ET_PLAYER;
    entity->s.density = NetworkBase + entity->doomNpc.type;
    entity->s.legsAnim = entity->doomNpc.clip;
    entity->s.torsoAnim = entity->doomNpc.clip;
    entity->s.effect1Time = entity->doomNpc.clipStart;
    VectorCopy(entity->client->ps.origin, entity->r.currentOrigin);
}
DOOM_SAVE_CALLBACK("G_DoomAIReplicate", G_DoomAIReplicate);

void G_DoomAIRun(gentity_t *entity) {
    auto &npc = entity->doomNpc;
    if (saveGamePending || g_reloading.integer || level.reloadPauseTime > level.time ||
        level.intermissiontime || trap_Cvar_VariableIntegerValue("doom_ai_disable")) {
        return;
    }
    auto &ps = entity->client->ps;
    const auto &definition = definitions[npc.type];
    vec3_t direction{};
    bool moving = false;
    if (entity->health > 0) {
        if (npc.charging && level.time >= npc.charging) {
            npc.charging = 0;
            VectorClear(ps.velocity);
            Animate(entity, Chase);
        }
        const auto &animation = Sprites(npc.type).clips[npc.clip];
        const int elapsed = level.time - npc.clipStart;
        if (npc.clip == Melee || npc.clip == Missile) {
            int time = 0;
            for (int i = 0; i < animation.count; ++i) {
                if (i >= npc.eventStep && elapsed * 35 >= time * 1000) {
                    npc.eventStep = i + 1;
                    if (animation.steps[i].event) {
                        AttackEnemy(entity);
                    }
                }
                time += animation.steps[i].tics;
            }
        }
        bool busy = (npc.clip == Melee || npc.clip == Missile || npc.clip == Pain) &&
                    elapsed < DurationMsec(animation);
        // A_SpidRefire loops the firing frames while the target remains visible;
        // the initial face-target delay runs only on entry to the attack state.
        if (!busy && npc.type == SpiderMastermind && npc.clip == Missile && npc.enemy >= 0 &&
            Alive(&g_entities[npc.enemy]) && Visible(entity, &g_entities[npc.enemy])) {
            npc.clipStart = level.time - animation.steps[0].tics * 1000 / 35;
            npc.eventStep = 1;
            busy = true;
        }
        if (!busy && level.time >= npc.nextDecision) {
            npc.nextDecision = level.time + ChaseInterval(entity);
            ChaseDecision(entity);
        }
        if (npc.clip == Chase && npc.enemy >= 0 && !npc.charging) {
            moving = MoveDirection(entity, direction);
        }
    } else if (!npc.bossDone &&
               level.time - npc.clipStart >= DurationMsec(Sprites(npc.type).clips[npc.clip])) {
        npc.bossDone = 1;
        G_DoomAIBossDeath(npc.type);
    }
    usercmd_t command{};
    command.serverTime = level.time;
    ps.speed = definition.speed *
               (G_DoomSkill() >= 4 && (npc.type == Demon || npc.type == Spectre) ? 2 : 1);
    if (moving) {
        ps.viewangles[YAW] = vectoyaw(direction);
        command.forwardmove = 127;
        if (definition.flying) {
            command.forwardmove = static_cast<signed char>(
                127 * sqrt(direction[0] * direction[0] + direction[1] * direction[1]));
            command.upmove = static_cast<signed char>(127 * direction[2]);
        }
    }
    for (int i = 0; i < 3; ++i) {
        command.angles[i] = ANGLE2SHORT(ps.viewangles[i]) - ps.delta_angles[i];
    }
    VectorCopy(ps.origin, entity->client->oldOrigin);
    if (npc.charging > level.time) {
        vec3_t end;
        VectorMA(ps.origin, min(0.1f, (level.time - level.previousTime) / 1000.0f), ps.velocity,
                 end);
        trace_t trace;
        trap_Trace(&trace, ps.origin, ps.mins, ps.maxs, end, entity->s.number, MonsterSolid);
        VectorCopy(trace.endpos, ps.origin);
        ps.commandTime = level.time;
        if (trace.fraction < 1 || trace.startsolid) {
            if (trace.entityNum < ENTITYNUM_WORLD && g_entities[trace.entityNum].takedamage) {
                G_Damage(&g_entities[trace.entityNum], entity, entity, nullptr, nullptr,
                         3 * (1 + Random(entity) % 8), DAMAGE_NO_KNOCKBACK, MOD_GAUNTLET);
            }
            npc.charging = 0;
            VectorClear(ps.velocity);
            Animate(entity, Chase);
        }
    } else {
        pmove_t movement{};
        movement.ps = &ps;
        movement.cmd = command;
        movement.tracemask = entity->health > 0 ? MonsterSolid : MASK_SOLID;
        movement.trace = trap_Trace;
        movement.pointcontents = trap_PointContents;
        movement.noFootsteps = qtrue;
        movement.noWeapClips = qtrue;
        Pmove(&movement);
        VectorCopy(movement.mins, entity->r.mins);
        VectorCopy(movement.maxs, entity->r.maxs);
    }
    G_DoomAIReplicate(entity);
    trap_LinkEntity(entity);
    if (entity->health > 0) {
        G_TouchTriggers(entity);
        G_DoomPlayerSector(entity);
    }
}
DOOM_SAVE_CALLBACK("G_DoomAIRun", G_DoomAIRun);

void G_DoomAIHear(int sourceNum, const vec3_t origin, float) {
    if (sourceNum < 0 || sourceNum >= MAX_CLIENTS || !Alive(&g_entities[sourceNum])) {
        return;
    }
    auto *source = &g_entities[sourceNum];
    for (int i = 1; i < level.maxclients; ++i) {
        auto *entity = &g_entities[i];
        if (!entity->doomNpc.type || !Alive(entity) || entity == source ||
            entity->doomNpc.enemy >= 0 || !G_DoomCanHear(origin, entity->r.currentOrigin) ||
            ((entity->doomNpc.options & 8) && !Visible(entity, source))) {
            continue;
        }
        entity->doomNpc.enemy = sourceNum;
        entity->doomNpc.attackReady = level.time + 8 * 1000 / 35;
        entity->doomNpc.lastSeen = level.time;
        VectorCopy(origin, entity->doomNpc.lastKnown);
        entity->doomNpc.lastKnown[2] += source->r.mins[2];
    }
}
DOOM_SAVE_CALLBACK("G_DoomAIHear", G_DoomAIHear);

bool G_DoomAIDamage(gentity_t *entity, gentity_t *attacker, int damage, int flags, int mod) {
    auto &npc = entity->doomNpc;
    if (!npc.type) {
        return false;
    }
    if (entity->health <= 0 || (entity->flags & FL_GODMODE) || damage <= 0) {
        return true;
    }
    if ((npc.type == Cyberdemon || npc.type == SpiderMastermind) && mod == MOD_ROCKET_SPLASH) {
        return true;
    }
    if (attacker && attacker != entity && Alive(attacker) &&
        (npc.targetThreshold == 0 || npc.enemy == attacker->s.number)) {
        npc.enemy = attacker->s.number;
        npc.justHit = 1;
        npc.targetThreshold = 100;
        npc.lastSeen = level.time;
        VectorCopy(attacker->r.currentOrigin, npc.lastKnown);
        npc.lastKnown[2] += attacker->r.mins[2];
    }
    entity->health -= damage;
    entity->client->ps.stats[STAT_HEALTH] = entity->health;
    if (entity->health <= 0) {
        const auto &definition = definitions[npc.type];
        Animate(entity, entity->health < -definition.health ? Gib : Death, true);
        Sound(entity, definition.deathSound);
        entity->takedamage = qfalse;
        entity->r.contents = CONTENTS_CORPSE;
        entity->client->ps.eFlags |= EF_DEAD;
        entity->client->ps.pm_type = PM_DEAD;
        entity->client->ps.maxs[2] = 8;
        entity->client->ps.powerups[PW_FLIGHT] = 0;
        npc.charging = 0;
        VectorClear(entity->client->ps.velocity);
        if (npc.type == Zombie || npc.type == ShotgunGuy) {
            extern void G_DoomDropMonsterPickup(gentity_t * entity, int type);
            G_DoomDropMonsterPickup(entity, npc.type);
        }
        G_UseTargets(entity, attacker);
    } else if (Random(entity) % 256 < static_cast<unsigned int>(definitions[npc.type].painChance)) {
        Animate(entity, Pain, true);
        npc.charging = 0;
        Sound(entity, definitions[npc.type].painSound);
    }
    return true;
}
DOOM_SAVE_CALLBACK("G_DoomAIDamage", G_DoomAIDamage);

void G_DoomAIProjectileRun(gentity_t *entity) {
    const int type = entity->s.density - NetworkBase;
    if (entity->s.time2 <= level.time) {
        G_FreeEntity(entity);
        return;
    }
    if (entity->s.legsAnim == Death) {
        return;
    }
    vec3_t end;
    BG_EvaluateTrajectory(&entity->s.pos, level.time, end);
    trace_t trace;
    trap_Trace(&trace, entity->r.currentOrigin, entity->r.mins, entity->r.maxs, end,
               entity->r.ownerNum, MASK_MISSILESHOT);
    VectorCopy(trace.endpos, entity->r.currentOrigin);
    if (trace.fraction < 1 || trace.startsolid) {
        if (trace.surfaceFlags & SURF_NOIMPACT) {
            G_FreeEntity(entity);
            return;
        }
        auto *victim = trace.entityNum < ENTITYNUM_WORLD ? &g_entities[trace.entityNum] : nullptr;
        if (victim && victim->takedamage) {
            // Same-species fireballs pass damage immunity through the impact path.
            if (!victim->doomNpc.type || !entity->parent ||
                victim->doomNpc.type != entity->parent->doomNpc.type || type == DoomRocket) {
                G_Damage(victim, entity, entity->parent, entity->s.pos.trDelta, trace.endpos,
                         entity->damage, DAMAGE_NO_KNOCKBACK, MOD_ROCKET);
            }
        }
        if (entity->splashDamage) {
            G_RadiusDamage(trace.endpos, entity->parent, entity->splashDamage, entity->splashRadius,
                           victim, MOD_ROCKET_SPLASH);
        }
        entity->s.pos.trType = TR_STATIONARY;
        VectorCopy(trace.endpos, entity->s.pos.trBase);
        entity->s.legsAnim = Death;
        entity->s.effect1Time = level.time;
        entity->s.time2 = level.time + DurationMsec(Sprites(type).clips[Death]);
        Sound(entity, type == DoomRocket ? "rxplod" : "firxpl");
        entity->r.contents = 0;
    }
    trap_LinkEntity(entity);
}
DOOM_SAVE_CALLBACK("G_DoomAIProjectileRun", G_DoomAIProjectileRun);

bool G_DoomAICommand(gentity_t *player, const char *command) {
    if (Q_stricmp(command, "doomai")) {
        return false;
    }
    if (!g_cheats.integer) {
        trap_SendServerCommand(player->s.number, "print \"doomai requires spdevmap.\n\"");
        return true;
    }
    char option[64];
    trap_Argv(1, option, sizeof(option));
    if (!Q_stricmp(option, "player") && player->client) {
        const auto &ps = player->client->ps;
        G_Printf("Doom player: at=%.1f %.1f %.1f height=%.2f skill=%d\n", ps.origin[0],
                 ps.origin[1], ps.origin[2], ps.maxs[2] - ps.mins[2], G_DoomSkill());
        int remaining = 0;
        for (int i = MAX_CLIENTS; i < level.num_entities; ++i) {
            if (g_entities[i].inuse && g_entities[i].s.eType == ET_DOOM_PICKUP) {
                ++remaining;
            }
        }
        G_Printf("Doom inventory: health=%d armor=%d type=%d keys=%d berserk=%d map=%d items=%d "
                 "remaining=%d powers=%d,%d,%d,%d\n",
                 ps.stats[STAT_HEALTH], ps.stats[STAT_ARMOR], ps.stats[STAT_DOOM_ARMOR_TYPE],
                 ps.stats[STAT_KEYS], ps.stats[STAT_DOOM_BERSERK], ps.stats[STAT_DOOM_MAP],
                 ps.stats[STAT_DOOM_ITEMS], remaining,
                 max(0, ps.powerups[PW_INVULNERABLE] - level.time),
                 max(0, ps.powerups[PW_INVIS] - level.time),
                 max(0, ps.powerups[PW_BATTLESUIT] - level.time),
                 max(0, ps.powerups[PW_QUAD] - level.time));
        return true;
    }
    if (!Q_stricmp(option, "nav")) {
        for (int world = 0; world < 4; ++world) {
            trap_AAS_SetCurrentWorld(world);
            G_Printf("Doom navigation: world=%d initialized=%d queries=%d routes=%d\n", world,
                     trap_AAS_Initialized(), navigationQueries[world], navigationRoutes[world]);
        }
        trap_AAS_SetCurrentWorld(0);
        return true;
    }
    if (!Q_stricmp(option, "spawn")) {
        char name[64];
        trap_Argv(2, name, sizeof(name));
        int type = TypeForClass(name);
        for (int candidate = Zombie; candidate < Marine; ++candidate) {
            if (!Q_stricmp(name, definitions[candidate].name)) {
                type = candidate;
            }
        }
        if (!type) {
            G_Printf("Unknown Doom monster: %s\n", name);
            return true;
        }
        auto *source = G_Spawn();
        source->classname = const_cast<char *>(definitions[type].classname);
        source->doomNpc.type = type;
        source->doomNpc.options = 7;
        vec3_t direction, origin;
        AngleVectors(player->client->ps.viewangles, direction, nullptr, nullptr);
        direction[2] = 0;
        VectorNormalize(direction);
        VectorMA(player->r.currentOrigin, 160 + definitions[type].radius, direction, origin);
        origin[2] += 2;
        vec3_t mins, maxs;
        VectorSet(mins, -definitions[type].radius, -definitions[type].radius, 0);
        VectorSet(maxs, definitions[type].radius, definitions[type].radius,
                  definitions[type].height);
        trace_t trace;
        trap_Trace(&trace, origin, mins, maxs, origin, player->s.number, MonsterSolid);
        if (trace.startsolid || trace.allsolid) {
            G_Printf("No clear space to spawn %s\n", name);
            G_FreeEntity(source);
            return true;
        }
        G_SetOrigin(source, origin);
        source->s.angles[YAW] = player->client->ps.viewangles[YAW] + 180;
        G_DoomAISpawnThink(source);
        return true;
    }
    if (*option >= '0' && *option <= '9') {
        const int slot = atoi(option);
        char action[32];
        trap_Argv(2, action, sizeof(action));
        if (slot < 1 || slot >= level.maxclients || !g_entities[slot].doomNpc.type) {
            return true;
        }
        auto *entity = &g_entities[slot];
        if (!Q_stricmp(action, "route")) {
            vec3_t origin, goal, point;
            VectorCopy(entity->client->ps.origin, origin);
            VectorCopy(player->r.currentOrigin, goal);
            origin[2] += 1;
            goal[2] += player->r.mins[2] + 1;
            trap_AAS_SetCurrentWorld(definitions[entity->doomNpc.type].world);
            const int area = trap_AAS_PointAreaNum(origin);
            const int destination = trap_AAS_PointAreaNum(goal);
            const int routed =
                trap_AAS_Route(origin, goal, TFL_WALK | TFL_WALKOFFLEDGE | TFL_AIR, point);
            G_Printf("Doom route: actor=%d area=%d destination=%d routed=%d origin=%.1f %.1f %.1f "
                     "goal=%.1f %.1f %.1f\n",
                     slot, area, destination, routed, origin[0], origin[1], origin[2], goal[0],
                     goal[1], goal[2]);
            trap_AAS_SetCurrentWorld(0);
        } else if (!Q_stricmp(action, "wake")) {
            entity->doomNpc.enemy = player->s.number;
            entity->doomNpc.lastSeen = level.time;
            VectorCopy(player->r.currentOrigin, entity->doomNpc.lastKnown);
            entity->doomNpc.lastKnown[2] += player->r.mins[2];
        } else if (!Q_stricmp(action, "hurt") || !Q_stricmp(action, "kill")) {
            G_DoomAIDamage(entity, player, !Q_stricmp(action, "kill") ? 10000 : 5, 0,
                           MOD_MACHINEGUN);
        } else if (!Q_stricmp(action, "visit")) {
            const float distance = definitions[entity->doomNpc.type].radius + 128.0f;
            for (int side = 0; side < 16; ++side) {
                const float angle = side * static_cast<float>(M_PI / 8);
                vec3_t origin, down;
                VectorCopy(entity->r.currentOrigin, origin);
                origin[0] += cos(angle) * distance;
                origin[1] += sin(angle) * distance;
                origin[2] += 25;
                VectorCopy(origin, down);
                down[2] -= 64;
                trace_t trace;
                trap_Trace(&trace, origin, player->r.mins, player->r.maxs, down, player->s.number,
                           MonsterSolid);
                if (trace.startsolid || trace.allsolid || trace.fraction == 1) {
                    continue;
                }
                vec3_t eye, target, look;
                VectorCopy(trace.endpos, origin);
                VectorCopy(origin, eye);
                eye[2] += player->client->ps.viewheight;
                VectorCopy(entity->r.currentOrigin, target);
                target[2] += definitions[entity->doomNpc.type].height * 0.5f;
                trap_Trace(&trace, eye, nullptr, nullptr, target, player->s.number, MASK_SHOT);
                if (trace.fraction < 1 && trace.entityNum != slot) {
                    continue;
                }
                VectorCopy(origin, player->client->ps.origin);
                G_SetOrigin(player, origin);
                VectorClear(player->client->ps.velocity);
                VectorSubtract(target, eye, look);
                vec3_t angles;
                vectoangles(look, angles);
                SetClientViewAngle(player, angles);
                trap_LinkEntity(player);
                break;
            }
        }
        return true;
    }
    for (int i = 1; i < level.maxclients; ++i) {
        const auto &entity = g_entities[i];
        if (entity.inuse && entity.doomNpc.type) {
            G_Printf("Doom AI: %d %s health=%d clip=%d enemy=%d shots=%d routes=%d at=%.1f %.1f "
                     "%.1f dir=%d count=%d velocity=%.1f %.1f %.1f\n",
                     i, entity.classname, entity.health, entity.doomNpc.clip, entity.doomNpc.enemy,
                     entity.doomNpc.shots, entity.doomNpc.routes, entity.r.currentOrigin[0],
                     entity.r.currentOrigin[1], entity.r.currentOrigin[2],
                     entity.doomNpc.moveDirection, entity.doomNpc.moveCount,
                     entity.client->ps.velocity[0], entity.client->ps.velocity[1],
                     entity.client->ps.velocity[2]);
        }
    }
    return true;
}
DOOM_SAVE_CALLBACK("G_DoomAICommand", G_DoomAICommand);
