/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).  

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

// SPDX-License-Identifier: GPL-3.0-or-later
// Damage sprite behavior adapted from id Software's RTCW SP cg_view.cpp.
// Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
#include "cg_local.h"
#include "cg_damageview.h"

void CG_DamageBlendBlob() {
    // Doom uses the red screen flash in CG_DrawFlashDamage. Its asset set does not
    // include RTCW's viewBloodBlend sprites, which would render as default squares.
    if (cg.snap && cg.snap->ps.stats[STAT_DOOM_WEAPONS]) {
        return;
    }
    if (cgs.glconfig.hardwareType == GLHW_RAGEPRO) {
        return;
    }
    for (auto &damage : cg.viewDamage) {
        if (!damage.damageValue) {
            continue;
        }
        const int duration = damage.damageDuration;
        const int elapsed = cg.time - damage.damageTime;
        if (elapsed <= 0 || elapsed >= duration) {
            damage.damageValue = 0;
            continue;
        }
        if (!damage.damageX && !damage.damageY) {
            continue;
        }
        const float fraction = static_cast<float>(elapsed) / duration;
        const int frame = static_cast<int>(floor(fraction * 4.9f));
        const qhandle_t shader = cgs.media.viewBloodAni[frame];
        if (!shader) {
            continue;
        }
        refEntity_t entity{};
        entity.reType = RT_SPRITE;
        entity.renderfx = RF_FIRST_PERSON;
        VectorMA(cg.refdef.vieworg, 8, cg.refdef.viewaxis[0], entity.origin);
        VectorMA(entity.origin, damage.damageX * -8, cg.refdef.viewaxis[1], entity.origin);
        VectorMA(entity.origin, damage.damageY * 8, cg.refdef.viewaxis[2], entity.origin);
        entity.radius = damage.damageValue * 0.4f * (0.5f + 0.5f * fraction) *
                        (0.75f + 0.5f * fabs(sin(damage.damageTime)));
        entity.customShader = shader;
        memset(entity.shaderRGBA, 255, sizeof(entity.shaderRGBA));
        trap_R_AddRefEntityToScene(&entity);
    }
}
