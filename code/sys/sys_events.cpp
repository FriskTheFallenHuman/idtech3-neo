/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (“RTCW SP Source Code”).  

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

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

// sys_events.cpp: the SDL3 event pump. This replaces the old Win32 window
// procedure; SDL hands us window, keyboard and mouse events and we queue
// them for the rest of the engine with Sys_QueEvent.

#include "../client/client.h"
#include "sys_local.h"
#include "sys_editor.h"

SysVars_t g_sv;

extern cvar_t       *k_language;

// Console variables that we need to access from this module
cvar_t      *vid_xpos;          // X coordinate of window position
cvar_t      *vid_ypos;          // Y coordinate of window position
extern cvar_t *r_fullscreen;

static qboolean s_windowActive;
static qboolean s_windowMinimized;

/*
==================
Sys_UpdateWindowSize

Follow a window resize: tell the renderer and the console about the new
client size. Called from the SDL event loop and from the main loop.
==================
*/
void Sys_UpdateWindowSize() {
    if (!glw_state.window || !cls.rendererStarted || !re.ResizeWindow) {
        return;
    }
    if (SDL_GetWindowFlags(glw_state.window) & SDL_WINDOW_MINIMIZED) {
        return;
    }
    int width = 0, height = 0;
    SDL_GetWindowSize(glw_state.window, &width, &height);
    if (width > 0 && height > 0 &&
        (width != cls.glconfig.vidWidth || height != cls.glconfig.vidHeight)) {
        re.ResizeWindow(width, height, &cls.glconfig);
        Con_CheckResize();
        if (g_sv.activeApp) {
            IN_Activate(qfalse);
            IN_Activate(qtrue);
        }
    }
}

/*
==================
VID_AppActivate
==================
*/
static void VID_AppActivate( qboolean fActive, qboolean minimize ) {
	g_sv.isMinimized = minimize;

	Com_DPrintf( "VID_AppActivate: %i\n", fActive );

	Key_ClearStates();  // FIXME!!!

	// we don't want to act like we're active if we're minimized
	if ( fActive && !g_sv.isMinimized ) {
		g_sv.activeApp = qtrue;
	} else
	{
		g_sv.activeApp = qfalse;
	}

	// minimize/restore mouse-capture on demand
	if ( !g_sv.activeApp ) {
		// Release the win32 button latch so no mouse button stays stuck
		// down across the focus change
		IN_MouseEvent( 0 );
		IN_Activate( qfalse );
	} else
	{
		IN_Activate( qtrue );
	}
}

/*
=======
MapKey

Map from an SDL scancode to a quake keynum. The base mapping is a US
layout; cl_language swaps in the printable keys of a few European
layouts, exactly like the old scan code tables did.
=======
*/
static int MapKey( SDL_Scancode sc ) {
	int result;

	switch ( sc )
	{
	// letters
	case SDL_SCANCODE_A: return 'a';
	case SDL_SCANCODE_B: return 'b';
	case SDL_SCANCODE_C: return 'c';
	case SDL_SCANCODE_D: return 'd';
	case SDL_SCANCODE_E: return 'e';
	case SDL_SCANCODE_F: return 'f';
	case SDL_SCANCODE_G: return 'g';
	case SDL_SCANCODE_H: return 'h';
	case SDL_SCANCODE_I: return 'i';
	case SDL_SCANCODE_J: return 'j';
	case SDL_SCANCODE_K: return 'k';
	case SDL_SCANCODE_L: return 'l';
	case SDL_SCANCODE_M: return 'm';
	case SDL_SCANCODE_N: return 'n';
	case SDL_SCANCODE_O: return 'o';
	case SDL_SCANCODE_P: return 'p';
	case SDL_SCANCODE_Q: return 'q';
	case SDL_SCANCODE_R: return 'r';
	case SDL_SCANCODE_S: return 's';
	case SDL_SCANCODE_T: return 't';
	case SDL_SCANCODE_U: return 'u';
	case SDL_SCANCODE_V: return 'v';
	case SDL_SCANCODE_W: return 'w';
	case SDL_SCANCODE_X: return 'x';
	case SDL_SCANCODE_Y: return 'y';
	case SDL_SCANCODE_Z: return 'z';

	// numbers
	case SDL_SCANCODE_1: return '1';
	case SDL_SCANCODE_2: return '2';
	case SDL_SCANCODE_3: return '3';
	case SDL_SCANCODE_4: return '4';
	case SDL_SCANCODE_5: return '5';
	case SDL_SCANCODE_6: return '6';
	case SDL_SCANCODE_7: return '7';
	case SDL_SCANCODE_8: return '8';
	case SDL_SCANCODE_9: return '9';
	case SDL_SCANCODE_0: return '0';

	case SDL_SCANCODE_RETURN: return 13;
	case SDL_SCANCODE_ESCAPE: return 27;
	case SDL_SCANCODE_BACKSPACE: return K_BACKSPACE;
	case SDL_SCANCODE_TAB: return 9;
	case SDL_SCANCODE_SPACE: return ' ';
	case SDL_SCANCODE_MINUS: result = '-'; break;
	case SDL_SCANCODE_EQUALS: result = '='; break;
	case SDL_SCANCODE_LEFTBRACKET: result = '['; break;
	case SDL_SCANCODE_RIGHTBRACKET: result = ']'; break;
	case SDL_SCANCODE_BACKSLASH: result = '\\'; break;
	case SDL_SCANCODE_SEMICOLON: result = ';'; break;
	case SDL_SCANCODE_APOSTROPHE: result = '\''; break;
	case SDL_SCANCODE_GRAVE: return '`';
	case SDL_SCANCODE_COMMA: result = ','; break;
	case SDL_SCANCODE_PERIOD: result = '.'; break;
	case SDL_SCANCODE_SLASH: result = '/'; break;

	case SDL_SCANCODE_CAPSLOCK: return K_CAPSLOCK;
	case SDL_SCANCODE_F1: return K_F1;
	case SDL_SCANCODE_F2: return K_F2;
	case SDL_SCANCODE_F3: return K_F3;
	case SDL_SCANCODE_F4: return K_F4;
	case SDL_SCANCODE_F5: return K_F5;
	case SDL_SCANCODE_F6: return K_F6;
	case SDL_SCANCODE_F7: return K_F7;
	case SDL_SCANCODE_F8: return K_F8;
	case SDL_SCANCODE_F9: return K_F9;
	case SDL_SCANCODE_F10: return K_F10;
	case SDL_SCANCODE_F11: return K_F11;
	case SDL_SCANCODE_F12: return K_F12;

	case SDL_SCANCODE_PRINTSCREEN: return 0;
	case SDL_SCANCODE_SCROLLLOCK: return K_PAUSE;
	case SDL_SCANCODE_PAUSE: return K_PAUSE;
	case SDL_SCANCODE_INSERT: return K_INS;
	case SDL_SCANCODE_HOME: return K_HOME;
	case SDL_SCANCODE_PAGEUP: return K_PGUP;
	case SDL_SCANCODE_DELETE: return K_DEL;
	case SDL_SCANCODE_END: return K_END;
	case SDL_SCANCODE_PAGEDOWN: return K_PGDN;
	case SDL_SCANCODE_RIGHT: return K_RIGHTARROW;
	case SDL_SCANCODE_LEFT: return K_LEFTARROW;
	case SDL_SCANCODE_DOWN: return K_DOWNARROW;
	case SDL_SCANCODE_UP: return K_UPARROW;

	case SDL_SCANCODE_NUMLOCKCLEAR: return K_KP_NUMLOCK;
	case SDL_SCANCODE_KP_DIVIDE: return K_KP_SLASH;
	case SDL_SCANCODE_KP_MULTIPLY: return K_KP_STAR;
	case SDL_SCANCODE_KP_MINUS: return K_KP_MINUS;
	case SDL_SCANCODE_KP_PLUS: return K_KP_PLUS;
	case SDL_SCANCODE_KP_ENTER: return K_KP_ENTER;
	case SDL_SCANCODE_KP_1: return K_KP_END;
	case SDL_SCANCODE_KP_2: return K_KP_DOWNARROW;
	case SDL_SCANCODE_KP_3: return K_KP_PGDN;
	case SDL_SCANCODE_KP_4: return K_KP_LEFTARROW;
	case SDL_SCANCODE_KP_5: return K_KP_5;
	case SDL_SCANCODE_KP_6: return K_KP_RIGHTARROW;
	case SDL_SCANCODE_KP_7: return K_KP_HOME;
	case SDL_SCANCODE_KP_8: return K_KP_UPARROW;
	case SDL_SCANCODE_KP_9: return K_KP_PGUP;
	case SDL_SCANCODE_KP_0: return K_KP_INS;
	case SDL_SCANCODE_KP_PERIOD: return K_KP_DEL;
	case SDL_SCANCODE_KP_EQUALS: return K_KP_EQUALS;
	case SDL_SCANCODE_NONUSBACKSLASH: result = '\\'; break;

	case SDL_SCANCODE_LCTRL: return K_CTRL;
	case SDL_SCANCODE_LSHIFT: return K_SHIFT;
	case SDL_SCANCODE_LALT: return K_ALT;
	case SDL_SCANCODE_RCTRL: return K_CTRL;
	case SDL_SCANCODE_RSHIFT: return K_SHIFT;
	case SDL_SCANCODE_RALT: return K_ALT;

	default: return 0;
	}

	// The language layouts only differ on the printable keys, so remap
	// those before handing the key to the game.
	if ( cl_language->integer - 1 == LANGUAGE_FRENCH ) {
		switch ( sc )
		{
		case SDL_SCANCODE_A: return 'q';
		case SDL_SCANCODE_Z: return 'w';
		case SDL_SCANCODE_W: return 'z';
		case SDL_SCANCODE_Q: return 'a';
		case SDL_SCANCODE_M: return ',';
		case SDL_SCANCODE_SEMICOLON: return 'm';
		case SDL_SCANCODE_MINUS: return ')';
		case SDL_SCANCODE_EQUALS: return '=';
		case SDL_SCANCODE_LEFTBRACKET: return '^';
		case SDL_SCANCODE_RIGHTBRACKET: return '$';
		case SDL_SCANCODE_APOSTROPHE: return '%';
		case SDL_SCANCODE_BACKSLASH: return '*';
		case SDL_SCANCODE_COMMA: return ';';
		case SDL_SCANCODE_PERIOD: return ':';
		case SDL_SCANCODE_SLASH: return '!';
		case SDL_SCANCODE_NONUSBACKSLASH: return '<';
		default: return result;
		}
	}
	if ( cl_language->integer - 1 == LANGUAGE_GERMAN ) {
		switch ( sc )
		{
		case SDL_SCANCODE_Y: return 'z';
		case SDL_SCANCODE_Z: return 'y';
		case SDL_SCANCODE_MINUS: return '?';
		case SDL_SCANCODE_EQUALS: return '\'';
		case SDL_SCANCODE_LEFTBRACKET: return '=';
		case SDL_SCANCODE_RIGHTBRACKET: return '+';
		case SDL_SCANCODE_SEMICOLON: return '[';
		case SDL_SCANCODE_APOSTROPHE: return ']';
		case SDL_SCANCODE_BACKSLASH: return '#';
		case SDL_SCANCODE_SLASH: return '-';
		case SDL_SCANCODE_NONUSBACKSLASH: return '<';
		default: return result;
		}
	}
	if ( cl_language->integer - 1 == LANGUAGE_ITALIAN ) {
		switch ( sc )
		{
		case SDL_SCANCODE_MINUS: return '\'';
		case SDL_SCANCODE_EQUALS: return '^';
		case SDL_SCANCODE_SEMICOLON: return '@';
		case SDL_SCANCODE_APOSTROPHE: return '#';
		case SDL_SCANCODE_BACKSLASH: return '=';
		case SDL_SCANCODE_SLASH: return '-';
		case SDL_SCANCODE_NONUSBACKSLASH: return '<';
		default: return result;
		}
	}
	if ( cl_language->integer - 1 == LANGUAGE_SPANISH ) {
		switch ( sc )
		{
		case SDL_SCANCODE_MINUS: return '\'';
		case SDL_SCANCODE_EQUALS: return '!';
		case SDL_SCANCODE_SEMICOLON: return '=';
		case SDL_SCANCODE_APOSTROPHE: return '{';
		case SDL_SCANCODE_BACKSLASH: return '}';
		case SDL_SCANCODE_SLASH: return '-';
		case SDL_SCANCODE_NONUSBACKSLASH: return '<';
		default: return result;
		}
	}

	return result;
}

/*
====================
Sys_EventLoop

Pump all pending SDL events and queue them for the engine. Window focus
and resize handling mirrors the old Win32 window procedure.
====================
*/
void Sys_EventLoop( void ) {
	SDL_Event e;

	// make sure the window position cvars exist, like the old WM_CREATE path
	vid_xpos = Cvar_Get( "vid_xpos", "3", CVAR_ARCHIVE );
	vid_ypos = Cvar_Get( "vid_ypos", "22", CVAR_ARCHIVE );

	while ( SDL_PollEvent( &e ) ) {
		// While the editor is shown it consumes the input events; everything
		// else still reaches the engine translation below.
		if ( Sys_EditorProcessEvent( &e ) ) {
			continue;
		}
		switch ( e.type )
		{
		case SDL_EVENT_QUIT:
			Cbuf_AddText( "quit\n" );
			break;

		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			if ( glw_state.window && e.window.windowID == SDL_GetWindowID( glw_state.window ) ) {
				Cbuf_ExecuteText( EXEC_APPEND, "quit" );
			}
			break;

		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			if ( glw_state.window && e.window.windowID == SDL_GetWindowID( glw_state.window ) ) {
				s_windowActive = qtrue;
				s_windowMinimized = ( SDL_GetWindowFlags( glw_state.window ) & SDL_WINDOW_MINIMIZED ) != 0;
				g_sv.sysMsgTime = Sys_Milliseconds();
				VID_AppActivate( qtrue, s_windowMinimized );
				SNDDMA_Activate();
			}
			break;

		case SDL_EVENT_WINDOW_FOCUS_LOST:
			if ( glw_state.window && e.window.windowID == SDL_GetWindowID( glw_state.window ) ) {
				s_windowActive = qfalse;
				g_sv.sysMsgTime = Sys_Milliseconds();
				VID_AppActivate( qfalse, g_sv.isMinimized );
				SNDDMA_Activate();
			}
			break;

		case SDL_EVENT_WINDOW_MINIMIZED:
			if ( glw_state.window && e.window.windowID == SDL_GetWindowID( glw_state.window ) ) {
				s_windowMinimized = qtrue;
				g_sv.sysMsgTime = Sys_Milliseconds();
				VID_AppActivate( qfalse, qtrue );
			}
			break;

		case SDL_EVENT_WINDOW_RESTORED:
			if ( glw_state.window && e.window.windowID == SDL_GetWindowID( glw_state.window ) ) {
				s_windowMinimized = qfalse;
				g_sv.sysMsgTime = Sys_Milliseconds();
				VID_AppActivate( s_windowActive, qfalse );
			}
			break;

		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			if ( glw_state.window && e.window.windowID == SDL_GetWindowID( glw_state.window ) ) {
				g_sv.sysMsgTime = Sys_Milliseconds();
				Sys_UpdateWindowSize();
			}
			break;

		case SDL_EVENT_WINDOW_MOVED:
			if ( glw_state.window && e.window.windowID == SDL_GetWindowID( glw_state.window ) ) {
				if ( !r_fullscreen->integer ) {
					int xPos, yPos;
					SDL_GetWindowPosition( glw_state.window, &xPos, &yPos );
					Cvar_SetValue( "vid_xpos", xPos );
					Cvar_SetValue( "vid_ypos", yPos );
					if ( vid_xpos ) {
						vid_xpos->modified = qfalse;
					}
					if ( vid_ypos ) {
						vid_ypos->modified = qfalse;
					}
				}
			}
			break;

		case SDL_EVENT_KEY_DOWN:
			// alt-enter toggles fullscreen, like the old WM_SYSKEYDOWN path
			if ( e.key.scancode == SDL_SCANCODE_RETURN && ( e.key.mod & SDL_KMOD_ALT ) ) {
				if ( r_fullscreen ) {
					Cvar_SetValue( "r_fullscreen", !r_fullscreen->integer );
					Cbuf_AddText( "vid_restart\n" );
				}
				break;
			}
			if ( e.key.repeat ) {
				break;
			}
			g_sv.sysMsgTime = Sys_Milliseconds();
			Sys_QueEvent( g_sv.sysMsgTime, SE_KEY, MapKey( e.key.scancode ), qtrue, 0, NULL );
			break;

		case SDL_EVENT_KEY_UP:
			g_sv.sysMsgTime = Sys_Milliseconds();
			Sys_QueEvent( g_sv.sysMsgTime, SE_KEY, MapKey( e.key.scancode ), qfalse, 0, NULL );
			break;

		case SDL_EVENT_TEXT_INPUT:
			{
				const unsigned char *text = (const unsigned char *)e.text.text;
				while ( *text ) {
					if ( *text < 128 ) {
						Sys_QueEvent( g_sv.sysMsgTime, SE_CHAR, *text, 0, 0, NULL );
					}
					text++;
				}
			}
			break;

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
			{
				int key = 0;
				qboolean down = ( e.type == SDL_EVENT_MOUSE_BUTTON_DOWN );

				switch ( e.button.button )
				{
				case 1: key = K_MOUSE1; break;
				case 2: key = K_MOUSE3; break;  // middle
				case 3: key = K_MOUSE2; break;  // right
				case 4: key = K_MOUSE4; break;
				case 5: key = K_MOUSE5; break;
				default: key = 0; break;
				}
				if ( key ) {
					g_sv.sysMsgTime = Sys_Milliseconds();
					Sys_QueEvent( g_sv.sysMsgTime, SE_KEY, key, down, 0, NULL );
				}
			}
			break;

		case SDL_EVENT_MOUSE_WHEEL:
			if ( e.wheel.y > 0 ) {
				Sys_QueEvent( g_sv.sysMsgTime, SE_KEY, K_MWHEELUP, qtrue, 0, NULL );
				Sys_QueEvent( g_sv.sysMsgTime, SE_KEY, K_MWHEELUP, qfalse, 0, NULL );
			} else if ( e.wheel.y < 0 )      {
				Sys_QueEvent( g_sv.sysMsgTime, SE_KEY, K_MWHEELDOWN, qtrue, 0, NULL );
				Sys_QueEvent( g_sv.sysMsgTime, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL );
			}
			break;

		default:
			break;
		}
	}
}
