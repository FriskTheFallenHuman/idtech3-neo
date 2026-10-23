// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Shared by botlib, engine and game. Coordinates use the AAS feet origin.
struct doomAasRoute_t {
    vec3_t point, start, end;
    int travelType, travelTime, moverModel;
};
static_assert(sizeof(doomAasRoute_t) == 48, "Doom player route ABI changed");
