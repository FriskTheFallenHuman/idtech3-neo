// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/cgame/cg_local.h"
#include "game/cgame/cg_doomweapons.h"
#include "game/game/bg_doomweapons.h"
#include <cstdio>
#include <stdexcept>
#include <string>

cg_t cg{};
cgs_t cgs{};
vmCvar_t cg_animSpeed{}, cg_debugAnim{}, cg_noPlayerAnims{};
namespace {
std::string source;
int openFiles;
void Check(bool value, const char *why) {
    if (!value) {
        throw std::runtime_error(why);
    }
}
} // namespace
int trap_FS_FOpenFile(const char *, fileHandle_t *file, fsMode_t) {
    *file = 1;
    ++openFiles;
    return static_cast<int>(source.size());
}
void trap_FS_FCloseFile(fileHandle_t) {
    --openFiles;
}
void trap_FS_Read(void *buffer, int length, fileHandle_t) {
    memcpy(buffer, source.data(), length);
}
void QDECL CG_Printf(const char *, ...) {
}
void QDECL Com_Printf(const char *, ...) {
}
void QDECL Com_Error(int, const char *message, ...) {
    throw std::runtime_error(message);
}
void QDECL CG_Error(const char *message, ...) {
    throw std::runtime_error(message);
}

int main() {
    try {
        cg_animSpeed.integer = 1;
        for (const auto &weapon : doomweapon::definitions) {
            const std::string path =
                std::string("base/models/weapons/") + weapon.directory + "/weapon.cfg";
            FILE *file = fopen(path.c_str(), "rb");
            Check(file != nullptr, "Generated weapon.cfg exists");
            char buffer[4096];
            const size_t length = fread(buffer, 1, sizeof(buffer), file);
            fclose(file);
            source.assign(buffer, length);
            weaponInfo_t info{};
            Check(CG_ParseWeaponConfig(path.c_str(), &info),
                  "Real engine animation parser accepts generated config");
            for (int animation : {WEAP_IDLE1, WEAP_ATTACK1, WEAP_ATTACK2, WEAP_DROP, WEAP_RAISE}) {
                memset(&cg.predictedPlayerEntity.pe.weap, 0, sizeof(lerpFrame_t));
                playerState_t ps{};
                ps.weapAnim = animation | ANIM_TOGGLEBIT;
                cg.time = 1000;
                int oldFrame, frame;
                float backlerp;
                CG_WeaponAnimation(&ps, &info, &oldFrame, &frame, &backlerp);
                const auto &range = info.weapAnimations[animation];
                Check(frame == range.firstFrame,
                      "Existing animation starts at configured sprite frame");
                for (int time = 1010; time < 3500; time += 10) {
                    cg.time = time;
                    CG_WeaponAnimation(&ps, &info, &oldFrame, &frame, &backlerp);
                    Check(frame >= range.firstFrame && frame < range.firstFrame + range.numFrames,
                          "Existing interpolation stays within virtual sprite frames");
                }
                if (!range.loopFrames) {
                    Check(frame == range.firstFrame + range.numFrames - 1,
                          "Nonlooped animations finish on last frame");
                }
            }
        }
        weaponInfo_t info{};
        for (const char *invalid : {"", "doomSprites", "newfmt", "0 1 nan 0", "0 0 35 0",
                                    "0 1 -35 0", "0 1 35 9", "0 1 35"}) {
            source = invalid;
            Check(!CG_ParseWeaponConfig("malformed.cfg", &info),
                  "Malformed animation file rejected");
            Check(openFiles == 0, "Malformed/empty files close their handles");
        }
        source.assign(20001, ' ');
        Check(!CG_ParseWeaponConfig("oversized.cfg", &info) && !openFiles,
              "Oversized config closes handle");
        puts("Doom sprite animation checks passed using engine parser and interpolation: all eight "
             "weapon configs, idle/attack/drop/raise, loops, frame bounds and malformed input.");
        return 0;
    } catch (const std::exception &error) {
        fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
