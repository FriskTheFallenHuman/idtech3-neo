// SPDX-License-Identifier: GPL-3.0-or-later
#include "doommap.h"

namespace dm {
namespace {
struct Groups {
    std::vector<int> parent;
    explicit Groups(size_t n) : parent(n) { std::iota(parent.begin(), parent.end(), 0); }
    int root(int a) { return parent[a] == a ? a : parent[a] = root(parent[a]); }
    void join(int a, int b) {
        a = root(a);
        b = root(b);
        if (a != b)
            parent[b] = a;
    }
};
double lineY(const Line &l, double x) {
    return l.a.y + (l.b.y - l.a.y) * (x - l.a.x) / (l.b.x - l.a.x);
}
std::string nextRow(std::istream &f, int &number) {
    std::string s;
    while (std::getline(f, s)) {
        ++number;
        if (s.find_first_not_of(" \t\r") != std::string::npos)
            return s;
    }
    throw std::runtime_error("Unexpected end of DWD at line " + std::to_string(number));
}
bool inside(const Cell &c, Vec2 p) {
    for (size_t i = 0; i < c.polygon.size(); ++i)
        if (cross(c.polygon[(i + 1) % c.polygon.size()] - c.polygon[i], p - c.polygon[i]) < -0.001)
            return false;
    return true;
}
} // namespace
int Map::findSector(Vec2 p) const {
    int found = -1;
    for (auto &c : cells)
        if (inside(c, p)) {
            found = c.sector;
            if (sectors[found].def.ceiling > sectors[found].def.floor + 32)
                return found;
        }
    return found;
}

Map loadDwd(const fs::path &path, bool strict) {
    std::ifstream f(path);
    require(bool(f), "Cannot open DWD");
    return loadDwd(f, path.stem().string(), strict);
}
Map loadDwd(std::istream &f, const std::string &name, bool strict) {
    Map m;
    m.name = lower(name);
    require(!m.name.empty() && m.name.size() < 32 &&
                m.name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-") ==
                    std::string::npos,
            "Unsafe map name");
    int row = 0, version = 0, count = 0;
    auto s = nextRow(f, row);
    require(sscanf(s.c_str(), "WorldServer version %d", &version) == 1 && version == 4,
            "Expected WorldServer version 4");
    s = nextRow(f, row);
    require(sscanf(s.c_str(), "lines:%d", &count) == 1 && count > 0 && count <= 100000,
            "Invalid DWD line count");
    for (int i = 0; i < count; ++i) {
        Line l;
        s = nextRow(f, row);
        require(sscanf(s.c_str(), "(%lf,%lf) to (%lf,%lf) : %d : %d : %d", &l.a.x, &l.a.y, &l.b.x,
                       &l.b.y, &l.flags, &l.special, &l.tag) == 7,
                "Bad linedef at source line " + std::to_string(row));
        require(std::isfinite(l.a.x) && std::isfinite(l.a.y) && std::isfinite(l.b.x) &&
                    std::isfinite(l.b.y) && std::abs(l.a.x) < 120000 && std::abs(l.b.x) < 120000 &&
                    std::abs(l.a.y) < 120000 && std::abs(l.b.y) < 120000,
                "DWD coordinates out of bounds");
        for (int k = 0; k < ((l.flags & 4) ? 2 : 1); ++k) {
            Side side;
            char top[64], bottom[64], mid[64], floor[64], ceil[64];
            s = nextRow(f, row);
            require(sscanf(s.c_str(), " %d (%d : %63s / %63s / %63s )", &side.yOffset,
                           &side.xOffset, top, bottom, mid) == 5,
                    "Bad sidedef at source line " + std::to_string(row));
            s = nextRow(f, row);
            require(sscanf(s.c_str(), " %d : %63s %d : %63s %d %d %d", &side.def.floor, floor,
                           &side.def.ceiling, ceil, &side.def.light, &side.def.special,
                           &side.def.tag) == 7,
                    "Bad sector at source line " + std::to_string(row));
            side.top = upper(top);
            side.bottom = upper(bottom);
            side.middle = upper(mid);
            side.def.floorTex = upper(floor);
            side.def.ceilTex = upper(ceil);
            for (auto &name :
                 {side.top, side.bottom, side.middle, side.def.floorTex, side.def.ceilTex})
                require(name.size() <= 11 &&
                            name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-[]") ==
                                std::string::npos,
                        "Invalid Doom texture name: " + name);
            require(side.def.floor <= side.def.ceiling && side.def.light >= 0 &&
                        side.def.light <= 255,
                    "Invalid sector height/brightness at source line " + std::to_string(row));
            l.sides[k] = int(m.sides.size());
            m.sides.push_back(side);
        }
        if (length(l.b - l.a) < 0.0001) {
            m.warnings.push_back("Dropped zero-length linedef " + std::to_string(i));
            continue;
        }
        m.lines.push_back(l);
    }
    s = nextRow(f, row);
    require(sscanf(s.c_str(), "things:%d", &count) == 1 && count >= 0 && count <= 100000,
            "Invalid DWD thing count");
    for (int i = 0; i < count; ++i) {
        dbsp::Thing t{};
        s = nextRow(f, row);
        require(sscanf(s.c_str(), "(%d,%d, %d) :%d, %d", &t.x, &t.y, &t.angle, &t.type,
                       &t.options) == 5,
                "Bad thing at source line " + std::to_string(row));
        t.sector = -1;
        m.things.push_back(t);
    }
    while (std::getline(f, s))
        require(s.find_first_not_of(" \r\t") == std::string::npos, "Unexpected trailing DWD data");
    Groups groups(m.sides.size());
    // The original compiler groups equal sector definitions by connected geometry,
    // not by globally interning their height/texture/light tuples.
    std::map<std::pair<int64_t, int64_t>, std::vector<int>> vertices;
    std::vector<double> xs;
    for (auto &l : m.lines) {
        xs.push_back(l.a.x);
        xs.push_back(l.b.x);
        for (auto p : {l.a, l.b}) {
            auto &adjacent = vertices[{llround(p.x * 1024), llround(p.y * 1024)}];
            for (int side : l.sides)
                if (side >= 0) {
                    for (int other : adjacent)
                        if (m.sides[side].def == m.sides[other].def)
                            groups.join(side, other);
                    adjacent.push_back(side);
                }
        }
    }
    // Include genuine line intersections, so ordering cannot change inside a slab.
    for (size_t i = 0; i < m.lines.size(); ++i)
        for (size_t j = 0; j < i; ++j) {
            auto &a = m.lines[i];
            auto &b = m.lines[j];
            auto d = a.b - a.a, e = b.b - b.a;
            double den = cross(d, e);
            if (std::abs(den) < 1e-9)
                continue;
            double t = cross(b.a - a.a, e) / den, u = cross(b.a - a.a, d) / den;
            if (t > 1e-7 && t < 1 - 1e-7 && u > 1e-7 && u < 1 - 1e-7)
                xs.push_back(a.a.x + d.x * t);
        }
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end(),
                         [](double a, double b) { return std::abs(a - b) < 1e-7; }),
             xs.end());
    std::vector<int> previous;
    int inconsistent = 0;
    for (size_t slab = 1; slab < xs.size(); ++slab) {
        double a = xs[slab - 1], b = xs[slab], x = (a + b) / 2;
        std::vector<std::pair<double, int>> hits;
        for (size_t i = 0; i < m.lines.size(); ++i) {
            auto &l = m.lines[i];
            if (x > std::min(l.a.x, l.b.x) && x < std::max(l.a.x, l.b.x))
                hits.emplace_back(lineY(l, x), int(i));
        }
        std::sort(hits.begin(), hits.end());
        std::vector<int> current;
        for (size_t h = 1; h < hits.size(); ++h) {
            if (hits[h].first - hits[h - 1].first < 1e-6)
                continue;
            auto &low = m.lines[hits[h - 1].second];
            auto &high = m.lines[hits[h].second];
            int sid = low.sides[low.b.x > low.a.x ? 1 : 0],
                other = high.sides[high.b.x > high.a.x ? 0 : 1];
            if (sid < 0 && other < 0)
                continue;
            if (sid >= 0 && other >= 0 && m.sides[sid].def == m.sides[other].def)
                groups.join(sid, other);
            else {
                ++inconsistent;
                if (sid < 0)
                    sid = other;
            }
            Cell c;
            c.sector = sid;
            c.polygon = {
                {a, lineY(low, a)}, {b, lineY(low, b)}, {b, lineY(high, b)}, {a, lineY(high, a)}};
            std::vector<Vec2> clean;
            for (auto p : c.polygon)
                if (clean.empty() || length(p - clean.back()) > 1e-6)
                    clean.push_back(p);
            if (clean.size() > 1 && length(clean.front() - clean.back()) < 1e-6)
                clean.pop_back();
            if (clean.size() < 3)
                continue;
            c.polygon = clean;
            for (int prev : previous) {
                auto &pc = m.cells[prev];
                if (!(m.sides[pc.sector].def == m.sides[sid].def))
                    continue;
                double lo = 1e30, hi = -1e30, clo = 1e30, chi = -1e30;
                for (auto p : pc.polygon)
                    if (std::abs(p.x - a) < 1e-6) {
                        lo = std::min(lo, p.y);
                        hi = std::max(hi, p.y);
                    }
                for (auto p : c.polygon)
                    if (std::abs(p.x - a) < 1e-6) {
                        clo = std::min(clo, p.y);
                        chi = std::max(chi, p.y);
                    }
                if (std::min(hi, chi) > std::max(lo, clo) + 1e-6)
                    groups.join(pc.sector, sid);
            }
            current.push_back(int(m.cells.size()));
            m.cells.push_back(c);
        }
        previous = current;
    }
    if (inconsistent) {
        auto warning = std::to_string(inconsistent) +
                       " decomposition cells have inconsistent source sidedef sectors; lower "
                       "boundary takes precedence (use --strict to reject)";
        require(!strict, warning);
        m.warnings.push_back(warning);
    }
    require(!m.cells.empty(), "DWD has no enclosed sectors");
    std::map<int, int> sectorIds;
    for (auto &side : m.sides) {
        int root = groups.root(int(&side - m.sides.data()));
        auto it = sectorIds.find(root);
        if (it == sectorIds.end()) {
            int id = int(m.sectors.size());
            Sector sec;
            sec.def = side.def;
            m.sectors.push_back(sec);
            sectorIds[root] = id;
        }
        side.sector = sectorIds[root];
    }
    for (size_t i = 0; i < m.cells.size(); ++i) {
        auto &c = m.cells[i];
        c.sector = m.sides[c.sector].sector;
        auto &sec = m.sectors[c.sector];
        sec.cells.push_back(int(i));
        for (auto p : c.polygon) {
            sec.min.x = std::min(sec.min.x, p.x);
            sec.min.y = std::min(sec.min.y, p.y);
            sec.max.x = std::max(sec.max.x, p.x);
            sec.max.y = std::max(sec.max.y, p.y);
        }
    }
    for (auto &l : m.lines) {
        if (l.sides[1] < 0)
            continue;
        int a = m.sides[l.sides[0]].sector, b = m.sides[l.sides[1]].sector;
        if (a != b) {
            m.sectors[a].neighbors.push_back(b);
            m.sectors[b].neighbors.push_back(a);
        }
    }
    for (auto &t : m.things)
        t.sector = m.findSector({double(t.x), double(t.y)});
    for (auto &sec : m.sectors) {
        if (sec.cells.empty()) {
            sec.min = {0, 0};
            sec.max = {0, 0};
        }
        std::sort(sec.neighbors.begin(), sec.neighbors.end());
        sec.neighbors.erase(std::unique(sec.neighbors.begin(), sec.neighbors.end()),
                            sec.neighbors.end());
    }
    return m;
}
} // namespace dm
