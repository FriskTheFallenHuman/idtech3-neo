// SPDX-License-Identifier: GPL-3.0-or-later
#include "fx/common_headers.h"
#include "fx/FxScheduler.h"
#include "cg_doomfx.h"
#include "cg_doomweapons.h"
#include "cg_doomfx_materials.h"
#include "../game/bg_doomweapons.h"
#include "../game/bg_doommonsters.h"
#include <cstdlib>
#undef min
#undef max

namespace {
constexpr int WeaponCount = 8;
constexpr int MaterialCount = static_cast<int>(doomfx::Material::Count);
constexpr const char *WeaponNames[WeaponCount] = {"fist",   "pistol",   "shotgun", "chaingun",
                                                  "rocket", "chainsaw", "plasma",  "bfg"};
int impacts[WeaponCount][MaterialCount], trails[WeaponCount], blasts[WeaponCount], bfgSpray;
qhandle_t haloShader, coreShader;
int frameTime = -1, registered;
int bloodSplat;
qhandle_t bloodMarks[4];
int previewHandle, previewBlast;
vec3_t previewOrigin, previewNormal;
vmCvar_t quality;
doomfx::MaterialMap materialMap;
struct TrailState {
    int time, birth, weapon;
    vec3_t origin;
};
TrailState emitters[MAX_GENTITIES];

void LoadMaterials() {
    materialMap.Clear();
    fileHandle_t file = 0;
    const int length = trap_FS_FOpenFile(cgs.mapname, &file, FS_READ);
    if (!file || length <= 0 || length > 64 * 1024 * 1024) {
        if (file) {
            trap_FS_FCloseFile(file);
        }
        CG_Printf("Doom FX: material map unavailable: %s (%d bytes)\n", cgs.mapname, length);
        return;
    }
    void *data = std::malloc(static_cast<size_t>(length));
    if (!data) {
        trap_FS_FCloseFile(file);
        CG_Error("Doom FX: cannot allocate material map (%d bytes)", length);
        return;
    }
    trap_FS_Read(data, length, file);
    trap_FS_FCloseFile(file);
    const char *error = nullptr;
    const bool loaded = materialMap.Load(data, length, &error);
    std::free(data);
    if (!loaded) {
        CG_Printf("Doom FX: material map %s: %s\n", cgs.mapname, error);
    }
}
doomfx::Material MaterialAt(const vec3_t origin, const vec3_t normal, int target,
                            int surfaceFlags = 0, int contents = 0) {
    using doomfx::Material;
    if (contents & CONTENTS_LAVA) {
        return Material::Lava;
    }
    if (contents & CONTENTS_SLIME) {
        return Material::Slime;
    }
    if (contents & CONTENTS_WATER) {
        return Material::Water;
    }
    if (surfaceFlags & SURF_METAL) {
        return Material::Metal;
    }
    if (surfaceFlags & SURF_WOOD) {
        return Material::Wood;
    }
    if (surfaceFlags & SURF_GLASS) {
        return Material::Glass;
    }
    if (target >= 0 && target < ENTITYNUM_WORLD) {
        const auto &entity = cg_entities[target];
        if (entity.currentState.eType == ET_GENERAL &&
            (entity.currentState.density == doommonster::NetworkBase + doommonster::Barrel ||
             entity.currentState.density == doommonster::NetworkBase + doommonster::TechPillar)) {
            return Material::Metal;
        }
        if (entity.currentValid && entity.currentState.eType == ET_MOVER) {
            vec3_t delta, point, direction, axis[3], angles, position;
            // Snapshot events precede packet-entity interpolation. Evaluate at this render time.
            BG_EvaluateTrajectory(&entity.currentState.pos, cg.time, position);
            BG_EvaluateTrajectory(&entity.currentState.apos, cg.time, angles);
            AnglesToAxis(angles, axis);
            VectorSubtract(origin, position, delta);
            for (int component = 0; component < 3; ++component) {
                point[component] = DotProduct(delta, axis[component]);
                direction[component] = DotProduct(normal, axis[component]);
            }
            return materialMap.Find(point, direction, entity.currentState.modelindex);
        }
    }
    return materialMap.Find(origin, normal);
}
void Play(int handle, const vec3_t origin, const vec3_t direction) {
    if (!handle || !quality.integer || cg_paused.integer || fx_freeze.integer) {
        return;
    }
    vec3_t point, normal;
    VectorCopy(origin, point);
    VectorCopy(direction, normal);
    theFxScheduler.PlayEffect(handle, point, normal);
}
} // namespace

void CG_DoomFxInit() {
    trap_Cvar_Register(&quality, "doom_fxQuality", "2", CVAR_ARCHIVE);
    trap_Cvar_Register(&fx_debug, "fx_debug", "0", 0);
    trap_Cvar_Register(&fx_freeze, "fx_freeze", "0", CVAR_CHEAT);
    trap_Cvar_Register(&fx_expensivePhysics, "fx_expensivePhysics", "1", CVAR_ARCHIVE);
    FX_Init();
    memset(emitters, 0, sizeof(emitters));
    memset(trails, 0, sizeof(trails));
    memset(blasts, 0, sizeof(blasts));
    haloShader = trap_R_RegisterShader("doom/fx/glow");
    coreShader = trap_R_RegisterShader("doom/fx/core");
    frameTime = -1;
    previewHandle = 0;
    previewBlast = 0;
    bloodSplat = theFxScheduler.RegisterEffect("doom/blood_splat");
    registered = bloodSplat != 0;
    for (int index = 0; index < 4; ++index) {
        bloodMarks[index] = trap_R_RegisterShader(va("doom/fx/blood_splat%d", index));
    }
    for (int weapon = 0; weapon < WeaponCount; ++weapon) {
        for (int material = 0; material < MaterialCount; ++material) {
            impacts[weapon][material] = theFxScheduler.RegisterEffect(
                va("doom/%s_%s", WeaponNames[weapon], doomfx::MaterialNames[material]));
            registered += impacts[weapon][material] != 0;
        }
        if (weapon == WP_DOOM_ROCKET - WP_DOOM_FIST || weapon >= WP_DOOM_PLASMA - WP_DOOM_FIST) {
            trails[weapon] =
                theFxScheduler.RegisterEffect(va("doom/%s_trail", WeaponNames[weapon]));
            registered += trails[weapon] != 0;
            blasts[weapon] =
                theFxScheduler.RegisterEffect(va("doom/%s_blast", WeaponNames[weapon]));
            registered += blasts[weapon] != 0;
        }
    }
    LoadMaterials();
    bfgSpray = theFxScheduler.RegisterEffect("doom/bfg_spray");
    registered += bfgSpray != 0;
    CG_Printf("Doom FX: %d/72 weapon/material, blast and trail templates loaded\n", registered);
}
void CG_DoomFxShutdown() {
    FX_Free();
    materialMap.Clear();
    frameTime = -1;
}

void CG_DoomTeleportEffect(const vec3_t origin) {
    vec3_t soundOrigin;
    VectorCopy(origin, soundOrigin);
    trap_S_StartSound(soundOrigin, ENTITYNUM_WORLD, CHAN_AUTO,
                      trap_S_RegisterSound("sound/doom/monsters/telept.wav"));
    auto *effect = CG_AllocLocalEntity();
    effect->leType = LE_EXPLOSION;
    effect->startTime = cg.time;
    effect->endTime = cg.time + 12 * 6 * 1000 / 35;
    auto &sprite = effect->refEntity;
    sprite.reType = RT_SPRITE;
    sprite.radius = 64;
    sprite.customShader = trap_R_RegisterShader("doom/teleport");
    sprite.shaderTime = cg.time / 1000.0f;
    memset(sprite.shaderRGBA, 255, sizeof(sprite.shaderRGBA));
    VectorCopy(origin, sprite.origin);
    sprite.origin[2] += 32;
    VectorCopy(sprite.origin, sprite.oldorigin);
}
void CG_DoomFxBegin() {
    trap_Cvar_Update(&quality);
    trap_Cvar_Update(&fx_debug);
    trap_Cvar_Update(&fx_freeze);
    trap_Cvar_Update(&fx_expensivePhysics);
    if (cg.time < frameTime || !quality.integer) {
        FX_Stop();
        CG_InitMarkPolys();
        memset(emitters, 0, sizeof(emitters));
    }
    theFxHelper.AdjustTime(frameTime < 0 ? 0 : cg.time - frameTime);
    frameTime = cg.time;
}
void CG_DoomFxDraw() {
    // Console commands can run between simulation frames, including immediately after reload.
    if (previewHandle && theFxHelper.mFrameTime > 0) {
        Play(previewHandle, previewOrigin, previewNormal);
        vec3_t burstOrigin;
        VectorMA(previewOrigin, 12, previewNormal, burstOrigin);
        Play(previewBlast, burstOrigin, previewNormal);
        previewHandle = 0;
        previewBlast = 0;
    }
    theFxScheduler.AddScheduledEffects(false);
}
bool CG_DoomFxAllowDecal(const vec3_t origin, const vec3_t normal) {
    vec3_t start, end;
    VectorMA(origin, 2, normal, start);
    VectorMA(origin, -3, normal, end);
    trace_t trace;
    CG_Trace(&trace, start, nullptr, nullptr, end, cg.clientNum, MASK_SOLID);
    if (trace.fraction >= 1 || trace.startsolid ||
        (trace.surfaceFlags & (SURF_NOIMPACT | SURF_NOMARKS | SURF_SKY))) {
        return false;
    }
    return !doomfx::IsLiquid(
        MaterialAt(origin, normal, trace.entityNum, trace.surfaceFlags, trace.contents));
}
void CG_DoomFxImpact(int weapon, const vec3_t origin, const vec3_t direction, bool flesh,
                     int surfaceFlags, int contents, int target, const float *travel,
                     const float *exitOrigin, bool secondary) {
    if (!doomweapon::IsWeapon(weapon) || (surfaceFlags & (SURF_NOIMPACT | SURF_SKY))) {
        return;
    }
    vec3_t normal;
    if (VectorNormalize2(direction, normal) < 0.001f) {
        VectorSet(normal, 0, 0, 1);
    }
    vec3_t burstOrigin;
    // Pull volumetric light away from the contact plane so depth softening does
    // not erase the flash at a wall. Blood remains at the actual hit position.
    VectorMA(origin, weapon == WP_DOOM_BFG ? 12.0f : 6.0f, normal, burstOrigin);
    Play(secondary ? bfgSpray : blasts[weapon - WP_DOOM_FIST], burstOrigin, normal);
    if (flesh && !cg_blood.integer) {
        return;
    }
    const auto material = flesh ? doomfx::Material::Flesh
                                : MaterialAt(origin, normal, target, surfaceFlags, contents);
    Play(impacts[weapon - WP_DOOM_FIST][static_cast<int>(material)], origin, normal);
    if (flesh && travel && exitOrigin && quality.integer && !fx_freeze.integer &&
        !cg_paused.integer && weapon != WP_DOOM_FIST && weapon != WP_DOOM_CHAINSAW) {
        for (int axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(travel[axis]) || !std::isfinite(exitOrigin[axis])) {
                return;
            }
        }
        vec3_t forward;
        if (VectorNormalize2(travel, forward) < 0.001f) {
            return;
        }
        const float distance = DistanceSquared(origin, exitOrigin);
        if (!std::isfinite(distance) || distance > 514 * 514) {
            return;
        }
        Play(impacts[weapon - WP_DOOM_FIST][static_cast<int>(doomfx::Material::Flesh)], exitOrigin,
             forward);
        const float radius = weapon == WP_DOOM_ROCKET || weapon == WP_DOOM_BFG ? 32.0f
                             : weapon == WP_DOOM_SHOTGUN                       ? 24.0f
                                                                               : 20.0f;
        const bool wall =
            CG_DoomFxBloodWall(exitOrigin, forward, target, bloodMarks[FX_IRand(0, 3)], radius);
        if (fx_debug.integer) {
            CG_Printf("Doom blood: weapon=%d victim=%d exit=%.1f %.1f %.1f wall=%d\n", weapon,
                      target, exitOrigin[0], exitOrigin[1], exitOrigin[2], wall);
        }
    }
}
void CG_DoomFxTrail(centity_t *entity) {
    const auto &state = entity->currentState;
    if (!doomweapon::IsWeapon(state.weapon) || state.number < 0 || state.number >= MAX_GENTITIES ||
        !trails[state.weapon - WP_DOOM_FIST] || !quality.integer) {
        return;
    }
    auto &emitter = emitters[state.number];
    vec3_t normal;
    if (VectorNormalize2(state.pos.trDelta, normal) < 0.001f) {
        VectorSet(normal, 0, 0, 1);
    }
    const bool rocket = state.weapon == WP_DOOM_ROCKET;
    const bool bfg = state.weapon == WP_DOOM_BFG;
    const float pulse = 1.0f + 0.06f * sinf(theFxHelper.mTime * 0.025f + state.number);
    const float radius = rocket ? 16.0f : bfg ? 70.0f : 28.0f;
    const vec3_t color = {rocket ? 1.0f
                          : bfg  ? 0.18f
                                 : 0.12f,
                          rocket ? 0.34f
                          : bfg  ? 1.0f
                                 : 0.42f,
                          rocket ? 0.04f
                          : bfg  ? 0.035f
                                 : 1.0f};
    refEntity_t halo{};
    halo.reType = RT_SPRITE;
    halo.customShader = haloShader;
    halo.radius = radius * pulse;
    VectorMA(entity->lerpOrigin, rocket ? -9.0f : 0.0f, normal, halo.origin);
    for (int channel = 0; channel < 3; ++channel) {
        halo.shaderRGBA[channel] = static_cast<unsigned char>(color[channel] * 255);
    }
    halo.shaderRGBA[3] = rocket ? 165 : 145;
    trap_R_AddRefEntityToScene(&halo);
    halo.customShader = coreShader;
    halo.radius = rocket ? 4.5f : bfg ? 18.0f : 7.0f;
    halo.shaderRGBA[0] = bfg ? 180 : 230;
    halo.shaderRGBA[1] = rocket ? 170 : 255;
    halo.shaderRGBA[2] = rocket ? 60 : bfg ? 95 : 255;
    halo.shaderRGBA[3] = 220;
    trap_R_AddRefEntityToScene(&halo);
    trap_R_AddLightToScene(entity->lerpOrigin,
                           rocket ? 110
                           : bfg  ? 245
                                  : 95,
                           color[0], color[1], color[2], 0);
    const int baseInterval = bfg ? 20 : rocket ? 14 : 12;
    const int interval = quality.integer <= 1   ? baseInterval * 2
                         : quality.integer == 2 ? baseInterval
                                                : std::max(8, baseInterval * 2 / 3);
    if (emitter.birth != state.pos.trTime || emitter.weapon != state.weapon ||
        cg.time < emitter.time || cg.time - emitter.time > 250 ||
        DistanceSquared(emitter.origin, entity->lerpOrigin) > 512 * 512) {
        emitter.birth = state.pos.trTime;
        emitter.weapon = state.weapon;
        emitter.time = cg.time - interval;
        VectorCopy(entity->lerpOrigin, emitter.origin);
    }
    if (cg.time - emitter.time < interval) {
        return;
    }
    const int count = std::min(24, std::max(1, (cg.time - emitter.time) / interval));
    for (int step = 1; step <= count; ++step) {
        vec3_t point;
        const float fraction = static_cast<float>(step) / count;
        for (int axis = 0; axis < 3; ++axis) {
            point[axis] =
                emitter.origin[axis] + fraction * (entity->lerpOrigin[axis] - emitter.origin[axis]);
        }
        Play(trails[state.weapon - WP_DOOM_FIST], point, normal);
    }
    emitter.time = cg.time;
    VectorCopy(entity->lerpOrigin, emitter.origin);
}
void CG_DoomFxCommand() {
    char name[MAX_QPATH];
    trap_Argv(1, name, sizeof(name));
    if (!Q_stricmp(name, "status")) {
        extern int activeFx, drawnFx;
        CG_Printf("Doom FX: templates=%d active=%d drawn=%d scheduled=%d quality=%d step=%d\n",
                  registered, activeFx, drawnFx, theFxScheduler.NumScheduledFx(), quality.integer,
                  theFxHelper.mFrameTime);
        return;
    }
    char cheats[8];
    trap_Cvar_VariableStringBuffer("sv_cheats", cheats, sizeof(cheats));
    if (!atoi(cheats)) {
        CG_Printf("doomfx preview/reload requires spdevmap\n");
        return;
    }
    if (!Q_stricmp(name, "reload")) {
        CG_DoomFxInit();
        CG_InitMarkPolys();
        return;
    }
    vec3_t end, normal;
    VectorMA(cg.refdef.vieworg, 512, cg.refdef.viewaxis[0], end);
    trace_t trace;
    CG_Trace(&trace, cg.refdef.vieworg, nullptr, nullptr, end, cg.clientNum, MASK_SOLID);
    VectorCopy(trace.plane.normal, normal);
    if (trace.fraction == 1) {
        VectorMA(cg.refdef.vieworg, 80, cg.refdef.viewaxis[0], trace.endpos);
        VectorScale(cg.refdef.viewaxis[0], -1, normal);
    }
    if (!Q_stricmp(name, "blood_splat")) {
        if (!cg_blood.integer) {
            return;
        }
        if (trace.fraction == 1) {
            vec3_t start;
            VectorCopy(trace.endpos, start);
            VectorMA(start, 32, cg.refdef.viewaxis[1], start);
            VectorCopy(start, end);
            end[2] -= 256;
            CG_Trace(&trace, start, nullptr, nullptr, end, cg.clientNum, MASK_SOLID);
            VectorCopy(trace.plane.normal, normal);
        }
        if (trace.fraction < 1 && !trace.startsolid) {
            previewHandle = bloodSplat;
            VectorCopy(trace.endpos, previewOrigin);
            VectorCopy(normal, previewNormal);
        }
        return;
    }
    for (int weapon = 0; weapon < WeaponCount; ++weapon) {
        if (blasts[weapon] && !Q_stricmp(name, va("%s_blast", WeaponNames[weapon]))) {
            previewHandle = blasts[weapon];
            previewBlast = 0;
            VectorMA(trace.endpos, 8, normal, previewOrigin);
            VectorCopy(normal, previewNormal);
            return;
        }
        for (int material = 0; material < MaterialCount; ++material) {
            if (!Q_stricmp(name,
                           va("%s_%s", WeaponNames[weapon], doomfx::MaterialNames[material]))) {
                if (material != static_cast<int>(doomfx::Material::Flesh) || cg_blood.integer) {
                    previewHandle = impacts[weapon][material];
                    previewBlast = blasts[weapon];
                    VectorCopy(trace.endpos, previewOrigin);
                    VectorCopy(normal, previewNormal);
                    if (previewBlast) {
                        vec3_t burstOrigin;
                        VectorMA(previewOrigin, 8, previewNormal, burstOrigin);
                        CG_DoomImpact(WP_DOOM_FIST + weapon, burstOrigin);
                    }
                }
                return;
            }
        }
    }
    CG_Printf("doomfx status | reload | blood_splat | "
              "<weapon>_<wall|metal|wood|glass|water|slime|lava|flesh>\n");
}
