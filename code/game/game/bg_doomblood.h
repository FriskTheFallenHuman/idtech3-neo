// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cmath>
#include <algorithm>

namespace doomblood {
// A shot enters the collision hull slightly outside its front plane. Intersect the
// whole ray with the live server bounds and step beyond its far plane for exit FX.
inline bool ExitPoint(const float *entry, const float *direction, const float *minimum,
                      const float *maximum, float *exit) {
    float lengthSquared = 0;
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(entry[axis]) || !std::isfinite(direction[axis]) ||
            !std::isfinite(minimum[axis]) || !std::isfinite(maximum[axis]) ||
            minimum[axis] > maximum[axis]) {
            return false;
        }
        lengthSquared += direction[axis] * direction[axis];
    }
    if (!std::isfinite(lengthSquared) || lengthSquared < 0.000001f) {
        return false;
    }
    float forward[3];
    const float length = std::sqrt(lengthSquared);
    float first = -1000000, last = 1000000;
    for (int axis = 0; axis < 3; ++axis) {
        forward[axis] = direction[axis] / length;
        if (std::fabs(forward[axis]) < 0.00001f) {
            if (entry[axis] < minimum[axis] - 0.25f || entry[axis] > maximum[axis] + 0.25f) {
                return false;
            }
            continue;
        }
        const float a = (minimum[axis] - entry[axis]) / forward[axis];
        const float b = (maximum[axis] - entry[axis]) / forward[axis];
        first = (std::max)(first, (std::min)(a, b));
        last = (std::min)(last, (std::max)(a, b));
    }
    if (last < (std::max)(first, 0.0f) || last > 512) {
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        exit[axis] = entry[axis] + (last + 1) * forward[axis];
    }
    return true;
}
} // namespace doomblood
