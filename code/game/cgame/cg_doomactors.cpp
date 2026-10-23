// SPDX-License-Identifier: GPL-3.0-or-later
#include "cg_local.h"
#include "cg_doomactors.h"
#include "cg_doomsprite_geometry.h"
#include "../game/bg_doommonsters.h"

namespace {
doommonster::SpriteSet sprites[doommonster::SpeciesCount];
qhandle_t shaders[doommonster::SpeciesCount][doommonster::MaxPatches];
qhandle_t marineTeamShaders[2][doommonster::MaxPatches];
bool loaded[doommonster::SpeciesCount];

void Load(int type) {
    if (loaded[type]) {
        return;
    }
    char text[60000];
    fileHandle_t file = 0;
    const char *path = va("models/players/%s/animation.cfg", doommonster::definitions[type].name);
    const int length = trap_FS_FOpenFile(path, &file, FS_READ);
    if (!file || length <= 0 || length >= static_cast<int>(sizeof(text))) {
        if (file) {
            trap_FS_FCloseFile(file);
        }
        CG_Error("Missing or oversized Doom sprite config: %s", path);
    }
    trap_FS_Read(text, length, file);
    trap_FS_FCloseFile(file);
    text[length] = 0;
    if (!doommonster::Parse(text, sprites[type])) {
        CG_Error("Invalid Doom sprite config: %s", path);
    }
    for (int i = 0; i < sprites[type].patchCount; ++i) {
        shaders[type][i] = trap_R_RegisterShader(sprites[type].patches[i].path);
        if (!shaders[type][i]) {
            CG_Error("Missing Doom sprite shader: %s", sprites[type].patches[i].path);
        }
        if (type == doommonster::Marine) {
            const char *name = strrchr(sprites[type].patches[i].path, '/');
            if (!name) {
                CG_Error("Invalid Doom marine frame path");
            }
            for (int team = 0; team < 2; ++team) {
                marineTeamShaders[team][i] = trap_R_RegisterShader(
                    va("models/players/doomguy/frames_%s/%s", team ? "blue" : "red", name + 1));
            }
        }
    }
    loaded[type] = true;
}
} // namespace

void CG_DoomActorsInit() {
    memset(loaded, 0, sizeof(loaded));
    memset(shaders, 0, sizeof(shaders));
    memset(marineTeamShaders, 0, sizeof(marineTeamShaders));
}

qboolean CG_DoomActor(centity_t *entity) {
    const auto &state = entity->currentState;
    if (!doommonster::IsSprite(state.density)) {
        return qfalse;
    }
    if ((state.eFlags & EF_NODRAW) || (state.number == cg.snap->ps.clientNum &&
                                       !cg.renderingThirdPerson && state.eType == ET_PLAYER)) {
        return qtrue;
    }
    const int type = state.density - doommonster::NetworkBase;
    Load(type);
    int clip = state.legsAnim;
    int start = state.effect1Time;
    if (type == doommonster::Marine) {
        // Human movement still uses the shared player animation state. This
        // adapter selects the third-person Doom clip without requiring a mesh.
        clip = state.eFlags & EF_DEAD                        ? doommonster::Death
               : state.eFlags & EF_FIRING                    ? doommonster::Missile
               : VectorLengthSquared(state.pos.trDelta) > 16 ? doommonster::Chase
                                                             : doommonster::Idle;
        if (entity->pe.legs.animationNumber != clip) {
            entity->pe.legs.animationNumber = clip;
            entity->pe.legs.animationTime = cg.time;
        }
        start = entity->pe.legs.animationTime;
    }
    const auto &step = doommonster::Sample(sprites[type], clip, cg.time - start);
    vec3_t toViewer;
    VectorSubtract(cg.refdef.vieworg, entity->lerpOrigin, toViewer);
    const int rotation = doommonster::Rotation(entity->lerpAngles[YAW], vectoyaw(toViewer));
    const auto &view = sprites[type].views[step.frame][rotation];
    const auto &patch = sprites[type].patches[view.patch];
    vec3_t ambient, directed, lightDirection;
    trap_R_LightForPoint(entity->lerpOrigin, ambient, directed, lightDirection);
    polyVert_t vertices[4]{};
    doomsprite::BuildQuad(entity->lerpOrigin, toViewer, cg.refdef.viewaxis[1], patch,
                          view.flip != 0, type == doommonster::Marine ? 24.0f : 0.0f, vertices,
                          type <= doommonster::Marine || type >= doommonster::TechPillar);
    for (int i = 0; i < 4; ++i) {
        for (int color = 0; color < 3; ++color) {
            vertices[i].modulate[color] = static_cast<byte>(
                step.bright ? 255
                            : max(16.0f, min(255.0f, ambient[color] + directed[color] * 0.5f)));
        }
        const bool invisible =
            type == doommonster::Marine && (entity->currentState.powerups & (1 << PW_INVIS));
        vertices[i].modulate[3] = type == doommonster::Spectre || invisible ? 100 : 255;
    }
    qhandle_t shader = shaders[type][view.patch];
    if (type == doommonster::Marine && cgs.gametype == GT_TEAM && state.clientNum >= 0 &&
        state.clientNum < MAX_CLIENTS) {
        const auto team = cgs.clientinfo[state.clientNum].team;
        if (team == TEAM_RED || team == TEAM_BLUE) {
            shader = marineTeamShaders[team == TEAM_BLUE ? 1 : 0][view.patch];
        }
    }
    trap_R_AddPolyToScene(shader, 4, vertices);
    return qtrue;
}
