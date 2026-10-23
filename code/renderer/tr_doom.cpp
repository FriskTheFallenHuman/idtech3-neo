// SPDX-License-Identifier: GPL-3.0-or-later
#include "tr_local.h"
#include "tr_doomlighting.h"
#include <initializer_list>

void R_LoadDoomData(world_t *world, const dbsp::Header *header) {
    const byte *data = reinterpret_cast<const byte *>(header);
    const auto &sectors = header->lumps[dbsp::Sectors];
    world->doomNumSectors = sectors.length / sizeof(dbsp::Sector);
    if (!world->doomNumSectors)
        return;
    world->doomSectors = static_cast<dbsp::Sector *>(ri.Hunk_Alloc(sectors.length, h_low));
    memcpy(world->doomSectors, data + sectors.offset, sectors.length);
    const auto &lines = header->lumps[dbsp::Lines], &sides = header->lumps[dbsp::Sides];
    world->doomNumLines = lines.length / sizeof(dbsp::Line);
    world->doomNumSides = sides.length / sizeof(dbsp::Side);
    world->doomLines = static_cast<dbsp::Line *>(ri.Hunk_Alloc(lines.length, h_low));
    world->doomSides = static_cast<dbsp::Side *>(ri.Hunk_Alloc(sides.length, h_low));
    memcpy(world->doomLines, data + lines.offset, lines.length);
    memcpy(world->doomSides, data + sides.offset, sides.length);
    const auto &surfaces = header->lumps[dbsp::SurfaceSectors];
    world->doomSurfaceSectors =
        static_cast<dbsp::SurfaceSector *>(ri.Hunk_Alloc(surfaces.length, h_low));
    memcpy(world->doomSurfaceSectors, data + surfaces.offset, surfaces.length);
    world->doomLightMin =
        static_cast<int *>(ri.Hunk_Alloc(world->doomNumSectors * sizeof(int), h_low));
    world->doomLightOverride =
        static_cast<int *>(ri.Hunk_Alloc(world->doomNumSectors * sizeof(int), h_low));
    for (int i = 0; i < world->doomNumSectors; ++i) {
        world->doomLightMin[i] = world->doomSectors[i].light;
        world->doomLightOverride[i] = -1;
    }
    for (int i = 0; i < world->doomNumLines; ++i) {
        const auto &l = world->doomLines[i];
        if (l.sides[1] < 0)
            continue;
        int a = world->doomSides[l.sides[0]].sector, b = world->doomSides[l.sides[1]].sector;
        world->doomLightMin[a] = min(world->doomLightMin[a], world->doomSectors[b].light);
        world->doomLightMin[b] = min(world->doomLightMin[b], world->doomSectors[a].light);
    }
    world->doomLightTime = -1;
    int n = world->doomNumSectors, p = 0;
    world->doomVisible = static_cast<byte *>(ri.Hunk_Alloc(n, h_low));
    memset(world->doomVisible, 1, n);
    world->doomFloors = static_cast<float *>(ri.Hunk_Alloc(n * sizeof(float), h_low));
    world->doomCeilings = static_cast<float *>(ri.Hunk_Alloc(n * sizeof(float), h_low));
    world->doomQueue = static_cast<int *>(ri.Hunk_Alloc(n * sizeof(int), h_low));
    world->doomPortalHead = static_cast<int *>(ri.Hunk_Alloc(n * sizeof(int), h_low));
    int bytes = world->doomNumLines * 2 * sizeof(int);
    world->doomPortalNext = static_cast<int *>(ri.Hunk_Alloc(bytes, h_low));
    world->doomPortalNeighbor = static_cast<int *>(ri.Hunk_Alloc(bytes, h_low));
    for (int i = 0; i < n; ++i)
        world->doomPortalHead[i] = -1;
    for (int i = 0; i < world->doomNumLines; ++i) {
        auto &line = world->doomLines[i];
        if (line.sides[0] < 0 || line.sides[1] < 0)
            continue;
        int a = world->doomSides[line.sides[0]].sector, b = world->doomSides[line.sides[1]].sector;
        if (a == b)
            continue;
        for (int side = 0; side < 2; ++side) {
            int from = side ? b : a, to = side ? a : b;
            world->doomPortalNeighbor[p] = to;
            world->doomPortalNext[p] = world->doomPortalHead[from];
            world->doomPortalHead[from] = p++;
        }
    }
    world->doomVisibleCount = -1;
}

void R_InitDoomSurfaces(world_t *world) {
    if (!world->doomNumSectors)
        return;
    int bytes = world->numsurfaces * sizeof(shader_t *);
    world->doomBaseShaders = static_cast<shader_t **>(ri.Hunk_Alloc(bytes, h_low));
    world->doomSwitchShaders = static_cast<shader_t **>(ri.Hunk_Alloc(bytes, h_low));
    world->numfogs = doomlight::Levels + 1;
    world->fogs = static_cast<fog_t *>(ri.Hunk_Alloc(world->numfogs * sizeof(fog_t), h_low));
    for (int i = 1; i < world->numfogs; ++i) {
        world->fogs[i].colorInt = ColorBytes4(0, 0, 0, 1);
    }
    if (!tr.doomFogImage) {
        byte pixels[doomlight::Width * doomlight::Levels * 4];
        for (int row = 0; row < doomlight::Levels; ++row) {
            for (int x = 0; x < doomlight::Width; ++x) {
                byte *pixel = pixels + (row * doomlight::Width + x) * 4;
                pixel[0] = pixel[1] = pixel[2] = 0;
                pixel[3] = doomlight::Alpha(row * 16, x * doomlight::DistanceStep);
            }
        }
        tr.doomFogImage = R_CreateImage("*doomDistance", pixels, doomlight::Width,
                                        doomlight::Levels, qfalse, qfalse, GL_CLAMP);
    }
    for (int i = 0; i < world->numsurfaces; ++i) {
        auto *base = world->surfaces[i].shader;
        world->doomBaseShaders[i] = world->doomSwitchShaders[i] = base;
        if (world->doomSurfaceSectors[i].line < 0)
            continue;
        char name[MAX_QPATH];
        Q_strncpyz(name, base->name, sizeof(name));
        char *texture = strrchr(name, '/');
        texture = texture ? texture + 1 : name;
        if (Q_stricmpn(texture, "sw", 2) || (texture[2] != '1' && texture[2] != '2'))
            continue;
        texture[2] = texture[2] == '1' ? '2' : '1';
        auto *alternate = R_FindShader(name, LIGHTMAP_BY_VERTEX, qtrue);
        if (!alternate->defaultShader)
            world->doomSwitchShaders[i] = alternate;
    }
}

void RE_SetDoomSwitches(const char *state) {
    world_t *w = tr.world;
    if (!w || !w->doomBaseShaders)
        return;
    int length = int(strlen(state));
    for (int i = 0; i < w->numsurfaces; ++i) {
        int line = w->doomSurfaceSectors[i].line;
        int digit = line >= 0 && line / 4 < length ? state[line / 4] : '0';
        int bits = digit >= '0' && digit <= '9'   ? digit - '0'
                   : digit >= 'a' && digit <= 'f' ? digit - 'a' + 10
                                                  : 0;
        bool on = line >= 0 && (bits & (1 << (line % 4)));
        shader_t *next = on ? w->doomSwitchShaders[i] : w->doomBaseShaders[i];
        if (next != w->surfaces[i].shader)
            ri.Printf(PRINT_DEVELOPER, "Doom switch %d: %s\n", line, on ? "pressed" : "released");
        w->surfaces[i].shader = next;
    }
}

static int R_DoomPointSector(const vec3_t p) {
    world_t *w = tr.world;
    float nearest = -1e30f;
    int sector = -1;
    for (int i = 0; i < w->doomNumLines; ++i) {
        auto &l = w->doomLines[i];
        float dx = l.b[0] - l.a[0];
        if (fabs(dx) < 0.0001f || p[0] < min(l.a[0], l.b[0]) || p[0] >= max(l.a[0], l.b[0]))
            continue;
        float y = l.a[1] + (l.b[1] - l.a[1]) * (p[0] - l.a[0]) / dx;
        if (y <= p[1] && y > nearest) {
            nearest = y;
            int side = l.sides[dx > 0 ? 1 : 0];
            sector = side < 0 ? -1 : w->doomSides[side].sector;
        }
    }
    return sector;
}

void R_UpdateDoomVisibility() {
    world_t *w = tr.world;
    if (!w || !w->doomVisible)
        return;
    float modelZ[256] = {};
    bool present[256] = {};
    for (int i = 0; i < tr.refdef.num_entities; ++i) {
        auto &ent = tr.refdef.entities[i].e;
        auto *model = R_GetModelByHandle(ent.hModel);
        if (ent.reType != RT_MODEL || model->type != MOD_BRUSH)
            continue;
        for (int j = 0; j < w->doomNumSectors; ++j) {
            for (int id : {w->doomSectors[j].floorModel, w->doomSectors[j].ceilingModel}) {
                if (id > 0 && id < 256 && model->bmodel == w->bmodels + id) {
                    present[id] = true;
                    modelZ[id] = ent.origin[2];
                }
            }
        }
    }
    for (int i = 0; i < w->doomNumSectors; ++i) {
        auto &s = w->doomSectors[i];
        w->doomFloors[i] = s.floor;
        w->doomCeilings[i] = s.ceiling;
        // Missing mover snapshots must not hide potentially visible sectors.
        if (s.floorModel > 0)
            w->doomFloors[i] = present[s.floorModel] ? s.floor + modelZ[s.floorModel] : -1e9f;
        if (s.ceilingModel > 0)
            w->doomCeilings[i] =
                present[s.ceilingModel] ? s.ceiling + modelZ[s.ceilingModel] : 1e9f;
    }
    int start = R_DoomPointSector(tr.viewParms.or.origin);
    int count = w->doomNumSectors;
    if (r_novis->integer || start < 0 || tr.viewParms.or.origin[2] < w->doomFloors[start] ||
        tr.viewParms.or.origin[2] > w->doomCeilings[start]) {
        memset(w->doomVisible, 1, count);
    } else {
        memset(w->doomVisible, 0, count);
        w->doomVisible[start] = 1;
        int head = 0;
        count = 1;
        w->doomQueue[0] = start;
        while (head < count) {
            int from = w->doomQueue[head++];
            for (int edge = w->doomPortalHead[from]; edge >= 0; edge = w->doomPortalNext[edge]) {
                int to = w->doomPortalNeighbor[edge];
                if (w->doomVisible[to] || min(w->doomCeilings[from], w->doomCeilings[to]) <=
                                              max(w->doomFloors[from], w->doomFloors[to]) + .001f)
                    continue;
                w->doomVisible[to] = 1;
                w->doomQueue[count++] = to;
            }
        }
    }
    if (r_showcluster->integer && count != w->doomVisibleCount)
        ri.Printf(PRINT_ALL, "Doom visible sectors: %d/%d\n", count, w->doomNumSectors);
    w->doomVisibleCount = count;
}

qboolean R_DoomSurfaceVisible(msurface_t *surface) {
    world_t *w = tr.world;
    if (!w || !w->doomVisible)
        return qtrue;
    int index = int(surface - w->surfaces);
    if (index < 0 || index >= w->numsurfaces)
        return qtrue;
    int sector = w->doomSurfaceSectors[index].sector;
    return w->doomVisible[sector] != 0;
}
int R_DoomSectorLight(int sector, int time) {
    if (tr.refdef.rdflags & RDF_DOOM_FULLBRIGHT) {
        return 255;
    }
    world_t *w = tr.world;
    if (!w || sector < 0 || sector >= w->doomNumSectors)
        return 255;
    if (w->doomLightOverride[sector] >= 0)
        return w->doomLightOverride[sector];
    const auto &s = w->doomSectors[sector];
    int high = s.light, low = w->doomLightMin[sector];
    unsigned tick = unsigned(time) * 35 / 1000;
    switch (s.special) {
    case 1: {
        unsigned phase = tick / 7;
        unsigned hash = (phase + unsigned(sector) * 97) * 1664525u + 1013904223u;
        return (hash & 7) < 2 ? low : high;
    }
    case 2:
    case 4:
    case 12:
        return ((tick + (s.special == 12 ? 0 : sector * 7)) % 20) < 5 ? high : low;
    case 3:
    case 13:
        return ((tick + (s.special == 13 ? 0 : sector * 7)) % 40) < 5 ? high : low;
    case 8: {
        int range = high - low;
        if (range <= 0)
            return high;
        int phase = int(tick * 8 % (range * 2));
        return high - (phase < range ? phase : range * 2 - phase);
    }
    case 17: {
        unsigned hash = (tick / 4 + unsigned(sector) * 11) * 1103515245u + 12345u;
        return max(low + 16, high - int((hash >> 16) & 3) * 16);
    }
    default:
        return high;
    }
}
void RE_SetDoomSectorLight(int sector, int light) {
    if (!tr.world || sector < 0 || sector >= tr.world->doomNumSectors)
        return;
    if (light == 256) {
        tr.world->doomSectors[sector].special = 3;
        tr.world->doomLightOverride[sector] = -1;
    } else
        tr.world->doomLightOverride[sector] = light < 0 ? -1 : min(light, 255);
    tr.world->doomLightTime = -1;
}
void R_UpdateDoomSectorLights() {
    world_t *w = tr.world;
    if (!w || !w->doomNumSectors || w->doomLightTime == tr.refdef.time)
        return;
    w->doomLightTime = tr.refdef.time;
    for (int i = 0; i < w->numsurfaces; ++i) {
        auto *data = w->surfaces[i].data;
        if (*data != SF_TRIANGLES)
            continue;
        auto *tri = reinterpret_cast<srfTriangles_t *>(data);
        int sector = w->doomSurfaceSectors[i].sector;
        int light = R_DoomSectorLight(sector, tr.refdef.time);
        const auto &source = w->doomSurfaceSectors[i];
        if (source.line >= 0 && (source.kind == dbsp::Wall || source.kind == dbsp::MaskedWall)) {
            const auto &line = w->doomLines[source.line];
            // Doom gives axis-aligned walls one extra or one fewer light level.
            if (line.a[0] == line.b[0]) {
                light += 16;
            } else if (line.a[1] == line.b[1]) {
                light -= 16;
            }
        }
        w->surfaces[i].fogIndex =
            (w->surfaces[i].shader->isSky || (tr.refdef.rdflags & RDF_DOOM_FULLBRIGHT))
                ? 0
                : doomlight::FogIndex(light);
        for (int j = 0; j < tri->numVerts; ++j) {
            tri->verts[j].color[0] = tri->verts[j].color[1] = tri->verts[j].color[2] = 255;
        }
    }
}
int R_DoomLightForPoint(const vec3_t p) {
    world_t *w = tr.world;
    if (!w || !w->doomNumSectors)
        return -1;
    int sector = R_DoomPointSector(p);
    return sector < 0 ? -1 : R_DoomSectorLight(sector, tr.refdef.time);
}

void R_LightDoomPoly(srfPoly_t *poly) {
    poly->fogIndex = 0;
    if (tr.refdef.rdflags & (RDF_DOOM_FULLBRIGHT | RDF_NOWORLDMODEL)) {
        return;
    }
    const auto &color = poly->verts[0].modulate;
    if (color[0] == 255 && color[1] == 255 && color[2] == 255) {
        return;
    }
    vec3_t center = {}, delta;
    for (int i = 0; i < poly->numVerts; ++i) {
        VectorAdd(center, poly->verts[i].xyz, center);
    }
    VectorScale(center, 1.0f / poly->numVerts, center);
    const int sector = R_DoomPointSector(center);
    if (sector < 0) {
        return;
    }
    VectorSubtract(center, tr.viewParms.or.origin, delta);
    const int brightness = 255 - doomlight::Alpha(R_DoomSectorLight(sector, tr.refdef.time),
                                                  DotProduct(delta, tr.viewParms.or.axis[0]));
    // Sprites use one shade for the whole patch, like Doom's original columns.
    // This also avoids a separate fog pass filling the transparent patch bounds.
    for (int i = 0; i < poly->numVerts; ++i) {
        poly->verts[i].modulate[0] = poly->verts[i].modulate[1] = poly->verts[i].modulate[2] =
            static_cast<byte>(brightness);
    }
}

void R_DoomVisibilityInfo() {
    if (!tr.world || !tr.world->doomNumSectors) {
        return;
    }
    int fogged = 0;
    for (int i = 0; i < tr.world->numsurfaces; ++i) {
        fogged += tr.world->surfaces[i].fogIndex != 0;
    }
    const int sector = R_DoomPointSector(tr.viewParms.or.origin);
    ri.Printf(
        PRINT_ALL,
        "Doom view: at=%.1f %.1f %.1f sector=%d light=%d visible=%d/%d fogged=%d fullbright=%d\n",
        tr.viewParms.or.origin[0], tr.viewParms.or.origin[1], tr.viewParms.or.origin[2], sector,
        R_DoomSectorLight(sector, tr.refdef.time), tr.world->doomVisibleCount,
        tr.world->doomNumSectors, fogged, (tr.refdef.rdflags & RDF_DOOM_FULLBRIGHT) != 0);
}
