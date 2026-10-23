#pragma once
#include <array>
#include "formats/floor_slope.h"
#include "formats/sector_rotation.h"
#include "formats/texture_uv.h"
#include "formats/thing_height.h"
#include <cstdint>
#include <string>
#include <vector>
namespace de {
using Bytes = std::vector<uint8_t>;
struct Vec2 {
    float x = 0, y = 0;
};
inline Vec2 operator+(Vec2 a, Vec2 b) {
    return {a.x + b.x, a.y + b.y};
}
inline Vec2 operator-(Vec2 a, Vec2 b) {
    return {a.x - b.x, a.y - b.y};
}
inline Vec2 operator*(Vec2 a, float s) {
    return {a.x * s, a.y * s};
}
inline bool operator==(Vec2 a, Vec2 b) {
    return a.x == b.x && a.y == b.y;
}
struct SectorDef {
    FloorSlope slope, ceilingSlope;
    SectorRotation rotation;
    TextureUV floorUV, ceilingUV;
    float ceilingAt(Vec2 p) const { return float(ceilingheight + ceilingSlope.offset(p.x, p.y)); }
    float floorAt(Vec2 p) const { return float(floorheight + slope.offset(p.x, p.y)); }
    int floorheight = 0, ceilingheight = 128;
    std::string floorflat = "FLOOR0_1", ceilingflat = "CEIL1_1";
    int lightlevel = 160, special = 0, tag = 0;
    bool operator==(const SectorDef &b) const;
};
struct Side {
    TextureUV uv;
    int flags = 0, firstcolumn = 0, sector = -1;
    std::string toptexture = "-", bottomtexture = "-", midtexture = "STARTAN3";
    SectorDef ends;
};
struct Point {
    Vec2 pt;
    int selected = 0;
};
struct Line {
    int p1 = 0, p2 = 0, flags = 1, special = 0, tag = 0, selected = 0;
    std::array<Side, 2> side;
};
struct Thing {
    Vec2 origin;
    int angle = 0, type = 1, options = 7, selected = 0;
    int splineId = 0;
    int height = 0;
};
struct ThingSpline {
    int id = 0, type = 1, options = 7;
    float spacing = 32, angleOffset = 0;
    bool smooth = true, closed = false;
    std::vector<Vec2> nodes;
};
struct ReferenceSettings {
    std::string path;
    Vec2 center{};
    float scale = 1, opacity = .5f, rotation = 0;
    bool operator==(const ReferenceSettings &b) const {
        return path == b.path && center == b.center && scale == b.scale && opacity == b.opacity &&
               rotation == b.rotation;
    }
};
struct Patch {
    int x = 0, y = 0, stepdir = 1, colormap = 0;
    std::string name;
};
struct Texture {
    std::string name;
    int width = 128, height = 128, wadIndex = 0;
    std::vector<Patch> patches;
};
struct Image {
    int width = 0, height = 0;
    std::vector<uint32_t> pixels;
};
constexpr int ML_TWOSIDED = 4;
} // namespace de
