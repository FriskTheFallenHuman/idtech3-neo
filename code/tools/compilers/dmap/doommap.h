// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../../../qcommon/doom_bsp.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace dm {
namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;
struct Vec2 {
    double x = 0, y = 0;
};
struct Vec3 {
    double x = 0, y = 0, z = 0;
};
inline Vec2 operator+(Vec2 a, Vec2 b) {
    return {a.x + b.x, a.y + b.y};
}
inline Vec2 operator-(Vec2 a, Vec2 b) {
    return {a.x - b.x, a.y - b.y};
}
inline Vec2 operator*(Vec2 a, double t) {
    return {a.x * t, a.y * t};
}
inline double cross(Vec2 a, Vec2 b) {
    return a.x * b.y - a.y * b.x;
}
inline double length(Vec2 a) {
    return std::hypot(a.x, a.y);
}
inline void require(bool ok, const std::string &message) {
    if (!ok)
        throw std::runtime_error(message);
}
Bytes readFile(const fs::path &path);
void writeFile(const fs::path &path, const Bytes &bytes);
std::string upper(std::string value);
std::string lower(std::string value);
std::string lumpName(const uint8_t *value, size_t size = 8);
struct Image {
    int width = 0, height = 0;
    Bytes pixels;
    bool masked = false;
};
void writePng(const fs::path &path, const Image &image);
class Wad {
    struct Entry {
        uint32_t offset, size;
        std::string name;
        bool flat;
    };
    struct Texture {
        int width, height;
        struct Patch {
            int x, y, index;
        };
        std::vector<Patch> patches;
    };
    Bytes data;
    std::vector<Entry> entries;
    std::map<std::string, int> lookup;
    std::vector<std::string> patchNames;
    std::map<std::string, Texture> textures;
    std::array<uint8_t, 768> palette{};
    Image patch(const std::string &name) const;

  public:
    explicit Wad(const fs::path &path);
    Image sprite(const std::string &name) const {
        auto im = patch(name);
        for (size_t i = 3; i < im.pixels.size(); i += 4)
            if (im.pixels[i] != 255)
                im.masked = true;
        return im;
    }
    bool has(const std::string &name, bool flat) const;
    Image image(const std::string &name, bool flat) const;
    std::vector<std::string> animation(const std::string &name, bool flat) const;
};
struct SectorDef {
    int floor = 0, ceiling = 0, light = 0, special = 0, tag = 0;
    std::string floorTex, ceilTex;
    bool operator==(const SectorDef &b) const {
        return floor == b.floor && ceiling == b.ceiling && light == b.light &&
               special == b.special && tag == b.tag && floorTex == b.floorTex &&
               ceilTex == b.ceilTex;
    }
};
struct Side {
    int xOffset = 0, yOffset = 0, sector = -1;
    std::string top, bottom, middle;
    SectorDef def;
};
struct Line {
    Vec2 a, b;
    int flags = 0, special = 0, tag = 0;
    std::array<int, 2> sides{{-1, -1}};
};
struct Cell {
    std::vector<Vec2> polygon;
    int sector = -1;
};
struct Sector {
    SectorDef def;
    std::vector<int> cells, neighbors;
    Vec2 min{1e30, 1e30}, max{-1e30, -1e30};
    int floorModel = -1, ceilModel = -1, linkedTag = -1;
    double stairHeight = 0, donutHeight = 0;
    double minFloor = 0, maxFloor = 0, minCeil = 0, maxCeil = 0;
};
struct Map {
    std::string name;
    std::vector<Line> lines;
    std::vector<Side> sides;
    std::vector<dbsp::Thing> things;
    std::vector<Cell> cells;
    std::vector<Sector> sectors;
    std::vector<std::string> warnings;
    int findSector(Vec2 p) const;
};
Map loadDwd(const fs::path &path, bool strict);
Map loadDwd(std::istream &input, const std::string &name, bool strict);
struct Assets {
    std::string mapName;
    fs::path base;
    const Wad &wad;
    std::set<std::string> scrolling;
    std::map<std::string, Image> images;
    std::map<std::string, bool> flats;
    fs::path sourceTextures;
    Image image(const std::string &name, bool flat) const;
    std::string texture(const std::string &name, bool flat);
    void write();
};
struct CompileResult {
    int sectors = 0, lines = 0, cells = 0, models = 0, surfaces = 0, brushes = 0, textures = 0;
};
CompileResult compile(Map &map, Assets &assets, const fs::path &output);
} // namespace dm
