// SPDX-License-Identifier: GPL-3.0-or-later
#include "SectorAuthoring.h"
#include "idfunctions.h"
#include "pathops.h"
#include <algorithm>
#include <cmath>

namespace de {
namespace {
bool intersects(Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
    const float abC = cross(b - a, c - a), abD = cross(b - a, d - a);
    const float cdA = cross(d - c, a - c), cdB = cross(d - c, b - c);
    if (((abC > 0 && abD < 0) || (abC < 0 && abD > 0)) &&
        ((cdA > 0 && cdB < 0) || (cdA < 0 && cdB > 0))) {
        return true;
    }
    return distanceToLine(a, c, d) < .001f || distanceToLine(b, c, d) < .001f ||
           distanceToLine(c, a, b) < .001f || distanceToLine(d, a, b) < .001f;
}
} // namespace

void authorSector(EditWorld &world, const std::vector<Vec2> &polygon, const Line &prototype) {
    require(polygon.size() >= 3 && polygon.size() <= 4096, "A sector needs 3..4096 vertices");
    double area = 0;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const auto a = polygon[i], b = polygon[(i + 1) % polygon.size()];
        require(std::isfinite(a.x) && std::isfinite(a.y) && std::abs(a.x) <= 32767 &&
                    std::abs(a.y) <= 32767,
                "Sector coordinates must be finite and within +/-32767 map units");
        require(!(a == b), "Sector has a zero-length wall");
        area += double(a.x) * b.y - double(b.x) * a.y;
        for (size_t j = i + 1; j < polygon.size(); ++j) {
            if (j == i + 1 || (i == 0 && j == polygon.size() - 1)) {
                continue;
            }
            require(!intersects(a, b, polygon[j], polygon[(j + 1) % polygon.size()]),
                    "Sector walls cross or touch; draw a simple closed loop");
        }
    }
    require(std::abs(area) >= 1, "Sector has no area");
    EditWorld candidate = world;
    candidate.deselect();
    Line wall = prototype;
    wall.flags = (wall.flags | 1) & ~ML_TWOSIDED;
    wall.selected = 0;
    wall.side[0].sector = wall.side[1].sector = -1;
    // Doom's front side is on the right of the directed wall.
    auto points = polygon;
    if (area > 0) {
        std::reverse(points.begin(), points.end());
    }
    for (size_t i = 0; i < points.size(); ++i) {
        candidate.newLine(points[i], points[(i + 1) % points.size()], wall);
    }
    try {
        connectSectors(candidate);
    } catch (const std::exception &) {
        require(finishDrawnSector(candidate, static_cast<int>(candidate.lines.size()) - 1, true),
                "Cannot form a sector here; check intersections and existing boundaries");
        connectSectors(candidate);
    }
    world.checkpoint();
    static_cast<WorldState &>(world) = static_cast<const WorldState &>(candidate);
}

std::vector<std::pair<int, int>> sectorAt(const EditWorld &world, Vec2 point) {
    EditWorld connected = world;
    const auto sectors = connectSectors(connected);
    for (const auto &sector : sectors) {
        bool inside = false;
        for (auto [index, side] : sector.sides) {
            const auto &line = connected.lines[index];
            const auto a = connected.points[side ? line.p2 : line.p1].pt;
            const auto b = connected.points[side ? line.p1 : line.p2].pt;
            if ((a.y > point.y) != (b.y > point.y) &&
                point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x) {
                inside = !inside;
            }
        }
        if (inside) {
            return sector.sides;
        }
    }
    return {};
}

void authorDivider(EditWorld &world, const std::vector<Vec2> &path) {
    require(path.size() >= 2 && path.size() <= 4096, "A divider needs 2..4096 vertices");
    auto onWall = [&](Vec2 point) {
        return std::any_of(world.lines.begin(), world.lines.end(), [&](const Line &line) {
            return line.selected >= 0 && distanceToLine(point, world.points.at(line.p1).pt,
                                                        world.points.at(line.p2).pt) < .001f;
        });
    };
    require(onWall(path.front()) && onWall(path.back()),
            "Both ends of a divider must meet existing walls");
    const auto middle = (path[0] + path[1]) * .5f;
    const auto parent = sectorAt(world, middle);
    require(!parent.empty(), "A divider must be inside a sector");
    Line prototype;
    prototype.side[0].ends = prototype.side[1].ends =
        world.lines[parent.front().first].side[parent.front().second].ends;
    EditWorld candidate = world;
    candidate.deselect();
    for (size_t i = 1; i < path.size(); ++i) {
        const auto a = path[i - 1], b = path[i];
        require(std::isfinite(b.x) && std::isfinite(b.y) && std::abs(b.x) <= 32767 &&
                    std::abs(b.y) <= 32767 && !(a == b),
                "Invalid divider coordinate or zero-length segment");
        for (size_t j = i + 1; j < path.size(); ++j) {
            require(!intersects(a, b, path[j - 1], path[j]) || j == i + 1,
                    "Divider crosses itself");
        }
        for (const auto &line : world.lines) {
            if (line.selected < 0) {
                continue;
            }
            const auto c = world.points.at(line.p1).pt, d = world.points.at(line.p2).pt;
            const bool touchesEnd = (i == 1 && distanceToLine(a, c, d) < .001f) ||
                                    (i == path.size() - 1 && distanceToLine(b, c, d) < .001f);
            require(!intersects(a, b, c, d) || touchesEnd, "Divider crosses an existing wall");
        }
        candidate.newLine(a, b, prototype);
    }
    require(joinAdjacentSectors(candidate) > 0, "Divider did not split a sector");
    auto before = world;
    const auto oldCount = connectSectors(before).size();
    require(connectSectors(candidate).size() == oldCount + 1,
            "Divider must split exactly one sector");
    world.checkpoint();
    static_cast<WorldState &>(world) = static_cast<const WorldState &>(candidate);
}

void applySurfaceTexture(EditWorld &world, const std::vector<std::pair<int, int>> &sides,
                         const std::string &texture, int surface) {
    require(surface >= 0 && surface <= 4, "Invalid texture surface");
    require(!texture.empty() && texture.size() <= 11 &&
                texture.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") ==
                    std::string::npos,
            "Invalid texture name");
    require(!sides.empty(), "Select a wall or sector first");
    for (auto [index, side] : sides) {
        require(index >= 0 && index < static_cast<int>(world.lines.size()) && side >= 0 &&
                    side < ((world.lines[index].flags & ML_TWOSIDED) ? 2 : 1) &&
                    world.lines[index].selected >= 0,
                "Invalid selected wall side");
    }
    world.checkpoint();
    for (auto [index, side] : sides) {
        auto &target = world.lines[index].side[side];
        switch (surface) {
        case 0:
            target.midtexture = texture;
            break;
        case 1:
            target.toptexture = texture;
            break;
        case 2:
            target.bottomtexture = texture;
            break;
        case 3:
            target.ends.floorflat = texture;
            break;
        case 4:
            target.ends.ceilingflat = texture;
            break;
        }
    }
}
} // namespace de
