#include "pathops.h"
#include <algorithm>
#include <cmath>
namespace de {
float cross(Vec2 a, Vec2 b) {
    return a.x * b.y - a.y * b.x;
}
float distanceToLine(Vec2 p, Vec2 a, Vec2 b) {
    Vec2 d = b - a;
    float len = d.x * d.x + d.y * d.y;
    float t = len ? std::clamp(((p.x - a.x) * d.x + (p.y - a.y) * d.y) / len, 0.f, 1.f) : 0;
    Vec2 q = p - (a + d * t);
    return std::sqrt(q.x * q.x + q.y * q.y);
}
bool lineInRect(Vec2 a, Vec2 b, Vec2 lo, Vec2 hi) {
    float t0 = 0, t1 = 1;
    auto clip = [&](float p, float q) {
        if (p == 0)
            return q >= 0;
        float r = q / p;
        if (p < 0) {
            if (r > t1)
                return false;
            t0 = std::max(t0, r);
        } else {
            if (r < t0)
                return false;
            t1 = std::min(t1, r);
        }
        return true;
    };
    Vec2 d = b - a;
    return clip(-d.x, a.x - lo.x) && clip(d.x, hi.x - a.x) && clip(-d.y, a.y - lo.y) &&
           clip(d.y, hi.y - a.y);
}
bool pointInPolygon(Vec2 p, const std::vector<Vec2> &v) {
    bool in = false;
    if (v.empty())
        return false;
    for (size_t i = 0, j = v.size() - 1; i < v.size(); j = i++) {
        auto a = v[i], b = v[j];
        if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)
            in = !in;
    }
    return in;
}
} // namespace de
