// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include <cmath>

// IBSP 48: Quake geometry/collision lumps plus Doom sector/line/thing metadata.
// All scalar values are little endian. Names are zero-terminated, uppercase.
namespace dbsp {
constexpr int Version = 48;
constexpr int Ident = 0x50534249;
enum Lump {
    Entities,
    Shaders,
    Planes,
    Nodes,
    Leafs,
    LeafSurfaces,
    LeafBrushes,
    Models,
    Brushes,
    BrushSides,
    Vertices,
    Indices,
    Fogs,
    Surfaces,
    Lightmaps,
    LightGrid,
    Visibility,
    Sectors,
    Lines,
    Sides,
    SurfaceSectors,
    Things,
    LumpCount
};
struct LumpInfo {
    int32_t offset, length;
};
struct Header {
    int32_t ident, version;
    LumpInfo lumps[LumpCount];
};
struct Shader {
    char name[64];
    int32_t surfaceFlags, contents;
};
struct Plane {
    float normal[3], distance;
};
struct Node {
    int32_t plane, children[2], mins[3], maxs[3];
};
struct Leaf {
    int32_t cluster, area, mins[3], maxs[3], firstSurface, numSurfaces, firstBrush, numBrushes;
};
struct Model {
    float mins[3], maxs[3];
    int32_t firstSurface, numSurfaces, firstBrush, numBrushes;
};
struct Brush {
    int32_t firstSide, numSides, shader;
};
struct BrushSide {
    int32_t plane, shader;
};
struct Vertex {
    float xyz[3], st[2], lightmap[2], normal[3];
    uint8_t color[4];
};
struct Surface {
    int32_t shader, fog, type, firstVertex, numVertices, firstIndex, numIndices;
    int32_t lightmap, lightmapX, lightmapY, lightmapWidth, lightmapHeight;
    float lightmapOrigin[3], lightmapVectors[3][3];
    int32_t patchWidth, patchHeight;
};
struct Sector {
    int32_t floor, ceiling, light, special, tag, floorModel, ceilingModel;
    char floorTexture[12], ceilingTexture[12];
    float bounds[4];
};
struct Line {
    float a[2], b[2];
    int32_t flags, special, tag, sides[2];
};
struct Side {
    int32_t xOffset, yOffset, sector;
    char upper[12], lower[12], middle[12];
};
enum SurfaceKind { Wall, Floor, Ceiling, MaskedWall };
struct SurfaceSector {
    int32_t sector, line, kind, flags;
};
struct Thing {
    int32_t x, y, angle, type, options, sector;
};
static_assert(sizeof(Header) == 184 && sizeof(Vertex) == 44 && sizeof(Surface) == 104, "BSP ABI");
static_assert(sizeof(Sector) == 68 && sizeof(Line) == 36 && sizeof(Side) == 48, "Doom lump ABI");

// Both engine loaders reject malformed extensions before dereferencing any lump.
inline const char *validate(const void *buffer, int length) {
    if (length < int(sizeof(Header)))
        return "truncated BSP header";
    const auto *h = static_cast<const Header *>(buffer);
    const auto *base = static_cast<const uint8_t *>(buffer);
    if (h->ident != Ident || h->version != Version)
        return "expected IBSP version 48";
    const int sizes[LumpCount] = {1, 72, 16,  36, 48, 4, 4,  40, 12, 8,  44,
                                  4, 72, 104, 1,  1,  1, 68, 36, 48, 16, 24};
    for (int i = 0; i < LumpCount; ++i) {
        const auto &l = h->lumps[i];
        if (l.offset < int(sizeof(Header)) || l.offset > length || l.length < 0 ||
            l.length > length - l.offset || l.offset % 4 || l.length % sizes[i])
            return "invalid BSP lump bounds or record size";
        for (int j = 0; j < i; ++j) {
            const auto &p = h->lumps[j];
            if (l.length && p.length && l.offset < p.offset + p.length &&
                p.offset < l.offset + l.length)
                return "overlapping BSP lumps";
        }
    }
    int counts[LumpCount];
    for (int i = 0; i < LumpCount; ++i)
        counts[i] = h->lumps[i].length / sizes[i];
    if (!counts[Models] || counts[Models] > 256 || !counts[Nodes] || !counts[Leafs] ||
        !counts[Planes] || !counts[Sectors] || counts[SurfaceSectors] != counts[Surfaces])
        return "missing or excessive BSP records";
    if (h->lumps[Lightmaps].length || h->lumps[LightGrid].length)
        return "Doom BSP must not contain baked lighting";
    auto range = [](int first, int count, int total) {
        return first >= 0 && count >= 0 && first <= total && count <= total - first;
    };
    const auto *models = reinterpret_cast<const Model *>(base + h->lumps[Models].offset);
    for (int i = 0; i < counts[Models]; ++i)
        if (!range(models[i].firstSurface, models[i].numSurfaces, counts[Surfaces]) ||
            !range(models[i].firstBrush, models[i].numBrushes, counts[Brushes]))
            return "invalid model ranges";
    const auto *nodes = reinterpret_cast<const Node *>(base + h->lumps[Nodes].offset);
    for (int i = 0; i < counts[Nodes]; ++i) {
        if (nodes[i].plane < 0 || nodes[i].plane >= counts[Planes])
            return "invalid node plane";
        for (int c : nodes[i].children)
            if (c >= 0 ? (c <= i || c >= counts[Nodes])
                       : (c == INT32_MIN || -c - 1 >= counts[Leafs]))
                return "invalid or cyclic BSP child";
    }
    const auto *leafs = reinterpret_cast<const Leaf *>(base + h->lumps[Leafs].offset);
    for (int i = 0; i < counts[Leafs]; ++i)
        if (!range(leafs[i].firstSurface, leafs[i].numSurfaces, counts[LeafSurfaces]) ||
            !range(leafs[i].firstBrush, leafs[i].numBrushes, counts[LeafBrushes]) ||
            leafs[i].cluster > 0 || leafs[i].area != 0)
            return "invalid Doom leaf";
    for (int n = LeafSurfaces; n <= LeafBrushes; ++n) {
        auto *indices = reinterpret_cast<const int32_t *>(base + h->lumps[n].offset);
        int total = n == LeafSurfaces ? counts[Surfaces] : counts[Brushes];
        for (int i = 0; i < counts[n]; ++i)
            if (indices[i] < 0 || indices[i] >= total)
                return "invalid leaf reference";
    }
    const auto *brushes = reinterpret_cast<const Brush *>(base + h->lumps[Brushes].offset);
    const auto *sides = reinterpret_cast<const BrushSide *>(base + h->lumps[BrushSides].offset);
    for (int i = 0; i < counts[Brushes]; ++i)
        if (brushes[i].numSides < 6 ||
            !range(brushes[i].firstSide, brushes[i].numSides, counts[BrushSides]) ||
            brushes[i].shader < 0 || brushes[i].shader >= counts[Shaders])
            return "invalid brush";
    for (int i = 0; i < counts[BrushSides]; ++i)
        if (sides[i].plane < 0 || sides[i].plane >= counts[Planes] || sides[i].shader < 0 ||
            sides[i].shader >= counts[Shaders])
            return "invalid brush side";
    const auto *surfaces = reinterpret_cast<const Surface *>(base + h->lumps[Surfaces].offset);
    const auto *indices = reinterpret_cast<const int32_t *>(base + h->lumps[Indices].offset);
    const auto *surfaceSectors =
        reinterpret_cast<const SurfaceSector *>(base + h->lumps[SurfaceSectors].offset);
    for (int i = 0; i < counts[Surfaces]; ++i) {
        const auto &s = surfaces[i];
        if (s.type != 3 || s.lightmap != -3 || s.shader < 0 || s.shader >= counts[Shaders] ||
            !range(s.firstVertex, s.numVertices, counts[Vertices]) ||
            !range(s.firstIndex, s.numIndices, counts[Indices]) || s.numIndices % 3)
            return "invalid Doom draw surface";
        for (int j = 0; j < s.numIndices; ++j)
            if (indices[s.firstIndex + j] < 0 || indices[s.firstIndex + j] >= s.numVertices)
                return "invalid draw index";
        if (surfaceSectors[i].line < -1 || surfaceSectors[i].line >= counts[Lines] ||
            surfaceSectors[i].kind < Wall || surfaceSectors[i].kind > MaskedWall)
            return "invalid surface linedef or kind";
        if (surfaceSectors[i].sector < 0 || surfaceSectors[i].sector >= counts[Sectors])
            return "invalid surface sector";
    }
    const auto *sectors = reinterpret_cast<const Sector *>(base + h->lumps[Sectors].offset);
    for (int i = 0; i < counts[Sectors]; ++i) {
        auto &s = sectors[i];
        if (s.floor > s.ceiling || s.light < 0 || s.light > 255 || s.floorModel < -1 ||
            s.floorModel >= counts[Models] || s.ceilingModel < -1 ||
            s.ceilingModel >= counts[Models])
            return "invalid sector data";
    }
    const auto *dsides = reinterpret_cast<const Side *>(base + h->lumps[Sides].offset);
    for (int i = 0; i < counts[Sides]; ++i)
        if (dsides[i].sector < 0 || dsides[i].sector >= counts[Sectors])
            return "invalid sidedef sector";
    const auto *lines = reinterpret_cast<const Line *>(base + h->lumps[Lines].offset);
    for (int i = 0; i < counts[Lines]; ++i) {
        for (int axis = 0; axis < 2; ++axis) {
            if (!std::isfinite(lines[i].a[axis]) || !std::isfinite(lines[i].b[axis]) ||
                std::fabs(lines[i].a[axis]) > 1000000 || std::fabs(lines[i].b[axis]) > 1000000)
                return "invalid linedef coordinates";
        }
        if (lines[i].sides[0] < 0 || lines[i].sides[0] >= counts[Sides] || lines[i].sides[1] < -1 ||
            lines[i].sides[1] >= counts[Sides])
            return "invalid linedef sidedef";
    }
    return nullptr;
}
} // namespace dbsp
