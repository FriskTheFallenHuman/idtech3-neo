// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/game/g_local.h"
#include <cstdio>
#include <stdexcept>

level_locals_t level{};
gentity_t g_entities[MAX_GENTITIES]{};
gclient_t clients[MAX_CLIENTS]{};
vmCvar_t g_gametype{}, g_reloading{}, g_cheats{};
vmCvar_t g_soldierChargeTime{}, g_engineerChargeTime{};
doomProgress_t doomProgress{};
qboolean saveGamePending;

namespace {
int skill = 2, damageCalls, releasedActors;
int collisionEntity = ENTITYNUM_NONE;
bool obstructed;
bool pillar;
FILE *assetFile;
gentity_t missile{};
void Check(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
gentity_t *Spawn(int type, float x) {
    auto *source = &g_entities[MAX_CLIENTS + type];
    memset(source, 0, sizeof(*source));
    source->s.number = MAX_CLIENTS + type;
    source->inuse = qtrue;
    source->classname = const_cast<char *>(doommonster::definitions[type].classname);
    source->doomNpc.type = type;
    source->doomNpc.options = 7;
    source->r.currentOrigin[0] = x;
    G_DoomAISpawnThink(source);
    for (int slot = 1; slot < MAX_CLIENTS; ++slot) {
        if (g_entities[slot].inuse && g_entities[slot].doomNpc.type == type) {
            return &g_entities[slot];
        }
    }
    throw std::runtime_error("Spawn did not create a client actor");
}
void Frame(gentity_t *actor, int time) {
    level.previousTime = level.time;
    level.time = time;
    G_DoomAIRun(actor);
}
} // namespace

int G_DoomSkill() {
    return skill;
}
bool G_DoomCanHear(const vec3_t, const vec3_t) {
    return true;
}
void QDECL G_Printf(const char *, ...) {
}
void QDECL Com_Printf(const char *, ...) {
}
void QDECL G_Error(const char *message, ...) {
    throw std::runtime_error(message);
}
void QDECL Com_Error(int, const char *message, ...) {
    throw std::runtime_error(message);
}
int trap_FS_FOpenFile(const char *name, fileHandle_t *file, fsMode_t) {
    assetFile = std::fopen(va("base/%s", name), "rb");
    *file = assetFile ? 1 : 0;
    if (!assetFile) {
        return -1;
    }
    std::fseek(assetFile, 0, SEEK_END);
    const int size = static_cast<int>(std::ftell(assetFile));
    std::rewind(assetFile);
    return size;
}
void trap_FS_Read(void *buffer, int size, fileHandle_t) {
    Check(std::fread(buffer, 1, size, assetFile) == static_cast<size_t>(size), "Read sprite cfg");
}
void trap_FS_FCloseFile(fileHandle_t) {
    std::fclose(assetFile);
}
void trap_LinkEntity(gentity_t *) {
}
void trap_UnlinkEntity(gentity_t *) {
}
void trap_SetUserinfo(int, const char *) {
}
void trap_SetConfigstring(int, const char *) {
}
void trap_Cvar_Set(const char *, const char *) {
}
void trap_Cvar_Register(vmCvar_t *, const char *, const char *, int) {
}
int trap_Cvar_VariableIntegerValue(const char *) {
    return 0;
}
void trap_Cvar_VariableStringBuffer(const char *, char *, int) {
}
void trap_SnapVector(float *vector) {
    SnapVector(vector);
}
int trap_BotLibVarSet(char *, char *) {
    return 0;
}
int trap_BotLibSetup() {
    return 0;
}
int trap_BotLibShutdown() {
    return 0;
}
int trap_BotLibStartFrame(float) {
    return 0;
}
int trap_BotLibLoadMap(const char *) {
    return 0;
}
void trap_AAS_SetCurrentWorld(int) {
}
int trap_AAS_Initialized() {
    return 0;
}
int trap_AAS_PointAreaNum(vec3_t) {
    return 0;
}
qboolean trap_AAS_Route(vec3_t, vec3_t, int, vec3_t) {
    return qfalse;
}
void trap_Argv(int, char *buffer, int) {
    buffer[0] = 0;
}
void trap_SendServerCommand(int, const char *) {
}
int G_SoundIndex(const char *) {
    return 1;
}
void G_AddEvent(gentity_t *, int, int) {
}
int trap_BotAllocateClient() {
    for (int slot = 1; slot < MAX_CLIENTS; ++slot) {
        if (!g_entities[slot].inuse) {
            return slot;
        }
    }
    return -1;
}
void trap_BotFreeClient(int slot) {
    Check(slot > 0 && slot < MAX_CLIENTS && (g_entities[slot].r.svFlags & SVF_AI),
          "Only native map actor slots may be released");
    ++releasedActors;
}
qboolean G_SpawnInt(const char *, const char *value, int *result) {
    *result = atoi(value);
    return qtrue;
}
void G_FreeEntity(gentity_t *entity) {
    entity->inuse = qfalse;
}
gentity_t *G_Spawn() {
    memset(&missile, 0, sizeof(missile));
    missile.inuse = qtrue;
    return &missile;
}
void G_SetOrigin(gentity_t *entity, vec3_t origin) {
    VectorCopy(origin, entity->r.currentOrigin);
    VectorCopy(origin, entity->s.pos.trBase);
}
void G_UseTargets(gentity_t *, gentity_t *) {
}
void G_DoomDropMonsterPickup(gentity_t *, int) {
}
void G_DoomAIBossDeath(int) {
}
void SetClientViewAngle(gentity_t *, vec3_t) {
}
void G_TouchTriggers(gentity_t *) {
}
void G_DoomPlayerSector(gentity_t *) {
}
qboolean G_DoomTraceLine(gentity_t *, const vec3_t, const vec3_t, qboolean) {
    return qfalse;
}
qboolean G_RadiusDamage(vec3_t, gentity_t *, float, float, gentity_t *, int) {
    return qtrue;
}
int trap_PointContents(const vec3_t, int) {
    return 0;
}
void trap_Trace(trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs,
                const vec3_t end, int, int) {
    memset(trace, 0, sizeof(*trace));
    trace->fraction = 1;
    trace->entityNum = ENTITYNUM_NONE;
    trace->plane.normal[2] = 1;
    VectorCopy(end, trace->endpos);
    if (end[2] < start[2] && mins) {
        if (end[2] + mins[2] < 0) {
            trace->fraction = max(0.0f, (start[2] + mins[2]) / (start[2] - end[2]));
            trace->entityNum = ENTITYNUM_WORLD;
        }
    } else if (obstructed || collisionEntity != ENTITYNUM_NONE) {
        trace->fraction = 0.5f;
        trace->entityNum = collisionEntity == ENTITYNUM_NONE ? ENTITYNUM_WORLD : collisionEntity;
    }
    if (pillar) {
        // Sweep against an independent rectangular obstacle using slab entry
        // times. Expanding by the actor bounds also checks species clearance.
        const float low[3] = {64 - (maxs ? maxs[0] : 0), -100 - (maxs ? maxs[1] : 0), -1000};
        const float high[3] = {96 - (mins ? mins[0] : 0), 100 - (mins ? mins[1] : 0), 1000};
        float entry = 0, exit = 1;
        int axis = -1;
        float normal = 0;
        bool intersects = true;
        for (int i = 0; i < 2; ++i) {
            const float delta = end[i] - start[i];
            if (fabs(delta) < 0.00001f) {
                if (start[i] < low[i] || start[i] > high[i]) {
                    intersects = false;
                }
                continue;
            }
            float nearTime = (low[i] - start[i]) / delta;
            float farTime = (high[i] - start[i]) / delta;
            if (nearTime > farTime) {
                const float temporary = nearTime;
                nearTime = farTime;
                farTime = temporary;
            }
            if (nearTime >= entry) {
                entry = nearTime;
                axis = i;
                normal = delta > 0 ? -1 : 1;
            }
            exit = min(exit, farTime);
        }
        if (intersects && axis >= 0 && entry <= exit && entry < trace->fraction) {
            trace->fraction = max(0.0f, entry - 0.0001f);
            VectorClear(trace->plane.normal);
            trace->plane.normal[axis] = normal;
            trace->entityNum = ENTITYNUM_WORLD;
        }
    }
    for (int i = 0; i < 3; ++i) {
        trace->endpos[i] = start[i] + trace->fraction * (end[i] - start[i]);
    }
}
void G_Damage(gentity_t *target, gentity_t *, gentity_t *attacker, vec3_t, vec3_t, int damage,
              int flags, int mod) {
    ++damageCalls;
    if (!G_DoomAIDamage(target, attacker, damage, flags, mod)) {
        target->health -= damage;
    }
}

int main() {
    try {
        using namespace doommonster;
        g_gametype.integer = GT_SINGLE_PLAYER;
        level.clients = clients;
        level.maxclients = MAX_CLIENTS;
        level.num_entities = MAX_CLIENTS + 100;
        level.time = 1000;
        auto *player = &g_entities[0];
        player->inuse = qtrue;
        player->client = &clients[0];
        player->health = 1000;
        clients[0].pers.connected = CON_CONNECTED;
        clients[0].sess.sessionTeam = TEAM_FREE;
        clients[0].ps.viewheight = 17;
        VectorSet(player->r.mins, -16, -16, -24);
        VectorSet(player->r.maxs, 16, 16, 32);
        player->r.currentOrigin[2] = 24;
        player->r.currentOrigin[0] = 128;
        auto *imp = Spawn(Imp, 0);
        Check(imp->client == &clients[1] && imp->s.density == NetworkBase + Imp &&
                  imp->r.mins[0] == -20 + HullInset && imp->r.maxs[2] == 56 - 2 * HullInset &&
                  imp->client->modelInfo == nullptr,
              "Fake client spawns from species data without a mesh");
        Frame(imp, 1100);
        Check(imp->doomNpc.enemy == 0, "Visible player acquisition");
        // Force an attack with its original release timing, then skip across
        // several server frames. Each event must fire exactly once.
        imp->doomNpc.clip = Missile;
        imp->doomNpc.clipStart = 1200;
        imp->doomNpc.eventStep = 0;
        Frame(imp, 1600);
        Check(imp->doomNpc.shots == 0, "Windup cannot damage early");
        Frame(imp, 1700);
        Check(imp->doomNpc.shots == 1 && missile.inuse &&
                  missile.s.density == NetworkBase + ImpFireball,
              "Action frame emits the original fireball sprite projectile");
        Frame(imp, 1710);
        Check(imp->doomNpc.shots == 1, "Repeated frames cannot repeat an attack event");
        Check(imp->client->ps.commandTime == 1710 && imp->s.effect1Time == imp->doomNpc.clipStart,
              "Movement and animation replication run together");
        imp->doomNpc.enemy = -1;
        imp->doomNpc.options = 8;
        obstructed = true;
        G_DoomAIHear(0, player->r.currentOrigin, 2048);
        Check(imp->doomNpc.enemy == -1, "Ambush monster ignores sound");
        obstructed = false;
        imp->doomNpc.options = 7;
        G_DoomAIHear(0, player->r.currentOrigin, 2048);
        Check(imp->doomNpc.enemy == 0, "Gunshot wakes ordinary monsters");
        auto *demon = Spawn(Demon, -64);
        G_DoomAIDamage(imp, demon, 1, 0, MOD_MACHINEGUN);
        Check(imp->doomNpc.enemy == demon->s.number, "Damage establishes monster infighting");
        G_DoomAIDamage(imp, player, 1000, 0, MOD_ROCKET);
        G_DoomAIReplicate(imp);
        Check(imp->doomNpc.clip == Gib && imp->r.contents == CONTENTS_CORPSE &&
                  imp->s.eType == ET_PLAYER && (imp->s.eFlags & EF_DEAD),
              "Death replicates a solid-height corpse sprite");
        auto *cyber = Spawn(Cyberdemon, -256);
        G_DoomAIDamage(cyber, player, 128, 0, MOD_ROCKET_SPLASH);
        Check(cyber->health == 4000, "Cyberdemon resists blast splash");
        G_DoomAIDamage(cyber, player, 80, 0, MOD_ROCKET);
        Check(cyber->health == 3920, "Direct rocket impact still damages a boss");
        obstructed = true;
        demon->doomNpc.enemy = -1;
        player->r.currentOrigin[0] = 100;
        Frame(demon, 5000);
        Check(demon->doomNpc.enemy == -1, "Walls prevent visual acquisition");
        obstructed = false;
        // Exercise actual server AI, shared Pmove and independent collision.
        // A melee monster must pursue, route around a wall, then attack only
        // when it reaches the target. No mocked movement or route result.
        pillar = true;
        VectorSet(player->r.currentOrigin, 300, 0, 24);
        VectorSet(demon->client->ps.origin, 0, 0, 0);
        VectorCopy(demon->client->ps.origin, demon->r.currentOrigin);
        demon->doomNpc.enemy = 0;
        demon->doomNpc.clip = Chase;
        demon->doomNpc.nextDecision = level.time;
        bool wentAround = false;
        for (int time = 5050; time <= 17000; time += 50) {
            Frame(demon, time);
            const auto &origin = demon->client->ps.origin;
            wentAround = wentAround || fabs(origin[1]) >= 129.8f;
            Check(
                !(origin[0] + demon->r.maxs[0] > 64.01f && origin[0] + demon->r.mins[0] < 95.99f &&
                  origin[1] + demon->r.maxs[1] > -99.99f && origin[1] + demon->r.mins[1] < 99.99f),
                "Pursuit cannot pass through the expanded wall");
        }
        Check(wentAround && demon->r.currentOrigin[0] > 180,
              "Original chase direction search makes progress around a wall");
        Check(demon->doomNpc.shots > 0, "Demon eventually reaches melee range");
        pillar = false;
        auto *zombie = Spawn(Zombie, -512);
        VectorSet(player->r.currentOrigin, 500, 0, 24);
        zombie->doomNpc.enemy = 0;
        zombie->doomNpc.moveCount = 0;
        zombie->doomNpc.attackReady = 0;
        bool movedAfterAttack = false;
        for (int time = 17050; time <= 27000; time += 50) {
            Frame(zombie, time);
            movedAfterAttack =
                movedAfterAttack || (zombie->doomNpc.shots > 0 && zombie->doomNpc.clip == Chase &&
                                     VectorLength(zombie->client->ps.velocity) > 1);
        }
        Check(zombie->r.currentOrigin[0] > -400 && movedAfterAttack,
              "Missile attacks are interleaved with real chase movement");
        int actors = 0;
        for (int slot = 1; slot < MAX_CLIENTS; ++slot) {
            actors += g_entities[slot].inuse && (g_entities[slot].r.svFlags & SVF_AI);
        }
        G_DoomAIShutdown();
        Check(actors > 0 && releasedActors == actors,
              "VM shutdown releases every map actor without reconnecting player bots");
        std::puts("Doom server spawn, action timing, damage, hearing, collision and replication "
                  "checks passed.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
