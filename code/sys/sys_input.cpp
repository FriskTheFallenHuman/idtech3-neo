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

// sys_input.cpp: mouse and gamepad input through SDL3. The mouse runs in
// SDL's relative mode while the game is active, the gamepad is polled once
// a frame. The old DirectInput MIDI code is gone with DirectInput itself.

#include "../client/client.h"
#include "sys_local.h"
#include "sys_editor.h"

typedef struct {
	int oldButtonState;

	qboolean mouseActive;
	qboolean mouseInitialized;
} SysMouseVars_t;

static SysMouseVars_t s_smv;

//
// Joystick definitions
//
typedef struct {
	qboolean avail;
	SDL_Gamepad *gamepad;

	int oldbuttonstate;
	int oldpovstate;
} joystickInfo_t;

static joystickInfo_t joy;

cvar_t  *k_language;

cvar_t  *in_mouse;
cvar_t  *in_joystick;
cvar_t  *in_joyBallScale;
cvar_t  *in_debugJoystick;
cvar_t  *joy_threshold;

qboolean in_appactive;

// forward-referenced functions
void IN_StartupJoystick( void );
void IN_JoyMove( void );

/*
============================================================

MOUSE CONTROL

============================================================
*/

/*
===========
IN_ActivateMouse

Called when the window gains focus or changes in some way
===========
*/
void IN_ActivateMouse( void ) {
	if ( !s_smv.mouseInitialized ) {
		return;
	}
	if ( !in_mouse->integer ) {
		s_smv.mouseActive = qfalse;
		return;
	}
	if ( s_smv.mouseActive ) {
		return;
	}

	s_smv.mouseActive = qtrue;

	// grab the mouse and hide the cursor while the game has focus
	if ( glw_state.window ) {
		SDL_SetWindowRelativeMouseMode( glw_state.window, true );
		SDL_HideCursor();
	}
}


/*
===========
IN_DeactivateMouse

Called when the window loses focus
===========
*/
void IN_DeactivateMouse( void ) {
	if ( !s_smv.mouseInitialized ) {
		return;
	}
	if ( !s_smv.mouseActive ) {
		return;
	}
	s_smv.mouseActive = qfalse;

	if ( glw_state.window ) {
		SDL_SetWindowRelativeMouseMode( glw_state.window, false );
		SDL_ShowCursor();
	}
}


/*
===========
IN_StartupMouse
===========
*/
void IN_StartupMouse( void ) {
	s_smv.mouseInitialized = qfalse;

	if ( in_mouse->integer == 0 ) {
		Com_Printf( "Mouse control not active.\n" );
		return;
	}

	s_smv.mouseInitialized = qtrue;
}

/*
===========
IN_MouseEvent

The buttons now arrive as SDL events, so this is only used to release a
stuck button latch when the window loses focus.
===========
*/
void IN_MouseEvent( int mstate ) {
	int i;

	if ( !s_smv.mouseInitialized ) {
		return;
	}

// perform button actions
	for ( i = 0 ; i < 3 ; i++ )
	{
		if ( ( mstate & ( 1 << i ) ) &&
			 !( s_smv.oldButtonState & ( 1 << i ) ) ) {
			Sys_QueEvent( g_sv.sysMsgTime, SE_KEY, K_MOUSE1 + i, qtrue, 0, NULL );
		}

		if ( !( mstate & ( 1 << i ) ) &&
			 ( s_smv.oldButtonState & ( 1 << i ) ) ) {
			Sys_QueEvent( g_sv.sysMsgTime, SE_KEY, K_MOUSE1 + i, qfalse, 0, NULL );
		}
	}

	s_smv.oldButtonState = mstate;
}


/*
===========
IN_MouseMove
===========
*/
void IN_MouseMove( void ) {
	float fmx, fmy;
	int mx, my;

	SDL_GetRelativeMouseState( &fmx, &fmy );
	mx = ( int )fmx;
	my = ( int )fmy;

	if ( !mx && !my ) {
		return;
	}

	Sys_QueEvent( 0, SE_MOUSE, mx, my, 0, NULL );
}


/*
=========================================================================

=========================================================================
*/

/*
===========
IN_Startup
===========
*/
void IN_Startup( void ) {
	IN_StartupMouse();
	IN_StartupJoystick();

	in_mouse->modified = qfalse;
	in_joystick->modified = qfalse;
}


/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown( void ) {
	IN_DeactivateMouse();

	if ( joy.gamepad ) {
		SDL_CloseGamepad( joy.gamepad );
		joy.gamepad = NULL;
	}
	joy.avail = qfalse;
}


/*
===========
IN_Init
===========
*/
void IN_Init( void ) {
	// mouse variables
	in_mouse                = Cvar_Get( "in_mouse",                  "1",     CVAR_ARCHIVE | CVAR_LATCH );

	// joystick variables
	in_joystick             = Cvar_Get( "in_joystick",               "0",     CVAR_ARCHIVE | CVAR_LATCH );
	in_joyBallScale         = Cvar_Get( "in_joyBallScale",           "0.02",      CVAR_ARCHIVE );
	in_debugJoystick        = Cvar_Get( "in_debugjoystick",          "0",     CVAR_TEMP );

	joy_threshold           = Cvar_Get( "joy_threshold",         "0.15",      CVAR_ARCHIVE );

	k_language              = Cvar_Get( "k_language",               "american", CVAR_ARCHIVE | CVAR_NORESTART );

	IN_Startup();
}


/*
===========
IN_Activate

Called when the main window gains or loses focus.
The window may have been destroyed and recreated
between a deactivate and an activate.
===========
*/
void IN_Activate( qboolean active ) {
	in_appactive = active;

	if ( !active ) {
		IN_DeactivateMouse();
	}
}


/*
==================
IN_Frame

Called every frame, even if not generating commands
==================
*/
void IN_Frame( void ) {
	if ( Sys_EditorVisible() ) {
		IN_DeactivateMouse();
		return;
	}

	// post joystick events
	IN_JoyMove();

	if ( !s_smv.mouseInitialized ) {
		return;
	}

	if ( cls.keyCatchers & KEYCATCH_CONSOLE ) {
		// temporarily deactivate if not in the game and
		// running on the desktop
		if ( Cvar_VariableValue( "r_fullscreen" ) == 0 ) {
			IN_DeactivateMouse();
			return;
		}
	}

	if ( !in_appactive ) {
		IN_DeactivateMouse();
		return;
	}

	// Only capture the mouse while the game window really is the
	// foreground window, otherwise the cursor stays hidden while
	// another application has focus.
	if ( glw_state.window &&
		 !( SDL_GetWindowFlags( glw_state.window ) & SDL_WINDOW_INPUT_FOCUS ) ) {
		IN_DeactivateMouse();
		return;
	}

	IN_ActivateMouse();

	// post events to the system que
	IN_MouseMove();
}


/*
===================
IN_ClearStates
===================
*/
void IN_ClearStates( void ) {
	s_smv.oldButtonState = 0;
}


/*
=========================================================================

JOYSTICK

=========================================================================
*/

/*
===============
IN_StartupJoystick
===============
*/
void IN_StartupJoystick( void ) {
	int count = 0;
	SDL_JoystickID *gamepads;

	// assume no joystick
	joy.avail = qfalse;
	joy.gamepad = NULL;

	if ( !in_joystick->integer ) {
//		Com_DPrintf ("Joystick is not active.\n");
		return;
	}

	if ( !SDL_Init( SDL_INIT_GAMEPAD ) ) {
		Com_Printf( "joystick not found -- gamepad subsystem unavailable (%s)\n", SDL_GetError() );
		return;
	}

	gamepads = SDL_GetGamepads( &count );
	if ( !gamepads || count <= 0 ) {
		Com_Printf( "joystick not found -- no valid gamepads\n" );
		if ( gamepads ) {
			SDL_free( gamepads );
		}
		return;
	}

	// open the first gamepad
	joy.gamepad = SDL_OpenGamepad( gamepads[0] );
	SDL_free( gamepads );
	if ( !joy.gamepad ) {
		Com_Printf( "joystick not found -- could not open gamepad (%s)\n", SDL_GetError() );
		return;
	}

	Com_DPrintf( "Joystick found.\n" );
	Com_DPrintf( "Name: %s\n", SDL_GetGamepadName( joy.gamepad ) );

	// old button and POV states default to no buttons pressed
	joy.oldbuttonstate = 0;
	joy.oldpovstate = 0;

	// mark the joystick as available
	joy.avail = qtrue;
}

/*
===========
JoyToF
===========
*/
float JoyToF( int value ) {
	float fValue;

	// convert range from -32768..32767 to -1..1
	fValue = (float)value / 32768.0;

	if ( fValue < -1 ) {
		fValue = -1;
	}
	if ( fValue > 1 ) {
		fValue = 1;
	}
	return fValue;
}

int joyDirectionKeys[16] = {
	K_LEFTARROW, K_RIGHTARROW,
	K_UPARROW, K_DOWNARROW,
	K_JOY16, K_JOY17,
	K_JOY18, K_JOY19,
	K_JOY20, K_JOY21,
	K_JOY22, K_JOY23,

	K_JOY24, K_JOY25,
	K_JOY26, K_JOY27
};

/*
===========
IN_JoyMove
===========
*/
void IN_JoyMove( void ) {
	float fAxisValue;
	unsigned int buttonstate, povstate;
	int i;

	// verify joystick is available and that the user wants to use it
	if ( !joy.avail ) {
		return;
	}

	povstate = 0;

	// loop through the gamepad buttons
	buttonstate = 0;
	for ( i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++ ) {
		if ( SDL_GetGamepadButton( joy.gamepad, (SDL_GamepadButton)i ) ) {
			buttonstate |= 1 << i;
		}
	}
	for ( i = 0 ; i < SDL_GAMEPAD_BUTTON_COUNT && i < 15 ; i++ ) {
		if ( ( buttonstate & ( 1 << i ) ) && !( joy.oldbuttonstate & ( 1 << i ) ) ) {
			Sys_QueEvent( g_sv.sysMsgTime, SE_KEY, K_JOY1 + i, qtrue, 0, NULL );
		}
		if ( !( buttonstate & ( 1 << i ) ) && ( joy.oldbuttonstate & ( 1 << i ) ) ) {
			Sys_QueEvent( g_sv.sysMsgTime, SE_KEY, K_JOY1 + i, qfalse, 0, NULL );
		}
	}
	joy.oldbuttonstate = buttonstate;

	// convert main gamepad motion into 6 direction button bits
	fAxisValue = JoyToF( SDL_GetGamepadAxis( joy.gamepad, SDL_GAMEPAD_AXIS_LEFTX ) );
	if ( fAxisValue < -joy_threshold->value ) {
		povstate |= ( 1 << 0 );
	} else if ( fAxisValue > joy_threshold->value ) {
		povstate |= ( 1 << 1 );
	}
	fAxisValue = JoyToF( SDL_GetGamepadAxis( joy.gamepad, SDL_GAMEPAD_AXIS_LEFTY ) );
	if ( fAxisValue < -joy_threshold->value ) {
		povstate |= ( 1 << 2 );
	} else if ( fAxisValue > joy_threshold->value ) {
		povstate |= ( 1 << 3 );
	}
	fAxisValue = JoyToF( SDL_GetGamepadAxis( joy.gamepad, SDL_GAMEPAD_AXIS_RIGHTX ) );
	if ( fAxisValue < -joy_threshold->value ) {
		povstate |= ( 1 << 4 );
	} else if ( fAxisValue > joy_threshold->value ) {
		povstate |= ( 1 << 5 );
	}
	fAxisValue = JoyToF( SDL_GetGamepadAxis( joy.gamepad, SDL_GAMEPAD_AXIS_RIGHTY ) );
	if ( fAxisValue < -joy_threshold->value ) {
		povstate |= ( 1 << 6 );
	} else if ( fAxisValue > joy_threshold->value ) {
		povstate |= ( 1 << 7 );
	}

	// convert the d-pad into 4 button bits
	if ( SDL_GetGamepadButton( joy.gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP ) ) {
		povstate |= 1 << 12;
	}
	if ( SDL_GetGamepadButton( joy.gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN ) ) {
		povstate |= 1 << 13;
	}
	if ( SDL_GetGamepadButton( joy.gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT ) ) {
		povstate |= 1 << 14;
	}
	if ( SDL_GetGamepadButton( joy.gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT ) ) {
		povstate |= 1 << 15;
	}

	// determine which bits have changed and key an auxillary event for each change
	for ( i = 0 ; i < 16 ; i++ ) {
		if ( ( povstate & ( 1 << i ) ) && !( joy.oldpovstate & ( 1 << i ) ) ) {
			Sys_QueEvent( g_sv.sysMsgTime, SE_KEY, joyDirectionKeys[i], qtrue, 0, NULL );
		}

		if ( !( povstate & ( 1 << i ) ) && ( joy.oldpovstate & ( 1 << i ) ) ) {
			Sys_QueEvent( g_sv.sysMsgTime, SE_KEY, joyDirectionKeys[i], qfalse, 0, NULL );
		}
	}
	joy.oldpovstate = povstate;
}
