// SPDX-License-Identifier: GPL-3.0-or-later
// Original Doom intermission patches/layout and world-map locations (wi_stuff.cpp).
#include "cg_local.h"
#include "cg_doomscreens.h"
#include "../game/bg_doomlevel.h"
#include <initializer_list>

namespace {
struct DoomPatch {
    const char *name;
    int width, height, left, top, textureWidth, textureHeight;
    qhandle_t shader;
};
#include "../qcommon/doom_ui_assets.h"
float sx, sy, ox, oy;
int progress, lastCounter, lastSound, lastPhase;
qhandle_t white;
DoomPatch *Find(const char *name) {
    for (auto &patch : doomPatches) {
        if (!Q_stricmp(name, patch.name)) {
            return &patch;
        }
    }
    return nullptr;
}
void Geometry() {
    float height = static_cast<float>(cgs.glconfig.vidHeight);
    if (cgs.glconfig.vidWidth * 3 < cgs.glconfig.vidHeight * 4) {
        height = cgs.glconfig.vidWidth * 0.75f;
    }
    sx = height / 240;
    sy = height / 200;
    ox = (cgs.glconfig.vidWidth - 320 * sx) * 0.5f;
    oy = (cgs.glconfig.vidHeight - 200 * sy) * 0.5f;
    if (!white) {
        white = trap_R_RegisterShaderNoMip("white");
    }
    trap_R_SetColor(nullptr);
    CG_FillRect(0, 0, 640, 480, colorBlack);
}
void Patch(const char *name, float x, float y) {
    auto *patch = Find(name);
    if (!patch) {
        return;
    }
    if (!patch->shader) {
        char path[MAX_QPATH];
        Com_sprintf(path, sizeof(path), "ui/doom/%s", name);
        Q_strlwr(path);
        patch->shader = trap_R_RegisterShaderNoMip(path);
    }
    trap_R_DrawStretchPic(ox + (x - patch->left) * sx, oy + (y - patch->top) * sy,
                          patch->width * sx, patch->height * sy, 0, 0,
                          static_cast<float>(patch->width) / patch->textureWidth,
                          static_cast<float>(patch->height) / patch->textureHeight, patch->shader);
}
void Center(const char *name, int y) {
    const auto *patch = Find(name);
    if (patch) {
        Patch(name, (320 - patch->width) * 0.5f + patch->left, y + patch->top);
    }
}
void Text(int x, int y, const char *text) {
    for (; *text; ++text) {
        const int letter = toupper(static_cast<unsigned char>(*text));
        const auto *patch = Find(va("STCFN%03d", letter));
        if (patch && letter >= 33) {
            Patch(patch->name, static_cast<float>(x), static_cast<float>(y));
        }
        x += patch && letter >= 33 ? patch->width : 4;
    }
}
void Number(int right, int y, int value, int digits = 0) {
    value = max(0, min(value, 999999));
    do {
        right -= 14;
        Patch(va("WINUM%d", value % 10), static_cast<float>(right), static_cast<float>(y));
        value /= 10;
        --digits;
    } while (value || digits > 0);
}
void Time(int right, int y, int seconds) {
    if (seconds >= 3600) {
        const auto *patch = Find("WISUCKS");
        if (patch) {
            Patch("WISUCKS", static_cast<float>(right - patch->width), static_cast<float>(y));
        }
        return;
    }
    Number(right, y, seconds % 60, 2);
    Patch("WICOLON", static_cast<float>(right - 36), static_cast<float>(y));
    Number(right - 38, y, seconds / 60);
}
void Bar(int percent) {
    const vec4_t border = {0.28f, 0.1f, 0.07f, 1}, red = {0.8f, 0.05f, 0.03f, 1};
    trap_R_SetColor(border);
    trap_R_DrawStretchPic(ox + 40 * sx, oy + 174 * sy, 240 * sx, 8 * sy, 0, 0, 1, 1, white);
    trap_R_SetColor(red);
    trap_R_DrawStretchPic(ox + 42 * sx, oy + 176 * sy, 236 * sx * percent / 100, 4 * sy, 0, 0, 1, 1,
                          white);
    trap_R_SetColor(nullptr);
}
void Background(int episode, int time) {
    Patch(va("WIMAP%d", episode - 1), 0, 0);
    constexpr int positions[2][10][2] = {
        {{224, 104},
         {184, 160},
         {112, 136},
         {72, 112},
         {88, 96},
         {64, 48},
         {192, 40},
         {136, 16},
         {80, 16},
         {64, 24}},
        {{104, 168}, {40, 136}, {160, 96}, {104, 80}, {120, 32}, {40, 0}}};
    if (episode == 1 || episode == 3) {
        const int group = episode == 1 ? 0 : 1, count = episode == 1 ? 10 : 6;
        for (int i = 0; i < count; ++i) {
            Patch(va("WIA%d%02d%02d", episode - 1, i, (time / 314 + i) % 3),
                  static_cast<float>(positions[group][i][0]),
                  static_cast<float>(positions[group][i][1]));
        }
    }
}
int Animated(int target, int time, int start, int duration) {
    return time <= start
               ? 0
               : min(target,
                     static_cast<int>(static_cast<long long>(target) * (time - start) / duration));
}
} // namespace
void CG_DoomScreensInit() {
    for (auto &patch : doomPatches) {
        patch.shader = 0;
    }
    progress = 0;
    white = 0;
    lastCounter = lastSound = 0;
    lastPhase = -1;
}
void CG_DoomLoadingStage(const char *stage) {
    const struct {
        const char *name;
        int percent;
    } stages[] = {
        {"collision", 10},       {"sounds", 20},      {"graphics", 35},      {"maps/", 40},
        {"game media", 45},      {"textures", 50},    {"models", 60},        {"weapons", 70},
        {"items", 78},           {"inline", 84},      {"server models", 88}, {"particles", 90},
        {"game media done", 92}, {"flamechunks", 94}, {"clients", 96},       {"DoomPlayer", 98}};
    for (const auto &entry : stages) {
        if (strstr(stage, entry.name)) {
            progress = max(progress, entry.percent);
        }
    }
}
bool CG_DrawDoomLoading() {
    int episode, mission;
    const char *map = Info_ValueForKey(CG_ConfigString(CS_SERVERINFO), "mapname");
    if (cgs.gametype != GT_SINGLE_PLAYER || !doomlevel::Map(map, episode, mission)) {
        return false;
    }
    Geometry();
    Background(episode, trap_Milliseconds());
    Center(va("WILV%d%d", episode - 1, mission - 1), 26);
    Text(132, 140, "LOADING");
    Bar(min(progress, 98));
    const char *stage = reinterpret_cast<const char *>(cg.infoScreenText);
    char label[40];
    Q_strncpyz(label, stage, sizeof(label));
    Text(40, 155, label);
    return true;
}
bool CG_DrawDoomIntermission() {
    doomlevel::Summary data;
    if (cgs.gametype != GT_SINGLE_PLAYER ||
        !doomlevel::Parse(CG_ConfigString(CS_DOOM_INTERMISSION), data)) {
        return false;
    }
    Geometry();
    const bool entering = data.phase >= doomlevel::Entering;
    Background(entering ? data.nextEpisode : data.episode, cg.time);
    if (entering) {
        Center("WIENTER", 2);
        Center(va("WILV%d%d", data.nextEpisode - 1, data.nextMission - 1), 20);
        constexpr int nodes[3][9][2] = {{{185, 164},
                                         {148, 143},
                                         {69, 122},
                                         {209, 102},
                                         {116, 89},
                                         {166, 55},
                                         {71, 56},
                                         {135, 29},
                                         {71, 24}},
                                        {{254, 25},
                                         {97, 50},
                                         {188, 64},
                                         {128, 78},
                                         {214, 92},
                                         {133, 130},
                                         {208, 136},
                                         {148, 140},
                                         {235, 158}},
                                        {{156, 168},
                                         {48, 154},
                                         {174, 95},
                                         {265, 75},
                                         {130, 48},
                                         {279, 23},
                                         {198, 48},
                                         {140, 25},
                                         {281, 136}}};
        for (int i = 0; i < 9; ++i) {
            if (data.episode == data.nextEpisode && (data.visited & (1 << i))) {
                Patch("WISPLAT", static_cast<float>(nodes[data.episode - 1][i][0]),
                      static_cast<float>(nodes[data.episode - 1][i][1]));
            }
        }
        if ((cg.time / 300) & 1) {
            const auto &node = nodes[data.nextEpisode - 1][data.nextMission - 1];
            Patch(node[0] > 240 ? "WIURH1" : "WIURH0", static_cast<float>(node[0]),
                  static_cast<float>(node[1]));
        }
    } else {
        Center(va("WILV%d%d", data.episode - 1, data.mission - 1), 2);
        Center("WIF", 20);
        const int elapsed =
            data.phase == doomlevel::Counting ? max(0, cg.time - data.started) : 10000;
        const int values[] = {
            Animated(doomlevel::Percent(data.kills, data.totalKills), elapsed, 800, 1500),
            Animated(doomlevel::Percent(data.items, data.totalItems), elapsed, 3000, 1500),
            Animated(doomlevel::Percent(data.secrets, data.totalSecrets), elapsed, 5200, 1500)};
        const char *labels[] = {"WIOSTK", "WIOSTI", "WISCRT2"};
        for (int i = 0; i < 3; ++i) {
            Patch(labels[i], 50.0f, 50.0f + i * 33);
            Number(270, 50 + i * 33, values[i]);
            Patch("WIPCNT", 270, 50.0f + i * 33);
        }
        Patch("WITIME", 16, 168);
        Time(156, 168, Animated(data.seconds, elapsed, 7400, 1800));
        Patch("WIPAR", 176, 168);
        Time(304, 168, Animated(data.par, elapsed, 7400, 1800));
        const int counter = values[0] + values[1] + values[2];
        if (counter != lastCounter && cg.time - lastSound > 110) {
            trap_S_StartLocalSound(trap_S_RegisterSound("sound/doom/menu/pistol.wav"),
                                   CHAN_LOCAL_SOUND);
            lastSound = cg.time;
        }
        lastCounter = counter;
        Text(68, 190, "PRESS FIRE OR USE TO CONTINUE");
    }
    if (data.phase != lastPhase) {
        if (lastPhase < 0) {
            trap_S_StartBackgroundTrack("music/doom/d_inter.mid", "music/doom/d_inter.mid", 0);
        }
        trap_S_StartLocalSound(trap_S_RegisterSound("sound/doom/menu/pstop.wav"), CHAN_LOCAL_SOUND);
        lastPhase = data.phase;
    }
    return true;
}
