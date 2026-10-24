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

// win_local.h: shared header for the sys/win32 code that has no SDL3
// counterpart: the console window and Winsock networking.

#ifndef __WIN_LOCAL_H__
#define __WIN_LOCAL_H__

#include <windows.h>

#include <winsock.h>
#include <wsipx.h>

// module instance of the engine executable, used by the console window
HINSTANCE Win_GetInstance( void );

// native handle of the SDL game window, kept for the editor bridge
HWND Win_GetGameWindow( void );
void Win_SetGameWindow( HWND window );

#endif
