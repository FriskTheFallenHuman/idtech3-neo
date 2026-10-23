// SPDX-License-Identifier: GPL-3.0-or-later
// DWD geometry, picking and camera controls. All GPU work goes through EditorRenderAPI.
#include "Viewport3D.h"
#include "AssetImages.h"
#include "ModelProps.h"
#include "formats/model_material.h"
#include "formats/wall_strip.h"
#include "formats/sprite_pack.h"
#include "formats/sprite_profiles.h"
#include "ViewportSprites.h"
#include "ThingPalette.h"
#include "NextUI.h"
#include "DesktopPanels.h"
#include "BlockWorld.h"
#include "idfunctions.h"
#include "../compilers/dmap/doommap.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>
namespace de {
namespace {
using UINT = unsigned int;
constexpr float pi = 3.14159265359f;
struct V3 {
    float x, y, z;
};
struct Vertex {
    V3 p;
    float u, v, light;
    V3 normal;
    float sky;
};
struct Material {
    ImTextureID texture = 0;
    std::vector<uint32_t> pixels;
    int width = 64, height = 64, left = 0, top = 0;
    int logicalWidth = 64, logicalHeight = 64;
    doom_materials::Emission emission;
};
struct Batch {
    std::string texture;
    UINT start, count;
    int kind = 0, index = -1, side = 0;
    bool modelFullbright = false;
    V3 center{};
    float radius = 0;
};
struct Edge {
    Vec2 a, b;
};
struct Face {
    SectorDef def;
    std::vector<std::pair<int, int>> sides;
    std::vector<Edge> edges;
};
std::map<std::string, Material> materials;
std::map<int, doom_sprites::Frame> hdFrames;
std::vector<Vertex> mesh, pickVertices;
std::vector<Batch> batches, pickBatches;
std::vector<Face> faces;
std::vector<Point> geometryPoints;
std::vector<Line> geometryLines;
std::string signature, resourceSignature, warning, cameraDocument;
bool geometryValid = false, fullbright = false, cameraSet = false, dragging = false;
bool yawGizmo = false, yawDragging = false, yawCheckpoint = false, yawUsePlane = false;
int yawThing = -1, yawOriginal = 0, missing = 0;
float yawGrab = 0, yawMouseX = 0, yaw = 0, pitch = 0;
V3 eye{};
UINT width = 0, height = 0;
uint64_t sceneRevision = 0, output = 0;
float light(const SectorDef &s) {
    return fullbright ? 1.f : std::clamp(s.lightlevel / 255.f, .12f, 1.f);
}
// Even/odd trapezoid decomposition handles concave outlines, islands and holes.
void tessellate(const std::vector<Edge> &edges,
                const std::function<void(Vec2, Vec2, Vec2, Vec2)> &emit) {
    std::vector<float> ys;
    for (auto &e : edges) {
        ys.push_back(e.a.y);
        ys.push_back(e.b.y);
    }
    std::sort(ys.begin(), ys.end());
    ys.erase(std::unique(ys.begin(), ys.end()), ys.end());
    auto xAt = [](const Edge &e, float y) {
        return e.a.x + (e.b.x - e.a.x) * (y - e.a.y) / (e.b.y - e.a.y);
    };
    for (size_t i = 1; i < ys.size(); ++i) {
        float y0 = ys[i - 1], y1 = ys[i], mid = (y0 + y1) * .5f;
        if (y1 - y0 < .0001f)
            continue;
        std::vector<const Edge *> hits;
        for (auto &e : edges)
            if (mid > std::min(e.a.y, e.b.y) && mid < std::max(e.a.y, e.b.y))
                hits.push_back(&e);
        std::sort(hits.begin(), hits.end(),
                  [&](auto a, auto b) { return xAt(*a, mid) < xAt(*b, mid); });
        for (size_t j = 1; j < hits.size(); j += 2) {
            auto &l = *hits[j - 1];
            auto &r = *hits[j];
            if (xAt(r, mid) - xAt(l, mid) < .0001f)
                continue;
            emit({xAt(l, y0), y0}, {xAt(r, y0), y0}, {xAt(r, y1), y1}, {xAt(l, y1), y1});
        }
    }
}
bool contains(const Face &f, Vec2 p) {
    bool inside = false;
    for (auto &e : f.edges)
        if ((e.a.y > p.y) != (e.b.y > p.y))
            if (p.x < (e.b.x - e.a.x) * (p.y - e.a.y) / (e.b.y - e.a.y) + e.a.x)
                inside = !inside;
    return inside;
}
const SectorDef *sectorAt(Vec2 p) {
    for (auto &f : faces)
        if (contains(f, p))
            return &f.def;
    return nullptr;
}
Material &material(App &a, const std::string &key) {
    auto found = materials.find(key);
    if (found != materials.end())
        return found->second;
    Image image;
    Material m;
    try {
        std::string name = key.substr(2);
        if (key[0] == 'H') {
            image = sprite_baker::readImage(std::filesystem::u8path(name));
            m.emission = doom_models::skinEmission(std::filesystem::u8path(name));
        } else if (key[0] == 'P') {
            image = a.project.wad.patch(name);
            auto &bytes = a.project.wad.get(name);
            m.left = i16(bytes, 4);
            m.top = i16(bytes, 6);
        } else {
            image = a.project.wad.surfaceImage(name);
        }
    } catch (const std::exception &error) {
        a.message("3D material " + key + ": " + error.what());
        ++missing;
        image.width = image.height = 64;
        image.pixels.resize(4096);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x)
                image.pixels[y * 64 + x] = ((x / 8 + y / 8) & 1) ? 0xffc030c0 : 0xff282828;
        m.left = 32;
        m.top = 64;
    }
    m.pixels = image.pixels;
    m.width = image.width;
    m.height = image.height;
    m.logicalWidth = m.width;
    m.logicalHeight = m.height;
    if (key[0] != 'P') {
        for (const auto &definition : a.project.wad.materials()) {
            if (definition.name == key.substr(2)) {
                m.logicalWidth = definition.width;
                m.logicalHeight = definition.height;
                break;
            }
        }
    }
    m.texture = uploadImage(image);
    return materials.emplace(key, std::move(m)).first->second;
}
void quad(std::vector<Vertex> &out, std::vector<Batch> &groups, const std::string &tex,
          const Vertex &a, const Vertex &b, const Vertex &c, const Vertex &d, int kind = 0,
          int index = -1, int side = 0) {
    UINT first = UINT(out.size());
    out.insert(out.end(), {a, b, c, a, c, d});
    if (!groups.empty() && groups.back().texture == tex && groups.back().kind == kind &&
        groups.back().index == index && groups.back().side == side &&
        groups.back().start + groups.back().count == first)
        groups.back().count += 6;
    else
        groups.push_back({tex, first, 6, kind, index, side});
}
bool isSky(const App &a, const std::string &name) {
    if (name == "F_SKY1")
        return true;
    for (const auto &d : a.project.wad.materials())
        if (d.name == name)
            return d.kind == 'S';
    return false;
}
// Sky ceilings use the selected sky material.
bool skyCeiling(const App &a, const SectorDef &s) {
    return isSky(a, s.ceilingflat);
}
std::string skyTexture(const App &a) {
    if (a.mapName.size() > 1 && a.mapName[0] == 'E') {
        if (a.mapName[1] == '2')
            return "SKY2";
        if (a.mapName[1] == '3' || a.mapName[1] == '4')
            return "SKY3";
    }
    if (a.mapName.rfind("MAP", 0) == 0) {
        int n = atoi(a.mapName.c_str() + 3);
        if (n >= 21)
            return "SKY3";
        if (n >= 12)
            return "SKY2";
    }
    return "SKY1";
}
void buildScene(App &a) {
    ++sceneRevision;
    mesh.clear();
    batches.clear();
    faces.clear();
    warning.clear();
    // Sector connection needs a working copy, but never the undo/redo history.
    EditWorld copy;
    static_cast<WorldState &>(copy) = static_cast<const WorldState &>(a.world);
    try {
        auto sectors = connectSectors(copy);
        for (auto &sec : sectors) {
            Face f;
            f.def = sec.def;
            f.sides = sec.sides;
            for (auto [index, side] : sec.sides) {
                auto &l = copy.lines[index];
                if ((l.flags & 4) && l.side[0].sector == l.side[1].sector)
                    continue;
                f.edges.push_back(
                    {copy.points[side ? l.p2 : l.p1].pt, copy.points[side ? l.p1 : l.p2].pt});
            }
            faces.push_back(std::move(f));
        }
    } catch (const std::exception &e) {
        warning = std::string("Inconsistent source sectors: ") + e.what();
        try {
            // Classic DWDs can have conflicting definitions on one geometric
            // face (E1M2's light recesses do). Use the compiler's exact cells
            // rather than dropping every floor/ceiling or rewriting the source.
            std::istringstream input(a.world.serializeForCompiler());
            const auto planar = dm::loadDwd(input, "preview", false);
            std::vector<std::vector<std::pair<int, int>>> sides(planar.sectors.size());
            size_t sourceLine = 0;
            for (size_t index = 0; index < copy.lines.size(); ++index) {
                if (copy.lines[index].selected < 0) {
                    continue;
                }
                const auto &line = planar.lines.at(sourceLine++);
                for (int side = 0; side < 2; ++side) {
                    if (line.sides[side] >= 0) {
                        const int sector = planar.sides[line.sides[side]].sector;
                        sides.at(sector).emplace_back(static_cast<int>(index), side);
                    }
                }
            }
            for (const auto &cell : planar.cells) {
                Face face;
                face.sides = sides.at(cell.sector);
                const auto [line, side] = face.sides.at(0);
                face.def = copy.lines[line].side[side].ends;
                for (size_t index = 0; index < cell.polygon.size(); ++index) {
                    const auto &p = cell.polygon[index];
                    const auto &q = cell.polygon[(index + 1) % cell.polygon.size()];
                    face.edges.push_back({{static_cast<float>(p.x), static_cast<float>(p.y)},
                                          {static_cast<float>(q.x), static_cast<float>(q.y)}});
                }
                faces.push_back(std::move(face));
            }
            warning += ". Preview uses dmap's surface decomposition.";
        } catch (const std::exception &fallbackError) {
            warning += std::string(". Surface preview unavailable: ") + fallbackError.what();
        }
    }
    float skyCap = 1024;
    for (const auto &f : faces)
        if (skyCeiling(a, f.def))
            skyCap = std::max(skyCap, f.def.ceilingheight + 1024.f);
    for (auto &f : faces) {
        tessellate(f.edges, [&](Vec2 p1, Vec2 p2, Vec2 p3, Vec2 p4) {
            for (int ceiling = 0; ceiling < 2; ++ceiling) {
                const auto &flat = ceiling ? f.def.ceilingflat : f.def.floorflat;
                bool sky = isSky(a, flat);
                std::string tex = sky && flat == "F_SKY1" ? "W:" + skyTexture(a) : "F:" + flat;
                const auto &planeMaterial = material(a, tex);
                const auto &plane = ceiling ? f.def.ceilingSlope : f.def.slope;
                V3 n = ceiling ? V3{plane.dx / 65536.f, plane.dy / 65536.f, -1}
                               : V3{-plane.dx / 65536.f, -plane.dy / 65536.f, 1};
                const int rotation = ceiling ? f.def.rotation.ceiling : f.def.rotation.floor;
                auto v = [&](Vec2 p) {
                    const auto rotated = rotatedPlaneUV(p.x, p.y, rotation);
                    const auto uv =
                        (ceiling ? f.def.ceilingUV : f.def.floorUV).apply(rotated[0], rotated[1]);
                    return Vertex{
                        {p.x, p.y,
                         ceiling ? (sky ? skyCap : f.def.ceilingAt(p)) : f.def.floorAt(p)},
                        sky ? rotation / 360000.f : uv[0] / planeMaterial.logicalWidth,
                        sky ? 0.f : uv[1] / planeMaterial.logicalHeight,
                        light(f.def),
                        n,
                        sky ? (flat == "F_SKY1" ? 1.f : 2.f) : 0.f};
                };
                quad(mesh, batches, tex, v(p1), v(p2), v(p3), v(p4), 1, int(&f - faces.data()),
                     ceiling);
            }
        });
    }
    for (auto &l : copy.lines) {
        if (l.selected < 0)
            continue;
        auto p1 = copy.points.at(l.p1).pt, p2 = copy.points.at(l.p2).pt;
        for (int side = 0; side < ((l.flags & 4) ? 2 : 1); ++side) {
            auto p = side ? p2 : p1, q = side ? p1 : p2;
            auto &s = l.side[side];
            auto &other = l.side[1 - side];
            float len = std::hypot(q.x - p.x, q.y - p.y);
            if (len < .001f)
                continue;
            V3 n{(q.y - p.y) / len, -(q.x - p.x) / len, 0};
            auto wall = [&](const std::string &name, WallStrip strip) {
                if (!strip.clip())
                    return;
                const std::string tex = "W:" + name;
                auto &m = material(a, tex);
                auto vertex = [&](int end, bool upper) {
                    const float t = end ? strip.t1 : strip.t0;
                    Vec2 xy = p + (q - p) * t;
                    float z = upper ? strip.top[end] : strip.bottom[end];
                    const auto uv =
                        s.uv.apply(s.firstcolumn + len * t, strip.anchor[end] - z + s.flags);
                    return Vertex{{xy.x, xy.y, z},
                                  uv[0] / m.logicalWidth,
                                  uv[1] / m.logicalHeight,
                                  light(s.ends),
                                  n,
                                  0};
                };
                quad(mesh, batches, tex, vertex(0, true), vertex(1, true), vertex(1, false),
                     vertex(0, false), 2, int(&l - copy.lines.data()), side);
            };
            std::array<float, 2> floor{s.ends.floorAt(p), s.ends.floorAt(q)};
            std::array<float, 2> back{other.ends.floorAt(p), other.ends.floorAt(q)};
            const float ceil = float(s.ends.ceilingheight), bc = float(other.ends.ceilingheight);
            std::array<float, 2> ceiling{s.ends.ceilingAt(p), s.ends.ceilingAt(q)},
                backCeiling{other.ends.ceilingAt(p), other.ends.ceilingAt(q)};
            auto cuts = wallCuts({floor, back, ceiling, backCeiling});
            for (size_t i = 1; i < cuts.size(); ++i) {
                float t0 = cuts[i - 1], t1 = cuts[i];
                auto at = [](std::array<float, 2> h, float t) { return h[0] + t * (h[1] - h[0]); };
                std::array<float, 2> f{at(floor, t0), at(floor, t1)},
                    bf{at(back, t0), at(back, t1)};
                std::array<float, 2> c{at(ceiling, t0), at(ceiling, t1)},
                    b{at(backCeiling, t0), at(backCeiling, t1)};
                if (!(l.flags & 4)) {
                    float h = float(material(a, "W:" + s.midtexture).logicalHeight);
                    wall(s.midtexture,
                         {t0, t1, f, c,
                          (l.flags & 16)
                              ? std::array<float, 2>{s.ends.floorheight + h, s.ends.floorheight + h}
                              : std::array<float, 2>{ceil, ceil}});
                } else {
                    wall(s.bottomtexture,
                         {t0,
                          t1,
                          f,
                          {std::min(c[0], bf[0]), std::min(c[1], bf[1])},
                          (l.flags & 16) ? std::array<float, 2>{ceil, ceil}
                                         : std::array<float, 2>{float(other.ends.floorheight),
                                                                float(other.ends.floorheight)}});
                    if ((b[0] < c[0] || b[1] < c[1]) &&
                        !(skyCeiling(a, s.ends) && skyCeiling(a, other.ends))) {
                        float h = float(material(a, "W:" + s.toptexture).logicalHeight);
                        float anchor = (l.flags & 8) ? ceil : bc + h;
                        wall(s.toptexture, {t0,
                                            t1,
                                            {std::max(f[0], b[0]), std::max(f[1], b[1])},
                                            c,
                                            {anchor, anchor}});
                    }
                    if (!s.midtexture.empty() && s.midtexture != "-") {
                        float h = float(material(a, "W:" + s.midtexture).logicalHeight) *
                                  s.uv.scaleV * .001f,
                              hi = std::min(ceil, bc);
                        const float shiftedRow = s.flags + s.uv.offsetV * .000001f * s.uv.scaleV;
                        std::array<float, 2> upper{std::min(c[0], b[0]), std::min(c[1], b[1])};
                        std::array<float, 2> lo{std::max(f[0], bf[0]), std::max(f[1], bf[1])};
                        std::array<float, 2> anchor =
                            (l.flags & 16) ? std::array<float, 2>{std::max(s.ends.floorheight,
                                                                           other.ends.floorheight) +
                                                                      h,
                                                                  std::max(s.ends.floorheight,
                                                                           other.ends.floorheight) +
                                                                      h}
                                           : std::array<float, 2>{hi, hi};
                        wall(s.midtexture, {t0,
                                            t1,
                                            {std::max(lo[0], anchor[0] + shiftedRow - h),
                                             std::max(lo[1], anchor[1] + shiftedRow - h)},
                                            {std::min(upper[0], anchor[0] + shiftedRow),
                                             std::min(upper[1], anchor[1] + shiftedRow)},
                                            anchor});
                    }
                }
            }
            if (skyCeiling(a, s.ends) && (!(l.flags & 4) || !skyCeiling(a, other.ends))) {
                const bool hd = s.ends.ceilingflat != "F_SKY1";
                const std::string key = hd ? "F:" + s.ends.ceilingflat : "W:" + skyTexture(a);
                material(a, key);
                auto vertex = [&](Vec2 xy, float z) {
                    return Vertex{{xy.x, xy.y, z}, s.ends.rotation.ceiling / 360000.f, 0, 1, n,
                                  hd ? 2.f : 1.f};
                };
                int faceIndex = -1;
                for (size_t fi = 0; fi < faces.size(); ++fi)
                    if (std::find(faces[fi].sides.begin(), faces[fi].sides.end(),
                                  std::make_pair(int(&l - copy.lines.data()), side)) !=
                        faces[fi].sides.end())
                        faceIndex = int(fi);
                quad(mesh, batches, key, vertex(p, skyCap), vertex(q, skyCap),
                     vertex(q, s.ends.ceilingAt(q)), vertex(p, s.ends.ceilingAt(p)),
                     faceIndex >= 0 ? 1 : 0, faceIndex, 1);
            }
        }
    }
}
const Face *thingFace(const Thing &t, bool = false) {
    for (const auto &f : faces) {
        Vec2 p = t.origin;
        if (contains(f, p))
            return &f;
    }
    return nullptr;
}
const SectorDef *thingSector(const App &, const Thing &t) {
    auto *f = thingFace(t);
    return f ? &f->def : sectorAt(t.origin);
}
void resetCamera(App &a) {
    const SectorDef *playerSector = nullptr;
    Vec2 p = a.view.center;
    yaw = 0;
    for (auto &t : a.world.things)
        if (t.selected >= 0 && t.type == 1) {
            p = t.origin;
            playerSector = thingSector(a, t);
            yaw = t.angle * pi / 180;
            break;
        }
    auto s = playerSector ? playerSector : sectorAt(p);
    if (!s && !faces.empty()) {
        auto &e = faces.front().edges;
        if (!e.empty()) {
            auto d = e[0].b - e[0].a;
            float len = std::hypot(d.x, d.y);
            if (len > 0)
                p = (e[0].a + e[0].b) * .5f + Vec2{d.y / len, -d.x / len} * 24;
            s = sectorAt(p);
        }
    }
    eye = {p.x, p.y, s ? s->floorAt(p) + 41 : 41};
    pitch = 0;
    cameraSet = true;
}
void things(App &a, std::vector<Vertex> &out, std::vector<Batch> &groups, bool models = true,
            bool sprites = true) {
    for (auto &t : a.world.things) {
        if (t.selected < 0)
            continue;
        if (const auto *definition = modelProps().find(t.type)) {
            if (!models)
                continue;
            const auto &model = modelProps().load(t.type);
            auto sec = thingSector(a, t);
            const float base = (sec ? sec->floorAt(t.origin) : 0) + t.height,
                        angle = t.angle * pi / 180;
            for (const auto &surface : model.surfaces) {
                const auto key = "H:" + doom_models::relativeAsset(definition->file.parent_path(),
                                                                   surface.shader)
                                            .u8string();
                material(a, key);
                const UINT first = UINT(out.size());
                for (const auto &triangle : surface.triangles)
                    for (auto index : triangle) {
                        auto vertex =
                            doom_models::sample(surface, *definition, index, ImGui::GetTime());
                        auto p = doom_models::rotate(vertex.position, angle),
                             n = doom_models::rotate(vertex.normal, angle);
                        const float lum = surface.fullbright() || fullbright ||
                                                  materials.at(key).emission.strength > 0
                                              ? 1
                                              : (sec ? light(*sec) : .8f) * doom_models::shade(n);
                        out.push_back(
                            {{t.origin.x + p.x * definition->scale,
                              t.origin.y + p.y * definition->scale, base + p.z * definition->scale},
                             surface.uv[index][0],
                             surface.uv[index][1],
                             lum,
                             {},
                             0});
                    }
                groups.push_back({key, first, UINT(out.size()) - first, 3,
                                  int(&t - a.world.things.data()), 0,
                                  surface.fullbright() || materials.at(key).emission.strength > 0});
            }
            continue;
        }
        if (!sprites)
            continue;
        if (((t.type >= 1 && t.type <= 4) || t.type == 11) &&
            std::hypot(t.origin.x - eye.x, t.origin.y - eye.y) < 48.f)
            continue;
        const PreviewSprite *def = nullptr;
        for (auto &s : previewSprites)
            if (s.type == t.type) {
                def = &s;
                break;
            }
        std::string prefix;
        bool bright = false, ceiling = false;
        for (auto &custom : a.thingDefs)
            if (custom.type == t.type && !custom.sprite.empty() && custom.sprite != "-") {
                prefix = upper(custom.sprite);
                break;
            }
        if (prefix.empty() && def) {
            prefix = def->frame;
            bright = def->fullbright;
            ceiling = def->ceiling;
        }
        if (prefix.empty() && (t.type <= 4 || t.type == 11))
            prefix = "PLAYA";
        if (prefix.size() == 4)
            prefix += 'A';
        std::string name;
        bool flip = false;
        if (a.project.wad.find(prefix) >= 0)
            name = prefix;
        float angle = std::atan2(eye.y - t.origin.y, eye.x - t.origin.x) - t.angle * pi / 180;
        while (angle < 0)
            angle += 2 * pi;
        int rot = (int(std::floor(angle / (pi / 4) + .5f)) & 7) + 1;
        const doom_sprites::Frame *hd = nullptr;
        for (const auto &profile : doom_sprites::profiles())
            if (profile.thingType == t.type) {
                auto f = hdFrames.find(doom_sprites::key(profile.spawnState, rot));
                if (f != hdFrames.end())
                    hd = &f->second;
                break;
            }
        if (hd) {
            const auto texture = "H:" + (hd->directory / hd->file).u8string();
            material(a, texture);
            auto sec = thingSector(a, t);
            const float base = (ceiling && sec ? sec->ceilingAt(t.origin) - hd->height
                                : sec          ? sec->floorAt(t.origin)
                                               : 0) +
                               t.height;
            float top = base + hd->top, bottom = top - hd->height;
            float rx = std::sin(yaw), ry = -std::cos(yaw), left = -hd->left,
                  right = left + hd->width;
            Vec2 p{t.origin.x + rx * left, t.origin.y + ry * left},
                q{t.origin.x + rx * right, t.origin.y + ry * right};
            float lum = bright || fullbright ? 1 : sec ? light(*sec) : .8f;
            quad(out, groups, texture, {{p.x, p.y, top}, 0, 0, lum, {}, 0},
                 {{q.x, q.y, top}, 1, 0, lum, {}, 0}, {{q.x, q.y, bottom}, 1, 1, lum, {}, 0},
                 {{p.x, p.y, bottom}, 0, 1, lum, {}, 0}, 3, int(&t - a.world.things.data()));
            continue;
        }
        if (name.empty() && prefix.size() == 5) {
            for (auto &lump : a.project.wad.lumps) {
                auto &n = lump.name;
                if (n.size() < 6 || n.substr(0, 4) != prefix.substr(0, 4))
                    continue;
                if (n[4] == prefix[4] && (n[5] == '0' || n[5] == '0' + rot)) {
                    name = n;
                    break;
                }
                if (n.size() == 8 && n[6] == prefix[4] && n[7] == '0' + rot) {
                    name = n;
                    flip = true;
                    break;
                }
            }
        }
        if (name.empty() && prefix.size() == 5) {
            for (auto &lump : a.project.wad.lumps)
                if (lump.name.substr(0, 5) == prefix) {
                    name = lump.name;
                    break;
                }
        }
        auto &m = material(a, "P:" + name);
        auto sec = thingSector(a, t);
        float bottom = sec ? sec->floorAt(t.origin) : 0;
        float top = (ceiling && sec ? sec->ceilingAt(t.origin) : bottom + m.height) + t.height;
        float rx = std::sin(yaw), ry = -std::cos(yaw);
        float left = -float(m.left), right = left + m.width;
        if (flip) {
            left = float(m.left) - m.width;
            right = float(m.left);
        }
        Vec2 p{t.origin.x + rx * left, t.origin.y + ry * left},
            q{t.origin.x + rx * right, t.origin.y + ry * right};
        float lum = bright || fullbright ? 1 : sec ? light(*sec) : .8f;
        float u0 = flip ? 1.f : 0.f, u1 = 1 - u0;
        quad(out, groups, "P:" + name, {{p.x, p.y, top}, u0, 0, lum, {}, 0},
             {{q.x, q.y, top}, u1, 0, lum, {}, 0}, {{q.x, q.y, top - m.height}, u1, 1, lum, {}, 0},
             {{p.x, p.y, top - m.height}, u0, 1, lum, {}, 0}, 3, int(&t - a.world.things.data()));
    }
}
V3 sub(V3 a, V3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
V3 cross(V3 a, V3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float dot(V3 a, V3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
struct Hit {
    int kind = 0, index = -1, side = 0;
    V3 point{};
    float distance = 65536;
};
Hit pick(V3 origin, V3 direction) {
    Hit hit;
    for (const auto &batch : pickBatches) {
        auto &mat = materials.at(batch.texture);
        for (UINT i = batch.start; i < batch.start + batch.count; i += 3) {
            auto &a = pickVertices[i];
            auto &b = pickVertices[i + 1];
            auto &c = pickVertices[i + 2];
            if (dot(a.normal, a.normal) > .1f && dot(sub(origin, a.p), a.normal) <= 0)
                continue;
            auto e1 = sub(b.p, a.p), e2 = sub(c.p, a.p), h = cross(direction, e2);
            float det = dot(e1, h);
            if (std::abs(det) < .000001f)
                continue;
            auto offset = sub(origin, a.p);
            float u = dot(offset, h) / det;
            if (u < 0 || u > 1)
                continue;
            auto q = cross(offset, e1);
            float v = dot(direction, q) / det;
            if (v < 0 || u + v > 1)
                continue;
            float t = dot(e2, q) / det;
            if (t < 1 || t >= hit.distance)
                continue;
            float tu = a.u + u * (b.u - a.u) + v * (c.u - a.u),
                  tv = a.v + u * (b.v - a.v) + v * (c.v - a.v);
            int x = std::min(mat.width - 1, int((tu - std::floor(tu)) * mat.width));
            int y = std::min(mat.height - 1, int((tv - std::floor(tv)) * mat.height));
            if ((mat.pixels[y * mat.width + x] >> 24) < 77)
                continue;
            hit = {batch.kind,
                   batch.index,
                   batch.side,
                   {origin.x + t * direction.x, origin.y + t * direction.y,
                    origin.z + t * direction.z},
                   t};
        }
    }
    return hit;
}
V3 pointerRay(float x, float y) {
    float c = std::cos(yaw), s = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
    V3 d{c * cp + s * x - c * sp * y, s * cp - c * x - s * sp * y, sp + cp * y};
    float len = std::sqrt(dot(d, d));
    return {d.x / len, d.y / len, d.z / len};
}
void selectHit(App &a, const Hit &hit, bool openPanel = true) {
    a.world.deselect();
    a.pickedSectorSides.clear();
    std::string panel;
    if (hit.kind == 1) {
        a.pickedSectorSides = faces.at(hit.index).sides;
        int first = int(a.world.lines.size());
        for (auto [index, side] : a.pickedSectorSides) {
            a.world.lines[index].selected = side + 1;
            if (index < first) {
                first = index;
                desktopLineSide() = side;
            }
        }
        a.view.mode = 1;
        panel = "SectorEditor:Sector Editor";
    } else if (hit.kind == 2) {
        a.world.lines.at(hit.index).selected = hit.side + 1;
        desktopLineSide() = hit.side;
        a.view.mode = 1;
        panel = "line:Line Inspector";
    } else if (hit.kind == 3) {
        auto &t = a.world.things.at(hit.index);
        t.selected = 1;
        a.thingType = t.type;
        a.view.mode = 2;
        panel = "thing:Thing Inspector";
    }
    if (openPanel && !panel.empty())
        showNativePanel(panel);
}
int normalizedYaw(float degrees) {
    return (int(std::lround(degrees)) % 360 + 360) % 360;
}
void applyYawDrag(App &a, int index, float degrees, bool snap) {
    if (snap)
        degrees = std::round(degrees / 15) * 15;
    int next = normalizedYaw(degrees);
    auto &thing = a.world.things.at(index);
    if (next == thing.angle)
        return;
    if (!yawCheckpoint) {
        a.world.checkpoint();
        yawCheckpoint = true;
    }
    thing.angle = next;
}
// Editor overlays use exactly the same camera projection as the mesh renderer.
bool projectOverlay(V3 p, ImVec2 pos, ImVec2 area, ImVec2 &screen) {
    auto d = sub(p, eye);
    float c = std::cos(yaw), s = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
    float z = dot(d, {c * cp, s * cp, sp});
    if (z <= 1.f)
        return false;
    screen = {pos.x + area.x * .5f * (1 + dot(d, {s, -c, 0}) / z),
              pos.y + area.y * .5f - area.x * .5f * dot(d, {-c * sp, -s * sp, cp}) / z};
    return true;
}
bool drawYawOverlays(App &a, ImVec2 pos, ImVec2 area, bool interactive) {
    auto *draw = ImGui::GetWindowDrawList();
    auto &io = ImGui::GetIO();
    int selected = -1;
    for (int i = 0; i < int(a.world.things.size()); ++i)
        if (a.world.things[i].selected > 0) {
            selected = i;
            break;
        }
    if (!interactive || selected != yawThing || !ImGui::IsMouseDown(0))
        yawDragging = false;
    bool consumed = yawDragging;
    draw->PushClipRect(pos, {pos.x + area.x, pos.y + area.y}, true);
    for (int i = 0; i < int(a.world.things.size()); ++i) {
        auto t = a.world.things[i];
        if (t.selected <= 0)
            continue;
        const auto *face = thingFace(t);
        const auto *sec = face ? &face->def : nullptr;
        V3 center{t.origin.x, t.origin.y, (sec ? sec->floorAt(t.origin) : 0) + t.height + 24.f};
        auto point = [&](float angle, float radius) {
            return V3{center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius,
                      center.z};
        };
        const auto arrowColor =
            t.selected > 0 ? IM_COL32(255, 210, 40, 255) : IM_COL32(80, 220, 255, 230);
        float angle = t.angle * pi / 180;
        ImVec2 start, tip;
        if (projectOverlay(center, pos, area, start) &&
            projectOverlay(point(angle, 40), pos, area, tip)) {
            float dx = tip.x - start.x, dy = tip.y - start.y;
            float length = std::hypot(dx, dy);
            if (length > .1f) {
                dx /= length;
                dy /= length;
                length = std::min(length, 64 * a.dpi);
                tip = {start.x + dx * length, start.y + dy * length};
                float head = std::min(8 * a.dpi, length * .4f);
                draw->AddTriangleFilled(
                    tip, {tip.x - dx * head - dy * head * .6f, tip.y - dy * head + dx * head * .6f},
                    {tip.x - dx * head + dy * head * .6f, tip.y - dy * head - dx * head * .6f},
                    arrowColor);
            }
            draw->AddLine(start, tip, IM_COL32(0, 0, 0, 220), 4 * a.dpi);
            draw->AddLine(start, tip, arrowColor, 2 * a.dpi);
        }
        if (!yawGizmo || i != selected)
            continue;
        float nearest = 1e9f;
        for (int segment = 0; segment < 96; ++segment) {
            ImVec2 p, q;
            if (!projectOverlay(point(segment * 2 * pi / 96, 48), pos, area, p) ||
                !projectOverlay(point((segment + 1) * 2 * pi / 96, 48), pos, area, q))
                continue;
            draw->AddLine(p, q, arrowColor, 2 * a.dpi);
            float dx = q.x - p.x, dy = q.y - p.y;
            float f = std::clamp(((io.MousePos.x - p.x) * dx + (io.MousePos.y - p.y) * dy) /
                                     std::max(.001f, dx * dx + dy * dy),
                                 0.f, 1.f);
            nearest = std::min(
                nearest, std::hypot(io.MousePos.x - p.x - f * dx, io.MousePos.y - p.y - f * dy));
        }
        auto ray = pointerRay(2 * (io.MousePos.x - pos.x) / area.x - 1,
                              (1 - 2 * (io.MousePos.y - pos.y) / area.y) * area.y / area.x);
        float distance = std::abs(ray.z) > .001f ? (center.z - eye.z) / ray.z : -1;
        bool plane = distance > 0 && std::abs(center.z - eye.z) > 2;
        float pointerAngle =
            std::atan2(eye.y + ray.y * distance - center.y, eye.x + ray.x * distance - center.x);
        if (interactive && ImGui::IsItemHovered() && nearest < 9 * a.dpi &&
            ImGui::IsMouseClicked(0)) {
            yawDragging = true;
            yawCheckpoint = false;
            yawThing = i;
            yawOriginal = t.angle;
            yawGrab = pointerAngle;
            yawMouseX = io.MousePos.x;
            yawUsePlane = plane;
            consumed = true;
        }
        if (yawDragging && yawThing == i) {
            float degrees = yawOriginal +
                            (yawUsePlane ? std::remainder(pointerAngle - yawGrab, 2 * pi) * 180 / pi
                                         : (io.MousePos.x - yawMouseX) * .5f);
            // Keep the last valid rotation when the pointer crosses the plane's horizon.
            if (!yawUsePlane || plane)
                applyYawDrag(a, i, degrees, io.KeyShift);
        }
        if (projectOverlay(center, pos, area, start)) {
            char label[96];
            snprintf(label, sizeof(label), "Yaw %d deg | drag ring | Shift: 15 deg", t.angle);
            draw->AddText({start.x + 8 * a.dpi, start.y - 22 * a.dpi}, arrowColor, label);
        }
    }
    draw->PopClipRect();
    return consumed;
}
std::string assetKey(App &a) {
    std::ostringstream s;
    s << a.project.wadpath.u8string() << a.project.wad.lumps.size() << a.mapName;
    for (auto &t : a.project.textures) {
        s << t.name << t.width << t.height;
        for (auto &p : t.patches)
            s << p.name << p.x << p.y;
    }
    return s.str();
}
bool mapGeometryChanged(const EditWorld &world) {
    bool changed = !geometryValid || geometryPoints.size() != world.points.size() ||
                   geometryLines.size() != world.lines.size();
    for (size_t i = 0; !changed && i < world.points.size(); ++i)
        changed = !(world.points[i].pt == geometryPoints[i].pt) ||
                  (world.points[i].selected < 0) != (geometryPoints[i].selected < 0);
    for (size_t i = 0; !changed && i < world.lines.size(); ++i) {
        const auto &a = world.lines[i], &b = geometryLines[i];
        changed = a.p1 != b.p1 || a.p2 != b.p2 || a.flags != b.flags ||
                  (a.selected < 0) != (b.selected < 0);
        for (int side = 0; !changed && side < 2; ++side) {
            const auto &x = a.side[side], &y = b.side[side];
            changed = !(x.ends == y.ends) || !(x.uv == y.uv) || x.flags != y.flags ||
                      x.firstcolumn != y.firstcolumn || x.toptexture != y.toptexture ||
                      x.bottomtexture != y.bottomtexture || x.midtexture != y.midtexture;
        }
    }
    if (changed) {
        geometryPoints = world.points;
        geometryLines = world.lines;
        geometryValid = true;
    }
    return changed;
}
void render(App &a, UINT w, UINT h) {
    width = w;
    height = h;
    pickVertices = mesh;
    pickBatches = batches;
    things(a, pickVertices, pickBatches);
    std::vector<EditorSceneVertex> vertices;
    std::vector<EditorSceneBatch> groups;
    vertices.reserve(pickVertices.size());
    for (const auto &vertex : pickVertices) {
        const auto lum = static_cast<unsigned char>(std::clamp(vertex.light, 0.f, 1.f) * 255);
        const uint32_t color = 0xff000000u | (uint32_t(lum) * 0x010101u);
        // Sky uses a world-space projection in the editable geometry preview.
        const float u = vertex.sky ? vertex.p.x / 512 : vertex.u;
        const float v = vertex.sky ? vertex.p.y / 512 : vertex.v;
        vertices.push_back({{vertex.p.x, vertex.p.y, vertex.p.z}, {u, v}, color});
    }
    for (const auto &batch : pickBatches) {
        groups.push_back(
            {static_cast<uint64_t>(materials.at(batch.texture).texture), batch.start, batch.count});
    }
    const float c = std::cos(yaw), s = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
    const EditorPreviewScene scene{vertices.data(),
                                   groups.data(),
                                   static_cast<uint32_t>(vertices.size()),
                                   static_cast<uint32_t>(groups.size()),
                                   static_cast<int>(w),
                                   static_cast<int>(h),
                                   {eye.x, eye.y, eye.z},
                                   {c * cp, s * cp, sp},
                                   {s, -c, 0},
                                   {-c * sp, -s * sp, cp}};
    output = editorRenderer().renderPreview(&scene);
    require(output != 0, "Engine renderer could not draw the editor preview");
}
} // namespace
void invalidate3DTextures() {
    for (const auto &[key, material] : materials) {
        releaseImage(material.texture);
    }
    materials.clear();
    resourceSignature.clear();
    geometryValid = false;
}
void shutdown3D() {
    invalidate3DTextures();
    mesh.clear();
    batches.clear();
    pickVertices.clear();
    pickBatches.clear();
    faces.clear();
    cameraSet = false;
}
void drawViewport3D(App &a, ImVec2 size) {
    try {
        bool refresh = ImGui::Button("Reload");
        ImGui::SameLine();
        bool reset = ImGui::Button("Camera: player");
        ImGui::SameLine();
        refresh |= ImGui::Checkbox("Fullbright", &fullbright);
        auto document = a.world.pathname.u8string() + "|" + a.documentTitle;
        if (document != cameraDocument) {
            cameraSet = false;
            yawGizmo = yawDragging = false;
            yawThing = -1;
            cameraDocument = document;
        }
        auto assets = assetKey(a);
        if (refresh || assets != resourceSignature) {
            invalidate3DTextures();
            reloadModelProps();
            hdFrames = doom_sprites::discover(sprite_baker::packsDirectory());
            missing = 0;
            resourceSignature = assets;
            signature.clear();
            geometryValid = false;
        }
        if (mapGeometryChanged(a.world)) {
            buildScene(a);
        }
        if (!cameraSet || reset)
            resetCamera(a);
        if (!warning.empty())
            ImGui::TextWrapped("%s", warning.c_str());
        if (missing)
            ImGui::Text("%d missing images shown as checkerboards", missing);
        ImVec2 area = ImGui::GetContentRegionAvail();
        area.x = std::max(1.f, std::min(area.x, size.x));
        area.y = std::max(1.f, area.y - 4 * a.dpi);
        auto pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("3D navigation", area,
                               ImGuiButtonFlags_MouseButtonLeft |
                                   ImGuiButtonFlags_MouseButtonRight);
        auto &io = ImGui::GetIO();
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1))
            dragging = true;
        if (!ImGui::IsMouseDown(1) || !ImGui::IsWindowFocused())
            dragging = false;
        if (dragging && !io.WantTextInput) {
            yaw -= io.MouseDelta.x * .004f;
            yaw = std::remainder(yaw, 2 * pi);
            pitch = std::clamp(pitch - io.MouseDelta.y * .004f, -1.5f, 1.5f);
            float step = std::min(io.DeltaTime, .1f) * (io.KeyShift ? 640.f : 160.f);
            float f = float(ImGui::IsKeyDown(ImGuiKey_W)) - float(ImGui::IsKeyDown(ImGuiKey_S)),
                  r = float(ImGui::IsKeyDown(ImGuiKey_D)) - float(ImGui::IsKeyDown(ImGuiKey_A));
            eye.x += step * (f * std::cos(pitch) * std::cos(yaw) + r * std::sin(yaw));
            eye.y += step * (f * std::cos(pitch) * std::sin(yaw) - r * std::cos(yaw));
            eye.z += step * f * std::sin(pitch);
            eye.z +=
                step * (float(ImGui::IsKeyDown(ImGuiKey_E)) - float(ImGui::IsKeyDown(ImGuiKey_Q)));
        }
        render(a, UINT(std::clamp(area.x, 1.f, 4096.f)), UINT(std::clamp(area.y, 1.f, 4096.f)));
        bool keyboard = ImGui::IsWindowFocused() && !io.WantTextInput &&
                        !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
        if (keyboard && !dragging) {
            if (!io.KeyCtrl && !io.KeyAlt && ImGui::IsItemHovered() &&
                ImGui::IsKeyPressed(ImGuiKey_E, false)) {
                bool selected = false;
                for (auto &t : a.world.things)
                    selected |= t.selected > 0;
                if (!selected) {
                    auto hit = pick(eye, pointerRay(2 * (io.MousePos.x - pos.x) / area.x - 1,
                                                    (1 - 2 * (io.MousePos.y - pos.y) / area.y) *
                                                        area.y / area.x));
                    if (hit.kind == 3) {
                        selectHit(a, hit, false);
                        selected = true;
                    }
                }
                if (selected)
                    yawGizmo = !yawGizmo;
                else
                    a.status = "Select or hover an entity, then press E to rotate its yaw";
                yawDragging = false;
            }
            if (!io.KeyCtrl && !io.KeyAlt) {
                const int step = io.KeyShift ? 1 : 8;
                const int delta = (ImGui::IsKeyPressed(ImGuiKey_1) ? step : 0) -
                                  (ImGui::IsKeyPressed(ImGuiKey_2) ? step : 0);
                const int slopeStep = io.KeyShift ? 512 : 4096;
                const int slopeDelta = (ImGui::IsKeyPressed(ImGuiKey_4) ? slopeStep : 0) -
                                       (ImGui::IsKeyPressed(ImGuiKey_3) ? slopeStep : 0);
                const bool selectedThing =
                    std::any_of(a.world.things.begin(), a.world.things.end(),
                                [](const Thing &t) { return t.selected > 0; });
                if (delta && selectedThing) {
                    if (a.world.moveSelectedThingsVertically(delta))
                        a.status = delta > 0 ? "Raised selected things" : "Lowered selected things";
                } else if (delta || slopeDelta) {
                    if (ImGui::IsItemHovered()) {
                        const auto hit =
                            pick(eye, pointerRay(2 * (io.MousePos.x - pos.x) / area.x - 1,
                                                 (1 - 2 * (io.MousePos.y - pos.y) / area.y) *
                                                     area.y / area.x));
                        if (hit.kind == 1) {
                            const auto &face = faces.at(hit.index);
                            const bool ceiling = hit.side != 0;
                            const auto original = a.world.lines[face.sides.front().first]
                                                      .side[face.sides.front().second]
                                                      .ends;
                            const auto next = a.world.adjustedSurface(original, ceiling, delta,
                                                                      slopeDelta, face.sides);
                            if (!(next == original)) {
                                a.world.checkpoint();
                                for (auto [line, side] : face.sides)
                                    a.world.lines[line].side[side].ends = next;
                                a.pickedSectorSides.clear();
                                a.status =
                                    ceiling ? "Adjusted hovered ceiling" : "Adjusted hovered floor";
                            } else {
                                a.status = "Surface unchanged: height or slope limit reached";
                            }
                        } else {
                            a.status = "Hover a floor or ceiling to adjust its height or slope";
                        }
                    }
                }
            }
            if (io.KeyCtrl) {
                if (ImGui::IsKeyPressed(ImGuiKey_S, false))
                    a.saveMap();
                if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                    yawDragging = false;
                    a.pickedSectorSides.clear();
                    a.world.undo();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
                    yawDragging = false;
                    a.pickedSectorSides.clear();
                    a.world.redo();
                }
            } else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
                a.pickedSectorSides.clear();
                a.world.deleteSelected();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                yawGizmo = yawDragging = false;
                selectHit(a, {});
            }
        }
        ImGui::GetWindowDrawList()->AddImage(static_cast<ImTextureID>(output), pos,
                                             {pos.x + area.x, pos.y + area.y}, {0, 1}, {1, 0});
        bool yawConsumed = drawYawOverlays(a, pos, area, keyboard && !dragging);
        if (ImGui::IsItemHovered() && !dragging && !io.WantTextInput && !yawConsumed) {
            bool click = ImGui::IsMouseClicked(0);
            bool place =
                keyboard && !io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_S, false);
            if (click || place) {
                auto hit = pick(
                    eye, pointerRay(2 * (io.MousePos.x - pos.x) / area.x - 1,
                                    (1 - 2 * (io.MousePos.y - pos.y) / area.y) * area.y / area.x));
                if (place) {
                    if (hit.kind) {
                        placeThingAt(a, {hit.point.x, hit.point.y});
                    } else
                        a.status = "Point at a map surface to place a thing";
                } else
                    selectHit(a, hit);
            }
        }
    } catch (const std::exception &e) {
        ImGui::TextWrapped("3D view: %s", e.what());
        if (a.smoke3d)
            throw;
    }
}
} // namespace de
