#pragma once
#include <algorithm>
#include <array>
#include <vector>

struct WallStrip {
    float t0 = 0, t1 = 1;
    std::array<float, 2> bottom{}, top{}, anchor{};
    bool clip() {
        float h0 = top[0] - bottom[0], h1 = top[1] - bottom[1];
        if (h0 <= 0 && h1 <= 0)
            return false;
        if (h0 < 0 || h1 < 0) {
            float t = h0 / (h0 - h1);
            int end = h0 < 0 ? 0 : 1;
            bottom[end] = bottom[0] + t * (bottom[1] - bottom[0]);
            top[end] = bottom[end];
            anchor[end] = anchor[0] + t * (anchor[1] - anchor[0]);
            (end ? t1 : t0) = t0 + t * (t1 - t0);
        }
        return true;
    }
};
// Split at every change of ordering so max/min portal bounds remain linear.
inline std::vector<float> wallCuts(const std::vector<std::array<float, 2>> &heights) {
    std::vector<float> cuts{0, 1};
    for (size_t i = 0; i < heights.size(); ++i)
        for (size_t j = 0; j < i; ++j) {
            float a = heights[i][0] - heights[j][0], b = heights[i][1] - heights[j][1];
            if ((a < 0 && b > 0) || (a > 0 && b < 0))
                cuts.push_back(a / (a - b));
        }
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
    return cuts;
}
