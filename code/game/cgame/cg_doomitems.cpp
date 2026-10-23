// SPDX-License-Identifier: GPL-3.0-or-later
#include "cg_local.h"
#include "cg_doomitems.h"
#include "cg_doommap.h"
#include "cg_doomsprite_geometry.h"
#include "../game/bg_doomitems.h"

namespace {
doommonster::SpriteSet sprites[doomitem::Count];
qhandle_t shaders[doomitem::Count][doommonster::MaxPatches];
bool loaded[doomitem::Count];
int pickupFlashTime;

void Load(int index) {
    if (loaded[index]) {
        return;
    }
    char text[60000];
    fileHandle_t file = 0;
    const char *path = va("models/items/%s/animation.cfg", doomitem::definitions[index].directory);
    const int length = trap_FS_FOpenFile(path, &file, FS_READ);
    if (!file || length <= 0 || length >= static_cast<int>(sizeof(text))) {
        if (file) {
            trap_FS_FCloseFile(file);
        }
        CG_Error("Missing or oversized Doom item config: %s", path);
        return;
    }
    trap_FS_Read(text, length, file);
    trap_FS_FCloseFile(file);
    text[length] = 0;
    if (!doommonster::Parse(text, sprites[index])) {
        CG_Error("Invalid Doom item config: %s", path);
        return;
    }
    for (int i = 0; i < sprites[index].patchCount; ++i) {
        shaders[index][i] = trap_R_RegisterShader(sprites[index].patches[i].path);
    }
    loaded[index] = true;
}
} // namespace

void CG_DoomItemsInit() {
    memset(loaded, 0, sizeof(loaded));
    memset(shaders, 0, sizeof(shaders));
    pickupFlashTime = 0;
    CG_DoomMapInit();
}
void CG_DoomItem(centity_t *entity) {
    const int index = entity->currentState.frame;
    if (index < 0 || index >= doomitem::Count) {
        return;
    }
    Load(index);
    const auto &set = sprites[index];
    const auto &step =
        doommonster::Sample(set, doommonster::Idle, cg.time - entity->currentState.effect1Time);
    const int patchIndex = set.views[step.frame][0].patch;
    polyVert_t vertices[4]{};
    vec3_t viewer, ambient, directed, light;
    VectorSubtract(cg.refdef.vieworg, entity->lerpOrigin, viewer);
    doomsprite::BuildQuad(entity->lerpOrigin, viewer, cg.refdef.viewaxis[1],
                          set.patches[patchIndex], false, 0, vertices);
    trap_R_LightForPoint(entity->lerpOrigin, ambient, directed, light);
    for (auto &vertex : vertices) {
        for (int color = 0; color < 3; ++color) {
            vertex.modulate[color] = static_cast<byte>(
                step.bright ? 255
                            : max(16.0f, min(255.0f, ambient[color] + directed[color] * 0.5f)));
        }
        vertex.modulate[3] = 255;
    }
    trap_R_AddPolyToScene(shaders[index][patchIndex], 4, vertices);
}
void CG_DoomPickupFlash() {
    pickupFlashTime = cg.time + 200;
}
void CG_DrawDoomItemEffects() {
    if (!cg.snap || !cg.snap->ps.stats[STAT_DOOM_WEAPONS]) {
        return;
    }
    CG_DoomMapUpdate();
    CG_DrawDoomMap();
    const auto &ps = cg.snap->ps;
    vec4_t color = {0, 0, 0, 0};
    if (ps.powerups[PW_INVULNERABLE] > cg.time &&
        (ps.powerups[PW_INVULNERABLE] - cg.time > 4000 || (cg.time / 115) & 1)) {
        VectorSet(color, 0.85f, 0.85f, 0.85f);
        color[3] = 0.3f;
    } else if (ps.powerups[PW_BATTLESUIT] > cg.time &&
               (ps.powerups[PW_BATTLESUIT] - cg.time > 4000 || (cg.time / 115) & 1)) {
        color[1] = 1;
        color[3] = 0.12f;
    }
    if (pickupFlashTime > cg.time) {
        VectorSet(color, 1, 0.8f, 0.1f);
        color[3] = (pickupFlashTime - cg.time) * 0.0006f;
    }
    if (color[3] > 0) {
        CG_FillRect(0, 0, 640, 480, color);
    }
}
