#include "EditWorld.h"
#include "BlockWorld.h"
#include "idfunctions.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <map>
namespace de {
void EditWorld::placeObject(Vec2 point, int type, int angle, int options) {
    require(std::isfinite(point.x) && std::isfinite(point.y) && std::abs(point.x) <= 32768 &&
                std::abs(point.y) <= 32768,
            "Object position is outside the map coordinate range");
    require(type > 0 && type <= 65535, "Object type must be between 1 and 65535");
    checkpoint();
    deselect();
    things.push_back({point, ((angle % 360) + 360) % 360, type, options, 1});
}

bool SectorDef::operator==(const SectorDef &b) const {
    return floorUV == b.floorUV && ceilingUV == b.ceilingUV && rotation == b.rotation &&
           slope == b.slope && ceilingSlope == b.ceilingSlope && floorheight == b.floorheight &&
           ceilingheight == b.ceilingheight && floorflat == b.floorflat &&
           ceilingflat == b.ceilingflat && lightlevel == b.lightlevel && special == b.special &&
           tag == b.tag;
}
SectorDef EditWorld::adjustedFloor(const SectorDef &s, int delta, int slopeDelta,
                                   const std::vector<std::pair<int, int>> &boundaries) const {
    return adjustedSurface(s, false, delta, slopeDelta, boundaries);
}
SectorDef EditWorld::adjustedSurface(const SectorDef &s, bool ceiling, int delta, int slopeDelta,
                                     const std::vector<std::pair<int, int>> &boundaries) const {
    auto sides = boundaries;
    if (sides.empty())
        for (size_t i = 0; i < lines.size(); ++i)
            if (lines[i].selected >= 0)
                for (int side = 0; side < ((lines[i].flags & 4) ? 2 : 1); ++side)
                    if (lines[i].side[side].ends == s)
                        sides.emplace_back(int(i), side);
    if (sides.empty())
        return s;
    std::sort(sides.begin(), sides.end());
    SectorDef next = s;
    auto &height = ceiling ? next.ceilingheight : next.floorheight;
    auto &plane = ceiling ? next.ceilingSlope : next.slope;
    height += delta;
    if (height < -32768 || height > 32767)
        return s;
    if (slopeDelta) {
        bool found = false;
        for (auto [index, side] : sides) {
            const auto &line = lines[index];
            // Internal lines with this sector on both sides are not pivot walls.
            if (std::find(sides.begin(), sides.end(), std::make_pair(index, 1 - side)) !=
                sides.end())
                continue;
            Vec2 p = points[side ? line.p2 : line.p1].pt;
            Vec2 q = points[side ? line.p1 : line.p2].pt;
            const float length = std::hypot(q.x - p.x, q.y - p.y);
            if (length < .001f)
                continue;
            if (!plane.active()) {
                plane.x = int(std::lround(p.x));
                plane.y = int(std::lround(p.y));
            }
            plane.dx += int(std::lround((q.y - p.y) / length * slopeDelta));
            plane.dy += int(std::lround(-(q.x - p.x) / length * slopeDelta));
            if (!plane.active())
                plane = {};
            found = true;
            break;
        }
        if (!found)
            return s;
    }
    if (std::abs(plane.dx) > 16 * 65536 || std::abs(plane.dy) > 16 * 65536)
        return s;
    for (auto [index, side] : sides)
        for (int point : {lines[index].p1, lines[index].p2}) {
            float floorZ = next.floorAt(points[point].pt);
            float ceilingZ = next.ceilingAt(points[point].pt);
            if (floorZ < -32768 || ceilingZ > 32767 || floorZ > ceilingZ)
                return s;
        }
    return next;
}
void EditWorld::checkpoint() {
    undoStates.push_back(*this);
    if (undoStates.size() > 100)
        undoStates.erase(undoStates.begin());
    redoStates.clear();
}
bool EditWorld::moveSelectedThingsVertically(int delta) {
    if (!delta)
        return false;
    bool changed = false;
    for (const auto &t : things)
        if (t.selected > 0 &&
            std::clamp(int64_t(t.height) + delta, int64_t(-32768), int64_t(32767)) != t.height)
            changed = true;
    if (!changed)
        return false;
    checkpoint();
    for (auto &t : things)
        if (t.selected > 0)
            t.height = int(std::clamp(int64_t(t.height) + delta, int64_t(-32768), int64_t(32767)));
    return true;
}
bool EditWorld::undo() {
    if (undoStates.empty())
        return false;
    redoStates.push_back(*this);
    static_cast<WorldState &>(*this) = std::move(undoStates.back());
    undoStates.pop_back();
    return true;
}
bool EditWorld::redo() {
    if (redoStates.empty())
        return false;
    undoStates.push_back(*this);
    static_cast<WorldState &>(*this) = std::move(redoStates.back());
    redoStates.pop_back();
    return true;
}
bool EditWorld::dirty() const {
    return serialize() != saved;
}
void EditWorld::markSaved() {
    saved = serialize();
}
void EditWorld::clear() {
    points.clear();
    lines.clear();
    things.clear();
    thingSplines.clear();
    reference = {};
    undoStates.clear();
    redoStates.clear();
    pathname.clear();
    explicitSectors = false;
    markSaved();
}
void EditWorld::deselect() {
    for (auto &p : points)
        if (p.selected >= 0)
            p.selected = 0;
    for (auto &l : lines)
        if (l.selected >= 0)
            l.selected = 0;
    for (auto &t : things)
        if (t.selected >= 0)
            t.selected = 0;
}
void EditWorld::selectAll(int mode) {
    if (mode == 0)
        for (auto &p : points)
            if (p.selected >= 0)
                p.selected = 1;
    if (mode == 1)
        for (auto &l : lines)
            if (l.selected >= 0)
                l.selected = 1;
    if (mode == 2)
        for (auto &t : things)
            if (t.selected >= 0)
                t.selected = 1;
}
int EditWorld::newPoint(Vec2 p) {
    require(std::isfinite(p.x) && std::isfinite(p.y), "Non-finite point");
    for (size_t i = 0; i < points.size(); ++i)
        if (points[i].selected >= 0 && points[i].pt == p)
            return int(i);
    points.push_back({p, 0});
    return int(points.size() - 1);
}
int EditWorld::newLine(Vec2 a, Vec2 b, const Line &proto) {
    require(!(a == b), "Zero-length line");
    Line l = proto;
    l.p1 = newPoint(a);
    l.p2 = newPoint(b);
    l.selected = 0;
    lines.push_back(l);
    return int(lines.size() - 1);
}
void EditWorld::deleteSelected() {
    checkpoint();
    std::set<int> splineIds;
    for (const auto &t : things)
        if (t.selected > 0 && t.splineId)
            splineIds.insert(t.splineId);
    for (int id : splineIds)
        deleteSpline(id);
    for (auto &l : lines)
        if (l.selected > 0 || points[l.p1].selected > 0 || points[l.p2].selected > 0)
            l.selected = -1;
    for (auto &p : points)
        if (p.selected > 0)
            p.selected = -1;
    for (auto &t : things)
        if (t.selected > 0)
            t.selected = -1;
    std::set<int> used;
    for (auto &l : lines)
        if (l.selected >= 0) {
            used.insert(l.p1);
            used.insert(l.p2);
        }
    for (size_t i = 0; i < points.size(); ++i)
        if (!used.count(int(i)))
            points[i].selected = -1;
}
void EditWorld::flipSelected() {
    checkpoint();
    for (auto &l : lines)
        if (l.selected > 0) {
            std::swap(l.p1, l.p2);
            if (l.flags & ML_TWOSIDED)
                std::swap(l.side[0], l.side[1]);
        }
}
void EditWorld::fusePoints() {
    checkpoint();
    for (size_t i = 0; i < points.size(); ++i)
        if (points[i].selected >= 0)
            for (size_t j = i + 1; j < points.size(); ++j)
                if (points[j].selected >= 0 && points[i].pt == points[j].pt) {
                    for (auto &l : lines) {
                        if (l.p1 == j)
                            l.p1 = int(i);
                        if (l.p2 == j)
                            l.p2 = int(i);
                    }
                    points[j].selected = -1;
                }
}
void EditWorld::separatePoints() {
    checkpoint();
    for (auto &l : lines)
        if (l.selected > 0) {
            auto a = points[l.p1], b = points[l.p2];
            l.p1 = int(points.size());
            points.push_back(a);
            l.p2 = int(points.size());
            points.push_back(b);
        }
}
void EditWorld::splitLine(int i, Vec2 at) {
    require(i >= 0 && i < int(lines.size()), "Invalid line");
    auto l = lines[i];
    if (at == points[l.p1].pt || at == points[l.p2].pt)
        return;
    checkpoint();
    int p = newPoint(at);
    lines[i].p2 = p;
    l.p1 = p;
    lines.push_back(l);
}
void EditWorld::moveSelected(Vec2 d) {
    std::set<int> ids;
    for (size_t i = 0; i < points.size(); ++i)
        if (points[i].selected > 0)
            ids.insert(int(i));
    for (auto &l : lines)
        if (l.selected > 0) {
            ids.insert(l.p1);
            ids.insert(l.p2);
        }
    // A complete moved sector carries its plane; partial vertex edits reshape its boundary.
    const bool hasSlopes = std::any_of(lines.begin(), lines.end(), [](const Line &line) {
        return line.selected >= 0 &&
               (line.side[0].ends.slope.active() || line.side[1].ends.slope.active() ||
                line.side[0].ends.ceilingSlope.active() || line.side[1].ends.ceilingSlope.active());
    });
    if (hasSlopes && d.x == std::round(d.x) && d.y == std::round(d.y)) {
        try {
            auto topology = *this;
            for (const auto &sector : connectSectors(topology)) {
                if (!sector.def.slope.active() && !sector.def.ceilingSlope.active())
                    continue;
                bool complete = true;
                for (auto [line, side] : sector.sides)
                    complete &= ids.count(lines[line].p1) && ids.count(lines[line].p2);
                if (complete)
                    for (auto [line, side] : sector.sides) {
                        for (auto *plane : {&lines[line].side[side].ends.slope,
                                            &lines[line].side[side].ends.ceilingSlope})
                            if (plane->active()) {
                                plane->x += int(d.x);
                                plane->y += int(d.y);
                            }
                    }
            }
        } catch (const std::exception &) {
            // Open drawings still allow point movement; they have no resolved sectors yet.
        }
    }
    for (int i : ids)
        points[i].pt = points[i].pt + d;
    std::set<int> movedSplines;
    for (auto &t : things)
        if (t.selected > 0 && t.splineId)
            movedSplines.insert(t.splineId);
        else if (t.selected > 0)
            t.origin = t.origin + d;
    for (int id : movedSplines) {
        for (auto &p : findSpline(id)->nodes)
            p = p + d;
        for (auto &t : things)
            if (t.splineId == id)
                t.origin = t.origin + d;
    }
}
Vec2 EditWorld::selectionCenter() const {
    Vec2 c{};
    int n = 0;
    for (auto &l : lines)
        if (l.selected > 0) {
            c = c + points[l.p1].pt + points[l.p2].pt;
            n += 2;
        }
    for (auto &t : things)
        if (t.selected > 0) {
            c = c + t.origin;
            ++n;
        }
    return n ? c * (1.f / n) : c;
}
void EditWorld::copy() {
    clipboard = {};
    auto c = selectionCenter();
    for (const auto &l : lines)
        if (l.selected > 0 &&
            (l.side[0].ends.slope.active() || l.side[1].ends.slope.active() ||
             l.side[0].ends.ceilingSlope.active() || l.side[1].ends.ceilingSlope.active())) {
            c = {std::round(c.x), std::round(c.y)};
            break;
        }
    for (auto l : lines)
        if (l.selected > 0) {
            auto a = points[l.p1], b = points[l.p2];
            a.pt = a.pt - c;
            b.pt = b.pt - c;
            l.p1 = int(clipboard.points.size());
            clipboard.points.push_back(a);
            l.p2 = int(clipboard.points.size());
            clipboard.points.push_back(b);
            for (auto &side : l.side)
                for (auto *plane : {&side.ends.slope, &side.ends.ceilingSlope})
                    if (plane->active()) {
                        plane->x -= int(c.x);
                        plane->y -= int(c.y);
                    }
            clipboard.lines.push_back(l);
        }
    for (auto t : things)
        if (t.selected > 0) {
            t.origin = t.origin - c;
            t.splineId = 0; // Copy individual instances as ordinary editable things.
            clipboard.things.push_back(t);
        }
}
void EditWorld::paste(Vec2 at) {
    checkpoint();
    deselect();
    std::map<int, int> sectorIds;
    int nextSector = 0;
    for (const auto &l : lines)
        if (l.selected >= 0)
            for (const auto &side : l.side)
                nextSector = std::max(nextSector, side.sector + 1);
    for (auto l : clipboard.lines) {
        for (auto &side : l.side)
            if (side.sector >= 0) {
                auto entry = sectorIds.emplace(side.sector, nextSector);
                if (entry.second)
                    ++nextSector;
                side.sector = entry.first->second;
            }
        int i = newLine(clipboard.points[l.p1].pt + at, clipboard.points[l.p2].pt + at, l);
        lines[i].selected = 1;
        for (auto &side : lines[i].side)
            for (auto *plane : {&side.ends.slope, &side.ends.ceilingSlope})
                if (plane->active()) {
                    plane->x += int(std::lround(at.x));
                    plane->y += int(std::lround(at.y));
                }
    }
    for (auto t : clipboard.things) {
        t.origin = t.origin + at;
        t.selected = 1;
        things.push_back(t);
    }
}
std::vector<std::string> EditWorld::validate(bool checkTextures) const {
    std::vector<std::string> out;
    for (size_t i = 0; i < lines.size(); ++i) {
        auto &l = lines[i];
        if (l.selected < 0)
            continue;
        auto error = [&](const char *s) { out.push_back("Line " + std::to_string(i) + ": " + s); };
        if (l.p1 < 0 || l.p2 < 0 || l.p1 >= points.size() || l.p2 >= points.size()) {
            error("invalid point index");
            continue;
        }
        if (points[l.p1].pt == points[l.p2].pt)
            error("zero length");
        for (int s = 0; s < ((l.flags & 4) ? 2 : 1); ++s) {
            auto &side = l.side[s];
            for (int vertex : {l.p1, l.p2}) {
                float floor = side.ends.floorAt(points[vertex].pt);
                if (floor > side.ends.ceilingAt(points[vertex].pt))
                    error("floor above ceiling");
                float ceiling = side.ends.ceilingAt(points[vertex].pt);
                if (ceiling < -32768 || ceiling > 32767)
                    error("ceiling outside map height range");
                if (floor < -32768 || floor > 32767)
                    error("floor outside map height range");
            }
            for (const auto &slope : {side.ends.slope, side.ends.ceilingSlope})
                if (slope.x < -32768 || slope.x > 32767 || slope.y < -32768 || slope.y > 32767 ||
                    slope.dx < -16 * 65536 || slope.dx > 16 * 65536 || slope.dy < -16 * 65536 ||
                    slope.dy > 16 * 65536)
                    error("surface slope outside supported bounds");
            if (side.ends.lightlevel < 0 || side.ends.lightlevel > 255)
                error("light outside 0..255");
            if (checkTextures && !(l.flags & 4) &&
                (side.midtexture.empty() || side.midtexture == "-"))
                error("missing middle texture");
        }
    }
    return out;
}
} // namespace de
