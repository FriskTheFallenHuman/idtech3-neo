#pragma once
#include "md3_model.h"
#include <numeric>
#include <limits>

namespace doom_models::collision {
inline Vec3 add(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
inline Vec3 sub(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
inline Vec3 mul(Vec3 a, float f) {
    return {a.x * f, a.y * f, a.z * f};
}
inline float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline Vec3 normalized(Vec3 a) {
    return mul(a, 1 / std::sqrt(std::max(dot(a, a), 1e-20f)));
}
struct Bounds {
    Vec3 lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
    void include(Vec3 p) {
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }
    bool overlaps(const Bounds &b) const {
        return lo.x <= b.hi.x && hi.x >= b.lo.x && lo.y <= b.hi.y && hi.y >= b.lo.y &&
               lo.z <= b.hi.z && hi.z >= b.lo.z;
    }
};
struct Pose {
    Vec3 origin;
    float scale = 1, angle = 0;
    Vec3 local(Vec3 p) const { return mul(rotate(sub(p, origin), -angle), 1 / scale); }
    Vec3 world(Vec3 p) const { return add(origin, rotate(mul(p, scale), angle)); }
};
struct Hit {
    bool hit = false;
    float fraction = 1;
    Vec3 normal{};
};
struct Triangle {
    Vec3 a, b, c;
    Bounds bounds;
};
struct Node {
    Bounds bounds;
    unsigned first = 0, count = 0, left = 0, right = 0;
};

// SAT against a swept Doom actor box. This tests the real surfaces, including
// thin fences and overhanging branches, rather than a radius at the thing origin.
inline Hit sweepTriangle(Vec3 a, Vec3 b, Vec3 c, Vec3 half, Vec3 motion) {
    const Vec3 edges[] = {sub(b, a), sub(c, b), sub(a, c)};
    std::array<Vec3, 13> axes{};
    axes[0] = {1, 0, 0};
    axes[1] = {0, 1, 0};
    axes[2] = {0, 0, 1};
    axes[3] = cross(edges[0], edges[1]);
    unsigned n = 4;
    for (auto e : edges)
        for (auto basis : {Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, 1}})
            axes[n++] = cross(e, basis);
    float enter = 0, exit = 1;
    Vec3 normal{};
    for (auto axis : axes) {
        if (dot(axis, axis) < 1e-12f)
            continue;
        const float r =
            std::abs(axis.x) * half.x + std::abs(axis.y) * half.y + std::abs(axis.z) * half.z;
        const float lo = std::min({dot(a, axis), dot(b, axis), dot(c, axis)}) - r;
        const float hi = std::max({dot(a, axis), dot(b, axis), dot(c, axis)}) + r;
        const float speed = dot(motion, axis);
        if (std::abs(speed) < 1e-10f) {
            if (lo > 0 || hi < 0)
                return {};
            continue;
        }
        float t0 = lo / speed, t1 = hi / speed;
        if (t0 > t1)
            std::swap(t0, t1);
        if (t0 > enter) {
            enter = t0;
            normal = mul(normalized(axis), speed > 0 ? -1.f : 1.f);
        }
        exit = std::min(exit, t1);
        if (enter > exit)
            return {};
    }
    if (exit < 0 || enter > 1)
        return {};
    return {true, std::max(0.f, enter), normal};
}

class Mesh {
    std::vector<Triangle> triangles;
    std::vector<unsigned> order;
    std::vector<Node> nodes;
    unsigned build(unsigned first, unsigned count) {
        const unsigned index = unsigned(nodes.size());
        nodes.push_back({});
        Bounds bounds, centers;
        for (unsigned i = first; i < first + count; ++i) {
            const auto &t = triangles[order[i]];
            bounds.include(t.bounds.lo);
            bounds.include(t.bounds.hi);
            centers.include(mul(add(t.bounds.lo, t.bounds.hi), .5f));
        }
        nodes[index].bounds = bounds;
        if (count <= 8) {
            nodes[index].first = first;
            nodes[index].count = count;
            return index;
        }
        const auto extent = sub(centers.hi, centers.lo);
        int axis = extent.y > extent.x ? 1 : 0;
        if (extent.z > (axis ? extent.y : extent.x))
            axis = 2;
        const unsigned mid = first + count / 2;
        std::nth_element(order.begin() + first, order.begin() + mid, order.begin() + first + count,
                         [&](unsigned x, unsigned y) {
                             auto center = [&](unsigned i) {
                                 auto c = add(triangles[i].bounds.lo, triangles[i].bounds.hi);
                                 return axis == 0 ? c.x : axis == 1 ? c.y : c.z;
                             };
                             return center(x) < center(y);
                         });
        const auto left = build(first, mid - first), right = build(mid, first + count - mid);
        nodes[index].left = left;
        nodes[index].right = right;
        return index;
    }
    template <class F> void visit(unsigned index, const Bounds &query, F &fn) const {
        const auto &node = nodes[index];
        if (!node.bounds.overlaps(query))
            return;
        if (node.count) {
            for (unsigned i = node.first; i < node.first + node.count; ++i) {
                const auto &t = triangles[order[i]];
                if (t.bounds.overlaps(query))
                    fn(t);
            }
            return;
        }
        visit(node.left, query, fn);
        visit(node.right, query, fn);
    }

  public:
    Mesh() = default;
    Mesh(const Model &model, unsigned frame) {
        for (const auto &s : model.surfaces)
            for (const auto &indices : s.triangles) {
                Triangle t{s.frames[frame][indices[0]].position,
                           s.frames[frame][indices[1]].position,
                           s.frames[frame][indices[2]].position,
                           {}};
                if (dot(cross(sub(t.b, t.a), sub(t.c, t.a)), cross(sub(t.b, t.a), sub(t.c, t.a))) <
                    1e-12f)
                    continue;
                t.bounds.include(t.a);
                t.bounds.include(t.b);
                t.bounds.include(t.c);
                triangles.push_back(t);
            }
        order.resize(triangles.size());
        std::iota(order.begin(), order.end(), 0u);
        if (!order.empty())
            build(0, unsigned(order.size()));
    }
    Hit sweep(const Pose &pose, Vec3 from, Vec3 to, float radius, float height) const {
        if (nodes.empty())
            return {};
        const Vec3 half{std::max(.001f, radius - .005f), std::max(.001f, radius - .005f),
                        std::max(.001f, height * .5f - .01f)};
        const auto start = add(from, {0, 0, height * .5f}), end = add(to, {0, 0, height * .5f});
        const auto a = pose.local(start), b = pose.local(end);
        const float xy =
            (std::abs(std::cos(pose.angle)) + std::abs(std::sin(pose.angle))) * half.x / pose.scale;
        const Vec3 localHalf{xy, xy, half.z / pose.scale};
        Bounds query;
        query.include(sub(a, localHalf));
        query.include(add(a, localHalf));
        query.include(sub(b, localHalf));
        query.include(add(b, localHalf));
        Hit best;
        const auto motion = sub(end, start);
        auto test = [&](const Triangle &t) {
            auto hit = sweepTriangle(sub(pose.world(t.a), start), sub(pose.world(t.b), start),
                                     sub(pose.world(t.c), start), half, motion);
            if (hit.hit && (!best.hit || hit.fraction < best.fraction))
                best = hit;
        };
        visit(0, query, test);
        return best;
    }
    Hit ray(const Pose &pose, Vec3 from, Vec3 to, bool upwardFacesOnly = false) const {
        if (nodes.empty())
            return {};
        const auto a = pose.local(from), b = pose.local(to), direction = sub(b, a);
        Bounds query;
        query.include(a);
        query.include(b);
        Hit best;
        auto test = [&](const Triangle &t) {
            const auto ab = sub(t.b, t.a), ac = sub(t.c, t.a), normal = cross(ab, ac);
            if (upwardFacesOnly && normal.z <= 1e-6f)
                return;
            const auto p = cross(direction, ac);
            const float determinant = dot(ab, p);
            if (std::abs(determinant) < 1e-9f)
                return;
            const auto offset = sub(a, t.a);
            const float u = dot(offset, p) / determinant;
            if (u < -.0001f || u > 1.0001f)
                return;
            const auto q = cross(offset, ab);
            const float v = dot(direction, q) / determinant;
            if (v < -.0001f || u + v > 1.0001f)
                return;
            const float fraction = dot(ac, q) / determinant;
            if (fraction < 0 || fraction > 1 || (best.hit && fraction >= best.fraction))
                return;
            auto n = rotate(normalized(normal), pose.angle);
            if (dot(n, sub(to, from)) > 0)
                n = mul(n, -1);
            best = {true, fraction, n};
        };
        visit(0, query, test);
        return best;
    }
};
} // namespace doom_models::collision
