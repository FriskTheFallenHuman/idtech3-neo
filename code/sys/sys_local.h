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

// sys_local.h: SDL3 platform header for the engine system layer.
// The window, GL context, input, timing and the small OS helpers all go
// through SDL3 here. The parts that cannot be ported - the Win32 console
// window, Winsock networking and the hardware gamma ramps - live in
// sys/win32 and are reached through the Win_* bridges below.

#ifndef __SYS_LOCAL_H__
#define __SYS_LOCAL_H__

#include <SDL3/SDL.h>

// Input subsystem

void    IN_Init( void );
void    IN_Shutdown( void );

void    IN_Activate( qboolean active );
void    IN_Frame( void );
void    IN_ClearStates( void );

void    IN_MouseEvent( int mstate );

// SDL event pump, called from Sys_GetEvent
void    Sys_EventLoop( void );

// Resize the renderer after a window size change
void    Sys_UpdateWindowSize( void );

void    Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr );

// Console window, implemented with Win32 in sys/win32/win_syscon.cpp

void    Sys_CreateConsole( void );
void    Sys_DestroyConsole( void );
void    Sys_ShowConsole( int visLevel, qboolean quitOnClose );
void    Sys_SetErrorText( const char *text );

char    *Sys_ConsoleInput( void );

void Conbuf_AppendText( const char *msg );

// Networking, implemented with Winsock in sys/win32/win_net.cpp

qboolean    Sys_GetPacket( netadr_t *net_from, msg_t *net_message );

// Win32-only helpers the SDL files call into, implemented in
// sys/win32/win_shared.cpp

void Win_SetProcessErrorMode( void );
void Win_SetTimerResolution( qboolean enable );
qboolean Win_PumpConsoleMessages( void );

void SNDDMA_Activate( void );

typedef struct
{
	qboolean activeApp;
	qboolean isMinimized;

	// when we get a window event, we store the time off so keyboard processing
	// can know the exact time of an event
	unsigned sysMsgTime;
} SysVars_t;

extern SysVars_t g_sv;

typedef struct
{
	SDL_Window *window;         // the engine window
	SDL_GLContext context;      // the OpenGL context

	qboolean cdsFullscreen;

	FILE *log_fp;

	int desktopBitsPixel;
	int desktopWidth, desktopHeight;
} glwstate_t;

extern glwstate_t glw_state;

#endif
