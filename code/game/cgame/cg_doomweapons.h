// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
qboolean CG_ParseWeaponConfig(const char *filename, weaponInfo_t *weapon);
void CG_WeaponAnimation(playerState_t *ps, weaponInfo_t *weapon, int *oldFrame, int *frame,
                        float *backlerp);
void CG_DoomWeaponsInit();
void CG_RegisterDoomWeapon(int weapon);
void CG_DrawDoomWeapon();
void CG_DoomSelectWeapon(int slot, int direction = 0);
void CG_DoomFireWeapon(centity_t *cent);
void CG_DoomPickup(centity_t *cent);
void CG_DoomMissile(centity_t *cent);
void CG_DoomImpact(int weapon, const vec3_t origin);
void CG_DoomExplosion(localEntity_t *effect);
