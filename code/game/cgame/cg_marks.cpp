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

// cg_marks.cpp -- wall marks

#include "cg_local.h"
#include <cmath>

/*
===================================================================

MARK POLYS

===================================================================
*/


markPoly_t cg_activeMarkPolys;          // double linked list
markPoly_t  *cg_freeMarkPolys;          // single linked list
markPoly_t cg_markPolys[MAX_MARK_POLYS];

/*
===================
CG_InitMarkPolys

This is called at startup and for tournement restarts
===================
*/
void    CG_InitMarkPolys( void ) {
	int i;
	markPoly_t *trav, *lasttrav;

	memset( cg_markPolys, 0, sizeof( cg_markPolys ) );

	cg_activeMarkPolys.nextMark = &cg_activeMarkPolys;
	cg_activeMarkPolys.prevMark = &cg_activeMarkPolys;
	cg_freeMarkPolys = cg_markPolys;
	for ( i = 0, trav = cg_markPolys + 1, lasttrav = cg_markPolys ; i < MAX_MARK_POLYS - 1 ; i++, trav++ ) {
		lasttrav->nextMark = trav;
		lasttrav = trav;
	}
}


/*
==================
CG_FreeMarkPoly
==================
*/
void CG_FreeMarkPoly( markPoly_t *le ) {
	if ( !le->prevMark ) {
		CG_Error( "CG_FreeLocalEntity: not active" );
	}

	// remove from the doubly linked active list
	le->prevMark->nextMark = le->nextMark;
	le->nextMark->prevMark = le->prevMark;

	// the free list is only singly linked
	le->nextMark = cg_freeMarkPolys;
	cg_freeMarkPolys = le;
}

/*
===================
CG_AllocMark

Will allways succeed, even if it requires freeing an old active mark
===================
*/
markPoly_t  *CG_AllocMark( int endTime ) {
	markPoly_t  *le; //, *trav, *lastTrav;
	int time;

	if ( !cg_freeMarkPolys ) {
		// no free entities, so free the one at the end of the chain
		// remove the oldest active entity
		time = cg_activeMarkPolys.prevMark->time;
		while ( cg_activeMarkPolys.prevMark && time == cg_activeMarkPolys.prevMark->time ) {
			CG_FreeMarkPoly( cg_activeMarkPolys.prevMark );
		}
	}

	le = cg_freeMarkPolys;
	cg_freeMarkPolys = cg_freeMarkPolys->nextMark;

	memset( le, 0, sizeof( *le ) );

	// Ridah, TODO: sort this, so the list is always sorted by longest duration -> shortest duration,
	// this way the shortest duration mark will always get overwritten first
	//for (trav = cg_activeMarkPolys.nextMark; (trav->duration + trav->time > endTime) && (trav != cg_activeMarkPolys.prevMark) ; lastTrav = trav, trav++ ) {
	// Respect the FOR loop
	//}

	// link into the active list
	le->nextMark = cg_activeMarkPolys.nextMark;
	le->prevMark = &cg_activeMarkPolys;
	cg_activeMarkPolys.nextMark->prevMark = le;
	cg_activeMarkPolys.nextMark = le;
	return le;
}



/*
=================
CG_ImpactMark

origin should be a point within a unit of the plane
dir should be the plane normal

temporary marks will not be stored or randomly oriented, but immediately
passed to the renderer.
=================
*/
// Ridah, increased this since we leave them around for longer
#define MAX_MARK_FRAGMENTS  384
#define MAX_MARK_POINTS     1024
//#define	MAX_MARK_FRAGMENTS	128
//#define	MAX_MARK_POINTS		384

// these are ignored now for the most part
//#define	MARK_TOTAL_TIME		20000	// (SA) made this a cvar: cg_markTime  (we could cap the time or remove marks quicker if too long a time starts to cause new marks to not appear)
#define MARK_FADE_TIME      10000

void CG_ImpactMark(qhandle_t markShader, const vec3_t origin, const vec3_t dir, float orientation,
                   float red, float green, float blue, float alpha, qboolean alphaFade,
                   float radius, qboolean temporary, int duration) {
    if (!cg_markTime.integer || !std::isfinite(radius) || radius <= 0 || !markShader ||
        !std::isfinite(orientation) || !std::isfinite(red) || !std::isfinite(green) ||
        !std::isfinite(blue) || !std::isfinite(alpha)) {
        return;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(origin[axis]) || !std::isfinite(dir[axis])) {
            return;
        }
    }
    refEntity_t entity = {};
    entity.reType = RT_SCREEN_DECAL;
    entity.customShader = markShader;
    entity.radius = radius;
    entity.effectThickness = Com_Clamp(1, 6, radius * .15f);
    VectorCopy(origin, entity.origin);
    if (VectorNormalize2(dir, entity.axis[0]) == 0) {
        return;
    }
    PerpendicularVector(entity.axis[1], entity.axis[0]);
    RotatePointAroundVector(entity.axis[2], entity.axis[0], entity.axis[1], orientation);
    CrossProduct(entity.axis[0], entity.axis[2], entity.axis[1]);
    float color[4] = {red, green, blue, alpha};
    for (int channel = 0; channel < 4; ++channel) {
        entity.shaderRGBA[channel] = static_cast<byte>(255 * Com_Clamp(0, 1, color[channel]));
    }
    if (temporary) {
        trap_R_AddRefEntityToScene(&entity);
        return;
    }
    if (duration < 0) {
        duration = cg_markTime.integer;
    }
    if (duration <= 0) {
        return;
    }
    markPoly_t *mark = CG_AllocMark(cg.time + duration);
    mark->time = cg.time;
    mark->duration = duration;
    mark->alphaFade = alphaFade;
    mark->markShader = markShader;
    mark->projector = entity;
    for (int channel = 0; channel < 4; ++channel) {
        mark->color[channel] = Com_Clamp(0, 1, color[channel]);
    }
}

void CG_AddMarks() {
    if ( !cg_markTime.integer ) {
		return;
	}
    for (markPoly_t *mark = cg_activeMarkPolys.nextMark, *next; mark != &cg_activeMarkPolys;
         mark = next) {
        next = mark->nextMark;
        int age = cg.time - mark->time;
        int remaining = mark->duration - age;
        if (remaining <= 0 || age < 0) {
            CG_FreeMarkPoly(mark);
            continue;
        }
        refEntity_t entity = mark->projector;
        float rgbFade = 1, alphaFade = 1;
        if (mark->markShader == cgs.media.energyMarkShader) {
            rgbFade *= Com_Clamp(0, 1, (450 - 450 * age / 3000.0f) / 255);
        }
        if (mark->markShader == cgs.media.zombieSpiritWallShader) {
            rgbFade *= Com_Clamp(0, 1, age / 2000.0f);
        }
        float lifetimeFade = Com_Clamp(0, 1, remaining * 2.0f / mark->duration);
        if (mark->alphaFade) {
            alphaFade = lifetimeFade;
        } else {
            rgbFade *= lifetimeFade;
        }
        for (int channel = 0; channel < 4; ++channel) {
            entity.shaderRGBA[channel] = static_cast<byte>(255 * mark->color[channel] *
                                                           (channel == 3 ? alphaFade : rgbFade));
        }
        trap_R_AddRefEntityToScene(&entity);
    }
}
