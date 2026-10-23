// SPDX-License-Identifier: GPL-3.0-or-later
#include "../tr_doomlighting.h"
#include <cstdio>
int main() {
    using namespace doomlight;
    if (FogIndex(-1) != 1 || FogIndex(999) != 16 || Alpha(255, 4096) != 0 ||
        Alpha(64, 128) != 255 || Alpha(192, 64) != 0 || Alpha(192, 128) != 16 ||
        Alpha(192, 1024) != 88 || Alpha(0, 0) != 0) {
        return 1;
    }
    for (int light = 0; light < 256; ++light) {
        int previous = 0;
        for (int depth = 0; depth < 4096; ++depth) {
            const int alpha = Alpha(light, static_cast<float>(depth));
            if (alpha < previous ||
                (light && alpha > Alpha(light - 1, static_cast<float>(depth)))) {
                return 2;
            }
            previous = alpha;
        }
    }
    std::puts("Doom sector distance lighting, fullbright and monotonic visibility passed.");
}
