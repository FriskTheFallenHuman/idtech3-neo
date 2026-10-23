// SPDX-License-Identifier: GPL-3.0-or-later
#include "cg_local.h"
#include "cg_doommap.h"
#include "cg_doomhud.h"
#include "../qcommon/doom_bsp.h"
#include <cstdlib>
#include <initializer_list>

namespace {
constexpr int MaxLines = 16384, MaxSides = 32768, MaxSectors = 4096;
dbsp::Line lines[MaxLines];
dbsp::Side sides[MaxSides];
dbsp::Sector sectors[MaxSectors];
byte discovered[MaxLines], pixels[256 * 256 * 4];
int lineCount, nextUpdate;
float centerX, centerY, scaleX, scaleY, mapWidth, mapHeight;

void Line(float ax, float ay, float bx, float by, byte red, byte green, byte blue) {
    int x = static_cast<int>(128 + (ax - centerX) * scaleX);
    int y = static_cast<int>(128 - (ay - centerY) * scaleY);
    const int endX = static_cast<int>(128 + (bx - centerX) * scaleX);
    const int endY = static_cast<int>(128 - (by - centerY) * scaleY);
    const int dx = abs(endX - x), dy = -abs(endY - y);
    const int sx = x < endX ? 1 : -1, sy = y < endY ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        if (x >= 0 && x < 256 && y >= 0 && y < 256) {
            byte *pixel = pixels + (y * 256 + x) * 4;
            pixel[0] = red;
            pixel[1] = green;
            pixel[2] = blue;
        }
        if (x == endX && y == endY) {
            break;
        }
        const int twice = error * 2;
        if (twice >= dy) {
            error += dy;
            x += sx;
        }
        if (twice <= dx) {
            error += dx;
            y += sy;
        }
    }
}
} // namespace
void CG_DoomMapInit() {
    lineCount = nextUpdate = 0;
    memset(discovered, 0, sizeof(discovered));
    fileHandle_t file = 0;
    const int length = trap_FS_FOpenFile(cgs.mapname, &file, FS_READ);
    if (!file || length <= 0 || length > 64 * 1024 * 1024) {
        if (file) {
            trap_FS_FCloseFile(file);
        }
        return;
    }
    auto *data = static_cast<byte *>(std::malloc(length));
    if (!data) {
        trap_FS_FCloseFile(file);
        CG_Error("No memory for Doom automap");
        return;
    }
    trap_FS_Read(data, length, file);
    trap_FS_FCloseFile(file);
    if (length < static_cast<int>(sizeof(dbsp::Header)) ||
        reinterpret_cast<const dbsp::Header *>(data)->version != dbsp::Version) {
        std::free(data);
        return;
    }
    const char *error = dbsp::validate(data, length);
    const auto &header = *reinterpret_cast<const dbsp::Header *>(data);
    if (error || header.lumps[dbsp::Lines].length > sizeof(lines) ||
        header.lumps[dbsp::Sides].length > sizeof(sides) ||
        header.lumps[dbsp::Sectors].length > sizeof(sectors)) {
        std::free(data);
        CG_Error("Invalid or oversized Doom automap: %s", error ? error : cgs.mapname);
        return;
    }
    memcpy(lines, data + header.lumps[dbsp::Lines].offset, header.lumps[dbsp::Lines].length);
    memcpy(sides, data + header.lumps[dbsp::Sides].offset, header.lumps[dbsp::Sides].length);
    memcpy(sectors, data + header.lumps[dbsp::Sectors].offset, header.lumps[dbsp::Sectors].length);
    lineCount = header.lumps[dbsp::Lines].length / sizeof(dbsp::Line);
    std::free(data);
    if (!lineCount) {
        return;
    }
    float left = lines[0].a[0], right = left, bottom = lines[0].a[1], top = bottom;
    for (int i = 0; i < lineCount; ++i) {
        for (const auto *point : {lines[i].a, lines[i].b}) {
            left = min(left, point[0]);
            right = max(right, point[0]);
            bottom = min(bottom, point[1]);
            top = max(top, point[1]);
        }
    }
    centerX = (left + right) * 0.5f;
    centerY = (bottom + top) * 0.5f;
    mapWidth = max(1.0f, right - left);
    mapHeight = max(1.0f, top - bottom);
}
void CG_DoomMapUpdate() {
    if (!cg.snap || !cg.snap->ps.stats[STAT_DOOM_WEAPONS] || cg.time < nextUpdate) {
        return;
    }
    nextUpdate = cg.time + 250;
    vec3_t eye;
    VectorCopy(cg.predictedPlayerState.origin, eye);
    eye[2] += cg.predictedPlayerState.viewheight;
    for (int i = 0; i < lineCount; ++i) {
        if (discovered[i] || (lines[i].flags & 128)) {
            continue;
        }
        vec3_t target = {(lines[i].a[0] + lines[i].b[0]) * 0.5f,
                         (lines[i].a[1] + lines[i].b[1]) * 0.5f, eye[2]};
        if (DistanceSquared(eye, target) > 2048.0f * 2048.0f) {
            continue;
        }
        trace_t trace;
        CG_Trace(&trace, eye, nullptr, nullptr, target, cg.clientNum, MASK_SOLID);
        if (trace.fraction >= 0.99f || DistanceSquared(trace.endpos, target) < 64) {
            discovered[i] = 1;
        }
    }
}
qboolean CG_DoomMapVisible() {
    if (!cg.showScores || !cg.snap || !cg.snap->ps.stats[STAT_DOOM_WEAPONS] || !lineCount) {
        return qfalse;
    }
    return qtrue;
}
void CG_DrawDoomMap() {
    if (!CG_DoomMapVisible()) {
        return;
    }
    const int viewWidth = cgs.glconfig.vidWidth;
    const int viewHeight =
        cgs.glconfig.vidHeight - (CG_DoomHudVisible() ? CG_DoomHudHeight() : 0);
    if (viewWidth <= 0 || viewHeight <= 0) {
        return;
    }
    // The original automap covers the whole view area above the status bar and
    // keeps Doom's 1.2 horizontal stretch of its 4:3 display.
    const float pixelsPerUnitY =
        0.97f * min(viewHeight / mapHeight, viewWidth / (mapWidth * 1.2f));
    const float pixelsPerUnitX = pixelsPerUnitY * 1.2f;
    scaleX = 256.0f * pixelsPerUnitX / viewWidth;
    scaleY = 256.0f * pixelsPerUnitY / viewHeight;
    memset(pixels, 0, sizeof(pixels));
    for (int i = 0; i < 256 * 256; ++i) {
        pixels[i * 4 + 3] = 255;
    }
    const bool all = cg.snap->ps.stats[STAT_DOOM_MAP] != 0;
    for (int i = 0; i < lineCount; ++i) {
        const auto &line = lines[i];
        if ((line.flags & 128) || (!all && !discovered[i])) {
            continue;
        }
        byte red = 180, green = 0, blue = 0;
        if (!discovered[i]) {
            red = green = blue = 80;
        } else if (line.sides[1] >= 0 && !(line.flags & 32)) {
            const auto &a = sectors[sides[line.sides[0]].sector];
            const auto &b = sectors[sides[line.sides[1]].sector];
            if (a.floor == b.floor && a.ceiling == b.ceiling) {
                continue;
            }
            red = 160;
            green = a.floor != b.floor ? 110 : 150;
            blue = 0;
        }
        Line(line.a[0], line.a[1], line.b[0], line.b[1], red, green, blue);
    }
    const auto &ps = cg.predictedPlayerState;
    vec3_t forward, right;
    AngleVectors(ps.viewangles, forward, right, nullptr);
    const float sizeX = 10.0f / pixelsPerUnitX, sizeY = 10.0f / pixelsPerUnitY;
    for (int side : {-1, 1}) {
        Line(ps.origin[0] + forward[0] * sizeX, ps.origin[1] + forward[1] * sizeY,
             ps.origin[0] - forward[0] * sizeX + right[0] * sizeX * side,
             ps.origin[1] - forward[1] * sizeY + right[1] * sizeY * side, 255, 255, 255);
    }
    trap_R_DrawDoomMap(0, 0, viewWidth, viewHeight, pixels);
}
