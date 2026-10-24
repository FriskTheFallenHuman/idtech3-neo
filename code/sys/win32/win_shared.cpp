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

// win_shared.cpp: the small Win32 pieces that have no SDL3 equivalent.
// The SDL code reaches these through the Win_* functions declared in
// sys_local.h.

#include "../game/q_shared.h"
#include "win_local.h"
#include <mmsystem.h>

static HWND gameWindow;

/*
==================
Win_GetInstance

The console window needs the module instance for RegisterClass and
CreateWindow; the executable has exactly one module handle.
==================
*/
HINSTANCE Win_GetInstance( void ) {
	return GetModuleHandle( NULL );
}

/*
==================
Win_GetGameWindow / Win_SetGameWindow

The editor bridge works with the native window handle; SDL owns the
window itself and sys_glimp publishes the handle here.
==================
*/
HWND Win_GetGameWindow( void ) {
	return gameWindow;
}

void Win_SetGameWindow( HWND window ) {
	gameWindow = window;
}

/*
==================
Win_SetProcessErrorMode

No abort/retry/fail error popups.
==================
*/
void Win_SetProcessErrorMode( void ) {
	SetErrorMode( SEM_FAILCRITICALERRORS );
}

/*
==================
Win_SetTimerResolution

Make sure the timer is high precision, otherwise NT gets 18ms
resolution.
==================
*/
void Win_SetTimerResolution( qboolean enable ) {
	if ( enable ) {
		timeBeginPeriod( 1 );
	} else {
		timeEndPeriod( 1 );
	}
}

/*
==================
Win_PumpConsoleMessages

Dispatch one pending message for the console window. Returns qfalse on
WM_QUIT so the caller can shut down.
==================
*/
qboolean Win_PumpConsoleMessages( void ) {
	MSG msg;

	if ( !GetMessage( &msg, NULL, 0, 0 ) ) {
		return qfalse;
	}
	TranslateMessage( &msg );
	DispatchMessage( &msg );

	return qtrue;
}
