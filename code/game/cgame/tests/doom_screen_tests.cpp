// SPDX-License-Identifier: GPL-3.0-or-later
#include "../cg_local.h"
#include "../cg_doomscreens.h"
#include "../../game/bg_doomlevel.h"
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
cg_t cg{};
cgs_t cgs{};
namespace {
std::map<int, std::string> shaders;
std::vector<std::string> draws;
std::string summary;
void Check(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
bool Drawn(const char *name) {
    for (const auto &path : draws) {
        if (path == name) {
            return true;
        }
    }
    return false;
}
} // namespace
void QDECL Com_Printf(const char *, ...) {
}
void QDECL Com_Error(int, const char *message, ...) {
    throw std::runtime_error(message);
}
qhandle_t trap_R_RegisterShaderNoMip(const char *name) {
    const int index = static_cast<int>(shaders.size()) + 1;
    shaders[index] = name;
    return index;
}
void trap_R_SetColor(const float *) {
}
void trap_R_DrawStretchPic(float, float, float w, float h, float, float, float, float,
                           qhandle_t shader) {
    Check(w >= 0 && h >= 0, "Screen dimensions are nonnegative");
    draws.push_back(shaders[shader]);
}
void CG_FillRect(float, float, float, float, const float *) {
}
const char *CG_ConfigString(int index) {
    return index == CS_DOOM_INTERMISSION ? summary.c_str() : R"(\mapname\e1m2)";
}
int trap_Milliseconds() {
    return 1000;
}
sfxHandle_t trap_S_RegisterSound(const char *) {
    return 1;
}
void trap_S_StartLocalSound(sfxHandle_t, int) {
}
void trap_S_StartBackgroundTrack(const char *, const char *, int) {
}
int main() {
    try {
        cgs.gametype = GT_SINGLE_PLAYER;
        cgs.glconfig.vidWidth = 1600;
        cgs.glconfig.vidHeight = 900;
        CG_DoomScreensInit();
        CG_DoomLoadingStage("graphics");
        Q_strncpyz(reinterpret_cast<char *>(cg.infoScreenText), "graphics",
                   sizeof(cg.infoScreenText));
        Check(CG_DrawDoomLoading() && Drawn("ui/doom/wimap0") && Drawn("ui/doom/wilv01"),
              "Loading uses the original episode background and correct mission title");
        doomlevel::Summary data{1,   1, 1, 2, 3, 6, 4, 10, 1, 3, 65, 30, 1, doomlevel::Statistics,
                                1000};
        char text[512];
        doomlevel::Write(text, sizeof(text), data);
        summary = text;
        draws.clear();
        cg.time = 2000;
        Check(CG_DrawDoomIntermission() && Drawn("ui/doom/wif") && Drawn("ui/doom/wiostk") &&
                  Drawn("ui/doom/wiosti") && Drawn("ui/doom/wiscrt2") && Drawn("ui/doom/witime") &&
                  Drawn("ui/doom/wipar"),
              "Completed-level screen draws every original statistics label");
        data.phase = doomlevel::Entering;
        doomlevel::Write(text, sizeof(text), data);
        summary = text;
        draws.clear();
        cg.time = 2100;
        Check(CG_DrawDoomIntermission() && Drawn("ui/doom/wienter") && Drawn("ui/doom/wilv01") &&
                  Drawn("ui/doom/wisplat") && Drawn("ui/doom/wiurh0"),
              "Next-level screen draws the destination, visited marker and blinking pointer");
        summary = "bad";
        Check(!CG_DrawDoomIntermission(), "Malformed summary is not rendered");
        std::puts("Doom loading and intermission screen rendering passed using the real cgame "
                  "implementation.");
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
