// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from Hexen2 player bots; Doom combat and item rules.
#include "g_doombots_local.h"
#include "be_aas.h"
#include "../botlib/aasfile.h"

int DoomBotTravelFlags() {
    return TFL_WALK | TFL_CROUCH | TFL_BARRIERJUMP | TFL_JUMP | TFL_LADDER | TFL_WALKOFFLEDGE |
           TFL_SWIM | TFL_WATERJUMP | TFL_TELEPORT | TFL_ELEVATOR | TFL_FUNCBOB | TFL_JUMPPAD |
           TFL_AIR | TFL_WATER;
}
int DoomBotArea(const vec3_t point) {
    vec3_t origin;
    VectorCopy(point, origin);
    origin[2] += 1;
    trap_AAS_SetCurrentWorld(0);
    return trap_AAS_PointAreaNum(origin);
}
qboolean DoomBotRoute(gentity_t *self, const vec3_t goal, doomAasRoute_t &route) {
    vec3_t start, end;
    VectorCopy(self->r.currentOrigin, start);
    start[2] += self->r.mins[2] + 1;
    VectorCopy(goal, end);
    end[2] += 1;
    trap_AAS_SetCurrentWorld(0);
    return trap_AAS_Initialized() &&
                   trap_AAS_DoomPlayerRoute(start, end, DoomBotTravelFlags(), &route)
               ? qtrue
               : qfalse;
}
void DoomBotView(gentity_t *self, usercmd_t &cmd, const vec3_t desired, float turn) {
    for (int i = 0; i < 3; ++i) {
        float current = self->client->ps.viewangles[i];
        float angle = current + Com_Clamp(-turn, turn, AngleNormalize180(desired[i] - current));
        if (i == PITCH) {
            angle = Com_Clamp(-85, 85, AngleNormalize180(angle));
        }
        if (i == ROLL) {
            angle = 0;
        }
        cmd.angles[i] = ANGLE2SHORT(angle) - self->client->ps.delta_angles[i];
    }
}
qboolean DoomBotSafeStep(gentity_t *self, const vec3_t direction, qboolean crossing,
                         qboolean water) {
    vec3_t start, end;
    VectorCopy(self->r.currentOrigin, start);
    start[2] += .25f;
    VectorMA(start, 40, direction, end);
    trace_t trace;
    for (int bump = 0; bump < 3; ++bump) {
        trap_Trace(&trace, start, self->r.mins, self->r.maxs, end, self->s.number,
                   MASK_PLAYERSOLID);
        if (trace.startsolid || trace.allsolid) {
            return qfalse;
        }
        if (trace.fraction == 1) {
            break;
        }
        if (trace.plane.normal[2] < .7f) {
            // First try the actual walking clearance. Raising a 56-unit hull
            // unconditionally rejects Doom's otherwise passable 64-unit doors.
            vec3_t raised, destination;
            VectorCopy(start, raised);
            raised[2] += 24;
            trap_Trace(&trace, start, self->r.mins, self->r.maxs, raised, self->s.number,
                       MASK_PLAYERSOLID);
            if (trace.startsolid || trace.fraction != 1) {
                return qfalse;
            }
            VectorCopy(end, destination);
            destination[2] += 24;
            trap_Trace(&trace, raised, self->r.mins, self->r.maxs, destination, self->s.number,
                       MASK_PLAYERSOLID);
            if (trace.startsolid || trace.fraction != 1) {
                return qfalse;
            }
            VectorCopy(destination, end);
            break;
        }
        vec3_t remaining;
        VectorSubtract(end, trace.endpos, remaining);
        remaining[2] =
            -(remaining[0] * trace.plane.normal[0] + remaining[1] * trace.plane.normal[1]) /
            trace.plane.normal[2];
        VectorMA(trace.endpos, .125f, trace.plane.normal, start);
        VectorAdd(start, remaining, end);
        if (bump == 2) {
            return qfalse;
        }
    }
    vec3_t floor;
    VectorCopy(end, floor);
    floor[2] -= crossing ? 184 : 88;
    trap_Trace(&trace, end, self->r.mins, self->r.maxs, floor, self->s.number, MASK_PLAYERSOLID);
    if (trace.startsolid || trace.allsolid) {
        return qfalse;
    }
    vec3_t contentsPoint;
    VectorCopy(trace.endpos, contentsPoint);
    contentsPoint[2] += self->r.mins[2] + 2;
    if (trap_PointContents(contentsPoint, self->s.number) & (CONTENTS_LAVA | CONTENTS_SLIME)) {
        return qfalse;
    }
    return water || crossing || (trace.fraction < 1 && trace.plane.normal[2] >= .7f) ? qtrue
                                                                                     : qfalse;
}

static void MoveDirection(gentity_t *self, usercmd_t &cmd, const vec3_t direction) {
    float yaw = SHORT2ANGLE((cmd.angles[YAW] + self->client->ps.delta_angles[YAW]) & 65535);
    vec3_t view = {0, yaw, 0}, forward, right;
    AngleVectors(view, forward, right, NULL);
    cmd.forwardmove = static_cast<signed char>(127 * DotProduct(direction, forward));
    cmd.rightmove = static_cast<signed char>(127 * DotProduct(direction, right));
}
static void Escape(doomBot_t &bot, gentity_t *self, usercmd_t &cmd) {
    // Back away from the obstruction before asking AAS for another route.
    // Rotate the preference per bot so a group does not dodge in lockstep.
    vec3_t direction;
    VectorSubtract(bot.route.point, self->r.currentOrigin, direction);
    vec3_t angles;
    vectoangles(direction, angles);
    const float offsets[] = {135, -135, 90, -90, 180, 45, -45, 0};
    VectorClear(bot.escapeDirection);
    for (int i = 0; i < 8; ++i) {
        vec3_t test = {0, angles[YAW] + offsets[i ^ (self->s.number & 1)], 0}, candidate;
        AngleVectors(test, candidate, NULL, NULL);
        if (!DoomBotSafeStep(self, candidate, qfalse, self->waterlevel >= 2 ? qtrue : qfalse)) {
            continue;
        }
        VectorCopy(candidate, bot.escapeDirection);
        break;
    }
    ++bot.recoveries;
    bot.escapeUntil = level.time + 900;
    bot.avoidGoalNum = bot.goalNum;
    bot.avoidUntil = level.time + 4000;
    bot.hasGoal = qfalse;
    bot.nextDecision = bot.escapeUntil;
    bot.nextRoute = bot.escapeUntil;
    bot.progressTime = level.time;
    VectorCopy(self->r.currentOrigin, bot.progressOrigin);
    MoveDirection(self, cmd, bot.escapeDirection);
}
void DoomBotNavigate(doomBot_t &bot, gentity_t *self, usercmd_t &cmd, int msec) {
    if (level.time < bot.escapeUntil) {
        if (DoomBotSafeStep(self, bot.escapeDirection, qfalse,
                            self->waterlevel >= 2 ? qtrue : qfalse)) {
            MoveDirection(self, cmd, bot.escapeDirection);
        }
        return;
    }
    if (!bot.hasGoal) {
        bot.progressTime = level.time;
        return;
    }
    int ground = self->client->ps.groundEntityNum;
    qboolean riding = ground >= 0 && ground < ENTITYNUM_WORLD && bot.routed &&
                              bot.route.moverModel &&
                              g_entities[ground].s.modelindex == bot.route.moverModel
                          ? qtrue
                          : qfalse;
    if (level.time >= bot.nextRoute && !riding) {
        bot.nextRoute = level.time + 150;
        vec3_t origin, goal;
        VectorCopy(self->r.currentOrigin, origin);
        origin[2] += self->r.mins[2] + 1;
        VectorCopy(bot.goal, goal);
        goal[2] += 1;
        memset(&bot.route, 0, sizeof(bot.route));
        VectorCopy(bot.goal, bot.route.point);
        trap_AAS_SetCurrentWorld(0);
        ++bot.routeAttempts;
        bot.routed = trap_AAS_Initialized() && trap_AAS_DoomPlayerRoute(
                                                   origin, goal, DoomBotTravelFlags(), &bot.route)
                         ? qtrue
                         : qfalse;
        if (bot.routed) {
            ++bot.routes;
        } else {
            VectorCopy(bot.goal, bot.route.point); // failed query clears its output
        }
    }
    if (riding) {
        VectorCopy(bot.route.end, bot.route.point);
        // Hold position during the ride, then walk toward the exit at its height.
        if (fabsf(self->r.currentOrigin[2] - bot.route.end[2]) > 24) {
            bot.progressTime = level.time;
            return;
        }
    }
    // AAS reachability entry points can alternate around a sloped area's
    // boundary. When standing on a ramp, prefer the unobstructed uphill/downhill
    // corridor to a walking goal, still subject to hull and floor checks.
    if (!bot.routed || bot.route.travelType == TRAVEL_WALK) {
        vec3_t start, floor;
        VectorCopy(self->r.currentOrigin, start);
        start[2] += 1;
        VectorCopy(start, floor);
        floor[2] -= 4;
        trace_t groundTrace;
        trap_Trace(&groundTrace, start, self->r.mins, self->r.maxs, floor, self->s.number,
                   MASK_PLAYERSOLID);
        if (!groundTrace.startsolid && groundTrace.fraction < 1 &&
            groundTrace.plane.normal[2] >= .7f && groundTrace.plane.normal[2] < .99f) {
            vec3_t direct;
            VectorSubtract(bot.goal, self->r.currentOrigin, direct);
            float rise = fabsf(direct[2]);
            direct[2] = 0;
            float run = VectorNormalize(direct);
            vec3_t eye, target;
            VectorCopy(self->r.currentOrigin, eye);
            VectorCopy(bot.goal, target);
            eye[2] += self->client->ps.viewheight;
            target[2] += self->client->ps.viewheight;
            trace_t sight;
            trap_Trace(&sight, eye, NULL, NULL, target, self->s.number, MASK_SOLID);
            if (rise <= run && sight.fraction == 1 &&
                DoomBotSafeStep(self, direct, qfalse, qfalse)) {
                VectorCopy(bot.goal, bot.route.point);
            }
        }
    }
    vec3_t direction;
    VectorSubtract(bot.route.point, self->r.currentOrigin, direction);
    float vertical = direction[2] - self->r.mins[2];
    direction[2] = 0;
    float distance = VectorNormalize(direction);
    int travel = bot.route.travelType;
    qboolean jump = bot.routed && (travel == TRAVEL_JUMP || travel == TRAVEL_BARRIERJUMP ||
                                   travel == TRAVEL_WATERJUMP)
                        ? qtrue
                        : qfalse;
    qboolean crossing =
        bot.routed && (jump || travel == TRAVEL_WALKOFFLEDGE || travel == TRAVEL_JUMPPAD) ? qtrue
                                                                                          : qfalse;
    qboolean water = self->waterlevel >= 2 ? qtrue : qfalse;
    qboolean lift =
        bot.routed && (travel == TRAVEL_ELEVATOR || travel == TRAVEL_FUNCBOB) ? qtrue : qfalse;
    vec3_t progress;
    VectorSubtract(self->r.currentOrigin, bot.progressOrigin, progress);
    // Sideways oscillation at a wall is not progress. Count approach to the
    // waypoint, a substantial detour, or advancing to a different reachability.
    if (DotProduct(progress, direction) > 24 || VectorLength(progress) > 96 ||
        Distance(bot.route.point, bot.progressPoint) > 64) {
        VectorCopy(self->r.currentOrigin, bot.progressOrigin);
        bot.progressTime = level.time;
        VectorCopy(bot.route.point, bot.progressPoint);
    }
    if (level.time - bot.progressTime > (lift ? 7000 : 2000)) {
        Escape(bot, self, cmd);
        return;
    }
    if (distance < 8 && !water && travel != TRAVEL_LADDER) {
        return;
    }
    trace_t obstacle;
    vec3_t start, end;
    VectorCopy(self->r.currentOrigin, start);
    start[2] += .25f;
    VectorMA(start, 48, direction, end);
    trap_Trace(&obstacle, start, self->r.mins, self->r.maxs, end, self->s.number, MASK_PLAYERSOLID);
    if (obstacle.fraction < 1 && obstacle.entityNum < ENTITYNUM_WORLD) {
        vec3_t useStart, useEnd;
        VectorCopy(self->r.currentOrigin, useStart);
        useStart[2] += self->client->ps.viewheight;
        VectorMA(useStart, 64, direction, useEnd);
        // Combat aim can point away from the route. Use the obstructing Doom
        // line along travel direction, with the same key checks as human use.
        if ((level.time / 250 + self->s.number) % 2) {
            G_DoomTraceLine(self, useStart, useEnd, qfalse);
        }
        if ((level.time / 250) % 2) {
            cmd.buttons |= BUTTON_ACTIVATE;
        }
    }
    // Airborne route execution must keep its direction rather than steer away
    // because a floor probe is temporarily empty during a jump.
    qboolean airborne = self->client->ps.groundEntityNum == ENTITYNUM_NONE ? qtrue : qfalse;
    if (!airborne && !DoomBotSafeStep(self, direction, crossing, water)) {
        vec3_t angles, candidate;
        vectoangles(direction, angles);
        qboolean found = qfalse;
        const float offsets[] = {35, -35, 70, -70, 110, -110};
        for (int i = 0; i < 6; ++i) {
            vec3_t test = {0, angles[YAW] + offsets[(i + self->s.number % 2) % 6], 0};
            AngleVectors(test, candidate, NULL, NULL);
            if (!DoomBotSafeStep(self, candidate, qfalse, water)) {
                continue;
            }
            VectorCopy(candidate, direction);
            found = qtrue;
            break;
        }
        if (!found) {
            // A small obstacle can be jumped only when the landing is clear.
            vec3_t high, landing;
            VectorCopy(start, high);
            high[2] += 32;
            VectorMA(high, 64, direction, landing);
            trap_Trace(&obstacle, high, self->r.mins, self->r.maxs, landing, self->s.number,
                       MASK_PLAYERSOLID);
            if (obstacle.fraction != 1 || obstacle.startsolid) {
                return;
            }
            vec3_t floor;
            VectorCopy(landing, floor);
            floor[2] -= 96;
            trap_Trace(&obstacle, landing, NULL, NULL, floor, self->s.number, MASK_PLAYERSOLID);
            vec3_t probe;
            VectorCopy(obstacle.endpos, probe);
            probe[2] += 2;
            if (obstacle.fraction == 1 || obstacle.plane.normal[2] < .7f ||
                trap_PointContents(probe, self->s.number) & (CONTENTS_LAVA | CONTENTS_SLIME)) {
                return;
            }
            jump = qtrue;
        }
    }
    vec3_t desired;
    vectoangles(direction, desired);
    desired[PITCH] = 0;
    if (!bot.visible) {
        DoomBotView(self, cmd, desired, msec * .24f);
    }
    // Commands are relative to the view angle submitted this frame.
    MoveDirection(self, cmd, direction);
    if (!airborne && !crossing) {
        // A full-speed command can overshoot a nearby reachability on every
        // server tick and oscillate across it instead of advancing the route.
        const float stepDistance =
            max(1.0f, self->client->ps.speed * self->client->ps.runSpeedScale * msec / 1000.0f);
        const float approach = Com_Clamp(.15f, 1.0f, distance / stepDistance);
        cmd.forwardmove = static_cast<signed char>(cmd.forwardmove * approach);
        cmd.rightmove = static_cast<signed char>(cmd.rightmove * approach);
    }
    if (travel == TRAVEL_CROUCH) {
        cmd.upmove = -127;
    } else if (water || travel == TRAVEL_LADDER) {
        cmd.upmove = vertical > 12 ? 127 : vertical < -24 ? -127 : 0;
    } else if (jump && !airborne &&
               (!bot.routed || travel == TRAVEL_WALK ||
                Distance(self->r.currentOrigin, bot.route.start) < 64) &&
               level.time >= bot.nextJump) {
        cmd.upmove = 127;
        bot.nextJump = level.time + 800;
    }
    // Elevators use the reachability's boarding point. Wait while the platform
    // is below it; walk onto it when an ordinary collision trace finds a floor.
    if (lift && !riding && Distance(self->r.currentOrigin, bot.route.start) < 48 && vertical > 48 &&
        !airborne) {
        cmd.forwardmove = cmd.rightmove = 0;
    }
}
