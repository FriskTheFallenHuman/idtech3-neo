// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace doomlight {
constexpr int Levels = 16;
constexpr int Width = 1024;
constexpr float DistanceStep = 4.0f;
inline int FogIndex(int light) {
    return 1 + (light < 0 ? 0 : light > 255 ? 255 : light) / 16;
}
// Original Doom distance colormap: startmap=(15-light/16)*4, with the
// 320-pixel reference projection contributing 1280/depth. RGB attenuation
// approximates the palette lookup while preserving its 16 light/32 shade steps.
inline unsigned char Alpha(int light, float depth) {
    const int level = FogIndex(light) - 1;
    const int scale = static_cast<int>(1280.0f / (depth < 1 ? 1 : depth));
    const int shade = (15 - level) * 4 - scale;
    return static_cast<unsigned char>(shade < 0 ? 0 : shade >= 32 ? 255 : shade * 8);
}
} // namespace doomlight
