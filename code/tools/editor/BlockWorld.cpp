// Planar half-edge traversal replaces the NeXT raster flood-fill. Every side
// belongs to the face on its right; nested positive loops are holes.
#include "BlockWorld.h"
#include "idfunctions.h"
#include "pathops.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
namespace de {
std::vector<std::pair<int, int>> makeInnerSector(EditWorld &world, Vec2 cursor) {
    using Key = std::pair<float, float>;
    auto key = [&](int vertex) {
        auto p = world.points.at(vertex).pt;
        return Key{p.x, p.y};
    };
    std::map<Key, std::vector<int>> adjacent;
    for (size_t i = 0; i < world.lines.size(); ++i) {
        const auto &l = world.lines[i];
        if (l.selected < 0 || (l.flags & ML_TWOSIDED))
            continue;
        adjacent[key(l.p1)].push_back(int(i));
        adjacent[key(l.p2)].push_back(int(i));
    }
    std::set<int> visited;
    std::vector<std::pair<int, bool>> chosen;
    std::vector<Vec2> polygon;
    double smallest = std::numeric_limits<double>::max(), chosenArea = 0;
    for (size_t seed = 0; seed < world.lines.size(); ++seed) {
        const auto &initial = world.lines[seed];
        if (initial.selected < 0 || (initial.flags & ML_TWOSIDED) || visited.count(int(seed)))
            continue;
        std::vector<std::pair<int, bool>> loop;
        std::vector<Vec2> points;
        Key start = key(initial.p1), at = start;
        int edge = int(seed);
        bool closed = false;
        while (!visited.count(edge)) {
            visited.insert(edge);
            const auto &l = world.lines[edge];
            const bool reversed = key(l.p1) != at;
            loop.push_back({edge, reversed});
            points.push_back({at.first, at.second});
            at = key(reversed ? l.p1 : l.p2);
            if (at == start) {
                closed = adjacent[at].size() == 2;
                break;
            }
            if (adjacent[at].size() != 2)
                break;
            edge = adjacent[at][0] == edge ? adjacent[at][1] : adjacent[at][0];
        }
        if (!closed || points.size() < 3 || !pointInPolygon(cursor, points))
            continue;
        double area = 0;
        for (size_t i = 0; i < points.size(); ++i) {
            auto a = points[i], b = points[(i + 1) % points.size()];
            area += double(a.x) * b.y - double(b.x) * a.y;
        }
        if (std::abs(area) < smallest) {
            smallest = std::abs(area);
            chosenArea = area;
            chosen = std::move(loop);
            polygon = std::move(points);
        }
    }
    require(!chosen.empty(),
            "Point inside a closed one-sided loop within an existing sector, then press Alt+S");
    auto parentWorld = world;
    for (auto [index, reversed] : chosen)
        parentWorld.lines[index].selected = -1;
    const auto parents = connectSectors(parentWorld);
    const ConnectedSector *parent = nullptr;
    auto contains = [&](const ConnectedSector &sector, Vec2 p) {
        bool inside = false;
        for (auto [index, side] : sector.sides) {
            const auto &l = world.lines[index];
            auto a = world.points[l.p1].pt, b = world.points[l.p2].pt;
            if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)
                inside = !inside;
        }
        return inside;
    };
    for (const auto &sector : parents)
        if (contains(sector, cursor)) {
            parent = &sector;
            break;
        }
    require(parent != nullptr, "The loop must be inside another sector");
    for (size_t i = 0; i < polygon.size(); ++i) {
        const auto a = polygon[i], b = polygon[(i + 1) % polygon.size()];
        require(contains(*parent, a) && contains(*parent, (a + b) * .5f),
                "Inner loop crosses the containing sector");
        for (auto [index, side] : parent->sides) {
            const auto &l = world.lines[index];
            const auto c = world.points[l.p1].pt, d = world.points[l.p2].pt;
            require(!(cross(b - a, c - a) * cross(b - a, d - a) <= 0 &&
                      cross(d - c, a - c) * cross(d - c, b - c) <= 0 &&
                      std::max(std::min(a.x, b.x), std::min(c.x, d.x)) <=
                          std::min(std::max(a.x, b.x), std::max(c.x, d.x)) &&
                      std::max(std::min(a.y, b.y), std::min(c.y, d.y)) <=
                          std::min(std::max(a.y, b.y), std::max(c.y, d.y))),
                    "Inner loop must not touch or cross its parent boundary");
        }
    }
    auto next = world;
    int newId = 0;
    for (const auto &l : world.lines)
        for (const auto &s : l.side)
            newId = std::max(newId, s.sector + 1);
    const auto parentSide = parent->sides.front();
    const int parentId = world.lines[parentSide.first].side[parentSide.second].sector;
    std::string wall = "STARTAN3";
    for (auto [index, side] : parent->sides) {
        const auto &name = world.lines[index].side[side].midtexture;
        if (!name.empty() && name != "-") {
            wall = name;
            break;
        }
    }
    std::vector<std::pair<int, int>> selected;
    next.deselect();
    for (auto [index, reversed] : chosen) {
        auto &l = next.lines[index];
        if (reversed != (chosenArea > 0))
            std::swap(l.p1, l.p2);
        l.flags = (l.flags | ML_TWOSIDED) & ~1;
        for (int side = 0; side < 2; ++side) {
            l.side[side].ends = parent->def;
            l.side[side].sector = side ? parentId : newId;
            l.side[side].midtexture = "-";
            l.side[side].toptexture = l.side[side].bottomtexture = wall;
        }
        l.selected = 1;
        selected.push_back({index, 0});
    }
    connectSectors(next); // Validate before changing the document or its undo stack.
    world.checkpoint();
    static_cast<WorldState &>(world) = static_cast<const WorldState &>(next);
    return selected;
}
int joinAdjacentSectors(EditWorld &world) {
    auto next = world;
    const auto original = world.serialize();
    int changes = 0;
    // Split collinear overlaps and T junctions at existing endpoints first.
    std::vector<Vec2> endpoints;
    for (const auto &line : next.lines)
        if (line.selected >= 0) {
            endpoints.push_back(next.points[line.p1].pt);
            endpoints.push_back(next.points[line.p2].pt);
        }
    const size_t count = next.lines.size();
    for (size_t i = 0; i < count; ++i) {
        const auto line = next.lines[i];
        if (line.selected < 0)
            continue;
        const Vec2 a = next.points[line.p1].pt, b = next.points[line.p2].pt, d = b - a;
        const double length2 = double(d.x) * d.x + double(d.y) * d.y;
        if (!length2)
            continue;
        std::vector<std::pair<double, Vec2>> cuts{{0, a}, {1, b}};
        for (Vec2 p : endpoints) {
            const double t = ((double(p.x) - a.x) * d.x + (double(p.y) - a.y) * d.y) / length2;
            if (t > 0 && t < 1 &&
                std::abs(double(d.x) * (p.y - a.y) - double(d.y) * (p.x - a.x)) < .001)
                cuts.push_back({t, p});
        }
        std::sort(cuts.begin(), cuts.end(), [](auto &a, auto &b) { return a.first < b.first; });
        cuts.erase(std::unique(cuts.begin(), cuts.end(),
                               [](auto &a, auto &b) { return a.second == b.second; }),
                   cuts.end());
        if (cuts.size() == 2)
            continue;
        next.lines[i].p2 = next.newPoint(cuts[1].second);
        if (line.flags & 4)
            next.lines[i].side[1].firstcolumn +=
                int(std::lround((1 - cuts[1].first) * std::sqrt(length2)));
        for (size_t j = 2; j < cuts.size(); ++j) {
            auto segment = line;
            const int offset = int(std::lround(cuts[j - 1].first * std::sqrt(length2)));
            segment.side[0].firstcolumn += offset;
            if (line.flags & 4)
                segment.side[1].firstcolumn +=
                    int(std::lround((1 - cuts[j].first) * std::sqrt(length2)));
            next.newLine(cuts[j - 1].second, cuts[j].second, segment);
        }
        ++changes;
    }
    using Key = std::array<float, 4>;
    std::map<Key, int> unique;
    for (size_t i = 0; i < next.lines.size(); ++i) {
        auto &line = next.lines[i];
        if (line.selected < 0)
            continue;
        auto a = next.points[line.p1].pt, b = next.points[line.p2].pt;
        const bool reverse = std::make_pair(a.x, a.y) > std::make_pair(b.x, b.y);
        Key key = reverse ? Key{b.x, b.y, a.x, a.y} : Key{a.x, a.y, b.x, b.y};
        auto [entry, inserted] = unique.emplace(key, int(i));
        if (inserted)
            continue;
        auto &kept = next.lines[entry->second];
        const bool same = next.points[kept.p1].pt == a;
        if (!same && !(kept.flags & 4) && !(line.flags & 4)) {
            kept.side[1] = line.side[0];
            for (auto &side : kept.side) {
                if (side.toptexture == "-")
                    side.toptexture = side.midtexture;
                if (side.bottomtexture == "-")
                    side.bottomtexture = side.midtexture;
                side.midtexture = "-";
            }
            kept.flags = (kept.flags | 4) & ~1;
        } else {
            require(same && kept.side[0].ends == line.side[0].ends && !(line.flags & 4),
                    "Overlapping walls have conflicting sector data");
        }
        line.selected = -1;
        ++changes;
    }
    // Infer only on the candidate copy. Invalid/open drawings remain untouched.
    next.explicitSectors = false;
    std::vector<int> flags;
    for (const auto &line : next.lines)
        flags.push_back(line.flags);
    connectSectors(next, true);
    for (size_t i = 0; i < next.lines.size(); ++i)
        if (flags[i] != next.lines[i].flags)
            ++changes;
    if (!changes)
        return 0;
    next.explicitSectors = world.explicitSectors;
    if (next.serialize() == original)
        return 0;
    static_cast<WorldState &>(world) = static_cast<const WorldState &>(next);
    return std::max(changes, 1);
}
bool finishDrawnSector(EditWorld &world, int lastLine, bool closedLoop) {
    auto portals = [](const EditWorld &w) {
        return std::count_if(w.lines.begin(), w.lines.end(), [](const Line &line) {
            return line.selected >= 0 && (line.flags & ML_TWOSIDED);
        });
    };
    auto candidate = world;
    try {
        if (joinAdjacentSectors(candidate) && (closedLoop || portals(candidate) > portals(world))) {
            static_cast<WorldState &>(world) = static_cast<const WorldState &>(candidate);
            return true;
        }
    } catch (const std::exception &) {
        // The new path may still be open, or may be an isolated loop inside a sector.
    }
    if (!closedLoop || lastLine < 0 || lastLine >= int(world.lines.size()))
        return false;
    const auto &line = world.lines[lastLine];
    const Vec2 a = world.points[line.p1].pt, b = world.points[line.p2].pt, d = b - a;
    const float length = std::hypot(d.x, d.y);
    if (!length)
        return false;
    for (float sign : {1.f, -1.f}) {
        candidate = world;
        try {
            const auto selected = makeInnerSector(
                candidate, (a + b) * .5f + Vec2{d.y, -d.x} * (sign * .01f / length));
            if (std::none_of(selected.begin(), selected.end(),
                             [&](auto side) { return side.first == lastLine; }))
                continue;
            static_cast<WorldState &>(world) = static_cast<const WorldState &>(candidate);
            return true;
        } catch (const std::exception &) {
            // Try the other side for counterclockwise drawings.
        }
    }
    return false;
}
int lineByPoint(const EditWorld &w, Vec2 p, int &side) {
    float best = std::numeric_limits<float>::max();
    int found = -1;
    for (size_t i = 0; i < w.lines.size(); ++i) {
        auto &l = w.lines[i];
        if (l.selected < 0)
            continue;
        auto a = w.points[l.p1].pt, b = w.points[l.p2].pt;
        float d = distanceToLine(p, a, b);
        if (d < best) {
            best = d;
            found = int(i);
            side = cross(b - a, p - a) > 0 ? 1 : 0;
        }
    }
    return found;
}
std::vector<ConnectedSector> connectSectors(EditWorld &w, bool joinSharedWalls) {
    // Retail maps can deliberately contain self-referencing or isolated lines.
    // Keep their explicit sector identity instead of inferring it from polygons.
    if (w.explicitSectors) {
        std::map<int, int> ids;
        std::vector<ConnectedSector> sectors;
        EditWorld added;
        added.points = w.points;
        std::vector<int> addedIndices;
        for (size_t i = 0; i < w.lines.size(); ++i) {
            auto &l = w.lines[i];
            if (l.selected < 0)
                continue;
            if (l.side[0].sector < 0) {
                added.lines.push_back(l);
                addedIndices.push_back(int(i));
                continue;
            }
            for (int side = 0; side < ((l.flags & 4) ? 2 : 1); ++side) {
                auto &s = l.side[side];
                require(s.sector >= 0, "New back side requires a sector assignment");
                auto it = ids.find(s.sector);
                int id;
                if (it == ids.end()) {
                    id = int(sectors.size());
                    ids[s.sector] = id;
                    sectors.push_back({s.ends, {}});
                } else {
                    id = it->second;
                    require(sectors[id].def == s.ends,
                            "Sector definitions differ at line " + std::to_string(i) +
                                "; select the entire sector to edit its properties");
                }
                sectors[id].sides.push_back({int(i), side});
            }
        }
        if (!added.lines.empty()) {
            auto more = connectSectors(added);
            for (auto &s : more) {
                for (auto &pair : s.sides)
                    pair.first = addedIndices[pair.first];
                sectors.push_back(std::move(s));
            }
        }
        for (size_t i = 0; i < sectors.size(); ++i)
            for (auto [line, side] : sectors[i].sides)
                w.lines[line].side[side].sector = int(i);
        return sectors;
    }
    struct Edge {
        int a, b, line, side, next = -1, face = -1;
    };
    struct Face {
        std::vector<int> edges;
        std::vector<Vec2> polygon;
        double area = 0;
        int sector = -1;
    };
    std::vector<Edge> e;
    std::vector<Face> f;
    std::vector<std::vector<int>> outgoing(w.points.size());
    // Weld equal positions for topology without mutating editable vertices.
    std::map<std::pair<float, float>, int> canonical;
    std::vector<int> vertex(w.points.size());
    for (size_t i = 0; i < w.points.size(); ++i) {
        auto p = w.points[i].pt;
        auto r = canonical.emplace(std::make_pair(p.x, p.y), int(i));
        vertex[i] = r.first->second;
    }
    for (size_t i = 0; i < w.lines.size(); ++i) {
        auto &l = w.lines[i];
        if (l.selected < 0)
            continue;
        require(l.p1 >= 0 && l.p2 >= 0 && l.p1 < w.points.size() && l.p2 < w.points.size(),
                "Invalid line vertex");
        int a = vertex[l.p1], b = vertex[l.p2];
        require(a != b, "Zero-length line " + std::to_string(i));
        for (int s = 0; s < 2; ++s) {
            int n = int(e.size());
            e.push_back({s ? b : a, s ? a : b, int(i), s});
            outgoing[e.back().a].push_back(n);
        }
    }
    for (size_t i = 0; i < e.size(); ++i) {
        auto &h = e[i];
        auto p = w.points[h.b].pt;
        auto reverse = w.points[h.a].pt - p;
        float base = std::atan2(reverse.y, reverse.x), best = 100;
        for (int candidate : outgoing[h.b]) {
            if (candidate == (int(i) ^ 1))
                continue;
            auto d = w.points[e[candidate].b].pt - p;
            float angle = std::atan2(d.y, d.x) - base;
            while (angle <= 0)
                angle += 6.283185307f;
            if (angle < best) {
                best = angle;
                h.next = candidate;
            }
        }
        if (h.next < 0)
            h.next = int(i) ^ 1;
        require(h.next >= 0, "Open boundary at line " + std::to_string(h.line));
    }
    for (size_t i = 0; i < e.size(); ++i)
        if (e[i].face < 0) {
            Face face;
            int n = int(i), id = int(f.size());
            do {
                require(e[n].face < 0, "Intersecting or overlapping boundary near line " +
                                           std::to_string(e[n].line));
                e[n].face = id;
                face.edges.push_back(n);
                auto a = w.points[e[n].a].pt, b = w.points[e[n].b].pt;
                face.polygon.push_back(a);
                face.area += double(a.x) * b.y - double(b.x) * a.y;
                n = e[n].next;
            } while (n != int(i));
            f.push_back(std::move(face));
        }
    std::vector<ConnectedSector> sectors;
    for (auto &face : f)
        if (face.area < 0) {
            bool occupied = false;
            for (int index : face.edges) {
                auto &h = e[index];
                if (h.side == 0 || (w.lines[h.line].flags & 4))
                    occupied = true;
            }
            if (occupied) {
                face.sector = int(sectors.size());
                sectors.push_back({});
            }
        }
    for (auto &face : f)
        if (face.area >= 0) {
            auto &h = e[face.edges[0]];
            auto a = w.points[h.a].pt, b = w.points[h.b].pt, d = b - a;
            float length = std::hypot(d.x, d.y);
            Vec2 sample = (a + b) * .5f + Vec2{d.y / length, -d.x / length} * .01f;
            double smallest = std::numeric_limits<double>::max();
            for (auto &container : f)
                if (container.area < 0 && container.sector >= 0 && -container.area < smallest &&
                    pointInPolygon(sample, container.polygon)) {
                    smallest = -container.area;
                    face.sector = container.sector;
                }
        }
    if (joinSharedWalls) {
        std::vector<int> shared;
        for (size_t i = 0; i < e.size(); i += 2) {
            const auto &front = f[e[i].face], &back = f[e[i + 1].face];
            // A containing sector may be represented by a positive-area hole face.
            // It is still a valid neighbor of the newly enclosed inner sector.
            if (front.sector >= 0 && back.sector >= 0 && front.sector != back.sector &&
                e[i].face != e[i + 1].face && !(w.lines[e[i].line].flags & 4))
                shared.push_back(int(i));
        }
        std::set<int> sharedLines;
        for (int i : shared)
            sharedLines.insert(e[i].line);
        for (int i : shared) {
            auto &line = w.lines[e[i].line];
            const auto wall = line.side[0].midtexture;
            for (int side = 0; side < 2; ++side) {
                const auto &face = f[e[i + side].face];
                bool found = false;
                for (int index : face.edges) {
                    const auto &edge = e[index];
                    const auto &source = w.lines[edge.line];
                    if (!sharedLines.count(edge.line) && (edge.side == 0 || (source.flags & 4))) {
                        line.side[side].ends = source.side[edge.side].ends;
                        found = true;
                        break;
                    }
                }
                require(found, "Shared wall has no surrounding sector definition");
                auto &dest = line.side[side];
                if (dest.toptexture == "-")
                    dest.toptexture = wall;
                if (dest.bottomtexture == "-")
                    dest.bottomtexture = wall;
                dest.midtexture = "-";
            }
            line.flags = (line.flags | 4) & ~1;
        }
    }
    for (auto &face : f)
        for (int index : face.edges) {
            auto &h = e[index];
            auto &l = w.lines[h.line];
            bool exists = h.side == 0 || (l.flags & 4);
            if (face.sector < 0) {
                require(!exists, "Line " + std::to_string(h.line) +
                                     " faces unbounded space (flip the line or close the sector)");
                continue;
            }
            if (!exists && e[index ^ 1].face == h.face)
                continue;
            require(exists,
                    "Inside of sector meets the back of one-sided line " + std::to_string(h.line));
            auto &s = sectors[face.sector];
            auto &side = l.side[h.side];
            if (s.sides.empty())
                s.def = side.ends;
            else
                require(s.def == side.ends,
                        "Sector definitions differ at line " + std::to_string(h.line));
            s.sides.push_back({h.line, h.side});
            side.sector = face.sector;
        }
    return sectors;
}
} // namespace de
