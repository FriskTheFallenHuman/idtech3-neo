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

// Shared model/sprite weapon animation parser and existing interpolation system.
#include "cg_local.h"
#include "cg_doomweapons.h"
#include <cmath>

namespace {
bool ParseAnimationInteger(char **text, int minimum, int maximum, int &result) {
    const char *token = COM_Parse(text);
    char *end = nullptr;
    const long value = strtol(token, &end, 10);
    if (!*token || !end || *end || value < minimum || value > maximum) {
        return false;
    }
    result = static_cast<int>(value);
    return true;
}
} // namespace

qboolean CG_ParseWeaponConfig(const char *filename, weaponInfo_t *weapon) {
    char text[20000];
    fileHandle_t file = 0;
    const int length = trap_FS_FOpenFile(filename, &file, FS_READ);
    if (!file || length <= 0 || length >= static_cast<int>(sizeof(text))) {
        if (file) {
            trap_FS_FCloseFile(file);
        }
        CG_Printf("Missing or oversized weapon animation file: %s\n", filename);
        return qfalse;
    }
    trap_FS_Read(text, length, file);
    trap_FS_FCloseFile(file);
    text[length] = 0;
    char *cursor = text;
    bool newFormat = false;
    for (;;) {
        char *previous = cursor;
        const char *token = COM_Parse(&cursor);
        if (!Q_stricmp(token, "newfmt")) {
            newFormat = true;
        } else if (!Q_stricmp(token, "whatever_variable")) {
            if (!*COM_Parse(&cursor)) {
                return qfalse;
            }
        } else if (*token >= '0' && *token <= '9') {
            cursor = previous;
            break;
        } else {
            CG_Printf("Invalid weapon animation header in %s\n", filename);
            return qfalse;
        }
    }
    animation_t animations[MAX_WP_ANIMATIONS]{};
    for (auto &animation : animations) {
        if (!ParseAnimationInteger(&cursor, 0, 1000000, animation.firstFrame) ||
            !ParseAnimationInteger(&cursor, 1, 1000000, animation.numFrames)) {
            return qfalse;
        }
        const char *token = COM_Parse(&cursor);
        char *end = nullptr;
        const float fps = strtof(token, &end);
        if (!*token || !end || *end || !std::isfinite(fps) || fps <= 0 || fps > 1000) {
            return qfalse;
        }
        animation.frameLerp = animation.initialLerp = static_cast<int>(1000 / fps);
        if (!ParseAnimationInteger(&cursor, 0, animation.numFrames, animation.loopFrames)) {
            return qfalse;
        }
        if (newFormat) {
            int barrels, animated, hidden;
            if (!ParseAnimationInteger(&cursor, 0, 255, barrels) ||
                !ParseAnimationInteger(&cursor, 0, 1, animated) ||
                !ParseAnimationInteger(&cursor, 0, 255, hidden)) {
                return qfalse;
            }
            animation.moveSpeed = barrels | (animated << W_MAX_PARTS) | (hidden << 8);
        }
    }
    // Commit only complete tables; malformed files cannot leave partially live animations.
    memcpy(weapon->weapAnimations, animations, sizeof(animations));
    return qtrue;
}

static void CG_SetWeapLerpFrameAnimation(weaponInfo_t *wi, lerpFrame_t *lf, int newAnimation) {
    animation_t *anim;

    lf->animationNumber = newAnimation;
    newAnimation &= ~ANIM_TOGGLEBIT;

    if (newAnimation < 0 || newAnimation >= MAX_WP_ANIMATIONS) {
        CG_Error("Bad animation number (CG_SWLFA): %i", newAnimation);
    }

    anim = &wi->weapAnimations[newAnimation];

    lf->animation = anim;
    lf->animationTime = lf->frameTime + anim->initialLerp;

    if (cg_debugAnim.integer & 2) {
        CG_Printf("Weap Anim: %d\n", newAnimation);
    }
}

/*
===============
CG_ClearWeapLerpFrame
===============
*/
void CG_ClearWeapLerpFrame(weaponInfo_t *wi, lerpFrame_t *lf, int animationNumber) {
    lf->frameTime = lf->oldFrameTime = cg.time;
    CG_SetWeapLerpFrameAnimation(wi, lf, animationNumber);
    lf->oldFrame = lf->frame = lf->animation->firstFrame;
}

/*
===============
CG_RunWeapLerpFrame

Sets cg.snap, cg.oldFrame, and cg.backlerp
cg.time should be between oldFrameTime and frameTime after exit
===============
*/
static void CG_RunWeapLerpFrame(clientInfo_t *ci, weaponInfo_t *wi, lerpFrame_t *lf,
                                int newAnimation, float speedScale) {
    int f;
    animation_t *anim;

    // debugging tool to get no animations
    if (cg_animSpeed.integer == 0) {
        lf->oldFrame = lf->frame = lf->backlerp = 0;
        return;
    }

    // see if the animation sequence is switching
    if (!lf->animation) {
        CG_ClearWeapLerpFrame(wi, lf, newAnimation);
    } else if (newAnimation != lf->animationNumber) {
        if ((newAnimation & ~ANIM_TOGGLEBIT) == WEAP_RAISE ||
            (newAnimation & ~ANIM_TOGGLEBIT) == WEAP_ALTSWITCHFROM ||
            (newAnimation & ~ANIM_TOGGLEBIT) == WEAP_ALTSWITCHTO) {
            CG_ClearWeapLerpFrame(
                wi, lf,
                newAnimation); // clear when switching to raise (since it should be out of view anyway)
        } else {
            CG_SetWeapLerpFrameAnimation(wi, lf, newAnimation);
        }
    }
    // RF, if the animation number is the same, but we are using a different weapon, we need to reset the lf->animation
    //else if ( memcmp( &wi->weapAnimations[newAnimation&~ANIM_TOGGLEBIT], lf->animation, sizeof(*lf->animation) ) ) {
    //	CG_ClearWeapLerpFrame(wi, lf, newAnimation );	// clear when switching to raise (since it should be out of view anyway)
    //}

    // if we have passed the current frame, move it to
    // oldFrame and calculate a new frame
    if (cg.time >= lf->frameTime) {
        lf->oldFrame = lf->frame;
        lf->oldFrameTime = lf->frameTime;

        // get the next frame based on the animation
        anim = lf->animation;
        if (!anim->frameLerp) {
            return; // shouldn't happen
        }
        if (cg.time < lf->animationTime) {
            lf->frameTime = lf->animationTime; // initial lerp
        } else {
            lf->frameTime = lf->oldFrameTime + anim->frameLerp;
        }
        f = (lf->frameTime - lf->animationTime) / anim->frameLerp;
        f *= speedScale; // adjust for haste, etc
        if (f >= anim->numFrames) {
            f -= anim->numFrames;
            if (anim->loopFrames) {
                f %= anim->loopFrames;
                f += anim->numFrames - anim->loopFrames;
            } else {
                f = anim->numFrames - 1;
                // the animation is stuck at the end, so it
                // can immediately transition to another sequence
                lf->frameTime = cg.time;
            }
        }
        lf->frame = anim->firstFrame + f;
        if (cg.time > lf->frameTime) {
            lf->frameTime = cg.time;
            if (cg_debugAnim.integer) {
                //				CG_Printf( "Clamp lf->frameTime\n");
            }
        }
    }

    if (lf->frameTime > cg.time + 200) {
        lf->frameTime = cg.time;
    }

    if (lf->oldFrameTime > cg.time) {
        lf->oldFrameTime = cg.time;
    }
    // calculate current lerp value
    if (lf->frameTime == lf->oldFrameTime) {
        lf->backlerp = 0;
    } else {
        lf->backlerp =
            1.0 - (float)(cg.time - lf->oldFrameTime) / (lf->frameTime - lf->oldFrameTime);
    }
}

/*
==============
CG_WeaponAnimation
==============
*/

//----(SA)	modified.  this is now client-side only (server does not dictate weapon animation info)
void CG_WeaponAnimation(playerState_t *ps, weaponInfo_t *weapon, int *weapOld, int *weap,
                        float *weapBackLerp) {

    centity_t *cent = &cg.predictedPlayerEntity;
    clientInfo_t *ci = &cgs.clientinfo[ps->clientNum];

    if (cg_noPlayerAnims.integer) {
        *weapOld = *weap = 0;
        return;
    }

    CG_RunWeapLerpFrame(ci, weapon, &cent->pe.weap, ps->weapAnim, 1);

    *weapOld = cent->pe.weap.oldFrame;
    *weap = cent->pe.weap.frame;
    *weapBackLerp = cent->pe.weap.backlerp;

    if (cg_debugAnim.integer == 3) {
        CG_Printf("oldframe: %d   frame: %d   backlerp: %f\n", cent->pe.weap.oldFrame,
                  cent->pe.weap.frame, cent->pe.weap.backlerp);
    }
}
