// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <map>
#include <vector>

namespace doomfx {
enum class Material { Wall, Metal, Wood, Glass, Water, Slime, Lava, Flesh, Count };
inline constexpr const char *MaterialNames[] = {"wall",  "metal", "wood", "glass",
                                                "water", "slime", "lava", "flesh"};
Material TextureMaterial(const char *name);
bool IsLiquid(Material material);

// Queries the actual draw triangles: collision brushes in existing Doom maps use caulk.
// Models retain local coordinates so doors and lifts can be queried after moving.
class MaterialMap {
  public:
    bool Load(const void *data, int size, const char **error);
    void Clear();
    Material Find(const float *point, const float *normal, int model = 0) const;

  private:
    struct Triangle {
        std::array<std::array<float, 3>, 3> vertices;
        std::array<float, 3> normal;
        Material material;
    };
    struct Model {
        std::vector<Triangle> triangles;
        std::map<std::pair<int, int>, std::vector<int>> cells;
    };
    std::vector<Model> models;
};
} // namespace doomfx
