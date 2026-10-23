// SPDX-License-Identifier: GPL-3.0-or-later
// Runtime adapters for the sector/line entities emitted by doommap.
#include "g_save_callbacks.h"
#include "../qcommon/doom_actions.h"
#include "../qcommon/doom_bsp.h"
#include "g_local.h"
#include "g_doomsound.h"
#include "g_doomlevel.h"

void InitMover(gentity_t *ent);
void SetMoverState(gentity_t *ent, moverState_t state, int time);
static qboolean DoomTeleport(gentity_t *line, gentity_t *actor);

namespace {
void DoomMoverSound(gentity_t *entity, const char *name) {
    vec3_t origin = {(entity->r.absmin[0] + entity->r.absmax[0]) * 0.5f,
                     (entity->r.absmin[1] + entity->r.absmax[1]) * 0.5f, entity->doom.floor + 32};
    auto *event = G_TempEntity(origin, EV_GENERAL_SOUND);
    event->s.eventParm = G_SoundIndex(va("sound/doom/monsters/%s.wav", name));
    G_DPrintf("Doom mover sound: %s sector=%d\n", name, entity->doom.sector);
}
bool DoomDoor(const gentity_t *entity) {
    const auto kind = doom::action(entity->doom.special).kind;
    return kind == doom::DoorRaise || kind == doom::DoorOpen || kind == doom::DoorClose ||
           kind == doom::DoorCloseWait;
}
} // namespace

void DoomMoveTo(gentity_t *ent, float height, float speed) {
    VectorCopy(ent->r.currentOrigin, ent->pos1);
    VectorCopy(ent->pos1, ent->pos2);
    ent->pos2[2] = height - ent->doom.base;
    float distance = fabs(ent->pos2[2] - ent->pos1[2]);
    ent->speed = speed;
    if (DoomDoor(ent)) {
        const bool opening = height > ent->doom.base + ent->r.currentOrigin[2];
        DoomMoverSound(ent, speed > 100 ? (opening ? "bdopn" : "bdcls")
                                        : (opening ? "doropn" : "dorcls"));
    } else {
        const auto kind = doom::action(ent->doom.special).kind;
        DoomMoverSound(ent,
                       kind == doom::Lift || kind == doom::PerpetualLift ? "pstart" : "stnmov");
    }
    ent->s.pos.trDuration = static_cast<int>(1000.0f * distance / speed);
    if (ent->s.pos.trDuration < 1)
        ent->s.pos.trDuration = 1;
    ent->gDuration = ent->gDurationBack = ent->s.pos.trDuration;
    ent->nextthink = 0;
    SetMoverState(ent, MOVER_1TO2, level.time);
}
DOOM_SAVE_CALLBACK("DoomMoveTo", DoomMoveTo);

void DoomReturn(gentity_t *ent) {
    ent->doom.returning = 1;
    DoomMoveTo(ent, ent->doom.returnHeight, ent->speed);
}
DOOM_SAVE_CALLBACK("DoomReturn", DoomReturn);

void DoomReached(gentity_t *ent) {
    if (!DoomDoor(ent)) {
        DoomMoverSound(ent, "pstop");
    }
    SetMoverState(ent, MOVER_POS2, level.time);
    if (ent->doom.cycle) {
        float current = ent->r.currentOrigin[2] + ent->doom.base;
        ent->doom.returnHeight =
            fabs(current - ent->doom.low) < 0.1f ? ent->doom.high : ent->doom.low;
        ent->think = DoomReturn;
        ent->nextthink = level.time + (ent->doom.waitMs ? ent->doom.waitMs : 1);
    } else if (!ent->doom.returning && ent->doom.waitMs > 0) {
        ent->think = DoomReturn;
        ent->nextthink = level.time + ent->doom.waitMs;
    } else {
        ent->doom.busy = 0;
        ent->nextthink = 0;
    }
}
DOOM_SAVE_CALLBACK("DoomReached", DoomReached);

void DoomBlocked(gentity_t *ent, gentity_t *other) {
    if (ent->doom.crush && other->takedamage) {
        if (ent->doom.damageTime <= level.time) {
            G_Damage(other, ent, ent, nullptr, nullptr, 10, DAMAGE_NO_PROTECTION, MOD_CRUSH);
            ent->doom.damageTime = level.time + 4 * 1000 / 35;
        }
    } else if (ent->doom.plane && ent->s.pos.trDelta[2] < 0) {
        // A closing door reopens when a player or actor obstructs it.
        ent->doom.returning = 0;
        DoomMoveTo(ent, ent->doom.high, ent->speed);
    }
}
DOOM_SAVE_CALLBACK("DoomBlocked", DoomBlocked);

qboolean DoomActivateMover(gentity_t *ent, int special) {
    doom::Action action = doom::action(special);
    doom::Kind kind = action.kind;
    if (ent->doom.plane ? !doom::ceilingMoves(kind) : !doom::floorMoves(kind))
        return qfalse;
    if (ent->doom.busy) {
        if (action.local && ent->doom.plane) {
            ent->doom.special = special;
            float target = ent->s.pos.trDelta[2] < 0 ? ent->doom.high : ent->doom.floor;
            ent->doom.returning = target == ent->doom.floor;
            DoomMoveTo(ent, target, 70);
            return qtrue;
        }
        return qfalse;
    }
    ent->doom.special = special;
    float current = ent->r.currentOrigin[2] + ent->doom.base;
    float target = current, speed = ent->doom.plane ? 70.0f : 35.0f;
    ent->doom.returning = 0;
    ent->doom.waitMs = 0;
    ent->doom.cycle = 0;
    ent->doom.crush = 0;
    switch (kind) {
    case doom::DoorRaise:
    case doom::DoorOpen:
        target = ent->doom.lowceil - 4;
        ent->doom.high = target;
        if (target < ent->doom.floor)
            target = ent->doom.floor;
        if (kind == doom::DoorRaise) {
            ent->doom.waitMs = 150 * 1000 / 35;
            ent->doom.returnHeight = ent->doom.floor;
        }
        if (special >= 105)
            speed *= 4;
        break;
    case doom::DoorClose:
    case doom::DoorCloseWait:
        target = ent->doom.floor;
        ent->doom.high = ent->doom.lowceil - 4;
        if (kind == doom::DoorCloseWait) {
            ent->doom.waitMs = 30000;
            ent->doom.returnHeight = current;
        }
        break;
    case doom::Lift:
    case doom::PerpetualLift:
        target = ent->doom.lowfloor;
        speed = 140;
        ent->doom.waitMs = 3000;
        ent->doom.returnHeight = ent->doom.floor;
        if (kind == doom::PerpetualLift) {
            ent->doom.low = ent->doom.lowfloor;
            ent->doom.high = ent->doom.highfloor;
            ent->doom.cycle = 1;
            speed = 35;
        }
        break;
    case doom::FloorLowest:
        target = ent->doom.lowfloor;
        break;
    case doom::FloorHighest:
        target = ent->doom.highfloor;
        break;
    case doom::TurboLower:
        target = ent->doom.highfloor + 8;
        speed = 140;
        break;
    case doom::FloorNext:
        target = ent->doom.nextfloor;
        break;
    case doom::FloorRaise:
        target = min(ent->doom.ceiling, ent->doom.lowceil);
        break;
    case doom::FloorCrush:
        target = min(ent->doom.ceiling, ent->doom.lowceil) - 8;
        ent->doom.crush = 1;
        break;
    case doom::Floor24:
        target = current + 24;
        break;
    case doom::Floor32:
        target = current + 32;
        break;
    case doom::FloorTexture:
        target = current + ent->doom.textureHeight;
        break;
    case doom::Stairs8:
        target = ent->doom.stairHeight;
        speed = 8.75f;
        break;
    case doom::Stairs16:
        target = ent->doom.stairHeight;
        speed = 140;
        break;
    case doom::Donut:
        target = ent->doom.donutHeight;
        speed = 17.5f;
        break;
    case doom::CeilingFloor:
        target = ent->doom.floor;
        break;
    case doom::CeilingCrush:
        target = ent->doom.floor + 8;
        ent->doom.crush = 1;
        speed = 35;
        break;
    case doom::Crusher:
    case doom::FastCrusher:
        ent->doom.low = ent->doom.floor + 8;
        ent->doom.high = ent->doom.ceiling;
        target = ent->doom.low;
        ent->doom.cycle = 1;
        ent->doom.crush = 1;
        speed = kind == doom::FastCrusher ? 70.0f : 35.0f;
        break;
    case doom::FloorCeiling:
        target = ent->doom.plane ? ent->doom.highceil : ent->doom.lowfloor;
        break;
    default:
        return qfalse;
    }
    if (fabs(target - current) < 0.01f)
        return qfalse;
    ent->doom.busy = 1;
    DoomMoveTo(ent, target, speed);
    return qtrue;
}
DOOM_SAVE_CALLBACK("DoomActivateMover", DoomActivateMover);

void DoomSetSectorLight(int sector, int light) {
    char old[MAX_STRING_CHARS], result[MAX_STRING_CHARS], *p;
    trap_GetConfigstring(CS_DOOM_LIGHTS, old, sizeof(old));
    result[0] = 0;
    p = old;
    while (*p) {
        int id = -1, value = -1;
        char *end = strchr(p, ';');
        if (!end)
            break;
        *end = 0;
        if (sscanf(p, "%d:%d", &id, &value) == 2 && id != sector)
            Q_strcat(result, sizeof(result), va("%d:%d;", id, value));
        p = end + 1;
    }
    Q_strcat(result, sizeof(result), va("%d:%d;", sector, light));
    trap_SetConfigstring(CS_DOOM_LIGHTS, result);
}
DOOM_SAVE_CALLBACK("DoomSetSectorLight", DoomSetSectorLight);

void DoomResetSwitch(gentity_t *line) {
    line->doom.plane = 0;
    line->think = nullptr;
    line->nextthink = 0;
}
DOOM_SAVE_CALLBACK("DoomResetSwitch", DoomResetSwitch);

qboolean DoomLineActivate(gentity_t *line, gentity_t *activator) {
    if (!activator || !activator->client)
        return qfalse;
    auto action = doom::action(line->doom.special);
    if (activator->doomNpc.type) {
        const int special = line->doom.special;
        const bool allowed = special == 1 || special == 4 || special == 10 || special == 39 ||
                             special == 88 || special == 97 || special == 125 || special == 126;
        if (!allowed || (action.use && special != 1)) {
            return qfalse;
        }
    }
    if (line->doom.lightMax && line->doom.plane && action.repeat)
        return qfalse; // A repeatable button stays pressed for one second.
    if (line->doom.activated && !action.repeat)
        return qfalse;
    if (action.key && !(activator->client->ps.stats[STAT_KEYS] & (1 << action.key)))
        return qfalse;
    qboolean success = qfalse;
    if (action.kind == doom::Teleport) {
        if ((line->doom.special == 125 || line->doom.special == 126) && !activator->doomNpc.type)
            return qfalse;
        success = DoomTeleport(line, activator);
    } else if (action.kind == doom::Exit || action.kind == doom::SecretExit) {
        success =
            G_DoomBeginIntermission(activator, action.kind == doom::SecretExit) ? qtrue : qfalse;
    }
    for (int i = MAX_CLIENTS; i < level.num_entities; ++i) {
        gentity_t *ent = &g_entities[i];
        if (!ent->inuse || !ent->classname)
            continue;
        bool complex = action.kind == doom::Stairs8 || action.kind == doom::Stairs16 ||
                       action.kind == doom::Donut;
        bool match = action.local ? ent->doom.sector == line->doom.sector
                                  : (line->doom.tag > 0 &&
                                     (ent->doom.tag == line->doom.tag ||
                                      (complex && ent->doom.linkedTag == line->doom.tag)));
        if (!match)
            continue;
        if (!Q_stricmp(ent->classname, "doom_mover")) {
            if ((action.kind == doom::StopLift && !ent->doom.plane) ||
                (action.kind == doom::StopCrusher && ent->doom.plane)) {
                ent->s.pos.trType = TR_STATIONARY;
                VectorCopy(ent->r.currentOrigin, ent->s.pos.trBase);
                ent->nextthink = 0;
                ent->doom.busy = 0;
                success = qtrue;
            } else
                success |= DoomActivateMover(ent, line->doom.special);
        } else if (!Q_stricmp(ent->classname, "doom_sector")) {
            int light = -1;
            if (action.kind == doom::LightBright)
                light = ent->doom.lightMax;
            if (action.kind == doom::Light255)
                light = 255;
            if (action.kind == doom::Light35)
                light = 35;
            if (action.kind == doom::LightDark)
                light = ent->doom.lightMin;
            if (action.kind == doom::LightStrobe)
                light = 256;
            if (light >= 0) {
                DoomSetSectorLight(ent->doom.sector, light);
                success = qtrue;
            }
        }
    }
    if (success) {
        line->doom.activated = 1;
        if (line->doom.lightMax && (action.use || action.shoot)) {
            line->doom.plane = 1;
            if (action.repeat) {
                line->think = DoomResetSwitch;
                line->nextthink = level.time + 1000;
            }
        }
        G_DPrintf("Doom line %d activated (special %d)\n", line->doom.line, line->doom.special);
    }
    return success;
}
DOOM_SAVE_CALLBACK("DoomLineActivate", DoomLineActivate);

void DoomLineUse(gentity_t *line, gentity_t *, gentity_t *activator) {
    DoomLineActivate(line, activator);
}
DOOM_SAVE_CALLBACK("DoomLineUse", DoomLineUse);

void DoomLineTouch(gentity_t *line, gentity_t *other, trace_t *) {
    if (!other->client)
        return;
    vec3_t d;
    VectorSubtract(line->doom.b, line->doom.a, d);
    float before = d[0] * (other->client->oldOrigin[1] - line->doom.a[1]) -
                   d[1] * (other->client->oldOrigin[0] - line->doom.a[0]);
    float after = d[0] * (other->client->ps.origin[1] - line->doom.a[1]) -
                  d[1] * (other->client->ps.origin[0] - line->doom.a[0]);
    // Assign points on the line to its back side. This also catches a move
    // which lands exactly on a line, without losing the following crossing.
    if ((before < 0) == (after < 0))
        return;
    float t = before / (before - after);
    float x = other->client->oldOrigin[0] +
              t * (other->client->ps.origin[0] - other->client->oldOrigin[0]);
    float y = other->client->oldOrigin[1] +
              t * (other->client->ps.origin[1] - other->client->oldOrigin[1]);
    float u =
        ((x - line->doom.a[0]) * d[0] + (y - line->doom.a[1]) * d[1]) / (d[0] * d[0] + d[1] * d[1]);
    if (u < 0 || u > 1)
        return;
    if ((doom::action(line->doom.special).kind == doom::Teleport) && before >= 0)
        return;
    DoomLineActivate(line, other);
}
DOOM_SAVE_CALLBACK("DoomLineTouch", DoomLineTouch);

void G_DoomCrossLines(gentity_t *actor) {
    if (!actor->client || actor->health <= 0 || actor->client->noclip ||
        actor->client->sess.sessionTeam == TEAM_SPECTATOR) {
        return;
    }
    // Test the movement segment, not just the final capsule: a fast move can
    // end entirely beyond a thin linedef trigger. Stop after teleportation so
    // the discontinuity cannot activate unrelated lines between the two rooms.
    for (int i = MAX_CLIENTS; i < level.num_entities; ++i) {
        auto *line = &g_entities[i];
        if (!line->inuse || line->touch != DoomLineTouch) {
            continue;
        }
        const auto &ps = actor->client->ps;
        if (ps.origin[2] + actor->r.maxs[2] < line->r.mins[2] ||
            ps.origin[2] + actor->r.mins[2] > line->r.maxs[2]) {
            continue;
        }
        const int flags = ps.eFlags;
        DoomLineTouch(line, actor, nullptr);
        if ((flags ^ ps.eFlags) & EF_TELEPORT_BIT) {
            return;
        }
    }
}
DOOM_SAVE_CALLBACK("G_DoomCrossLines", G_DoomCrossLines);

qboolean G_DoomTraceLine(gentity_t *player, const vec3_t start, const vec3_t end, qboolean shot) {
    gentity_t *best = nullptr;
    float bestT = 2;
    float dx = end[0] - start[0], dy = end[1] - start[1];
    // One-sided collision brushes extend one unit in front of the DWD line.
    // Allow that skin and the collision epsilon without using through a wall.
    float rayLength = sqrt(dx * dx + dy * dy);
    float maxT = rayLength > .001f ? 1.0f + 2.0f / rayLength : 1.0f;
    for (int i = MAX_CLIENTS; i < level.num_entities; ++i) {
        auto *line = &g_entities[i];
        if (!line->inuse || !line->classname || Q_stricmp(line->classname, "doom_line"))
            continue;
        auto a = doom::action(line->doom.special);
        if (shot ? !a.shoot : !a.use)
            continue;
        float ex = line->doom.b[0] - line->doom.a[0], ey = line->doom.b[1] - line->doom.a[1],
              den = dx * ey - dy * ex;
        if (fabs(den) < 0.001f)
            continue;
        float x = line->doom.a[0] - start[0], y = line->doom.a[1] - start[1];
        float t = (x * ey - y * ex) / den, u = (x * dy - y * dx) / den;
        float side = ex * (start[1] - line->doom.a[1]) - ey * (start[0] - line->doom.a[0]);
        if (t >= -0.01f && t <= maxT && u >= 0 && u <= 1 && side < 0 && t < bestT) {
            best = line;
            bestT = t;
        }
    }
    if (!best)
        return qfalse;
    DoomLineActivate(best, player);
    return qtrue;
}
DOOM_SAVE_CALLBACK("G_DoomTraceLine", G_DoomTraceLine);

void SP_doom_line(gentity_t *ent) {
    G_SpawnInt("doom_switch", "0", &ent->doom.lightMax);
    G_SpawnInt("doom_sector", "-1", &ent->doom.sector);
    G_SpawnInt("doom_tag", "0", &ent->doom.tag);
    G_SpawnInt("doom_line", "-1", &ent->doom.line);
    G_SpawnInt("doom_special", "0", &ent->doom.special);
    G_SpawnVector("doom_a", "0 0 0", ent->doom.a);
    G_SpawnVector("doom_b", "0 0 0", ent->doom.b);
    auto a = doom::action(ent->doom.special);
    ent->use = DoomLineUse;
    ent->r.svFlags |= SVF_NOCLIENT;
    if (!a.use && !a.shoot) {
        float lo, hi;
        G_SpawnFloat("doom_minz", "-4096", &lo);
        G_SpawnFloat("doom_maxz", "4096", &hi);
        VectorSet(ent->r.mins, min(ent->doom.a[0], ent->doom.b[0]) - 2,
                  min(ent->doom.a[1], ent->doom.b[1]) - 2, lo);
        VectorSet(ent->r.maxs, max(ent->doom.a[0], ent->doom.b[0]) + 2,
                  max(ent->doom.a[1], ent->doom.b[1]) + 2, hi);
        ent->r.contents = CONTENTS_TRIGGER;
        ent->touch = DoomLineTouch;
        trap_LinkEntity(ent);
    }
}
DOOM_SAVE_CALLBACK("SP_doom_line", SP_doom_line);

void DoomTimedMover(gentity_t *ent);
void SP_doom_mover(gentity_t *ent) {
    G_SpawnInt("doom_sector", "-1", &ent->doom.sector);
    G_SpawnInt("doom_tag", "0", &ent->doom.tag);
    G_SpawnInt("doom_plane", "0", &ent->doom.plane);
    G_SpawnFloat("doom_floor", "0", &ent->doom.floor);
    G_SpawnFloat("doom_ceiling", "128", &ent->doom.ceiling);
    G_SpawnFloat("doom_lowfloor", "0", &ent->doom.lowfloor);
    G_SpawnFloat("doom_highfloor", "0", &ent->doom.highfloor);
    G_SpawnFloat("doom_nextfloor", "0", &ent->doom.nextfloor);
    G_SpawnFloat("doom_lowceil", "128", &ent->doom.lowceil);
    G_SpawnFloat("doom_highceil", "128", &ent->doom.highceil);
    G_SpawnInt("doom_linkedtag", "-1", &ent->doom.linkedTag);
    G_SpawnFloat("doom_donutheight", "0", &ent->doom.donutHeight);
    G_SpawnFloat("doom_textureheight", "64", &ent->doom.textureHeight);
    G_SpawnFloat("doom_stairheight", "8", &ent->doom.stairHeight);
    ent->doom.base = ent->doom.plane ? ent->doom.ceiling : ent->doom.floor;
    trap_SetBrushModel(ent, ent->model);
    VectorClear(ent->pos1);
    VectorClear(ent->pos2);
    InitMover(ent);
    ent->use = nullptr;
    ent->reached = DoomReached;
    ent->blocked = DoomBlocked;
    ent->doom.high = ent->doom.lowceil - 4;
    G_SpawnInt("doom_special", "0", &ent->doom.special);
    if (ent->doom.plane && (ent->doom.special == 10 || ent->doom.special == 14)) {
        ent->think = DoomTimedMover;
        ent->nextthink = level.time + (ent->doom.special == 10 ? 30000 : 300000);
    }
}
DOOM_SAVE_CALLBACK("SP_doom_mover", SP_doom_mover);

void SP_doom_sector(gentity_t *ent) {
    G_SpawnInt("doom_sector", "-1", &ent->doom.sector);
    G_SpawnInt("doom_tag", "0", &ent->doom.tag);
    G_SpawnInt("doom_lightmax", "255", &ent->doom.lightMax);
    G_SpawnInt("doom_lightmin", "0", &ent->doom.lightMin);
    ent->r.svFlags |= SVF_NOCLIENT;
}
DOOM_SAVE_CALLBACK("SP_doom_sector", SP_doom_sector);

void DoomTimedMover(gentity_t *ent) {
    DoomActivateMover(ent, ent->doom.special == 10 ? 3 : 1);
}
DOOM_SAVE_CALLBACK("DoomTimedMover", DoomTimedMover);

void SP_doom_key(gentity_t *ent) {
    // Original thing metadata now supplies every pickup, including key flags.
    G_FreeEntity(ent);
}
DOOM_SAVE_CALLBACK("SP_doom_key", SP_doom_key);

static byte *doomMapData;
static int doomSectorCount, doomLineCount;
static dbsp::Sector *doomSectorData;
static dbsp::Line *doomLineData;
static dbsp::Side *doomSideData;
static dbsp::Thing *doomThingData;
static int doomThingCount;
static int *doomSoundHeads, *doomSoundLevels, *doomSoundQueue;
static doomsound::Link *doomSoundLinks;
static float *doomSoundFloors, *doomSoundCeilings;
static int doomSoundTime = -1;
static vec3_t doomSoundOrigin;

static qboolean DoomTeleport(gentity_t *line, gentity_t *actor) {
    if (actor->health <= 0 || actor->client->noclip ||
        actor->client->sess.sessionTeam == TEAM_SPECTATOR) {
        return qfalse;
    }
    // Match Doom's sector order, then original thing order. The BSP metadata
    // also works with already compiled maps and is independent of spawn flags.
    const dbsp::Thing *destination = nullptr;
    for (int sector = 0; sector < doomSectorCount && !destination; ++sector) {
        if (doomSectorData[sector].tag != line->doom.tag) {
            continue;
        }
        for (int i = 0; i < doomThingCount; ++i) {
            if (doomThingData[i].type == 14 && doomThingData[i].sector == sector) {
                destination = &doomThingData[i];
                break;
            }
        }
    }
    if (!destination) {
        return qfalse;
    }
    float floor = static_cast<float>(doomSectorData[destination->sector].floor);
    for (int i = MAX_CLIENTS; i < level.num_entities; ++i) {
        const auto &mover = g_entities[i];
        if (mover.inuse && mover.classname && !Q_stricmp(mover.classname, "doom_mover") &&
            mover.doom.sector == destination->sector && !mover.doom.plane) {
            floor = mover.doom.base + mover.r.currentOrigin[2];
            break;
        }
    }
    vec3_t origin = {static_cast<float>(destination->x), static_cast<float>(destination->y),
                     floor - actor->r.mins[2] + 0.125f};
    trace_t trace{};
    const int mask = MASK_SOLID | (actor->doomNpc.type ? CONTENTS_BODY : 0);
    trap_TraceCapsule(&trace, origin, actor->r.mins, actor->r.maxs, origin, actor->s.number, mask);
    if (trace.startsolid || trace.allsolid) {
        return qfalse;
    }
    vec3_t source;
    VectorCopy(actor->client->ps.origin, source);
    source[2] += actor->r.mins[2];
    trap_UnlinkEntity(actor);
    auto &ps = actor->client->ps;
    VectorCopy(origin, ps.origin);
    VectorClear(ps.velocity);
    ps.groundEntityNum = ENTITYNUM_NONE;
    ps.eFlags ^= EF_TELEPORT_BIT;
    if (!actor->doomNpc.type) {
        ps.pm_flags = (ps.pm_flags & ~PMF_ALL_TIMES) | PMF_TIME_TELEPORT;
        ps.pm_time = 18 * 1000 / 35;
        G_KillBox(actor);
    } else {
        actor->doomNpc.charging = 0;
    }
    vec3_t angles = {0, static_cast<float>(destination->angle), 0};
    SetClientViewAngle(actor, angles);
    if (actor->doomNpc.type) {
        G_DoomAIReplicate(actor);
    } else {
        BG_PlayerStateToEntityState(&ps, &actor->s, qtrue);
    }
    VectorCopy(origin, actor->r.currentOrigin);
    VectorCopy(origin, actor->client->oldOrigin);
    trap_LinkEntity(actor);
    auto *event = G_TempEntity(source, EV_PLAYER_TELEPORT_OUT);
    event->s.eventParm = 1; // Doom fog events are anchored at the actor's feet.
    vec3_t forward, fog = {origin[0], origin[1], floor};
    AngleVectors(angles, forward, nullptr, nullptr);
    VectorMA(fog, 20, forward, fog);
    event = G_TempEntity(fog, EV_PLAYER_TELEPORT_IN);
    event->s.eventParm = 1;
    return qtrue;
}

static int DoomPointSector(const vec3_t point) {
    float nearest = -1e30f;
    int sector = -1;
    for (int i = 0; i < doomLineCount; ++i) {
        const auto &line = doomLineData[i];
        const float dx = line.b[0] - line.a[0];
        if (fabs(dx) < 0.0001f || point[0] < min(line.a[0], line.b[0]) ||
            point[0] >= max(line.a[0], line.b[0])) {
            continue;
        }
        const float y = line.a[1] + (line.b[1] - line.a[1]) * (point[0] - line.a[0]) / dx;
        if (y <= point[1] && y > nearest) {
            nearest = y;
            const int side = line.sides[dx > 0 ? 1 : 0];
            sector = side < 0 ? -1 : doomSideData[side].sector;
        }
    }
    return sector;
}

bool G_DoomCanHear(const vec3_t source, const vec3_t listener) {
    if (!doomSectorCount || !doomSoundHeads) {
        return true;
    }
    if (doomSoundTime != level.time || !VectorCompare(source, doomSoundOrigin)) {
        for (int i = 0; i < doomSectorCount; ++i) {
            doomSoundFloors[i] = static_cast<float>(doomSectorData[i].floor);
            doomSoundCeilings[i] = static_cast<float>(doomSectorData[i].ceiling);
        }
        for (int i = MAX_CLIENTS; i < level.num_entities; ++i) {
            const auto &entity = g_entities[i];
            if (!entity.inuse || !entity.classname || Q_stricmp(entity.classname, "doom_mover") ||
                entity.doom.sector < 0 || entity.doom.sector >= doomSectorCount) {
                continue;
            }
            float *heights = entity.doom.plane ? doomSoundCeilings : doomSoundFloors;
            heights[entity.doom.sector] = entity.doom.base + entity.r.currentOrigin[2];
        }
        doomsound::Flood(DoomPointSector(source), doomSectorCount, doomSoundHeads, doomSoundLinks,
                         doomSoundFloors, doomSoundCeilings, doomSoundLevels, doomSoundQueue);
        doomSoundTime = level.time;
        VectorCopy(source, doomSoundOrigin);
    }
    const int sector = DoomPointSector(listener);
    return sector >= 0 && sector < doomSectorCount && doomSoundLevels[sector] < 2;
}
DOOM_SAVE_CALLBACK("G_DoomCanHear", G_DoomCanHear);

void G_DoomInitMap() {
    G_DoomShutdown();
    char map[MAX_QPATH];
    trap_Cvar_VariableStringBuffer("mapname", map, sizeof(map));
    fileHandle_t file;
    int size = trap_FS_FOpenFile(va("maps/%s.bsp", map), &file, FS_READ);
    if (!file)
        return;
    if (size < int(sizeof(dbsp::Header)) || size > 128 * 1024 * 1024) {
        trap_FS_FCloseFile(file);
        G_Error("Invalid Doom BSP size");
        return;
    }
    doomMapData = static_cast<byte *>(malloc(size));
    if (!doomMapData)
        G_Error("Doom BSP allocation failed");
    trap_FS_Read(doomMapData, size, file);
    trap_FS_FCloseFile(file);
    const char *error = dbsp::validate(doomMapData, size);
    if (error)
        G_Error("Doom sector load: %s", error);
    auto *h = reinterpret_cast<dbsp::Header *>(doomMapData);
    doomSectorCount = h->lumps[dbsp::Sectors].length / sizeof(dbsp::Sector);
    doomLineCount = h->lumps[dbsp::Lines].length / sizeof(dbsp::Line);
    doomSectorData = reinterpret_cast<dbsp::Sector *>(doomMapData + h->lumps[dbsp::Sectors].offset);
    doomLineData = reinterpret_cast<dbsp::Line *>(doomMapData + h->lumps[dbsp::Lines].offset);
    doomSideData = reinterpret_cast<dbsp::Side *>(doomMapData + h->lumps[dbsp::Sides].offset);
    doomThingCount = h->lumps[dbsp::Things].length / sizeof(dbsp::Thing);
    doomThingData = reinterpret_cast<dbsp::Thing *>(doomMapData + h->lumps[dbsp::Things].offset);
    doomSoundHeads = static_cast<int *>(G_Alloc(doomSectorCount * sizeof(int)));
    doomSoundLevels = static_cast<int *>(G_Alloc(doomSectorCount * sizeof(int)));
    doomSoundQueue = static_cast<int *>(G_Alloc(doomSectorCount * 2 * sizeof(int)));
    doomSoundFloors = static_cast<float *>(G_Alloc(doomSectorCount * sizeof(float)));
    doomSoundCeilings = static_cast<float *>(G_Alloc(doomSectorCount * sizeof(float)));
    doomSoundLinks =
        static_cast<doomsound::Link *>(G_Alloc(doomLineCount * 2 * sizeof(doomsound::Link)));
    for (int i = 0; i < doomSectorCount; ++i) {
        doomSoundHeads[i] = -1;
    }
    int linkCount = 0;
    for (int i = 0; i < doomLineCount; ++i) {
        const auto &line = doomLineData[i];
        if (line.sides[0] < 0 || line.sides[1] < 0) {
            continue;
        }
        const int first = doomSideData[line.sides[0]].sector;
        const int second = doomSideData[line.sides[1]].sector;
        for (int side = 0; side < 2; ++side) {
            const int from = side ? second : first;
            const int to = side ? first : second;
            doomSoundLinks[linkCount] = {doomSoundHeads[from], to, (line.flags & 64) != 0};
            doomSoundHeads[from] = linkCount++;
        }
    }
    trap_SetConfigstring(CS_MUSIC, va("music/doom/d_%s.mid music/doom/d_%s.mid", map, map));
    G_DoomLevelInit(doomMapData);
    G_DoomSpawnWeaponThings(doomMapData);
    G_DoomAISpawnThings(doomMapData);
    // Doom levels have no Wolf briefing or scripted playerstart sequence.
    trap_Cvar_Set("g_missionStats", "0");
    trap_Cvar_Set("cg_norender", "0");
    trap_Cvar_Set("g_reloading", "0");
    trap_Cvar_Set("g_playerstart", "0");
    G_Printf("Doom map ready: %s (%d sectors, %d lines)\n", map, doomSectorCount, doomLineCount);
}
DOOM_SAVE_CALLBACK("G_DoomInitMap", G_DoomInitMap);

void G_DoomShutdown() {
    free(doomMapData);
    doomMapData = nullptr;
    doomSectorCount = doomLineCount = 0;
    doomSectorData = nullptr;
    doomLineData = nullptr;
    doomSideData = nullptr;
    doomThingData = nullptr;
    doomThingCount = 0;
    doomSoundHeads = doomSoundLevels = doomSoundQueue = nullptr;
    doomSoundLinks = nullptr;
    doomSoundFloors = doomSoundCeilings = nullptr;
    doomSoundTime = -1;
}
DOOM_SAVE_CALLBACK("G_DoomShutdown", G_DoomShutdown);

qboolean G_DoomMapActive() {
    return doomSectorCount > 0;
}
DOOM_SAVE_CALLBACK("G_DoomMapActive", G_DoomMapActive);

void G_DoomUpdateSwitches() {
    if (!doomLineCount)
        return;
    // Four linedef states per hex digit, including high-numbered linedefs.
    char state[MAX_STRING_CHARS * 4];
    int length = (doomLineCount + 3) / 4;
    if (length >= int(sizeof(state)))
        G_Error("Too many Doom switch states");
    memset(state, '0', length);
    state[length] = 0;
    const char *digits = "0123456789abcdef";
    for (int i = MAX_CLIENTS; i < level.num_entities; ++i) {
        auto *ent = &g_entities[i];
        if (!ent->inuse || !ent->classname || Q_stricmp(ent->classname, "doom_line") ||
            !ent->doom.lightMax || !ent->doom.plane)
            continue;
        int line = ent->doom.line;
        if (line < 0 || line >= doomLineCount)
            continue;
        char &word = state[line / 4];
        int value = int(strchr(digits, word) - digits);
        word = digits[value | (1 << (line % 4))];
    }
    trap_SetConfigstring(CS_DOOM_SWITCHES, state);
}
DOOM_SAVE_CALLBACK("G_DoomUpdateSwitches", G_DoomUpdateSwitches);

void G_DoomPlayerSector(gentity_t *player) {
    if (!player->client || player->health <= 0 || !doomSectorCount)
        return;
    vec3_t p;
    VectorCopy(player->client->ps.origin, p);
    float nearest = -1e30f;
    int sector = -1;
    for (int i = 0; i < doomLineCount; ++i) {
        auto &l = doomLineData[i];
        float dx = l.b[0] - l.a[0];
        if (fabs(dx) < 0.0001f || p[0] < min(l.a[0], l.b[0]) || p[0] >= max(l.a[0], l.b[0]))
            continue;
        float y = l.a[1] + (l.b[1] - l.a[1]) * (p[0] - l.a[0]) / dx;
        if (y <= p[1] && y > nearest) {
            nearest = y;
            int side = l.sides[dx > 0 ? 1 : 0];
            sector = side < 0 ? -1 : doomSideData[side].sector;
        }
    }
    if (sector < 0 || sector >= doomSectorCount)
        return;
    auto &s = doomSectorData[sector];
    float floor = float(s.floor);
    for (int i = MAX_CLIENTS; i < level.num_entities; ++i) {
        auto *ent = &g_entities[i];
        if (!ent->inuse || ent->doom.sector != sector || !ent->classname)
            continue;
        if (!Q_stricmp(ent->classname, "doom_mover") && !ent->doom.plane)
            floor += ent->r.currentOrigin[2];
        if (!player->doomNpc.type && s.special == 9 && !Q_stricmp(ent->classname, "doom_sector") &&
            !ent->doom.activated) {
            ent->doom.activated = 1;
            trap_SendServerCommand(player->s.number, "cp \"A secret is revealed!\"");
        }
    }
    if (p[2] + player->r.mins[2] > floor + 2 || player->doom.damageTime > level.time)
        return;
    int damage = 0;
    if (s.special == 5)
        damage = 10;
    if (s.special == 7)
        damage = 5;
    if (s.special == 4 || s.special == 16 || s.special == 11)
        damage = 20;
    if (damage) {
        player->doom.damageTime = level.time + 32 * 1000 / 35;
    }
    if (damage && player->client->ps.powerups[PW_BATTLESUIT] > level.time &&
        (damage < 20 || rand() % 256 >= 5)) {
        damage = 0;
    }
    if (damage) {
        G_Damage(player, nullptr, nullptr, nullptr, nullptr, damage, DAMAGE_NO_KNOCKBACK, MOD_LAVA);
    }
}
DOOM_SAVE_CALLBACK("G_DoomPlayerSector", G_DoomPlayerSector);

void G_DoomAIBossDeath(int type) {
    char map[MAX_QPATH];
    trap_Cvar_VariableStringBuffer("mapname", map, sizeof(map));
    const bool barons = !Q_stricmp(map, "e1m8") && type == doommonster::Baron;
    const bool finale = (!Q_stricmp(map, "e2m8") && type == doommonster::Cyberdemon) ||
                        (!Q_stricmp(map, "e3m8") && type == doommonster::SpiderMastermind);
    if (!barons && !finale) {
        return;
    }
    for (int i = 1; i < level.maxclients; ++i) {
        if (g_entities[i].inuse && g_entities[i].doomNpc.type == type && g_entities[i].health > 0) {
            return;
        }
    }
    if (finale) {
        G_DoomBeginIntermission(G_FindActor("player"), false);
        return;
    }
    for (int i = MAX_CLIENTS; i < level.num_entities; ++i) {
        auto *mover = &g_entities[i];
        if (mover->inuse && mover->classname && !Q_stricmp(mover->classname, "doom_mover") &&
            mover->doom.tag == 666 && !mover->doom.plane) {
            DoomActivateMover(mover, 23);
        }
    }
}
DOOM_SAVE_CALLBACK("G_DoomAIBossDeath", G_DoomAIBossDeath);
