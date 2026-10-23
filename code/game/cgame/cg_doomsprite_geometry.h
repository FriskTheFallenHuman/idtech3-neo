// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../game/bg_doommonsters.h"
#include "tr_types.h"

namespace doomsprite {
inline void BuildQuad(const vec3_t origin, const vec3_t toViewer, const vec3_t cameraLeft,
                      const doommonster::Patch &patch, bool flip, float footOffset,
                      polyVert_t (&vertices)[4], bool grounded = true) {
    const float left =
        flip ? static_cast<float>(patch.left - patch.width) : -static_cast<float>(patch.left);
    // Doom patches can extend below their anchor. Keep grounded sprites above
    // the collision floor while preserving centered projectile placement.
    const float top =
        static_cast<float>(grounded ? max(patch.top, patch.height) : patch.top) - footOffset;
    const float horizontal[] = {left, left + patch.width, left + patch.width, left};
    const float vertical[] = {top - patch.height, top - patch.height, top, top};
    const float u = static_cast<float>(patch.width) / patch.textureWidth;
    const float v = static_cast<float>(patch.height) / patch.textureHeight;
    const float firstU = flip ? u : 0;
    const float lastU = flip ? 0 : u;
    const float texture[4][2] = {{firstU, v}, {lastU, v}, {lastU, 0}, {firstU, 0}};
    // Actor-to-viewer crossed with up gives camera right. Reversing this sign
    // mirrors every patch, including the already mirrored dual-name rotations.
    vec3_t right = {-toViewer[1], toViewer[0], 0};
    if (VectorNormalize(right) < 0.001f) {
        VectorScale(cameraLeft, -1, right);
        right[2] = 0;
        VectorNormalize(right);
    }
    for (int i = 0; i < 4; ++i) {
        VectorMA(origin, horizontal[i], right, vertices[i].xyz);
        vertices[i].xyz[2] += vertical[i];
        vertices[i].st[0] = texture[i][0];
        vertices[i].st[1] = texture[i][1];
    }
}
} // namespace doomsprite
