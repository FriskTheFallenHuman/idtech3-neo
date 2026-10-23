// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>

namespace doomconsole {
inline int PixelSize(int videoHeight, float referenceSize, float scale) {
    if (!std::isfinite(referenceSize)) {
        referenceSize = 16;
    }
    if (!std::isfinite(scale)) {
        scale = 1;
    }
    const float pixels = std::clamp(referenceSize, 8.0f, 32.0f) * std::clamp(scale, 0.5f, 3.0f) *
                         (std::max)(videoHeight, 1) / 720.0f;
    return static_cast<int>(std::clamp(std::round(pixels), 8.0f, 64.0f));
}
inline int Columns(int videoWidth, int advance) {
    return std::clamp(videoWidth / (std::max)(advance, 1) - 2, 1, 1022);
}
} // namespace doomconsole
