// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

void CG_DoomFxInit();
void CG_DoomFxShutdown();
void CG_DoomFxBegin();
void CG_DoomFxDraw();
void CG_DoomFxCommand();
void CG_DoomTeleportEffect(const vec3_t origin);
void CG_DoomFxImpact(int weapon, const vec3_t origin, const vec3_t normal, bool flesh,
                     int surfaceFlags = 0, int contents = 0, int target = ENTITYNUM_WORLD,
                     const float *travel = nullptr, const float *exitOrigin = nullptr,
                     bool secondary = false);
void CG_DoomFxTrail(centity_t *entity);
bool CG_DoomFxAllowDecal(const vec3_t origin, const vec3_t normal);
bool CG_DoomFxBloodWall(const vec3_t exitOrigin, const vec3_t travel, int victim, qhandle_t shader,
                        float radius);
