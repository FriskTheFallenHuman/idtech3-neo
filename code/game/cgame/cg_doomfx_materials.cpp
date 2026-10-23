// SPDX-License-Identifier: GPL-3.0-or-later
#include "cg_doomfx_materials.h"
#include "../qcommon/doom_bsp.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace doomfx {
namespace {
constexpr float CellSize = 256, Tolerance = 2;
int Cell(float value) {
    return static_cast<int>(std::floor(value / CellSize));
}
float Dot(const float *a, const float *b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
template <typename T> const T *Lump(const void *data, dbsp::Lump lump) {
    const auto *header = static_cast<const dbsp::Header *>(data);
    return reinterpret_cast<const T *>(static_cast<const unsigned char *>(data) +
                                       header->lumps[lump].offset);
}
} // namespace

Material TextureMaterial(const char *name) {
    std::string texture(name);
    for (char &character : texture) {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
    texture = texture.substr(texture.find_last_of('/') + 1);
    auto starts = [&](const char *prefix) { return texture.rfind(prefix, 0) == 0; };
    if (starts("NUKAGE") || starts("SLIME")) {
        return Material::Slime;
    }
    if (starts("LAVA") || starts("BLOOD") || starts("FIRELAV")) {
        return Material::Lava;
    }
    if (starts("FWATER") || starts("SWATER") || starts("WATER")) {
        return Material::Water;
    }
    if (starts("METAL") || starts("SHAWN") || starts("SUPPORT") || starts("DOOR") ||
        starts("TEKWALL") || starts("COMPT") || starts("SILVER") || starts("STEP")) {
        return Material::Metal;
    }
    if (starts("WOOD")) {
        return Material::Wood;
    }
    if (starts("GLASS") || starts("WINDOW")) {
        return Material::Glass;
    }
    return Material::Wall;
}
bool IsLiquid(Material material) {
    return material == Material::Water || material == Material::Slime || material == Material::Lava;
}
void MaterialMap::Clear() {
    models.clear();
}
bool MaterialMap::Load(const void *data, int size, const char **error) {
    Clear();
    *error = data ? dbsp::validate(data, size) : "missing material map";
    if (*error) {
        return false;
    }
    const auto &header = *static_cast<const dbsp::Header *>(data);
    const int modelCount = header.lumps[dbsp::Models].length / sizeof(dbsp::Model);
    const int triangleCount = header.lumps[dbsp::Indices].length / (3 * sizeof(int32_t));
    if (triangleCount > 524288) {
        *error = "excessive material triangles";
        return false;
    }
    const auto *sourceModels = Lump<dbsp::Model>(data, dbsp::Models);
    const auto *surfaces = Lump<dbsp::Surface>(data, dbsp::Surfaces);
    const auto *vertices = Lump<dbsp::Vertex>(data, dbsp::Vertices);
    const auto *indices = Lump<int32_t>(data, dbsp::Indices);
    const auto *shaders = Lump<dbsp::Shader>(data, dbsp::Shaders);
    models.resize(modelCount);
    int references = 0;
    for (int modelIndex = 0; modelIndex < modelCount; ++modelIndex) {
        auto &model = models[modelIndex];
        const auto &source = sourceModels[modelIndex];
        for (int surfaceIndex = source.firstSurface;
             surfaceIndex < source.firstSurface + source.numSurfaces; ++surfaceIndex) {
            const auto &surface = surfaces[surfaceIndex];
            const auto &shader = shaders[surface.shader];
            if (!std::memchr(shader.name, 0, sizeof(shader.name))) {
                *error = "unterminated material shader";
                Clear();
                return false;
            }
            for (int index = 0; index < surface.numIndices; index += 3) {
                Triangle triangle{};
                triangle.material = TextureMaterial(shader.name);
                for (int corner = 0; corner < 3; ++corner) {
                    const auto &vertex = vertices[surface.firstVertex +
                                                  indices[surface.firstIndex + index + corner]];
                    for (int axis = 0; axis < 3; ++axis) {
                        const float value = vertex.xyz[axis];
                        if (!std::isfinite(value) || std::fabs(value) > 131072) {
                            *error = "invalid material vertex";
                            Clear();
                            return false;
                        }
                        triangle.vertices[corner][axis] = value;
                    }
                }
                const auto &a = triangle.vertices[0];
                float ab[3], ac[3];
                for (int axis = 0; axis < 3; ++axis) {
                    ab[axis] = triangle.vertices[1][axis] - a[axis];
                    ac[axis] = triangle.vertices[2][axis] - a[axis];
                }
                triangle.normal = {ab[1] * ac[2] - ab[2] * ac[1], ab[2] * ac[0] - ab[0] * ac[2],
                                   ab[0] * ac[1] - ab[1] * ac[0]};
                const float length = std::sqrt(Dot(triangle.normal.data(), triangle.normal.data()));
                if (length < 0.0001f) {
                    continue;
                }
                for (float &component : triangle.normal) {
                    component /= length;
                }
                float minimum[2] = {a[0], a[1]}, maximum[2] = {a[0], a[1]};
                for (const auto &vertex : triangle.vertices) {
                    for (int axis = 0; axis < 2; ++axis) {
                        minimum[axis] = std::min(minimum[axis], vertex[axis]);
                        maximum[axis] = std::max(maximum[axis], vertex[axis]);
                    }
                }
                const int firstX = Cell(minimum[0] - Tolerance);
                const int lastX = Cell(maximum[0] + Tolerance);
                const int firstY = Cell(minimum[1] - Tolerance);
                const int lastY = Cell(maximum[1] + Tolerance);
                references += (lastX - firstX + 1) * (lastY - firstY + 1);
                if (references > 2097152) {
                    *error = "excessive material cell references";
                    Clear();
                    return false;
                }
                const int triangleIndex = static_cast<int>(model.triangles.size());
                model.triangles.push_back(triangle);
                for (int x = firstX; x <= lastX; ++x) {
                    for (int y = firstY; y <= lastY; ++y) {
                        model.cells[{x, y}].push_back(triangleIndex);
                    }
                }
            }
        }
    }
    return true;
}
Material MaterialMap::Find(const float *point, const float *normal, int modelIndex) const {
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(point[axis]) || std::fabs(point[axis]) > 131072 ||
            !std::isfinite(normal[axis])) {
            return Material::Wall;
        }
    }
    if (modelIndex < 0 || modelIndex >= static_cast<int>(models.size())) {
        return Material::Wall;
    }
    const auto &model = models[modelIndex];
    const auto cell = model.cells.find({Cell(point[0]), Cell(point[1])});
    if (cell == model.cells.end()) {
        return Material::Wall;
    }
    Material material = Material::Wall;
    float nearest = Tolerance;
    for (int index : cell->second) {
        const auto &triangle = model.triangles[index];
        const float *planeNormal = triangle.normal.data();
        if (std::fabs(Dot(planeNormal, normal)) < 0.9f) {
            continue;
        }
        float delta[3];
        for (int axis = 0; axis < 3; ++axis) {
            delta[axis] = point[axis] - triangle.vertices[0][axis];
        }
        const float distance = std::fabs(Dot(delta, planeNormal));
        if (distance > nearest) {
            continue;
        }
        bool inside = true;
        for (int edge = 0; edge < 3; ++edge) {
            const auto &a = triangle.vertices[edge];
            const auto &b = triangle.vertices[(edge + 1) % 3];
            float side[3], offset[3], cross[3];
            for (int axis = 0; axis < 3; ++axis) {
                side[axis] = b[axis] - a[axis];
                offset[axis] = point[axis] - a[axis];
            }
            cross[0] = side[1] * offset[2] - side[2] * offset[1];
            cross[1] = side[2] * offset[0] - side[0] * offset[2];
            cross[2] = side[0] * offset[1] - side[1] * offset[0];
            if (Dot(cross, planeNormal) < -Tolerance * std::sqrt(Dot(side, side))) {
                inside = false;
                break;
            }
        }
        if (inside) {
            nearest = distance;
            material = triangle.material;
        }
    }
    return material;
}
} // namespace doomfx
