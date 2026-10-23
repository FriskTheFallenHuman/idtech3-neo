// SPDX-License-Identifier: GPL-3.0-or-later
#include "qbsp.h"
#include "aas_map.h"
#include "doom_bsp.h"

namespace {
void AddBox(const vec3_t mins, const vec3_t maxs) {
    if (nummapbrushes >= MAX_MAPFILE_BRUSHES || nummapbrushsides > MAX_MAPFILE_BRUSHSIDES - 6) {
        Error("Doom navigation seal exceeds brush limits");
    }
    auto &brush = mapbrushes[nummapbrushes];
    memset(&brush, 0, sizeof(brush));
    brush.entitynum = 0;
    brush.brushnum = nummapbrushes;
    brush.contents = CONTENTS_SOLID;
    brush.numsides = 6;
    brush.leafnum = -1;
    brush.original_sides = &brushsides[nummapbrushsides];
    for (int i = 0; i < 6; ++i) {
        auto &side = brushsides[nummapbrushsides++];
        memset(&side, 0, sizeof(side));
        vec3_t normal = {0, 0, 0};
        const int axis = i / 2;
        const bool upper = (i & 1) != 0;
        normal[axis] = upper ? 1.0f : -1.0f;
        side.planenum = FindFloatPlane(normal, upper ? maxs[axis] : -mins[axis]);
        side.contents = CONTENTS_SOLID;
        side.flags = SFL_TEXTURED | SFL_VISIBLE;
    }
    AAS_CreateMapBrushes(&brush, &entities[0], false);
}
} // namespace

void BSPC_DoomAddSeal() {
    if (!bspcDoomMap) {
        return;
    }
    vec3_t mins, maxs;
    ClearBounds(mins, maxs);
    for (int i = 0; i < nummapbrushes; ++i) {
        if (mapbrushes[i].numsides > 0) {
            AddPointToBounds(mapbrushes[i].mins, mins, maxs);
            AddPointToBounds(mapbrushes[i].maxs, mins, maxs);
        }
    }
    for (int i = 0; i < 3; ++i) {
        mins[i] -= 128;
        maxs[i] += 128;
        if (mins[i] < -MAX_MAP_BOUNDS + 64 || maxs[i] > MAX_MAP_BOUNDS - 64) {
            Error("Doom navigation seal exceeds map bounds");
        }
    }
    // Sector BSPs permit open skies and retain authored topology gaps. A distant
    // tool-only hull gives BSPC a closed volume. Its floor is below every real
    // floor; directed walking routes cannot leave and climb back into the map.
    // Runtime collision and its checksum always use the unmodified source BSP.
    for (int axis = 0; axis < 3; ++axis) {
        for (int upper = 0; upper < 2; ++upper) {
            vec3_t boxMins, boxMaxs;
            for (int i = 0; i < 3; ++i) {
                boxMins[i] = mins[i] - 64;
                boxMaxs[i] = maxs[i] + 64;
            }
            boxMins[axis] = upper ? maxs[axis] : mins[axis] - 64;
            boxMaxs[axis] = upper ? maxs[axis] + 64 : mins[axis];
            AddBox(boxMins, boxMaxs);
        }
    }
}
