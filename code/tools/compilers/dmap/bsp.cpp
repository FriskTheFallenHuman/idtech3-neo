// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../../qcommon/doom_actions.h"
#include "doommap.h"
#include "../CompilerRuntime.h"

namespace dm {
namespace {
struct Bounds {
    Vec3 min{1e30, 1e30, 1e30}, max{-1e30, -1e30, -1e30};
    void add(Vec3 p) {
        min = {std::min(min.x, p.x), std::min(min.y, p.y), std::min(min.z, p.z)};
        max = {std::max(max.x, p.x), std::max(max.y, p.y), std::max(max.z, p.z)};
    }
    void merge(const Bounds &b) {
        add(b.min);
        add(b.max);
    }
};
std::string number(double n) {
    std::ostringstream s;
    s.precision(10);
    s << n;
    return s.str();
}
using Entity = std::map<std::string, std::string>;
struct Builder {
    Map &map;
    Assets &assets;
    std::vector<dbsp::Shader> shaders;
    std::map<std::string, int> shaderIds;
    std::vector<dbsp::Plane> planes;
    std::map<std::array<int64_t, 4>, int> planeIds;
    std::vector<dbsp::Surface> surfaces;
    std::vector<dbsp::SurfaceSector> surfaceSectors;
    std::vector<dbsp::Vertex> verts;
    std::vector<int32_t> indexes;
    std::vector<dbsp::Brush> brushes;
    std::vector<dbsp::BrushSide> brushSides;
    std::vector<dbsp::Model> models;
    std::vector<dbsp::Node> nodes;
    std::vector<dbsp::Leaf> leaves;
    std::vector<int32_t> leafSurfaces, leafBrushes;
    std::vector<Bounds> surfaceBounds, brushBounds;
    std::vector<Entity> entities;
    Bounds bounds;
    double bottom = 0, top = 0;
    int shader(const std::string &name, int contents = 1, int flags = 0) {
        auto key = name + ":" + std::to_string(contents) + ":" + std::to_string(flags);
        auto it = shaderIds.find(key);
        if (it != shaderIds.end())
            return it->second;
        require(name.size() < 64, "Shader name exceeds BSP limit");
        dbsp::Shader s{};
        std::copy(name.begin(), name.end(), s.name);
        s.contents = contents;
        s.surfaceFlags = flags;
        int id = int(shaders.size());
        shaders.push_back(s);
        shaderIds[key] = id;
        return id;
    }
    int plane(Vec3 n, double d) {
        std::array<int64_t, 4> key{{llround(n.x * 1000000), llround(n.y * 1000000),
                                    llround(n.z * 1000000), llround(d * 100000)}};
        auto it = planeIds.find(key);
        if (it != planeIds.end())
            return it->second;
        int id = int(planes.size());
        planes.push_back({{float(n.x), float(n.y), float(n.z)}, float(d)});
        planes.push_back({{float(-n.x), float(-n.y), float(-n.z)}, float(-d)});
        planeIds[key] = id;
        for (auto &v : key)
            v = -v;
        planeIds[key] = id + 1;
        return id;
    }
    int model() {
        dbsp::Model m{};
        m.firstSurface = int(surfaces.size());
        m.firstBrush = int(brushes.size());
        models.push_back(m);
        return int(models.size() - 1);
    }
    void endModel(int id) {
        auto &m = models[id];
        m.numSurfaces = int(surfaces.size()) - m.firstSurface;
        m.numBrushes = int(brushes.size()) - m.firstBrush;
        Bounds b;
        for (int i = 0; i < m.numSurfaces; ++i)
            b.merge(surfaceBounds[m.firstSurface + i]);
        for (int i = 0; i < m.numBrushes; ++i)
            b.merge(brushBounds[m.firstBrush + i]);
        if (b.min.x > b.max.x) {
            b.add({0, 0, 0});
        }
        m.mins[0] = float(b.min.x);
        m.mins[1] = float(b.min.y);
        m.mins[2] = float(b.min.z);
        m.maxs[0] = float(b.max.x);
        m.maxs[1] = float(b.max.y);
        m.maxs[2] = float(b.max.z);
        if (id == 0)
            bounds = b;
    }
    void surface(const std::vector<Vec3> &p, const std::vector<Vec2> &uv, Vec3 normal,
                 const std::string &texture, int sector, int line, int kind) {
        if (p.size() < 3 || texture.empty())
            return;
        auto &sdef = map.sectors[sector].def;
        dbsp::Surface s{};
        s.shader = shader(texture, 0);
        s.fog = -1;
        s.type = 3;
        s.firstVertex = int(verts.size());
        s.numVertices = int(p.size());
        s.firstIndex = int(indexes.size());
        s.numIndices = int((p.size() - 2) * 3);
        s.lightmap = -3;
        s.lightmapVectors[2][0] = float(normal.x);
        s.lightmapVectors[2][1] = float(normal.y);
        s.lightmapVectors[2][2] = float(normal.z);
        Bounds b;
        for (size_t i = 0; i < p.size(); ++i) {
            dbsp::Vertex v{};
            v.xyz[0] = float(p[i].x);
            v.xyz[1] = float(p[i].y);
            v.xyz[2] = float(p[i].z);
            v.st[0] = float(uv[i].x);
            v.st[1] = float(uv[i].y);
            v.normal[0] = float(normal.x);
            v.normal[1] = float(normal.y);
            v.normal[2] = float(normal.z);
            v.color[0] = v.color[1] = v.color[2] = uint8_t(sdef.light);
            v.color[3] = 255;
            verts.push_back(v);
            b.add(p[i]);
        }
        for (size_t i = 1; i + 1 < p.size(); ++i) {
            indexes.push_back(0);
            indexes.push_back(int(i));
            indexes.push_back(int(i + 1));
        }
        surfaces.push_back(s);
        surfaceBounds.push_back(b);
        surfaceSectors.push_back({sector, line, kind, 0});
    }
    void flat(const Cell &c, double z, bool ceiling) {
        auto &sec = map.sectors[c.sector];
        auto name = ceiling ? sec.def.ceilTex : sec.def.floorTex;
        auto tex = assets.texture(name, true);
        std::vector<Vec3> p;
        std::vector<Vec2> uv;
        auto poly = c.polygon;
        if (ceiling)
            std::reverse(poly.begin(), poly.end());
        for (auto q : poly) {
            p.push_back({q.x, q.y, z});
            uv.push_back({q.x / 64, -q.y / 64});
        }
        surface(p, uv, {0, 0, ceiling ? -1.0 : 1.0}, tex, c.sector, -1,
                ceiling ? dbsp::Ceiling : dbsp::Floor);
    }
    void prism(std::vector<Vec2> poly, double lo, double hi, int contents = 1) {
        if (hi - lo < 0.001 || poly.size() < 3)
            return;
        Bounds b;
        for (auto p : poly) {
            b.add({p.x, p.y, lo});
            b.add({p.x, p.y, hi});
        }
        dbsp::Brush br{int(brushSides.size()), 0, shader("textures/common/caulk", contents)};
        auto add = [&](Vec3 n, double d) { brushSides.push_back({plane(n, d), br.shader}); };
        // Engine CM_BoundBrush requires the six axial bounds in precisely this order.
        add({-1, 0, 0}, -b.min.x);
        add({1, 0, 0}, b.max.x);
        add({0, -1, 0}, -b.min.y);
        add({0, 1, 0}, b.max.y);
        add({0, 0, -1}, -lo);
        add({0, 0, 1}, hi);
        for (size_t i = 0; i < poly.size(); ++i) {
            Vec2 a = poly[i], d = poly[(i + 1) % poly.size()] - a;
            double len = length(d);
            if (len < 1e-6)
                continue;
            Vec3 n{d.y / len, -d.x / len, 0};
            add(n, n.x * a.x + n.y * a.y);
        }
        br.numSides = int(brushSides.size()) - br.firstSide;
        brushes.push_back(br);
        brushBounds.push_back(b);
    }
    void wall(const Line &l, int line, int side, double lo, double hi, const std::string &name,
              int kind = dbsp::Wall) {
        if (hi - lo < 0.001 || name == "-" || name.empty())
            return;
        auto &sd = map.sides[l.sides[side]];
        Vec2 a = side ? l.b : l.a, b = side ? l.a : l.b, d = b - a;
        double len = length(d);
        auto tex = assets.texture(name, false);
        if (l.special == 48) {
            assets.scrolling.insert(name);
            tex += "_scroll";
        }
        auto &im = assets.images.at(name);
        double anchor = hi;
        if (kind == dbsp::MaskedWall) {
            if (l.flags & 16)
                anchor = lo + im.height;
        } else if (l.sides[1 - side] >= 0 && name == sd.top)
            anchor = (l.flags & 8) ? hi : lo + im.height;
        else if (l.sides[1 - side] >= 0 && name == sd.bottom)
            anchor = (l.flags & 16) ? map.sectors[sd.sector].def.ceiling : hi;
        else if (l.flags & 16)
            anchor = lo + im.height;
        double u = sd.xOffset / double(im.width), v = (anchor + sd.yOffset) / im.height;
        surface({{a.x, a.y, lo}, {b.x, b.y, lo}, {b.x, b.y, hi}, {a.x, a.y, hi}},
                {{u, v - lo / im.height},
                 {u + len / im.width, v - lo / im.height},
                 {u + len / im.width, v - hi / im.height},
                 {u, v - hi / im.height}},
                {d.y / len, -d.x / len, 0}, tex, sd.sector, line, kind);
    }
    std::vector<Vec2> lineBox(const Line &l, double thickness) {
        Vec2 d = l.b - l.a, n{d.y / length(d) * thickness, -d.x / length(d) * thickness};
        return {l.a + n, l.b + n, l.b - n, l.a - n};
    }
    int tree(Bounds region, const std::vector<int> &sf, const std::vector<int> &br, int depth) {
        compilerCheckCancelled();
        if (depth > 0 && ((sf.size() < 48 && br.size() < 48) || depth >= 12)) {
            dbsp::Leaf l{};
            l.cluster = 0;
            l.area = 0;
            l.mins[0] = int(floor(region.min.x));
            l.mins[1] = int(floor(region.min.y));
            l.mins[2] = int(floor(region.min.z));
            l.maxs[0] = int(ceil(region.max.x));
            l.maxs[1] = int(ceil(region.max.y));
            l.maxs[2] = int(ceil(region.max.z));
            l.firstSurface = int(leafSurfaces.size());
            l.numSurfaces = int(sf.size());
            l.firstBrush = int(leafBrushes.size());
            l.numBrushes = int(br.size());
            leafSurfaces.insert(leafSurfaces.end(), sf.begin(), sf.end());
            leafBrushes.insert(leafBrushes.end(), br.begin(), br.end());
            leaves.push_back(l);
            return -int(leaves.size());
        }
        int axis = (region.max.x - region.min.x) > (region.max.y - region.min.y) ? 0 : 1;
        double cut = axis ? (region.min.y + region.max.y) / 2 : (region.min.x + region.max.x) / 2;
        Vec3 n{};
        if (axis)
            n.y = 1;
        else
            n.x = 1;
        int id = int(nodes.size());
        nodes.push_back({});
        dbsp::Node node{};
        node.plane = plane(n, cut);
        for (int j = 0; j < 3; ++j) {
            node.mins[j] = int(floor(j == 0 ? region.min.x : j == 1 ? region.min.y : region.min.z));
            node.maxs[j] = int(ceil(j == 0 ? region.max.x : j == 1 ? region.max.y : region.max.z));
        }
        for (int side = 0; side < 2; ++side) {
            Bounds r = region;
            if (axis) {
                if (side)
                    r.max.y = cut;
                else
                    r.min.y = cut;
            } else {
                if (side)
                    r.max.x = cut;
                else
                    r.min.x = cut;
            }
            std::vector<int> s, b;
            auto overlaps = [&](const Bounds &v) {
                return side ? (axis ? v.min.y : v.min.x) <= cut + 0.01
                            : (axis ? v.max.y : v.max.x) >= cut - 0.01;
            };
            for (int i : sf)
                if (overlaps(surfaceBounds[i]))
                    s.push_back(i);
            for (int i : br)
                if (overlaps(brushBounds[i]))
                    b.push_back(i);
            node.children[side] = tree(r, s, b, depth + 1);
        }
        nodes[id] = node;
        return id;
    }
    void entity(Entity e) { entities.push_back(std::move(e)); }
};
template <class T> Bytes bytes(const std::vector<T> &v) {
    Bytes b(v.size() * sizeof(T));
    if (!b.empty())
        memcpy(b.data(), v.data(), b.size());
    return b;
}
void copyName(char *dest, const std::string &s) {
    require(s.size() < 12, "Doom name overflow");
    memcpy(dest, s.c_str(), s.size() + 1);
}
} // namespace

CompileResult compile(Map &m, Assets &assets, const fs::path &output) {
    for (auto &side : m.sides) {
        assets.texture(side.top, false);
        assets.texture(side.bottom, false);
        assets.texture(side.middle, false);
        assets.texture(side.def.floorTex, true);
        assets.texture(side.def.ceilTex, true);
    }
    Builder b{m, assets};
    double minFloor = 1e30, maxCeil = -1e30;
    for (auto &sec : m.sectors) {
        minFloor = std::min(minFloor, double(sec.def.floor));
        maxCeil = std::max(maxCeil, double(sec.def.ceiling));
        sec.minFloor = sec.maxFloor = sec.def.floor;
        sec.minCeil = sec.neighbors.empty() ? sec.def.ceiling : 1e30;
        sec.maxCeil = sec.neighbors.empty() ? sec.def.ceiling : -1e30;
        sec.maxFloor = sec.neighbors.empty() ? sec.def.floor : -1e30;
        for (int n : sec.neighbors) {
            auto &d = m.sectors[n].def;
            sec.minFloor = std::min(sec.minFloor, double(d.floor));
            sec.maxFloor = std::max(sec.maxFloor, double(d.floor));
            sec.minCeil = std::min(sec.minCeil, double(d.ceiling));
            sec.maxCeil = std::max(sec.maxCeil, double(d.ceiling));
        }
    }
    b.bottom = minFloor - 128;
    b.top = maxCeil + 128;
    std::vector<bool> floorMoves(m.sectors.size()), ceilMoves(m.sectors.size());
    for (auto &l : m.lines) {
        auto a = doom::action(l.special);
        if (l.special && a.kind == doom::None)
            m.warnings.push_back("Unrecognized linedef special " + std::to_string(l.special) +
                                 " retained in metadata");
        for (size_t i = 0; i < m.sectors.size(); ++i) {
            bool match = a.local ? (l.sides[1] >= 0 && m.sides[l.sides[1]].sector == int(i))
                                 : (l.tag > 0 && m.sectors[i].def.tag == l.tag);
            if (match) {
                floorMoves[i] = floorMoves[i] || doom::floorMoves(a.kind);
                ceilMoves[i] = ceilMoves[i] || doom::ceilingMoves(a.kind);
            }
        }
    }
    // Stairs and donuts move connected untagged sectors as well as the tagged root.
    for (auto &trigger : m.lines) {
        auto action = doom::action(trigger.special);
        bool stairs = action.kind == doom::Stairs8 || action.kind == doom::Stairs16;
        if ((!stairs && action.kind != doom::Donut) || trigger.tag <= 0)
            continue;
        for (size_t root = 0; root < m.sectors.size(); ++root) {
            if (m.sectors[root].def.tag != trigger.tag)
                continue;
            if (stairs) {
                int current = int(root);
                double height = m.sectors[root].def.floor + (action.kind == doom::Stairs8 ? 8 : 16);
                std::set<int> seen;
                while (current >= 0 && !seen.count(current)) {
                    seen.insert(current);
                    auto &sec = m.sectors[current];
                    floorMoves[current] = true;
                    sec.linkedTag = trigger.tag;
                    sec.stairHeight = height;
                    int next = -1;
                    for (auto &edge : m.lines) {
                        if (edge.sides[1] < 0 || m.sides[edge.sides[0]].sector != current)
                            continue;
                        int other = m.sides[edge.sides[1]].sector;
                        if (!seen.count(other) &&
                            m.sectors[other].def.floorTex == m.sectors[root].def.floorTex) {
                            next = other;
                            break;
                        }
                    }
                    current = next;
                    height += action.kind == doom::Stairs8 ? 8 : 16;
                }
            } else if (!m.sectors[root].neighbors.empty()) {
                int ring = m.sectors[root].neighbors.front(), outer = -1;
                for (int n : m.sectors[ring].neighbors)
                    if (n != int(root)) {
                        outer = n;
                        break;
                    }
                if (outer >= 0)
                    for (int n : {int(root), ring}) {
                        floorMoves[n] = true;
                        m.sectors[n].linkedTag = trigger.tag;
                        m.sectors[n].donutHeight = m.sectors[outer].def.floor;
                    }
            }
        }
    }
    // Sector timers and executable boss sectors also need movable geometry.
    for (size_t i = 0; i < m.sectors.size(); ++i) {
        if (m.name == "e1m8" && m.sectors[i].def.tag == 666) {
            floorMoves[i] = true;
        }
        if (m.sectors[i].def.special == 10 || m.sectors[i].def.special == 14)
            ceilMoves[i] = true;
    }
    int world = b.model();
    for (auto &c : m.cells) {
        auto &d = m.sectors[c.sector].def;
        if (!floorMoves[c.sector]) {
            b.flat(c, d.floor, false);
            b.prism(c.polygon, b.bottom, d.floor);
        }
        if (!ceilMoves[c.sector]) {
            // A sky is a portal, not a finite ceiling image. Keep collision at
            // the original height while closing its visible volume above walls.
            b.flat(c, d.ceilTex == "F_SKY1" ? b.top : d.ceiling, true);
            b.prism(c.polygon, d.ceiling, b.top);
        }
    }
    for (size_t i = 0; i < m.lines.size(); ++i) {
        auto &l = m.lines[i];
        for (int k = 0; k < 2; ++k) {
            if (l.sides[k] < 0)
                continue;
            auto &side = m.sides[l.sides[k]];
            auto &d = side.def;
            int back = l.sides[1 - k];
            if (d.ceilTex == "F_SKY1" && (back < 0 || m.sides[back].def.ceilTex != "F_SKY1")) {
                const Vec2 a = k ? l.b : l.a, end = k ? l.a : l.b;
                const Vec2 direction = end - a;
                const double distance = length(direction);
                b.surface({{a.x, a.y, double(d.ceiling)},
                           {end.x, end.y, double(d.ceiling)},
                           {end.x, end.y, b.top},
                           {a.x, a.y, b.top}},
                          {{0, 0}, {1, 0}, {1, 1}, {0, 1}},
                          {direction.y / distance, -direction.x / distance, 0},
                          assets.texture("F_SKY1", true), side.sector, int(i), dbsp::Ceiling);
            }
            if (back < 0) {
                // A closed door has floor == ceiling. Its jambs still need
                // stationary walls covering the entire future opening.
                auto &sec = m.sectors[side.sector];
                double lo = floorMoves[side.sector] ? sec.minFloor : d.floor;
                double hi =
                    ceilMoves[side.sector] ? std::max(sec.maxCeil, double(d.ceiling)) : d.ceiling;
                b.wall(l, int(i), k, lo, hi, side.middle);
                b.prism(b.lineBox(l, 1), b.bottom, b.top);
                continue;
            }
            auto &other = m.sides[back];
            auto &e = other.def;
            if (e.ceiling < d.ceiling && !ceilMoves[other.sector] &&
                !(e.ceilTex == "F_SKY1" && d.ceilTex == "F_SKY1"))
                b.wall(l, int(i), k, e.ceiling, d.ceiling, side.top);
            // Shaft walls are stationary and must cover the lift's lowest stop,
            // including lifts that start flush with the neighboring floor.
            const double shaftFloor =
                floorMoves[side.sector] ? m.sectors[side.sector].minFloor : d.floor;
            if (e.floor > shaftFloor && !floorMoves[other.sector]) {
                b.wall(l, int(i), k, shaftFloor, e.floor, side.bottom);
            }
            if (side.middle != "-") {
                double lo = std::max(d.floor, e.floor), hi = std::min(d.ceiling, e.ceiling);
                b.wall(l, int(i), k, lo, hi, side.middle, dbsp::MaskedWall);
            }
            if (k == 0 && (l.flags & 2) && !(l.flags & 1))
                b.prism(b.lineBox(l, 0.25), std::max(d.floor, e.floor),
                        std::min(d.ceiling, e.ceiling), 0x20000);
            if (k == 0 && (l.flags & 1))
                b.prism(b.lineBox(l, 0.25), std::max(d.floor, e.floor),
                        std::min(d.ceiling, e.ceiling));
        }
    }
    b.endModel(world);
    b.entity({{"classname", "worldspawn"},
              {"message", m.name},
              {"doom_format", "1"},
              {"doom_units", "1"},
              {"gridsize", "128 128 128"}});
    for (size_t si = 0; si < m.sectors.size(); ++si) {
        auto &sec = m.sectors[si];
        auto &d = sec.def;
        b.entity({{"classname", "doom_sector"},
                  {"doom_sector", std::to_string(si)},
                  {"doom_tag", std::to_string(d.tag)},
                  {"doom_special", std::to_string(d.special)},
                  {"doom_light", std::to_string(d.light)},
                  {"doom_lightmax", std::to_string([&]() {
                       int light = d.light;
                       for (int n : sec.neighbors)
                           light = std::max(light, m.sectors[n].def.light);
                       return light;
                   }())},
                  {"doom_lightmin", std::to_string([&]() {
                       int light = d.light;
                       for (int n : sec.neighbors)
                           light = std::min(light, m.sectors[n].def.light);
                       return light;
                   }())}});
        for (int ceiling = 0; ceiling < 2; ++ceiling) {
            if (!(ceiling ? ceilMoves[si] : floorMoves[si]) || sec.cells.empty())
                continue;
            int model = b.model();
            (ceiling ? sec.ceilModel : sec.floorModel) = model;
            double lo = ceiling ? d.ceiling : b.bottom, hi = ceiling ? b.top : d.floor;
            for (int ci : sec.cells) {
                auto &c = m.cells[ci];
                b.flat(c, ceiling ? d.ceiling : d.floor, ceiling != 0);
                b.prism(c.polygon, lo, hi);
            }
            for (size_t li = 0; li < m.lines.size(); ++li) {
                auto &l = m.lines[li];
                for (int k = 0; k < 2; ++k) {
                    if (l.sides[k] < 0 || m.sides[l.sides[k]].sector != int(si))
                        continue;
                    auto &side = m.sides[l.sides[k]];
                    int back = l.sides[1 - k];
                    if (back < 0)
                        continue; // One-sided shaft/jamb walls belong to the static world.
                    if (back >= 0 && m.sides[back].sector == int(si))
                        continue;
                    auto &facing = m.sides[back];
                    std::string tex = ceiling ? facing.top : facing.bottom;
                    if (tex == "-")
                        tex = facing.middle;
                    if (tex == "-")
                        tex = side.middle;
                    if (tex == "-")
                        continue;
                    b.wall(l, int(li), 1 - k, ceiling ? d.ceiling : sec.minFloor - 64,
                           ceiling ? std::max(sec.maxCeil, double(d.ceiling + 4)) : double(d.floor),
                           tex, dbsp::Wall);
                }
            }
            b.endModel(model);
            double next = double(d.floor);
            for (int n : sec.neighbors) {
                int h = m.sectors[n].def.floor;
                if (h > d.floor && (next == d.floor || h < next))
                    next = h;
            }
            b.entity({{"classname", "doom_mover"},
                      {"model", "*" + std::to_string(model)},
                      {"doom_sector", std::to_string(si)},
                      {"doom_tag", std::to_string(d.tag)},
                      {"doom_plane", ceiling ? "1" : "0"},
                      {"doom_floor", number(d.floor)},
                      {"doom_ceiling", number(d.ceiling)},
                      {"doom_linkedtag", std::to_string(sec.linkedTag)},
                      {"doom_stairheight", number(sec.stairHeight)},
                      {"doom_donutheight", number(sec.donutHeight)},
                      {"doom_textureheight", number([&]() {
                           int height = 32767;
                           for (auto &line : m.lines)
                               for (int sid : line.sides)
                                   if (sid >= 0 && m.sides[sid].sector == int(si) &&
                                       m.sides[sid].bottom != "-")
                                       height = std::min(
                                           height, assets.images.at(m.sides[sid].bottom).height);
                           return height == 32767 ? 64 : height;
                       }())},
                      {"doom_lowfloor", number(sec.minFloor)},
                      {"doom_highfloor", number(sec.maxFloor)},
                      {"doom_nextfloor", number(next)},
                      {"doom_lowceil", number(sec.minCeil)},
                      {"doom_highceil", number(sec.maxCeil)},
                      {"doom_special", std::to_string(d.special)}});
        }
    }
    for (size_t li = 0; li < m.lines.size(); ++li) {
        auto &l = m.lines[li];
        auto a = doom::action(l.special);
        if (!l.special || a.kind == doom::Scroll)
            continue;
        int back = l.sides[1] < 0 ? -1 : m.sides[l.sides[1]].sector;
        auto &front = m.sides[l.sides[0]];
        auto isSwitch = [](const std::string &name) {
            return name.size() > 3 && (name.substr(0, 3) == "SW1" || name.substr(0, 3) == "SW2");
        };
        b.entity(
            {{"classname", "doom_line"},
             {"doom_line", std::to_string(li)},
             {"doom_special", std::to_string(l.special)},
             {"doom_switch",
              isSwitch(front.top) || isSwitch(front.bottom) || isSwitch(front.middle) ? "1" : "0"},
             {"doom_tag", std::to_string(l.tag)},
             {"doom_sector", std::to_string(back)},
             {"doom_a", number(l.a.x) + " " + number(l.a.y) + " 0"},
             {"doom_b", number(l.b.x) + " " + number(l.b.y) + " 0"},
             {"doom_minz", number(minFloor)},
             {"doom_maxz", number(maxCeil)}});
    }
    for (auto &t : m.things) {
        if (t.sector < 0)
            continue;
        auto &d = m.sectors[t.sector].def;
        std::string origin = std::to_string(t.x) + " " + std::to_string(t.y) + " " +
                             std::to_string(d.floor + (t.type >= 1 && t.type <= 4 ? 25 : 0));
        if ((t.type >= 1 && t.type <= 4) || t.type == 11)
            b.entity({{"classname", t.type == 11 ? "info_player_deathmatch" : "info_player_start"},
                      {"origin", origin},
                      {"angle", std::to_string(t.angle)},
                      {"doom_type", std::to_string(t.type)}});
        if (t.type == 5 || t.type == 6 || t.type == 13 || t.type == 38 || t.type == 39 ||
            t.type == 40) {
            int frame = t.type == 5    ? 0
                        : t.type == 6  ? 1
                        : t.type == 13 ? 2
                        : t.type == 39 ? 3
                        : t.type == 40 ? 4
                                       : 5;
            const char *names[] = {"BKEYA0", "YKEYA0", "RKEYA0", "BSKUA0", "YSKUA0", "RSKUA0"};
            assets.images[names[frame]] = assets.wad.sprite(names[frame]);
            assets.flats[names[frame]] = false;
            b.entity({{"classname", "doom_key"},
                      {"origin", std::to_string(t.x) + " " + std::to_string(t.y) + " " +
                                     std::to_string(d.floor + 16)},
                      {"doom_key", std::to_string(frame % 3 + 1)},
                      {"doom_frame", std::to_string(frame)}});
        }
        if (t.type == 14)
            b.entity({{"classname", "info_notnull"},
                      {"origin", origin},
                      {"angle", std::to_string(t.angle)},
                      {"targetname", "doom_teleport_" + std::to_string(d.tag)}});
    }
    // Preserve every original record, including things that do not yet have game actors.
    std::vector<dbsp::Sector> sectors;
    for (auto &s : m.sectors) {
        dbsp::Sector d{};
        d.floor = s.def.floor;
        d.ceiling = s.def.ceiling;
        d.light = s.def.light;
        d.special = s.def.special;
        d.tag = s.def.tag;
        d.floorModel = s.floorModel;
        d.ceilingModel = s.ceilModel;
        copyName(d.floorTexture, s.def.floorTex);
        copyName(d.ceilingTexture, s.def.ceilTex);
        d.bounds[0] = float(s.min.x);
        d.bounds[1] = float(s.min.y);
        d.bounds[2] = float(s.max.x);
        d.bounds[3] = float(s.max.y);
        sectors.push_back(d);
    }
    std::vector<dbsp::Line> lines;
    for (auto &l : m.lines)
        lines.push_back({{float(l.a.x), float(l.a.y)},
                         {float(l.b.x), float(l.b.y)},
                         l.flags,
                         l.special,
                         l.tag,
                         {l.sides[0], l.sides[1]}});
    std::vector<dbsp::Side> sides;
    for (auto &s : m.sides) {
        dbsp::Side d{};
        d.xOffset = s.xOffset;
        d.yOffset = s.yOffset;
        d.sector = s.sector;
        copyName(d.upper, s.top);
        copyName(d.lower, s.bottom);
        copyName(d.middle, s.middle);
        sides.push_back(d);
    }
    std::vector<int> sf(b.models[0].numSurfaces), br(b.models[0].numBrushes);
    std::iota(sf.begin(), sf.end(), 0);
    std::iota(br.begin(), br.end(), 0);
    b.tree(b.bounds, sf, br, 0);
    std::ostringstream ent;
    for (auto &e : b.entities) {
        ent << "{\n";
        for (auto &kv : e)
            ent << '"' << kv.first << "\" \"" << kv.second << "\"\n";
        ent << "}\n";
    }
    auto es = ent.str();
    es.push_back(0);
    require(b.entities.size() + 64 < 1022, "Map exceeds runtime entity capacity");
    require(es.size() < 0x40000 && b.models.size() <= 256 && b.shaders.size() < 1024 &&
                b.brushes.size() < 32768 && b.planes.size() < 131072 && b.verts.size() < 524288 &&
                b.indexes.size() < 524288,
            "Map exceeds engine BSP limits");
    std::array<Bytes, dbsp::LumpCount> lumps;
    lumps[dbsp::Entities] = Bytes(es.begin(), es.end());
    lumps[dbsp::Shaders] = bytes(b.shaders);
    lumps[dbsp::Planes] = bytes(b.planes);
    lumps[dbsp::Nodes] = bytes(b.nodes);
    lumps[dbsp::Leafs] = bytes(b.leaves);
    lumps[dbsp::LeafSurfaces] = bytes(b.leafSurfaces);
    lumps[dbsp::LeafBrushes] = bytes(b.leafBrushes);
    lumps[dbsp::Models] = bytes(b.models);
    lumps[dbsp::Brushes] = bytes(b.brushes);
    lumps[dbsp::BrushSides] = bytes(b.brushSides);
    lumps[dbsp::Vertices] = bytes(b.verts);
    lumps[dbsp::Indices] = bytes(b.indexes);
    lumps[dbsp::Surfaces] = bytes(b.surfaces);
    lumps[dbsp::Visibility] = {1, 0, 0, 0, 1, 0, 0, 0, 255};
    lumps[dbsp::Sectors] = bytes(sectors);
    lumps[dbsp::Lines] = bytes(lines);
    lumps[dbsp::Sides] = bytes(sides);
    lumps[dbsp::SurfaceSectors] = bytes(b.surfaceSectors);
    lumps[dbsp::Things] = bytes(m.things);
    dbsp::Header h{};
    h.ident = dbsp::Ident;
    h.version = dbsp::Version;
    Bytes out(sizeof(h));
    for (int i = 0; i < dbsp::LumpCount; ++i) {
        while (out.size() % 4)
            out.push_back(0);
        h.lumps[i] = {int(out.size()), int(lumps[i].size())};
        out.insert(out.end(), lumps[i].begin(), lumps[i].end());
    }
    memcpy(out.data(), &h, sizeof(h));
    const char *error = dbsp::validate(out.data(), int(out.size()));
    require(!error, error ? error : "");
    writeFile(output, out);
    return {int(sectors.size()),      int(lines.size()),      int(m.cells.size()),
            int(b.models.size()),     int(b.surfaces.size()), int(b.brushes.size()),
            int(assets.images.size())};
}
} // namespace dm
