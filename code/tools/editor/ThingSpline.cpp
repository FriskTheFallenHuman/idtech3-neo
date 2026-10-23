#include "ThingSpline.h"
#include "EditWorld.h"
#include "idfunctions.h"
#include <algorithm>
#include <cmath>
#include <functional>
namespace de {
namespace {
double length(Vec2 a, Vec2 b) {
    return std::hypot(double(a.x) - b.x, double(a.y) - b.y);
}
Vec2 mix(Vec2 a, Vec2 b, double t) {
    return a + (b - a) * float(t);
}
// Centripetal Catmull-Rom prevents cusps from unevenly spaced nodes.
Vec2 curve(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, double u) {
    double t0 = 0, t1 = std::sqrt(length(p0, p1)), t2 = t1 + std::sqrt(length(p1, p2)),
           t3 = t2 + std::sqrt(length(p2, p3));
    double t = t1 + (t2 - t1) * u;
    auto lerp = [&](Vec2 a, Vec2 b, double x, double y) { return mix(a, b, (t - x) / (y - x)); };
    auto a1 = lerp(p0, p1, t0, t1), a2 = lerp(p1, p2, t1, t2), a3 = lerp(p2, p3, t2, t3);
    return lerp(lerp(a1, a2, t0, t2), lerp(a2, a3, t1, t3), t1, t2);
}
} // namespace
void validateThingSpline(const ThingSpline &s) {
    require(s.id > 0 && s.id < 1000000000 && s.type > 0 && s.type <= 32767 && s.options >= 0 &&
                s.options <= 31,
            "Invalid spline thing settings");
    require(std::isfinite(s.spacing) && s.spacing >= 1 && s.spacing <= 65536 &&
                std::isfinite(s.angleOffset) && std::abs(s.angleOffset) <= 36000,
            "Invalid spline spacing or angle");
    require(!s.nodes.empty() && s.nodes.size() <= 4096, "Spline needs 1 to 4096 nodes");
    for (auto p : s.nodes)
        require(std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) <= 32767 &&
                    std::abs(p.y) <= 32767,
                "Spline node outside map range");
}
std::vector<SplineSample> sampleThingSpline(const ThingSpline &s) {
    validateThingSpline(s);
    std::vector<Vec2> nodes;
    std::vector<int> original;
    for (size_t i = 0; i < s.nodes.size(); ++i)
        if (nodes.empty() || length(nodes.back(), s.nodes[i]) > .0001) {
            nodes.push_back(s.nodes[i]);
            original.push_back(int(i));
        }
    if (s.closed && nodes.size() > 1 && length(nodes.front(), nodes.back()) < .0001) {
        nodes.pop_back();
        original.pop_back();
    }
    std::vector<SplineSample> out{{nodes.front(), 0, original.front()}};
    if (nodes.size() < 2)
        return out;
    bool closed = s.closed && nodes.size() > 2;
    int count = int(nodes.size()), segments = closed ? count : count - 1;
    auto node = [&](int i) {
        if (closed)
            return nodes[(i + count) % count];
        if (i < 0)
            return nodes[0] * 2 - nodes[1];
        if (i >= count)
            return nodes.back() * 2 - nodes[count - 2];
        return nodes[i];
    };
    auto append = [&](Vec2 p, int segment) {
        require(out.size() < 262144, "Spline too complex; simplify its nodes");
        double d = length(out.back().point, p);
        if (d > .000001)
            out.push_back({p, out.back().distance + d, segment});
    };
    for (int i = 0; i < segments; ++i) {
        auto eval = [&](double t) {
            return s.smooth && count > 2 ? curve(node(i - 1), node(i), node(i + 1), node(i + 2), t)
                                         : mix(node(i), node(i + 1), t);
        };
        std::function<void(double, Vec2, double, Vec2, int)> subdivide;
        subdivide = [&](double a, Vec2 pa, double b, Vec2 pb, int depth) {
            auto mid = eval((a + b) / 2), q1 = eval((3 * a + b) / 4), q3 = eval((a + 3 * b) / 4);
            double chord = length(pa, pb),
                   polygon = length(pa, q1) + length(q1, mid) + length(mid, q3) + length(q3, pb);
            if (depth < 16 && (polygon - chord > .005 || length(mid, mix(pa, pb, .5)) > .02)) {
                subdivide(a, pa, (a + b) / 2, mid, depth + 1);
                subdivide((a + b) / 2, mid, b, pb, depth + 1);
            } else
                append(pb, original[i]);
        };
        subdivide(0, node(i), 1, node(i + 1), 0);
    }
    return out;
}
Vec2 splinePointAt(const std::vector<SplineSample> &samples, double distance, bool closed) {
    double total = samples.back().distance;
    if (total < .000001)
        return samples.front().point;
    if (closed) {
        distance = std::fmod(distance, total);
        if (distance < 0)
            distance += total;
    } else
        distance = std::clamp(distance, 0., total);
    auto next = std::lower_bound(samples.begin(), samples.end(), distance,
                                 [](const SplineSample &p, double d) { return p.distance < d; });
    if (next == samples.begin())
        return next->point;
    if (next == samples.end())
        return samples.back().point;
    const auto &prev = *(next - 1);
    return mix(prev.point, next->point,
               (distance - prev.distance) / (next->distance - prev.distance));
}
std::vector<Thing> generateSplineThings(const ThingSpline &s) {
    const auto samples = sampleThingSpline(s);
    double total = samples.back().distance;
    bool closed = s.closed && samples.size() > 2 &&
                  length(samples.front().point, samples.back().point) < .0001;
    // Global arc distance, independent of control-node density.
    size_t count = std::max(size_t(1), size_t(std::floor((total + .00001) / s.spacing)));
    require(count <= 32767, "Spline would create too many things; increase spacing");
    double pitch = closed && total > 0 ? total / count : s.spacing;
    double first = closed ? 0 : (total - (count - 1) * pitch) / 2;
    std::vector<Thing> result;
    result.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        double d = first + i * pitch,
               look = std::max(.1, std::min(double(s.spacing) * .25, total * .25));
        auto p = splinePointAt(samples, d, closed);
        require(std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) <= 32767 &&
                    std::abs(p.y) <= 32767,
                "Spline curve leaves the map coordinate range");
        auto delta =
            splinePointAt(samples, d + look, closed) - splinePointAt(samples, d - look, closed);
        float angle = float(std::atan2(delta.y, delta.x) * 180 / 3.141592653589793) + s.angleOffset;
        Thing t{p, (int(std::lround(angle)) % 360 + 360) % 360, s.type, s.options, 0};
        t.splineId = s.id;
        result.push_back(t);
    }
    return result;
}
int EditWorld::nextSplineId() const {
    int id = 1;
    for (const auto &s : thingSplines)
        id = std::max(id, s.id + 1);
    return id;
}
ThingSpline *EditWorld::findSpline(int id) {
    for (auto &s : thingSplines)
        if (s.id == id)
            return &s;
    return nullptr;
}
const ThingSpline *EditWorld::findSpline(int id) const {
    for (const auto &s : thingSplines)
        if (s.id == id)
            return &s;
    return nullptr;
}
void EditWorld::rebuildSpline(int id) {
    const auto *s = findSpline(id);
    require(s != nullptr, "Unknown spline");
    auto generated = generateSplineThings(*s);
    things.erase(std::remove_if(things.begin(), things.end(),
                                [&](const Thing &t) { return t.splineId == id; }),
                 things.end());
    things.insert(things.end(), generated.begin(), generated.end());
}
void EditWorld::deleteSpline(int id) {
    things.erase(std::remove_if(things.begin(), things.end(),
                                [&](const Thing &t) { return t.splineId == id; }),
                 things.end());
    thingSplines.erase(std::remove_if(thingSplines.begin(), thingSplines.end(),
                                      [&](const ThingSpline &s) { return s.id == id; }),
                       thingSplines.end());
}
void EditWorld::bakeSpline(int id) {
    for (auto &t : things)
        if (t.splineId == id)
            t.splineId = 0;
    thingSplines.erase(std::remove_if(thingSplines.begin(), thingSplines.end(),
                                      [&](const ThingSpline &s) { return s.id == id; }),
                       thingSplines.end());
}
} // namespace de
