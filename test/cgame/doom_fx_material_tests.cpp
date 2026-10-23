// SPDX-License-Identifier: GPL-3.0-or-later
#include "cgame/cg_doomfx_materials.h"
#include "qcommon/doom_bsp.h"
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
void Check(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
template <typename T>
void Append(std::vector<unsigned char> &data, dbsp::Lump lump, const std::vector<T> &values) {
    auto *header = reinterpret_cast<dbsp::Header *>(data.data());
    header->lumps[lump] = {static_cast<int>(data.size()),
                           static_cast<int>(values.size() * sizeof(T))};
    const auto *bytes = reinterpret_cast<const unsigned char *>(values.data());
    if (!values.empty()) {
        data.insert(data.end(), bytes, bytes + values.size() * sizeof(T));
    }
}
std::vector<unsigned char> Fixture() {
    std::vector<unsigned char> data(sizeof(dbsp::Header));
    auto *header = reinterpret_cast<dbsp::Header *>(data.data());
    header->ident = dbsp::Ident;
    header->version = dbsp::Version;
    for (auto &lump : header->lumps) {
        lump.offset = sizeof(dbsp::Header);
    }
    std::vector<dbsp::Shader> shaders(3);
    std::strcpy(shaders[0].name, "textures/test/nukage1");
    std::strcpy(shaders[1].name, "textures/test/stone");
    std::strcpy(shaders[2].name, "textures/test/metal1");
    Append(data, dbsp::Shaders, shaders);
    Append(data, dbsp::Planes, std::vector<dbsp::Plane>{{{0, 0, 1}, 0}});
    dbsp::Node node{};
    node.children[0] = node.children[1] = -1;
    Append(data, dbsp::Nodes, std::vector<dbsp::Node>{node});
    Append(data, dbsp::Leafs, std::vector<dbsp::Leaf>(1));
    std::vector<dbsp::Model> models(2);
    models[0].numSurfaces = 2;
    models[1].firstSurface = 2;
    models[1].numSurfaces = 1;
    Append(data, dbsp::Models, models);
    const float points[9][3] = {{0, 0, 0},   {128, 0, 0}, {0, 128, 0},  {128, 128, 0}, {0, 128, 0},
                                {128, 0, 0}, {0,
 0, 16},  {128, 0, 16}, {0, 128, 16}};
    std::vector<dbsp::Vertex> vertices(9);
    for (int vertex = 0; vertex < 9; ++vertex) {
        std::memcpy(vertices[vertex].xyz, points[vertex], sizeof(points[vertex]));
    }
    Append(data, dbsp::Vertices, vertices);
    Append(data, dbsp::Indices, std::vector<int32_t>{0, 1, 2, 0, 1, 2, 0, 1, 2});
    std::vector<dbsp::Surface> surfaces(3);
    for (int index = 0; index < 3; ++index) {
        surfaces[index].shader = index;
        surfaces[index].type = 3;
        surfaces[index].lightmap = -3;
        surfaces[index].firstVertex = surfaces[index].firstIndex = index * 3;
        surfaces[index].numVertices = surfaces[index].numIndices = 3;
    }
    Append(data, dbsp::Surfaces, surfaces);
    dbsp::Sector sector{};
    sector.ceiling = 128;
    sector.light = 160;
    sector.floorModel = sector.ceilingModel = -1;
    Append(data, dbsp::Sectors, std::vector<dbsp::Sector>{sector});
    Append(data, dbsp::SurfaceSectors,
           std::vector<dbsp::SurfaceSector>(3, {0, -1, dbsp::Floor, 0}));
    return data;
}
} // namespace
int main() {
    try {
        using doomfx::Material;
        const std::pair<const char *, Material> names[] = {
            {"textures/e1m1/nukage2", Material::Slime},
            {"FWATER1", Material::Water},
            {"swATER4", Material::Water},
            {"lava3", Material::Lava},
            {"FIRELAVA", Material::Lava},
            {"METAL2", Material::Metal},
            {"doortrak", Material::Metal},
            {"WOOD1", Material::Wood},
            {"glass", Material::Glass},
            {"BROWN1", Material::Wall}};
        for (const auto &entry : names) {
            Check(doomfx::TextureMaterial(entry.first) == entry.second, "Texture material mapping");
        }
        auto data = Fixture();
        doomfx::MaterialMap map;
        const char *error = nullptr;
        Check(map.Load(data.data(), static_cast<int>(data.size()), &error),
              error ? error : "Fixture lo
ad");
        const float up[] = {0, 0, 1}, side[] = {1, 0, 0};
        const float slime[] = {30, 30, 0.125f}, wall[] = {100, 100, 0};
        const float outside[] = {-30, -30, 0}, above[] = {30, 30, 10};
        const float mover[] = {30, 30, 16};
        Check(map.Find(slime, up) == Material::Slime, "Hit plane tolerates collision epsilon");
        Check(map.Find(wall, up) == Material::Wall,
              "Adjacent triangle with identical bounds does not inherit slime");
        Check(map.Find(slime, side) == Material::Wall, "Wall normal cannot match liquid floor");
        Check(map.Find(outside, up) == Material::Wall, "Outside triangle cannot match floor");
        Check(map.Find(above, up) == Material::Wall, "Floor below the hit cannot classify a wall");
        Check(map.Find(mover, up, 1) == Material::Metal,
              "Inline model queried in local coordinates");
        Check(map.Find(mover, up, 9000) == Material::Wall, "Invalid model is bounded");
        float invalid[] = {std::numeric_limits<float>::quiet_NaN(), 0, 0};
        Check(map.Find(invalid, up) == Material::Wall, "Nonfinite query rejected");
        for (int size : {0, 12, static_cast<int>(sizeof(dbsp::Header))}) {
            Check(!map.Load(data.data(), size, &error), "Truncated map rejected");
        }
        auto *header = reinterpret_cast<dbsp::Header *>(data.data());
        auto *vertices =
            reinterpret_cast<dbsp::Vertex *>(data.data() + header->lumps[dbsp::Vertices].offset);
        vertices[0].xyz[0] = std::numeric_limits<float>::quiet_NaN();
        Check(!map.Load(data.data(), static_cast<int>(data.size()), &error),
              "Nonfinite geometry rejected");
        data = Fixture();
        header = reinterpret_cast<dbsp::Header *>(data.data());
        std::memset(data.data() + header->lumps[dbsp::Shaders].offset, 'X', 64);
        Check(!map.Load(data.data(), static_cast<int>(data.size()), &error),
              "Unterminated shader name rejected");
        da
ta = Fixture();
        header = reinterpret_cast<dbsp::Header *>(data.data());
        *reinterpret_cast<int32_t *>(data.data() + header->lumps[dbsp::Indices].offset) = 999;
        Check(!map.Load(data.data(), static_cast<int>(data.size()), &error),
              "Out-of-range triangle reference rejected");
        Check(map.Find(slime, up) == Material::Wall, "Failed reload clears stale geometry");
        std::puts("Doom FX materials: independent triangle/normal/model tests and malformed inputs "
                  "passed.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Doom FX material test: %s\n", error.what());
        return 1;
    }
}
