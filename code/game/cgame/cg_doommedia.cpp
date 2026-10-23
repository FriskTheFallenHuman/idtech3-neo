// SPDX-License-Identifier: GPL-3.0-or-later
#include "cg_local.h"

void CG_RegisterDoomMedia() {
    memset(&cg.refdef, 0, sizeof(cg.refdef));
    trap_R_ClearScene();
    CG_LoadingString(cgs.mapname);
    trap_R_LoadWorldMap(cgs.mapname);

    // These shared helpers are still used by crosshairs, debug text and the
    // multiplayer scoreboard. Doom weapons, pickups and FX register their own media.
    for (int i = 0; i < NUM_CROSSHAIRS; ++i) {
        cgs.media.crosshairShader[i] =
            trap_R_RegisterShaderNoMip(va("gfx/2d/crosshair%c", 'a' + i));
    }
    cgs.media.crosshairFriendly = trap_R_RegisterShaderNoMip("gfx/2d/friendlycross");
    cgs.media.lagometerShader = trap_R_RegisterShaderNoMip("lagometer");
    cgs.media.connectionShader = trap_R_RegisterShaderNoMip("disconnected");
    cgs.media.smokePuffShader = trap_R_RegisterShader("doom/fx/smoke0");
    cgs.media.shadowMarkShader = trap_R_RegisterShader("markShadow");
    cgs.media.wakeMarkShader = trap_R_RegisterShader("wake");
    cgs.media.waterBubbleShader = trap_R_RegisterShader("waterBubble");

    CG_DoomLights();
    CG_DoomSwitches();
    cgs.numInlineModels = trap_CM_NumInlineModels();
    if (cgs.numInlineModels > MAX_MODELS) {
        CG_Error("Too many inline BSP models");
    }
    for (int i = 1; i < cgs.numInlineModels; ++i) {
        cgs.inlineDrawModel[i] = trap_R_RegisterModel(va("*%d", i));
        vec3_t mins, maxs;
        trap_R_ModelBounds(cgs.inlineDrawModel[i], mins, maxs);
        for (int axis = 0; axis < 3; ++axis) {
            cgs.inlineModelMidpoints[i][axis] = (mins[axis] + maxs[axis]) * 0.5f;
        }
    }
    for (int i = 1; i < MAX_MODELS; ++i) {
        const auto *name = CG_ConfigString(CS_MODELS + i);
        if (!*name) {
            break;
        }
        cgs.gameModels[i] = trap_R_RegisterModel(name);
    }
    for (int i = 1; i < MAX_SOUNDS; ++i) {
        const auto *name = CG_ConfigString(CS_SOUNDS + i);
        if (!*name) {
            break;
        }
        if (*name != '*') {
            cgs.gameSounds[i] = trap_S_RegisterSound(name);
            cgs.gameSoundTypes[i] = 1;
        }
    }
    CG_ClearParticles();
}
