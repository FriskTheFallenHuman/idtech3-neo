// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/q_shared.h"
#include "game/bg_public.h"
#include "game/bg_doomweapons.h"
#include "game/bg_doommonsters.h"
#include <cstdio>
#include <initializer_list>
#include <stdexcept>

vmCvar_t g_gametype{}, g_soldierChargeTime{}, g_engineerChargeTime{}, g_reloading{};
void trap_SnapVector(float *vector) {
    SnapVector(vector);
}
void trap_Cvar_VariableStringBuffer(const char *, char *buffer, int) {
    buffer[0] = 0;
}
void QDECL Com_Printf(const char *, ...) {
}
void QDECL Com_Error(int, const char *message, ...) {
    throw std::runtime_error(message);
}

namespace {
float stairHeight = -1, ceilingHeight = 1000;
// Independent analytical collision against a floor and a vertical wall. This
// exercises the real Pmove/slide code with no registered model animation data.
void Trace(trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs,
           const vec3_t end, int, int) {
    memset(trace, 0, sizeof(*trace));
    trace->fraction = 1;
    trace->entityNum = ENTITYNUM_NONE;
    if (end[2] + mins[2] < 0 && end[2] < start[2]) {
        trace->fraction = max(0.0f, (start[2] + mins[2]) / (start[2] - end[2]));
        VectorSet(trace->plane.normal, 0, 0, 1);
        trace->entityNum = ENTITYNUM_WORLD;
    }
    if (stairHeight < 0 && end[0] + maxs[0] > 100 && end[0] > start[0]) {
        const float hit = max(0.0f, (100 - maxs[0] - start[0]) / (end[0] - start[0]));
        if (hit < trace->fraction) {
            trace->fraction = hit;
            VectorSet(trace->plane.normal, -1, 0, 0);
            trace->entityNum = ENTITYNUM_WORLD;
        }
    }
    if (stairHeight >= 0) {
        const float low[3] = {64 - maxs[0], -100000, -100000};
        const float high[3] = {100000, 100000, stairHeight - mins[2]};
        float entry = -1e30f, exit = 1;
        int axis = -1;
        float normal = 0;
        bool intersects = true;
        for (int i = 0; i < 3; ++i) {
            const float delta = end[i] - s
tart[i];
            if (fabs(delta) < 0.000001f) {
                if (start[i] <= low[i] || start[i] >= high[i]) {
                    intersects = false;
                }
                continue;
            }
            const float first = (low[i] - start[i]) / delta;
            const float second = (high[i] - start[i]) / delta;
            const float nearTime = min(first, second);
            if (nearTime > entry) {
                entry = nearTime;
                axis = i;
                normal = delta > 0 ? -1 : 1;
            }
            exit = min(exit, max(first, second));
        }
        if (intersects && axis >= 0 && entry >= 0 && entry <= exit && entry < trace->fraction) {
            trace->fraction = entry;
            VectorClear(trace->plane.normal);
            trace->plane.normal[axis] = normal;
            trace->entityNum = ENTITYNUM_WORLD;
        }
        if (end[2] + maxs[2] > ceilingHeight && end[2] > start[2]) {
            const float hit = max(0.0f, (ceilingHeight - maxs[2] - start[2]) / (end[2] - start[2]));
            if (hit < trace->fraction) {
                trace->fraction = hit;
                VectorSet(trace->plane.normal, 0, 0, -1);
                trace->entityNum = ENTITYNUM_WORLD;
            }
        }
    }
    for (int i = 0; i < 3; ++i) {
        trace->endpos[i] = start[i] + trace->fraction * (end[i] - start[i]);
    }
}
int Contents(const vec3_t, int) {
    return 0;
}
void Run(bool monster) {
    playerState_t ps{};
    ps.clientNum = 255;
    ps.pm_type = PM_NORMAL;
    ps.gravity = 800;
    ps.speed = 100;
    ps.friction = 1;
    ps.runSpeedScale = ps.sprintSpeedScale = ps.crouchSpeedScale = 1;
    ps.stats[STAT_HEALTH] = 100;
    ps.stats[STAT_MAX_HEALTH] = 100;
    ps.groundEntityNum = ENTITYNUM_NONE;
    VectorSet(ps.mins, monster ? -20 : -16, monster ? -20 : -16, monster ? 0 : -24);
    VectorSet(ps.maxs, monster ? 20 : 16, monster ? 20 : 16, monster ? 56 : 32);
    ps.maxs[2] -= 2 * doommonster::
HullInset;
    ps.origin[2] = monster ? 0 : 24;
    ps.crouchMaxZ = ps.maxs[2];
    ps.viewheight = ps.standViewHeight = monster ? 42 : 17;
    if (monster) {
        ps.stats[STAT_DOOM_MONSTER] = 3;
    } else {
        doomweapon::Initialize(ps);
    }
    for (int step = 1; step <= 30; ++step) {
        pmove_t move{};
        move.ps = &ps;
        move.trace = Trace;
        move.pointcontents = Contents;
        move.tracemask = CONTENTS_SOLID;
        move.cmd.serverTime = step * 50;
        move.cmd.forwardmove = 127;
        move.noFootsteps = qtrue;
        move.noWeapClips = qtrue;
        Pmove(&move);
    }
    if (stairHeight < 0) {
        const float limit = monster ? 80.0f : 84.0f;
        if (ps.origin[0] < limit - 0.5f || ps.origin[0] > limit + 0.01f ||
            fabs(ps.origin[2] - (monster ? 0.0f : 24.0f)) > 0.1f) {
            throw std::runtime_error("Sprite player failed floor/wall collision or movement");
        }
    } else if (stairHeight <= 24) {
        if (ps.origin[0] < 110 ||
            fabs(ps.origin[2] - stairHeight - (monster ? 0.0f : 24.0f)) > 0.1f) {
            throw std::runtime_error("Doom actor cannot climb a legal step under its ceiling");
        }
    } else if (ps.origin[0] > 64 - ps.maxs[0] + 0.1f) {
        throw std::runtime_error("Doom actor climbed an illegal 25-unit step");
    }
}
} // namespace

int main() {
    try {
        g_gametype.integer = GT_SINGLE_PLAYER;
        Run(false);
        Run(true);
        for (bool monster : {false, true}) {
            for (int height : {0, 8, 16, 24, 25}) {
                stairHeight = static_cast<float>(height);
                ceilingHeight = height <= 16 ? 56.0f + height : 128.0f;
                Run(monster);
            }
        }
        std::puts("Real shared Pmove handles sprite players and monsters against floor/wall "
                  "collision and low-ceiling steps without mesh scripts.");
        return 0;
    } catch (const std::exception &error) {
    
    std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
