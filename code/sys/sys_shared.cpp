/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein single player GPL Source Code ("RTCW SP Source Code").  

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

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, You may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

// sys_shared.cpp: the shared system helpers, on SDL3. SDL answers the
// millisecond clock and the CPU feature questions, so all the inline
// assembly the old Win32 file carried is gone.

#include "../game/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

#include <cmath>
#include <string.h>

/*
================
Sys_Milliseconds
================
*/
int sys_timeBase;

int Sys_Milliseconds( void ) {
	static qboolean initialized = qfalse;

	if ( !initialized ) {
		sys_timeBase = (int)SDL_GetTicks();
		initialized = qtrue;
	}

	return (int)SDL_GetTicks() - sys_timeBase;
}

/*
================
Sys_SnapVector
================
*/
void Sys_SnapVector( float *v ) {
	// plain rounding to nearest matches what the old fistp code did,
	// and every supported target has SSE these days
	v[0] = (float)std::lrint( v[0] );
	v[1] = (float)std::lrint( v[1] );
	v[2] = (float)std::lrint( v[2] );
}

/*
================
Sys_GetProcessorId

SDL has already asked the CPU what it supports.
================
*/
int Sys_GetProcessorId( void ) {
	// check for MMX
	if ( !SDL_HasMMX() ) {
		// Pentium or PPro
		return CPUID_INTEL_PENTIUM;
	}

	// see if we're an Intel Katmai
	if ( SDL_HasSSE() ) {
		return CPUID_INTEL_KATMAI;
	}

	// by default we're functionally a vanilla Pentium/MMX or P2/MMX
	return CPUID_INTEL_MMX;
}

int Sys_GetHighQualityCPU() {
	return SDL_HasSSE() ? 1 : 0;
}

//============================================

char *Sys_GetCurrentUser( void ) {
	static char s_userName[1024];
	char trimmed[1024];
	const char *home;
	size_t length;
	char *last;

	home = SDL_GetUserFolder( SDL_FOLDER_HOME );
	s_userName[0] = 0;

	if ( home && home[0] ) {
		Q_strncpyz( trimmed, home, sizeof( trimmed ) );

		// strip trailing separators, then keep only the last component
		// as the user name
		length = strlen( trimmed );
		while ( length > 0 && ( trimmed[length - 1] == '/' || trimmed[length - 1] == '\\' ) ) {
			trimmed[--length] = 0;
		}

		last = trimmed;
		for ( char *p = trimmed; *p; p++ ) {
			if ( *p == '/' || *p == '\\' ) {
				last = p + 1;
			}
		}
		Q_strncpyz( s_userName, last, sizeof( s_userName ) );
	}

	if ( !s_userName[0] ) {
		strcpy( s_userName, "player" );
	}

	return s_userName;
}

char    *Sys_DefaultHomePath( void ) {
	return NULL;
}

char *Sys_DefaultInstallPath( void ) {
	return Sys_Cwd();
}
