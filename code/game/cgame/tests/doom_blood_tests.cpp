// SPDX-License-Identifier: GPL-3.0-or-later
#include "../cg_local.h"
#include "../cg_doomfx.h"
#include "../../game/bg_doomblood.h"
#include <cstdio>
#include <limits>
#include <stdexcept>

centity_t cg_entities[MAX_GENTITIES]{};
namespace {
int marks, ignoredVictim, traceMask;
float wallDistance = 24, lastRadius;
int wallEntity = ENTITYNUM_WORLD, flags, contents;
bool startSolid, allowDecal = true;
bool flushWall;
vec3_t lastStart, lastEnd;
vec3_t wallNormal = {-1, 0, 0};
void Check(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace
void CG_Trace(trace_t *trace, const vec3_t start, const vec3_t, const vec3_t, const vec3_t end,
              int ignored, int mask) {
    VectorCopy(start, lastStart);
    VectorCopy(end, lastEnd);
    ignoredVictim = ignored;
    traceMask = mask;
    memset(trace, 0, sizeof(*trace));
    trace->fraction = wallDistance >= 96 ? 1 : wallDistance / 96;
    trace->entityNum = wallEntity;
    trace->surfaceFlags = flags;
    trace->contents = contents;
    trace->startsolid = startSolid ? qtrue : qfalse;
    if (flushWall) {
        trace->startsolid = start[0] > 20 ? qtrue : qfalse;
        trace->fraction = (20 - start[0]) / (end[0] - start[0]);
    }
    VectorCopy(wallNormal, trace->plane.normal);
    for (int axis = 0; axis < 3; ++axis) {
        trace->endpos[axis] = start[axis] + trace->fraction * (end[axis] - start[axis]);
    }
}
bool CG_DoomFxAllowDecal(const vec3_t, const vec3_t) {
    return allowDecal;
}
void CG_ImpactMark(qhandle_t shader, const vec3_t origin, const vec3_t normal, float rotation,
                   float red, float green, float blue, float alpha, qboolean alphaFade,
                   float radius, qboolean temporary, int duration) {
    ++marks;
    lastRadius = radius;
    Check(shader == 17 && origin[0] > lastStart[0] && normal[0] == -1,
          "Wall splat uses actual hit position and normal");
    Check(rotation >= 0 && rotation <= 360 && red > green && red > blue && alpha > .8f,
          "Blood splat is randomized, red and opaque");
    Check(alphaFade && !temporary && duration == 45000, "Large splat persists and fades");
}
void QDECL Com_Printf(const char *, ...) {
}
void QDECL Com_Error(int, const char *message, ...) {
    throw std::runtime_error(message);
}
int main() {
    try {
        vec3_t entry = {-20.125f, 0, 28}, direction = {1, 0, 0};
        vec3_t minimum = {-20, -20, 0}, maximum = {20, 20, 56}, exit;
        Check(doomblood::ExitPoint(entry, direction, minimum, maximum, exit),
              "Shot intersects monster bounds from collision epsilon");
        Check(fabs(exit[0] - 21) < .001f && exit[1] == 0 && exit[2] == 28,
              "Exit spray originates behind the far surface of the monster");
        vec3_t diagonal = {1, .5f, 0};
        Check(doomblood::ExitPoint(entry, diagonal, minimum, maximum, exit) && exit[0] > 19 &&
                  exit[1] > 19 && exit[1] < 21,
              "Oblique shot exits through the first far face");
        vec3_t miss = {-20, 25, 28}, zero = {0, 0, 0};
        Check(!doomblood::ExitPoint(miss, direction, minimum, maximum, exit),
              "Parallel shot outside bounds cannot invent an exit");
        Check(!doomblood::ExitPoint(entry, zero, minimum, maximum, exit), "Zero shot rejected");
        float invalid[] = {std::numeric_limits<float>::quiet_NaN(), 0, 0};
        Check(!doomblood::ExitPoint(entry, invalid, minimum, maximum, exit),
              "Nonfinite shot rejected");
        Check(doomblood::ExitPoint(entry, direction, minimum, maximum, exit), "Restore exit");
        Check(CG_DoomFxBloodWall(exit, direction, 7, 17, 24), "Nearby back wall receives splat");
        Check(marks == 1 && lastRadius > 20 && ignoredVictim == 7 && traceMask == MASK_SHOT &&
                  fabs(Distance(lastStart, lastEnd) - 96) < .001f,
              "Short trace starts outside victim and preserves nearby-wall range");
        wallDistance = 110;
        Check(!CG_DoomFxBloodWall(exit, direction, 7, 17, 24), "Distant wall gets no large splat");
        wallDistance = 24;
        for (int rejected : {SURF_NOIMPACT, SURF_NOMARKS, SURF_SKY}) {
            flags = rejected;
            Check(!CG_DoomFxBloodWall(exit, direction, 7, 17, 24),
                  "Surface restrictions respected");
        }
        flags = 0;
        contents = CONTENTS_SLIME;
        Check(!CG_DoomFxBloodWall(exit, direction, 7, 17, 24), "Liquid gets no solid blood mark");
        contents = 0;
        wallEntity = 8;
        Check(!CG_DoomFxBloodWall(exit, direction, 7, 17, 24),
              "Second actor blocks back-wall blood");
        wallEntity = ENTITYNUM_WORLD;
        VectorSet(wallNormal, 0, 0, 1);
        Check(!CG_DoomFxBloodWall(exit, direction, 7, 17, 24), "Floor is not a back-wall splat");
        VectorSet(wallNormal, 1, 0, 0);
        Check(!CG_DoomFxBloodWall(exit, direction, 7, 17, 24), "Wrong-facing surface rejected");
        VectorSet(wallNormal, -1, 0, 0);
        startSolid = true;
        Check(!CG_DoomFxBloodWall(exit, direction, 7, 17, 24), "Solid exit rejected");
        startSolid = false;
        allowDecal = false;
        Check(!CG_DoomFxBloodWall(exit, direction, 7, 17, 24), "Visible material policy respected");
        allowDecal = true;
        Check(!CG_DoomFxBloodWall(invalid, direction, 7, 17, 24), "Nonfinite exit rejected");
        Check(marks == 1, "Only the eligible nearby wall produces a mark");
        flushWall = true;
        Check(CG_DoomFxBloodWall(exit, direction, 7, 17, 24) && marks == 2,
              "Monster flush against a wall still produces a back-wall splat");
        std::puts("Doom blood: exit geometry, nearby wall projection, occlusion and malformed "
                  "inputs passed.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Doom blood test: %s\n", error.what());
        return 1;
    }
}
