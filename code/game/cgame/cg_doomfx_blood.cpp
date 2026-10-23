// SPDX-License-Identifier: GPL-3.0-or-later
#include "cg_local.h"
#include "cg_doomfx.h"
#include <cmath>

bool CG_DoomFxBloodWall(const vec3_t exitOrigin, const vec3_t travel, int victim, qhandle_t shader,
                        float radius) {
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(exitOrigin[axis]) || !std::isfinite(travel[axis])) {
            return false;
        }
    }
    vec3_t direction, end;
    if (!shader || !std::isfinite(radius) || radius <= 0 ||
        VectorNormalize2(travel, direction) < 0.001f) {
        return false;
    }
    // Stop at the first obstruction. A second monster must not paint a wall behind it.
    VectorMA(exitOrigin, 96, direction, end);
    trace_t trace;
    CG_Trace(&trace, exitOrigin, nullptr, nullptr, end, victim, MASK_SHOT);
    if (trace.startsolid) {
        // A monster can stand flush against the wall. Retry across its back plane,
        // ignoring that monster, instead of losing the mark inside the wall's brush.
        vec3_t start;
        VectorMA(exitOrigin, -4, direction, start);
        CG_Trace(&trace, start, nullptr, nullptr, end, victim, MASK_SHOT);
    }
    if (trace.fraction <= 0 || trace.fraction >= 1 || trace.startsolid || trace.allsolid ||
        (trace.surfaceFlags & (SURF_NOIMPACT | SURF_NOMARKS | SURF_SKY)) ||
        (trace.contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA))) {
        return false;
    }
    if (trace.entityNum != ENTITYNUM_WORLD &&
        (trace.entityNum < 0 || trace.entityNum >= ENTITYNUM_WORLD ||
         !cg_entities[trace.entityNum].currentValid ||
         cg_entities[trace.entityNum].currentState.eType != ET_MOVER)) {
        return false;
    }
    // This is back-wall spatter, not a floor pool or a decal on a grazing side wall.
    if (std::fabs(trace.plane.normal[2]) > 0.65f ||
        DotProduct(direction, trace.plane.normal) > -0.25f ||
        !CG_DoomFxAllowDecal(trace.endpos, trace.plane.normal)) {
        return false;
    }
    const float proximity = 1.0f - trace.fraction;
    CG_ImpactMark(shader, trace.endpos, trace.plane.normal, random() * 360, .42f, .012f, .006f, .9f,
                  qtrue, radius * (.8f + .2f * proximity), qfalse, 45000);
    return true;
}
