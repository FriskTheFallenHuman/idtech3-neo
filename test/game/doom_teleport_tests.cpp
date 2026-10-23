// SPDX-License-Identifier: GPL-3.0-or-later
// Include the adapter to exercise its real action path with controlled map metadata.
#include "game/g_doom.cpp"
#include <cstdio>
#include <initializer_list>
#include <stdexcept>

level_locals_t level{};
gentity_t g_entities[MAX_GENTITIES]{};
namespace {
gclient_t client{};
gentity_t actor{}, events[8]{};
dbsp::Sector sectors[2]{};
dbsp::Thing things[3]{};
int eventCount, killBoxes, traceMask, linked;
bool blocked;
void Check(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
gentity_t &Reset(int special = 97) {
    memset(g_entities, 0, sizeof(g_entities));
    actor = {};
    client = {};
    eventCount = killBoxes = traceMask = linked = 0;
    blocked = false;
    doomSectorData = sectors;
    doomSectorCount = 2;
    doomThingData = things;
    doomThingCount = 3;
    sectors[0].tag = sectors[1].tag = 7;
    sectors[0].floor = 64;
    sectors[1].floor = 128;
    // A later sector's marker appears first in thing order.
    things[0] = {900, 900, 0, 14, 0, 1};
    things[1] = {512, 256, 90, 14, 0, 0};
    things[2] = {700, 256, 180, 14, 0, 0};
    actor.client = &client;
    actor.health = client.ps.stats[STAT_HEALTH] = 100;
    VectorSet(actor.r.mins, -16, -16, -24);
    VectorSet(actor.r.maxs, 16, 16, 32);
    VectorSet(client.oldOrigin, 32, -64, 24);
    VectorSet(client.ps.origin, 32, 64, 24);
    VectorSet(client.ps.velocity, 200, 100, 40);
    level.num_entities = MAX_CLIENTS + 2;
    auto &line = g_entities[MAX_CLIENTS];
    line.inuse = qtrue;
    line.classname = "doom_line";
    line.touch = DoomLineTouch;
    line.doom.special = special;
    line.doom.tag = 7;
    VectorSet(line.doom.a, 0, 0, 0);
    VectorSet(line.doom.b, 64, 0, 0);
    line.r.mins[2] = -4096;
    line.r.maxs[2] = 4096;
    return line;
}
} // namespace
void QDECL Com_Printf(const char *, ...) {
}
void QDECL Com_Error(int, const char *message, ...) {
    throw std::runtime_error(message);
}

void QDECL G_DPrintf(const char *, ...) {
}
void trap_GetConfigstring(int, char *text, int) {
    text[0] = 0;
}
void trap_SetConfigstring(int, const char *) {
}
void trap_TraceCapsule(trace_t *trace, const vec3_t, const vec3_t, const vec3_t, const vec3_t, int,
                       int mask) {
    trace->startsolid = blocked ? qtrue : qfalse;
    traceMask = mask;
}
void trap_LinkEntity(gentity_t *) {
    ++linked;
}
void trap_UnlinkEntity(gentity_t *) {
}
void SetClientViewAngle(gentity_t *entity, vec3_t angles) {
    VectorCopy(angles, entity->client->ps.viewangles);
}
void G_KillBox(gentity_t *) {
    ++killBoxes;
}
gentity_t *G_TempEntity(vec3_t origin, int event) {
    Check(eventCount < 8, "Unexpected extra teleport events");
    auto &entity = events[eventCount++];
    entity = {};
    entity.s.event = event;
    VectorCopy(origin, entity.s.origin);
    return &entity;
}
void BG_PlayerStateToEntityState(playerState_t *ps, entityState_t *state, qboolean) {
    state->eFlags = ps->eFlags;
}
void G_DoomAIReplicate(gentity_t *entity) {
    entity->s.density = doommonster::NetworkBase + entity->doomNpc.type;
    entity->s.eFlags = entity->client->ps.eFlags;
}
bool G_DoomBeginIntermission(gentity_t *, bool) {
    return false;
}
void SetMoverState(gentity_t *, moverState_t, int) {
}
int G_SoundIndex(const char *) {
    return 1;
}
void G_Damage(gentity_t *, gentity_t *, gentity_t *, vec3_t, vec3_t, int, int, int) {
}
qboolean G_SpawnInt(const char *, const char *, int *) {
    return qfalse;
}
qboolean G_SpawnFloat(const char *, const char *, float *) {
    return qfalse;
}
qboolean G_SpawnVector(const char *, const char *, float *) {
    return qfalse;
}
void trap_SetBrushModel(gentity_t *, const char *) {
}
void InitMover(gentity_t *) {
}
void G_FreeEntity(gentity_t *) {
}
void trap_Cvar_VariableStringBuffer(const char *, char *buffer, int) {
    buffer[0] = 0;
}
int trap_FS_FOpenFile(const char *, fileHandle_t *, fsMode_t) {
    return 0;
}
void trap_FS_FClos
eFile(fileHandle_t) {
}
void QDECL G_Error(const char *message, ...) {
    throw std::runtime_error(message);
}
void trap_FS_Read(void *, int, fileHandle_t) {
}
void *G_Alloc(int) {
    throw std::runtime_error("Unexpected map allocation");
}
void G_DoomLevelInit(const void *) {
}
void G_DoomSpawnWeaponThings(const void *) {
}
void G_DoomAISpawnThings(const void *) {
}
void trap_Cvar_Set(const char *, const char *) {
}
void QDECL G_Printf(const char *, ...) {
}
void trap_SendServerCommand(int, const char *) {
}
gentity_t *G_FindActor(const char *) {
    return nullptr;
}
int main() {
    try {
        auto &line = Reset(39);
        G_DoomCrossLines(&actor);
        Check(line.doom.activated && client.ps.origin[0] == 512 && client.ps.origin[2] == 88.125f &&
                  client.ps.viewangles[YAW] == 90,
              "Fast crossing chooses first sector/marker and lands with feet above floor");
        Check(VectorLength(client.ps.velocity) == 0 && (client.ps.eFlags & EF_TELEPORT_BIT) &&
                  killBoxes == 1 && linked == 1 && client.ps.pm_time == 514 &&
                  (client.ps.pm_flags & PMF_TIME_TELEPORT),
              "Player stops, telefrags, relinks and signals predicted reaction delay");
        Check(eventCount == 2 && events[0].s.origin[2] == 0 && events[1].s.origin[1] == 276 &&
                  events[1].s.origin[2] == 64 && events[0].s.eventParm == 1 &&
                  events[1].s.eventParm == 1,
              "Both fog events use foot anchors and destination fog sits twenty units ahead");
        Check(!DoomLineActivate(&line, &actor), "One-use teleport remains consumed");
        Reset(97);
        Check(DoomLineActivate(&line, &actor) && DoomLineActivate(&line, &actor),
              "Repeatable teleport remains usable");
        Reset();
        VectorSet(client.oldOrigin, 32, 64, 24);
        VectorSet(client.ps.origin, 32, -64, 24);
        G_DoomCrossLines(&actor);
        Check(!eventCount, "Back-to-front crossing does not te
leport");
        Reset();
        client.ps.origin[1] = 0;
        G_DoomCrossLines(&actor);
        Check(eventCount == 2, "Landing exactly on the boundary activates once");
        Reset();
        client.ps.origin[0] = client.oldOrigin[0] = 65;
        G_DoomCrossLines(&actor);
        Check(!eventCount, "Crossing the infinite extension misses the finite line");
        Reset();
        auto &mover = g_entities[MAX_CLIENTS + 1];
        mover.inuse = qtrue;
        mover.classname = "doom_mover";
        mover.doom.sector = 0;
        mover.doom.base = 64;
        mover.r.currentOrigin[2] = 16;
        Check(DoomLineActivate(&line, &actor) && client.ps.origin[2] == 104.125f,
              "Destination follows the current moving floor");
        for (int special : {125, 126}) {
            Reset(special);
            Check(!DoomLineActivate(&line, &actor), "Monster-only teleport rejects humans");
            actor.doomNpc.type = doommonster::Imp;
            actor.r.mins[2] = 0;
            Check(DoomLineActivate(&line, &actor) && client.ps.origin[2] == 64.125f &&
                      (traceMask & CONTENTS_BODY) && !killBoxes && !client.ps.pm_time &&
                      actor.s.density == doommonster::NetworkBase + doommonster::Imp,
                  "Monster uses its own hull, occupancy rules and sprite replication");
        }
        Reset(39);
        blocked = true;
        Check(!DoomLineActivate(&line, &actor) && !line.doom.activated && !eventCount,
              "Blocked destination preserves the one-use trigger");
        blocked = false;
        Check(DoomLineActivate(&line, &actor), "Failed teleport can be retried");
        Reset();
        line.doom.tag = 999;
        Check(!DoomLineActivate(&line, &actor) && !line.doom.activated,
              "Missing marker/tag fails without consuming the line");
        Reset();
        actor.health = 0;
        Check(!DoomLineActivate(&line, &actor), "Dead actors cannot teleport");
        Reset();
        cl
ient.sess.sessionTeam = TEAM_SPECTATOR;
        Check(!DoomLineActivate(&line, &actor), "Spectators cannot consume map actions");
        std::puts("Doom teleport activation, metadata, landing, motion, effects and failure paths "
                  "passed.");
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
