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

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning the license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

// cl_cgame.cpp  -- client system interaction with client game

#include "client.h"

#include "../game/botlib.h"

extern botlib_export_t *botlib_export;

#include "../splines/camera.h"

// RF, this is only used when running a local server
extern void SV_SendMoveSpeedsToGame( int entnum, char *text );
extern qboolean SV_GetModelInfo( int clientNum, char *modelName, animModelInfo_t **modelInfo );


/*
====================
CL_GetGameState
====================
*/
void CL_GetGameState( gameState_t *gs ) {
	*gs = cl.gameState;
}

/*
====================
CL_GetGlconfig
====================
*/
void CL_GetGlconfig( glconfig_t *glconfig ) {
	*glconfig = cls.glconfig;
}


/*
====================
CL_GetUserCmd
====================
*/
qboolean CL_GetUserCmd( int cmdNumber, usercmd_t *ucmd ) {
	// cmds[cmdNumber] is the last properly generated command

	// can't return anything that we haven't created yet
	if ( cmdNumber > cl.cmdNumber ) {
		Com_Error( ERR_DROP, "CL_GetUserCmd: %i >= %i", cmdNumber, cl.cmdNumber );
	}

	// the usercmd has been overwritten in the wrapping
	// buffer because it is too far out of date
	if ( cmdNumber <= cl.cmdNumber - CMD_BACKUP ) {
		return qfalse;
	}

	*ucmd = cl.cmds[ cmdNumber & CMD_MASK ];

	return qtrue;
}

int CL_GetCurrentCmdNumber( void ) {
	return cl.cmdNumber;
}


/*
====================
CL_GetParseEntityState
====================
*/
qboolean    CL_GetParseEntityState( int parseEntityNumber, entityState_t *state ) {
	// can't return anything that hasn't been parsed yet
	if ( parseEntityNumber >= cl.parseEntitiesNum ) {
		Com_Error( ERR_DROP, "CL_GetParseEntityState: %i >= %i",
				   parseEntityNumber, cl.parseEntitiesNum );
	}

	// can't return anything that has been overwritten in the circular buffer
	if ( parseEntityNumber <= cl.parseEntitiesNum - MAX_PARSE_ENTITIES ) {
		return qfalse;
	}

	*state = cl.parseEntities[ parseEntityNumber & ( MAX_PARSE_ENTITIES - 1 ) ];
	return qtrue;
}

/*
====================
CL_GetCurrentSnapshotNumber
====================
*/
void    CL_GetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime ) {
	*snapshotNumber = cl.snap.messageNum;
	*serverTime = cl.snap.serverTime;
}

/*
====================
CL_GetSnapshot
====================
*/
qboolean    CL_GetSnapshot( int snapshotNumber, snapshot_t *snapshot ) {
	clSnapshot_t    *clSnap;
	int i, count;

	if ( snapshotNumber > cl.snap.messageNum ) {
		Com_Error( ERR_DROP, "CL_GetSnapshot: snapshotNumber > cl.snapshot.messageNum" );
	}

	// if the frame has fallen out of the circular buffer, we can't return it
	if ( cl.snap.messageNum - snapshotNumber >= PACKET_BACKUP ) {
		return qfalse;
	}

	// if the frame is not valid, we can't return it
	clSnap = &cl.snapshots[snapshotNumber & PACKET_MASK];
	if ( !clSnap->valid ) {
		return qfalse;
	}

	// if the entities in the frame have fallen out of their
	// circular buffer, we can't return it
	if ( cl.parseEntitiesNum - clSnap->parseEntitiesNum >= MAX_PARSE_ENTITIES ) {
		return qfalse;
	}

	// write the snapshot
	snapshot->snapFlags = clSnap->snapFlags;
	snapshot->serverCommandSequence = clSnap->serverCommandNum;
	snapshot->ping = clSnap->ping;
	snapshot->serverTime = clSnap->serverTime;
	memcpy( snapshot->areamask, clSnap->areamask, sizeof( snapshot->areamask ) );
	snapshot->ps = clSnap->ps;
	count = clSnap->numEntities;
	if ( count > MAX_ENTITIES_IN_SNAPSHOT ) {
		Com_DPrintf( "CL_GetSnapshot: truncated %i entities to %i\n", count, MAX_ENTITIES_IN_SNAPSHOT );
		count = MAX_ENTITIES_IN_SNAPSHOT;
	}
	snapshot->numEntities = count;
	for ( i = 0 ; i < count ; i++ ) {
		snapshot->entities[i] =
			cl.parseEntities[ ( clSnap->parseEntitiesNum + i ) & ( MAX_PARSE_ENTITIES - 1 ) ];
	}

	// FIXME: configstring changes and server commands!!!

	return qtrue;
}

/*
==============
CL_SetUserCmdValue
==============
*/
void CL_SetUserCmdValue( int userCmdValue, int holdableValue, float sensitivityScale, int cld ) {
	cl.cgameUserCmdValue        = userCmdValue;
	cl.cgameUserHoldableValue   = holdableValue;
	cl.cgameSensitivity         = sensitivityScale;
	cl.cgameCld                 = cld;
}

/*
==============
CL_AddCgameCommand
==============
*/
void CL_AddCgameCommand( const char *cmdName ) {
	Cmd_AddCommand( cmdName, NULL );
}

/*
==============
CL_CgameError
==============
*/
void CL_CgameError( const char *string ) {
	Com_Error( ERR_DROP, "%s", string );
}


/*
=====================
CL_ConfigstringModified
=====================
*/
void CL_ConfigstringModified( void ) {
	char        *old, *s;
	int i, index;
	char        *dup;
	gameState_t oldGs;
	int len;

	index = atoi( Cmd_Argv( 1 ) );
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error( ERR_DROP, "configstring > MAX_CONFIGSTRINGS" );
	}
//	s = Cmd_Argv(2);
	// get everything after the "cs <num>"
	s = Cmd_ArgsFrom( 2 );

	old = cl.gameState.stringData + cl.gameState.stringOffsets[ index ];
	if ( !strcmp( old, s ) ) {
		return;     // unchanged
	}

	// build the new gameState_t
	oldGs = cl.gameState;

	memset( &cl.gameState, 0, sizeof( cl.gameState ) );

	// leave the first 0 for uninitialized strings
	cl.gameState.dataCount = 1;

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( i == index ) {
			dup = s;
		} else {
			dup = oldGs.stringData + oldGs.stringOffsets[ i ];
		}
		if ( !dup[0] ) {
			continue;       // leave with the default empty string
		}

		len = strlen( dup );

		if ( len + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
			Com_Error( ERR_DROP, "MAX_GAMESTATE_CHARS exceeded" );
		}

		// append it to the gameState string buffer
		cl.gameState.stringOffsets[ i ] = cl.gameState.dataCount;
		memcpy( cl.gameState.stringData + cl.gameState.dataCount, dup, len + 1 );
		cl.gameState.dataCount += len + 1;
	}

	if ( index == CS_SYSTEMINFO ) {
		// parse serverId and other cvars
		CL_SystemInfoChanged();
	}

}


/*
===================
CL_GetServerCommand

Set up argc/argv for the given command
===================
*/
qboolean CL_GetServerCommand( int serverCommandNumber ) {
	char    *s;
	char    *cmd;
	static char bigConfigString[BIG_INFO_STRING];

	// if we have irretrievably lost a reliable command, drop the connection
	if ( serverCommandNumber <= clc.serverCommandSequence - MAX_RELIABLE_COMMANDS ) {
		// when a demo record was started after the client got a whole bunch of
		// reliable commands then the client never got those first reliable commands
		if ( clc.demoplaying ) {
			return qfalse;
		}
		Com_Error( ERR_DROP, "CL_GetServerCommand: a reliable command was cycled out" );
		return qfalse;
	}

	if ( serverCommandNumber > clc.serverCommandSequence ) {
		Com_Error( ERR_DROP, "CL_GetServerCommand: requested a command not received" );
		return qfalse;
	}

	s = clc.serverCommands[ serverCommandNumber & ( MAX_RELIABLE_COMMANDS - 1 ) ];
	clc.lastExecutedServerCommand = serverCommandNumber;

	Com_DPrintf( "serverCommand: %i : %s\n", serverCommandNumber, s );

rescan:
	Cmd_TokenizeString( s );
	cmd = Cmd_Argv( 0 );

	if ( !strcmp( cmd, "disconnect" ) ) {
		Com_Error( ERR_SERVERDISCONNECT,"Server disconnected\n" );
	}

	if ( !strcmp( cmd, "bcs0" ) ) {
		Com_sprintf( bigConfigString, BIG_INFO_STRING, "cs %s \"%s", Cmd_Argv( 1 ), Cmd_Argv( 2 ) );
		return qfalse;
	}

	if ( !strcmp( cmd, "bcs1" ) ) {
		s = Cmd_Argv( 2 );
		if ( strlen( bigConfigString ) + strlen( s ) >= BIG_INFO_STRING ) {
			Com_Error( ERR_DROP, "bcs exceeded BIG_INFO_STRING" );
		}
		strcat( bigConfigString, s );
		return qfalse;
	}

	if ( !strcmp( cmd, "bcs2" ) ) {
		s = Cmd_Argv( 2 );
		if ( strlen( bigConfigString ) + strlen( s ) + 1 >= BIG_INFO_STRING ) {
			Com_Error( ERR_DROP, "bcs exceeded BIG_INFO_STRING" );
		}
		strcat( bigConfigString, s );
		strcat( bigConfigString, "\"" );
		s = bigConfigString;
		goto rescan;
	}

	if ( !strcmp( cmd, "cs" ) ) {
		CL_ConfigstringModified();
		// reparse the string, because CL_ConfigstringModified may have done another Cmd_TokenizeString()
		Cmd_TokenizeString( s );
		return qtrue;
	}

	if ( !strcmp( cmd, "map_restart" ) ) {
		// clear notify lines and outgoing commands before passing
		// the restart to the cgame
		Con_ClearNotify();
		memset( cl.cmds, 0, sizeof( cl.cmds ) );
		return qtrue;
	}

	if ( !strcmp( cmd, "popup" ) ) { // direct server to client popup request, bypassing cgame
//		trap_UI_Popup(Cmd_Argv(1));
//		if ( cls.state == CA_ACTIVE && !clc.demoplaying ) {
//			VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_CLIPBOARD);
//			Menus_OpenByName(Cmd_Argv(1));
//		}
		return qfalse;
	}


	// the clientLevelShot command is used during development
	// to generate 128*128 screenshots from the intermission
	// point of levels for the menu system to use
	// we pass it along to the cgame to make apropriate adjustments,
	// but we also clear the console and notify lines here
	if ( !strcmp( cmd, "clientLevelShot" ) ) {
		// don't do it if we aren't running the server locally,
		// otherwise malicious remote servers could overwrite
		// the existing thumbnails
		if ( !com_sv_running->integer ) {
			return qfalse;
		}
		// close the console
		Con_Close();
		// take a special screenshot next frame
		Cbuf_AddText( "wait ; wait ; wait ; wait ; screenshot levelshot\n" );
		return qtrue;
	}

	// we may want to put a "connect to other server" command here

	// cgame can now act on the command
	return qtrue;
}


/*
====================
CL_CM_LoadMap

Just adds default parameters that cgame doesn't need to know about
====================
*/
void CL_CM_LoadMap( const char *mapname ) {
	int checksum;

	CM_LoadMap( mapname, qtrue, &checksum );
}

/*
====================
CL_ShutdonwCGame

====================
*/

// The loaded cgame DLL and the exports it provides.
static void *cgameDllHandle = NULL;
cgameExport_t *cgameExport = NULL;

void CL_ShutdownCGame( void ) {
	cls.keyCatchers &= ~KEYCATCH_CGAME;
	cls.cgameStarted = qfalse;
	if ( !cgameExport ) {
		return;
	}
	cgameExport->Shutdown();
	Sys_UnloadDll( cgameDllHandle );
	cgameDllHandle = NULL;
	cgameExport = NULL;
}

static void CG_Print( const char * a1 ) {
	Com_Printf( "%s", a1 );
}

static void CG_Error( const char * a1 ) {
	Com_Error( ERR_DROP, "%s", a1 );
}

static int CG_Milliseconds(  ) {
	return Sys_Milliseconds();
}

static void CG_Cvar_Register( vmCvar_t * a1, const char * a2, const char * a3, int a4 ) {
	Cvar_Register((vmCvar_t *)( a1),(const char *)( a2),(const char *)( a3), a4 );
}

static void CG_Cvar_Update( vmCvar_t * a1 ) {
	Cvar_Update((vmCvar_t *)( a1 ));
}

static void CG_Cvar_Set( const char * a1, const char * a2 ) {
	Cvar_Set((const char *)( a1),(const char *)( a2 ));
}

static void CG_Cvar_VariableStringBuffer( const char * a1, char * a2, int a3 ) {
	Cvar_VariableStringBuffer((const char *)( a1),(char *)( a2), a3 );
}

static int CG_Argc(  ) {
	return Cmd_Argc();
}

static void CG_Argv( int a1, char * a2, int a3 ) {
	Cmd_ArgvBuffer( a1,(char *)( a2), a3 );
}

static void CG_Args( char * a1, int a2 ) {
	Cmd_ArgsBuffer((char *)( a1), a2 );
}

static int CG_FS_FOpenFile( const char * a1, fileHandle_t * a2, fsMode_t a3 ) {
	return FS_FOpenFileByMode((const char *)( a1),(fileHandle_t *)( a2),(fsMode_t)( a3 ));
}

static void CG_FS_Read( void * a1, int a2, fileHandle_t a3 ) {
	FS_Read( a1, a2, a3 );
}

static void CG_FS_Write( const void * a1, int a2, fileHandle_t a3 ) {
	FS_Write( a1, a2, a3 );
}

static void CG_FS_FCloseFile( fileHandle_t a1 ) {
	FS_FCloseFile( a1 );
}

static void CG_SendConsoleCommand( const char * a1 ) {
	Cbuf_AddText((const char *)( a1 ));
}

static void CG_AddCommand( const char * a1 ) {
	CL_AddCgameCommand((const char *)( a1 ));
}

static void CG_SendClientCommand( const char * a1 ) {
	CL_AddReliableCommand((const char *)( a1 ));
}

static void CG_UpdateScreen(  ) {
	// this is used during lengthy level loading, so pump message loop
	//		Com_EventLoop();	// FIXME: if a server restarts here, BAD THINGS HAPPEN!
	// We can't call Com_EventLoop here, a restart will crash and this _does_ happen
	// if there is a map change while we are downloading at pk3.
	// ZOID
	SCR_UpdateScreen();
}

static void CG_CM_LoadMap( const char * a1 ) {
	CL_CM_LoadMap((const char *)( a1 ));
}

static int CG_CM_NumInlineModels(  ) {
	return CM_NumInlineModels();
}

static clipHandle_t CG_CM_InlineModel( int a1 ) {
	return CM_InlineModel( a1 );
}

static clipHandle_t CG_CM_TempBoxModel( const vec3_t a1, const vec3_t a2 ) {
	return CM_TempBoxModel((const vec_t *)( a1),(const vec_t *)( a2), qfalse );
}

static clipHandle_t CG_CM_TempCapsuleModel( const vec3_t a1, const vec3_t a2 ) {
	return CM_TempBoxModel((const vec_t *)( a1),(const vec_t *)( a2), qtrue );
}

static int CG_CM_PointContents( const vec3_t a1, clipHandle_t a2 ) {
	return CM_PointContents((const vec_t *)( a1), a2 );
}

static int CG_CM_TransformedPointContents( const vec3_t a1, clipHandle_t a2, const vec3_t a3, const vec3_t a4 ) {
	return CM_TransformedPointContents((const vec_t *)( a1), a2,(const vec_t *)( a3),(const vec_t *)( a4 ));
}

static void CG_CM_BoxTrace( trace_t * a1, const vec3_t a2, const vec3_t a3, const vec3_t a4, const vec3_t a5, clipHandle_t a6, int a7 ) {
	CM_BoxTrace((trace_t *)( a1),(const vec_t *)( a2),(const vec_t *)( a3),(const vec_t *)( a4),(const vec_t *)( a5), a6, a7, /*int capsule*/ qfalse );
}

static void CG_CM_TransformedBoxTrace( trace_t * a1, const vec3_t a2, const vec3_t a3, const vec3_t a4, const vec3_t a5, clipHandle_t a6, int a7, const vec3_t a8, const vec3_t a9 ) {
	CM_TransformedBoxTrace((trace_t *)( a1),(const vec_t *)( a2),(const vec_t *)( a3),(const vec_t *)( a4),(const vec_t *)( a5), a6, a7,(const vec_t *)( a8),(const vec_t *)( a9), /*int capsule*/ qfalse );
}

static void CG_CM_CapsuleTrace( trace_t * a1, const vec3_t a2, const vec3_t a3, const vec3_t a4, const vec3_t a5, clipHandle_t a6, int a7 ) {
	CM_BoxTrace((trace_t *)( a1),(const vec_t *)( a2),(const vec_t *)( a3),(const vec_t *)( a4),(const vec_t *)( a5), a6, a7, /*int capsule*/ qtrue );
}

static void CG_CM_TransformedCapsuleTrace( trace_t * a1, const vec3_t a2, const vec3_t a3, const vec3_t a4, const vec3_t a5, clipHandle_t a6, int a7, const vec3_t a8, const vec3_t a9 ) {
	CM_TransformedBoxTrace((trace_t *)( a1),(const vec_t *)( a2),(const vec_t *)( a3),(const vec_t *)( a4),(const vec_t *)( a5), a6, a7,(const vec_t *)( a8),(const vec_t *)( a9), /*int capsule*/ qtrue );
}

static int CG_CM_MarkFragments( int a1, const vec3_t * a2, const vec3_t a3, int a4, vec3_t a5, int a6, markFragment_t * a7 ) {
	return re.MarkFragments( a1, (const vec3_t *)(a2), (const vec_t *)(a3), a4, (vec_t *)(a5), a6, (markFragment_t *)(a7) );
}

static void CG_S_StartSound( vec3_t a1, int a2, int a3, sfxHandle_t a4 ) {
	S_StartSound((vec_t *)( a1), a2, a3, a4 );
}

static void CG_S_StartSoundEx( vec3_t a1, int a2, int a3, sfxHandle_t a4, int a5 ) {
	S_StartSoundEx((vec_t *)( a1), a2, a3, a4, a5 );
}

static void CG_S_StartLocalSound( sfxHandle_t a1, int a2 ) {
	S_StartLocalSound( a1, a2 );
}

static void CG_S_ClearLoopingSounds( qboolean a1 ) {
	S_ClearLoopingSounds(); // (SA) modified so no_pvs sounds can function
	// RF, if killall, then stop all sounds
	if ( a1 == 1 ) {
	S_ClearSounds( qtrue, qfalse );
	} else if ( a1 == 2 ) {
	S_ClearSounds( qtrue, qtrue );
	}
}

static void CG_S_AddLoopingSound( int a1, const vec3_t a2, const vec3_t a3, int a4, sfxHandle_t a5, int a6 ) {
	// FIXME MrE: handling of looping sounds changed
	S_AddLoopingSound( a1,(const vec_t *)( a2),(const vec_t *)( a3), a4, a5, a6 );
}

static void CG_S_StopLoopingSound( int a1 ) {
	// RF, not functional anymore, since we reverted to old looping code
	//S_StopLoopingSound( a1 );
}

static void CG_S_StopStreamingSound( int a1 ) {
	S_StopEntStreamingSound( a1 );
}

static void CG_S_UpdateEntityPosition( int a1, const vec3_t a2 ) {
	S_UpdateEntityPosition( a1,(const vec_t *)( a2 ));
}

static int CG_S_GetVoiceAmplitude( int a1 ) {
	return S_GetVoiceAmplitude( a1 );
}

static void CG_S_Respatialize( int a1, const vec3_t a2, vec3_t * a3, int a4 ) {
	S_Respatialize( a1,(const vec_t *)( a2),(vec3_t *)( a3), a4 );
}

static sfxHandle_t CG_S_RegisterSound( const char * a1 ) {
	#ifdef DOOMSOUND    ///// (SA) DOOMSOUND
	return S_RegisterSound( a1 );
	#else
	return S_RegisterSound((const char *)( a1), qfalse );
	#endif  ///// (SA) DOOMSOUND
}

static void CG_S_StartBackgroundTrack( const char * a1, const char * a2, int a3 ) {
	S_StartBackgroundTrack((const char *)( a1),(const char *)( a2), a3 );  //----(SA)	added fadeup time
}

static void CG_S_FadeBackgroundTrack( float a1, int a2, int a3 ) {
	S_FadeStreamingSound( a1, a2, a3 ); //----(SA)	added music/all-streaming options
}

static void CG_S_FadeAllSound( float a1, int a2 ) {
	S_FadeAllSounds( a1, a2 );   //----(SA)	added
}

static void CG_S_StartStreamingSound( const char * a1, const char * a2, int a3, int a4, int a5 ) {
	S_StartStreamingSound((const char *)( a1),(const char *)( a2), a3, a4, a5 );
}

static void CG_R_LoadWorldMap( const char * a1 ) {
	re.LoadWorld( (const char *)(a1) );
}

static qhandle_t CG_R_RegisterModel( const char * a1 ) {
	return re.RegisterModel( (const char *)(a1) );
}

static qboolean CG_R_GetSkinModel( qhandle_t a1, const char * a2, char * a3 ) {
	return re.GetSkinModel( a1, (const char *)(a2), (char *)(a3) );
}

static qhandle_t CG_R_GetShaderFromModel( qhandle_t a1, int a2, int a3 ) {
	return re.GetShaderFromModel( a1, a2, a3 );
}

static qhandle_t CG_R_RegisterSkin( const char * a1 ) {
	return re.RegisterSkin( (const char *)(a1) );
}

static qhandle_t CG_R_RegisterShader( const char * a1 ) {
	return re.RegisterShader( (const char *)(a1) );
}

static qhandle_t CG_R_RegisterShaderNoMip( const char * a1 ) {
	return re.RegisterShaderNoMip( (const char *)(a1) );
}

static void CG_R_RegisterFont( const char * a1, int a2, fontInfo_t * a3 ) {
	re.RegisterFont( (const char *)(a1), a2, (fontInfo_t *)(a3) );
}

static void CG_R_ClearScene(  ) {
	re.ClearScene();
}

static void CG_R_AddRefEntityToScene( const refEntity_t * a1 ) {
	re.AddRefEntityToScene( (const refEntity_t *)(a1) );
}

static void CG_R_AddPolyToScene( qhandle_t a1, int a2, const polyVert_t * a3 ) {
	re.AddPolyToScene( a1, a2, (const polyVert_t *)(a3) );
}

static void CG_R_AddPolysToScene( qhandle_t a1, int a2, const polyVert_t * a3, int a4 ) {
	re.AddPolysToScene( a1, a2, (const polyVert_t *)(a3), a4 );
}

static void CG_RB_ZombieFXAddNewHit( int a1, const vec3_t a2, const vec3_t a3 ) {
	re.ZombieFXAddNewHit( a1, (const vec_t *)(a2), (const vec_t *)(a3) );
}

static void CG_R_AddLightToScene( const vec3_t a1, float a2, float a3, float a4, float a5, int a6 ) {
	re.AddLightToScene( (const vec_t *)(a1), a2, a3, a4, a5, a6 );
}

static void CG_R_AddCoronaToScene( const vec3_t a1, float a2, float a3, float a4, float a5, int a6, int a7 ) {
	re.AddCoronaToScene( a1, a2, a3, a4, a5, a6, a7 );
}

static void CG_R_SetFog( int a1, int a2, int a3, float a4, float a5, float a6, float a7 ) {
	re.SetFog( a1, a2, a3, a4, a5, a6, a7 );
}

static void CG_R_RenderScene( const refdef_t * a1 ) {
	re.RenderScene( (const refdef_t *)(a1) );
}

static void CG_R_SetColor( const float * a1 ) {
	re.SetColor( (const float *)(a1) );
}

static void CG_R_DrawStretchPic( float a1, float a2, float a3, float a4, float a5, float a6, float a7, float a8, qhandle_t a9 ) {
	re.DrawStretchPic( a1, a2, a3, a4, a5, a6, a7, a8, a9 );
}

static void CG_R_DrawStretchPicGradient( float a1, float a2, float a3, float a4, float a5, float a6, float a7, float a8, qhandle_t a9, const float * a10, int a11 ) {
	re.DrawStretchPicGradient( a1, a2, a3, a4, a5, a6, a7, a8, a9, (const float *)(a10), a11 );
}

static void CG_R_ModelBounds( clipHandle_t a1, vec3_t a2, vec3_t a3 ) {
	re.ModelBounds( a1, (vec_t *)(a2), (vec_t *)(a3) );
}

static int CG_R_LerpTag( orientation_t * a1, const refEntity_t * a2, const char * a3, int a4 ) {
	return re.LerpTag( (orientation_t *)(a1), (const refEntity_t *)(a2), (const char *)(a3), a4 );
}

static void CG_R_RemapShader( const char * a1, const char * a2, const char * a3 ) {
	re.RemapShader( (const char *)(a1), (const char *)(a2), (const char *)(a3) );
}

static void CG_GetGlconfig( glconfig_t * a1 ) {
	CL_GetGlconfig((glconfig_t *)( a1 ));
}

static void CG_GetGameState( gameState_t * a1 ) {
	CL_GetGameState((gameState_t *)( a1 ));
}

static void CG_GetCurrentSnapshotNumber( int * a1, int * a2 ) {
	CL_GetCurrentSnapshotNumber((int *)( a1),(int *)( a2 ));
}

static qboolean CG_GetSnapshot( int a1, snapshot_t * a2 ) {
	return CL_GetSnapshot( a1,(snapshot_t *)( a2 ));
}

static qboolean CG_GetServerCommand( int a1 ) {
	return CL_GetServerCommand( a1 );
}

static int CG_GetCurrentCmdNumber(  ) {
	return CL_GetCurrentCmdNumber();
}

static qboolean CG_GetUserCmd( int a1, usercmd_t * a2 ) {
	return CL_GetUserCmd( a1,(usercmd_t *)( a2 ));
}

static void CG_SetUserCmdValue( int a1, int a2, float a3, int a4 ) {
	CL_SetUserCmdValue( a1, a2, a3, a4 );    //----(SA)	modified	// NERVE - SMF - added fourth arg [cld]
}

static int CG_MemoryRemaining(  ) {
	return Hunk_MemoryRemaining();
}

static qboolean CG_loadCamera( int a1, const char * a2 ) {
	return loadCamera( a1,(const char *)( a2 ));
}

static void CG_startCamera( int a1, int a2 ) {
	if ( a1 == 0 ) {  // CAM_PRIMARY
	cl.cameraMode = qtrue;  //----(SA)	added
	}
	startCamera( a1, a2 );
}

static void CG_stopCamera( int a1 ) {
	if ( a1 == 0 ) {  // CAM_PRIMARY
	cl.cameraMode = qfalse;
	}
	//		stopCamera(a1);
}

static qboolean CG_getCameraInfo( int a1, int a2, vec3_t * a3, vec3_t * a4, float * a5 ) {
	return getCameraInfo( a1, a2,(float *)( a3),(float *)( a4),(float *)( a5 ));
}

static qboolean CG_Key_IsDown( int a1 ) {
	return Key_IsDown( a1 );
}

static int CG_Key_GetCatcher(  ) {
	return Key_GetCatcher();
}

static void CG_Key_SetCatcher( int a1 ) {
	Key_SetCatcher( a1 );
}

static int CG_Key_GetKey( const char * a1 ) {
	return Key_GetKey((const char *)( a1 ));
}

static int CG_PC_AddGlobalDefine( char * a1 ) {
	return botlib_export->PC_AddGlobalDefine( (char *)(a1) );
}

static int CG_PC_LoadSource( const char * a1 ) {
	return botlib_export->PC_LoadSourceHandle( (const char *)(a1) );
}

static int CG_PC_FreeSource( int a1 ) {
	return botlib_export->PC_FreeSourceHandle( a1 );
}

static int CG_PC_ReadToken( int a1, pc_token_t * a2 ) {
	return botlib_export->PC_ReadTokenHandle( a1, (pc_token_t *)(a2) );
}

static int CG_PC_SourceFileAndLine( int a1, char * a2, int * a3 ) {
	return botlib_export->PC_SourceFileAndLine( a1, (char *)(a2), (int *)(a3) );
}

static void CG_S_StopBackgroundTrack(  ) {
	S_StopBackgroundTrack();
}

static int CG_RealTime( qtime_t * a1 ) {
	return Com_RealTime((qtime_t *)( a1 ));
}

static void CG_SnapVector( float * a1 ) {
	Sys_SnapVector((float *)( a1 ));
}

static void CG_SendMoveSpeedsToGame( int a1, char * a2 ) {
	SV_SendMoveSpeedsToGame( a1,(char *)( a2 ));
}

static int CG_CIN_PlayCinematic( const char * a1, int a2, int a3, int a4, int a5, int a6 ) {
	return CIN_PlayCinematic((const char *)( a1), a2, a3, a4, a5, a6 );
}

static e_status CG_CIN_StopCinematic( int a1 ) {
	return CIN_StopCinematic( a1 );
}

static e_status CG_CIN_RunCinematic( int a1 ) {
	return CIN_RunCinematic( a1 );
}

static void CG_CIN_DrawCinematic( int a1 ) {
	CIN_DrawCinematic( a1 );
}

static void CG_CIN_SetExtents( int a1, int a2, int a3, int a4, int a5 ) {
	CIN_SetExtents( a1, a2, a3, a4, a5 );
}

static qboolean CG_GetEntityToken( char * a1, int a2 ) {
	return re.GetEntityToken( (char *)(a1), a2 );
}

static void CG_UI_Popup( const char * a1 ) {
	if ( a1 && !Q_stricmp((const char *)( a1), "briefing" ) ) {  //----(SA) added
	uiExport->SetActiveMenu( UIMENU_BRIEFING );
	return;
	}

	if ( cls.state == CA_ACTIVE && !clc.demoplaying ) {
	// NERVE - SMF
	if ( a1 && !Q_stricmp((const char *)( a1), "UIMENU_WM_PICKTEAM" ) ) {
	uiExport->SetActiveMenu( UIMENU_WM_PICKTEAM );
	} else if ( a1 && !Q_stricmp((const char *)( a1), "UIMENU_WM_PICKPLAYER" ) )    {
	uiExport->SetActiveMenu( UIMENU_WM_PICKPLAYER );
	} else if ( a1 && !Q_stricmp((const char *)( a1), "UIMENU_WM_QUICKMESSAGE" ) )    {
	uiExport->SetActiveMenu( UIMENU_WM_QUICKMESSAGE );
	} else if ( a1 && !Q_stricmp((const char *)( a1), "UIMENU_WM_LIMBO" ) )    {
	uiExport->SetActiveMenu( UIMENU_WM_LIMBO );
	}
	// -NERVE - SMF
	else if ( a1 && !Q_stricmp((const char *)( a1), "hbook1" ) ) {   //----(SA)
	uiExport->SetActiveMenu( UIMENU_BOOK1 );
	} else if ( a1 && !Q_stricmp((const char *)( a1), "hbook2" ) )    { //----(SA)
	uiExport->SetActiveMenu( UIMENU_BOOK2 );
	} else if ( a1 && !Q_stricmp((const char *)( a1), "hbook3" ) )    { //----(SA)
	uiExport->SetActiveMenu( UIMENU_BOOK3 );
	} else if ( a1 && !Q_stricmp((const char *)( a1), "pregame" ) )    { //----(SA) added
	uiExport->SetActiveMenu( UIMENU_PREGAME );
	} else {
	uiExport->SetActiveMenu( UIMENU_CLIPBOARD );
	}
	}
}

static void CG_UI_LimboChat( const char * a1 ) {
	if ( a1 ) {
	CL_AddToLimboChat((const char *)( a1 ));
	}
}

static void CG_UI_ClosePopup( const char * a1 ) {
	uiExport->KeyEvent( K_ESCAPE, qtrue );
}

static qboolean CG_GetModelInfo( int a1, char * a2, void ** a3 ) {
	return SV_GetModelInfo( a1,(char *)( a2),(animModelInfo_t **)( a3 ));
}

static void CG_R_SetDoomSectorLight( int a1, int a2 ) {
	re.SetDoomSectorLight(a1, a2);
}

static void CG_R_SetDoomSwitches( const char * a1 ) {
	re.SetDoomSwitches(static_cast<const char *>(a1));
}

static int CG_R_LightForPoint( vec3_t a1, vec3_t a2, vec3_t a3, vec3_t a4 ) {
	return re.LightForPoint(static_cast<float *>(a1), static_cast<float *>(a2),
	static_cast<float *>(a3), static_cast<float *>(a4));
}

static void CG_R_DrawDoomMap( int a1, int a2, int a3, int a4, const byte * a5 ) {
	if (a3 <= 0 || a4 <= 0 || a3 > 16384 || a4 > 16384 || !a5) {
	Com_Error(ERR_DROP, "Invalid Doom automap image");
	}
	re.DrawStretchRaw(a1, a2, a3, a4, 256, 256,
	static_cast<const byte *>(a5), 31, qtrue);
	}

static const cgameImport_t cgameImportTable = {
	CGAME_IMPORT_API_VERSION,
	CG_Print,
	CG_Error,
	CG_Milliseconds,
	CG_Cvar_Register,
	CG_Cvar_Update,
	CG_Cvar_Set,
	CG_Cvar_VariableStringBuffer,
	CG_Argc,
	CG_Argv,
	CG_Args,
	CG_FS_FOpenFile,
	CG_FS_Read,
	CG_FS_Write,
	CG_FS_FCloseFile,
	CG_SendConsoleCommand,
	CG_AddCommand,
	CG_SendClientCommand,
	CG_UpdateScreen,
	CG_CM_LoadMap,
	CG_CM_NumInlineModels,
	CG_CM_InlineModel,
	CG_CM_TempBoxModel,
	CG_CM_TempCapsuleModel,
	CG_CM_PointContents,
	CG_CM_TransformedPointContents,
	CG_CM_BoxTrace,
	CG_CM_TransformedBoxTrace,
	CG_CM_CapsuleTrace,
	CG_CM_TransformedCapsuleTrace,
	CG_CM_MarkFragments,
	CG_S_StartSound,
	CG_S_StartSoundEx,
	CG_S_StartLocalSound,
	CG_S_ClearLoopingSounds,
	CG_S_AddLoopingSound,
	CG_S_StopLoopingSound,
	CG_S_StopStreamingSound,
	CG_S_UpdateEntityPosition,
	CG_S_GetVoiceAmplitude,
	CG_S_Respatialize,
	CG_S_RegisterSound,
	CG_S_StartBackgroundTrack,
	CG_S_FadeBackgroundTrack,
	CG_S_FadeAllSound,
	CG_S_StartStreamingSound,
	CG_R_LoadWorldMap,
	CG_R_RegisterModel,
	CG_R_GetSkinModel,
	CG_R_GetShaderFromModel,
	CG_R_RegisterSkin,
	CG_R_RegisterShader,
	CG_R_RegisterShaderNoMip,
	CG_R_RegisterFont,
	CG_R_ClearScene,
	CG_R_AddRefEntityToScene,
	CG_R_AddPolyToScene,
	CG_R_AddPolysToScene,
	CG_RB_ZombieFXAddNewHit,
	CG_R_AddLightToScene,
	CG_R_AddCoronaToScene,
	CG_R_SetFog,
	CG_R_RenderScene,
	CG_R_SetColor,
	CG_R_DrawStretchPic,
	CG_R_DrawStretchPicGradient,
	CG_R_ModelBounds,
	CG_R_LerpTag,
	CG_R_RemapShader,
	CG_GetGlconfig,
	CG_GetGameState,
	CG_GetCurrentSnapshotNumber,
	CG_GetSnapshot,
	CG_GetServerCommand,
	CG_GetCurrentCmdNumber,
	CG_GetUserCmd,
	CG_SetUserCmdValue,
	CG_MemoryRemaining,
	CG_loadCamera,
	CG_startCamera,
	CG_stopCamera,
	CG_getCameraInfo,
	CG_Key_IsDown,
	CG_Key_GetCatcher,
	CG_Key_SetCatcher,
	CG_Key_GetKey,
	CG_PC_AddGlobalDefine,
	CG_PC_LoadSource,
	CG_PC_FreeSource,
	CG_PC_ReadToken,
	CG_PC_SourceFileAndLine,
	CG_S_StopBackgroundTrack,
	CG_RealTime,
	CG_SnapVector,
	CG_SendMoveSpeedsToGame,
	CG_CIN_PlayCinematic,
	CG_CIN_StopCinematic,
	CG_CIN_RunCinematic,
	CG_CIN_DrawCinematic,
	CG_CIN_SetExtents,
	CG_GetEntityToken,
	CG_UI_Popup,
	CG_UI_LimboChat,
	CG_UI_ClosePopup,
	CG_GetModelInfo,
	CG_R_SetDoomSectorLight,
	CG_R_SetDoomSwitches,
	CG_R_LightForPoint,
	CG_R_DrawDoomMap,
};

/*
====================
CL_UpdateLevelHunkUsage

  This updates the "hunkusage.dat" file with the current map and it's hunk usage count

  This is used for level loading, so we can show a percentage bar dependant on the amount
  of hunk memory allocated so far

  This will be slightly inaccurate if some settings like sound quality are changed, but these
  things should only account for a small variation (hopefully)
====================
*/
void CL_UpdateLevelHunkUsage( void ) {
	int handle;
	char *memlistfile = "hunkusage.dat";
	char *buf, *outbuf;
	char *buftrav, *outbuftrav;
	char *token;
	char outstr[256];
	int len, memusage;

	memusage = Cvar_VariableIntegerValue( "com_hunkused" ) + Cvar_VariableIntegerValue( "hunk_soundadjust" );

	len = FS_FOpenFileByMode( memlistfile, &handle, FS_READ );
	if ( len >= 0 ) { // the file exists, so read it in, strip out the current entry for this map, and save it out, so we can append the new value

		buf = (char *)Z_Malloc( len + 1 );
		memset( buf, 0, len + 1 );
		outbuf = (char *)Z_Malloc( len + 1 );
		memset( outbuf, 0, len + 1 );

		FS_Read( (void *)buf, len, handle );
		FS_FCloseFile( handle );

		// now parse the file, filtering out the current map
		buftrav = buf;
		outbuftrav = outbuf;
		outbuftrav[0] = '\0';
		while ( ( token = COM_Parse( &buftrav ) ) && token[0] ) {
			if ( !Q_strcasecmp( token, cl.mapname ) ) {
				// found a match
				token = COM_Parse( &buftrav );  // read the size
				if ( token && token[0] ) {
					if ( atoi( token ) == memusage ) {  // if it is the same, abort this process
						Z_Free( buf );
						Z_Free( outbuf );
						return;
					}
				}
			} else {    // send it to the outbuf
				Q_strcat( outbuftrav, len + 1, token );
				Q_strcat( outbuftrav, len + 1, " " );
				token = COM_Parse( &buftrav );  // read the size
				if ( token && token[0] ) {
					Q_strcat( outbuftrav, len + 1, token );
					Q_strcat( outbuftrav, len + 1, "\n" );
				} else {
					Com_Error( ERR_DROP, "hunkusage.dat file is corrupt\n" );
				}
			}
		}

#ifdef __MACOS__    //DAJ MacOS file typing
		{
			extern _MSL_IMP_EXP_C long _fcreator, _ftype;
			_ftype = 'WlfB';
			_fcreator = 'WlfS';
		}
#endif
		handle = FS_FOpenFileWrite( memlistfile );
		if ( handle < 0 ) {
			Com_Error( ERR_DROP, "cannot create %s\n", memlistfile );
		}
		// input file is parsed, now output to the new file
		len = strlen( outbuf );
		if ( FS_Write( (void *)outbuf, len, handle ) != len ) {
			Com_Error( ERR_DROP, "cannot write to %s\n", memlistfile );
		}
		FS_FCloseFile( handle );

		Z_Free( buf );
		Z_Free( outbuf );
	}
	// now append the current map to the current file
	FS_FOpenFileByMode( memlistfile, &handle, FS_APPEND );
	if ( handle < 0 ) {
		Com_Error( ERR_DROP, "cannot write to hunkusage.dat, check disk full\n" );
	}
	Com_sprintf( outstr, sizeof( outstr ), "%s %i\n", cl.mapname, memusage );
	FS_Write( outstr, strlen( outstr ), handle );
	FS_FCloseFile( handle );

	// now just open it and close it, so it gets copied to the pak dir
	len = FS_FOpenFileByMode( memlistfile, &handle, FS_READ );
	if ( len >= 0 ) {
		FS_FCloseFile( handle );
	}
}

/*
====================
CL_InitCGame

Should only by called by CL_StartHunkUsers
====================
*/
void CL_InitCGame( void ) {
	const char          *info;
	const char          *mapname;
	int t1, t2;

	t1 = Sys_Milliseconds();

	// put away the console
	Con_Close();

	// find the current mapname
	info = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
	mapname = Info_ValueForKey( info, "mapname" );
	Com_sprintf( cl.mapname, sizeof( cl.mapname ), "maps/%s.bsp", mapname );

	// load the cgame DLL
	cgameExport = (cgameExport_t *)Sys_LoadDll( "cgame", &cgameDllHandle, "GetCGameAPI", (void *)&cgameImportTable );
	if ( !cgameExport ) {
		Com_Error( ERR_DROP, "Sys_LoadDll on cgame failed" );
	}
	cls.state = CA_LOADING;

	// init for this gamestate
	// use the lastExecutedServerCommand instead of the serverCommandSequence
	// otherwise server commands sent just before a gamestate are dropped
	cgameExport->Init( clc.serverMessageSequence, clc.lastExecutedServerCommand );

	// we will send a usercmd this frame, which
	// will cause the server to send us the first snapshot
	cls.state = CA_PRIMED;

	t2 = Sys_Milliseconds();

	Com_Printf( "CL_InitCGame: %5.2f seconds\n", ( t2 - t1 ) / 1000.0 );

	// have the renderer touch all its images, so they are present
	// on the card even if the driver does deferred loading
	re.EndRegistration();

	// make sure everything is paged in
	if ( !Sys_LowPhysicalMemory() ) {
		Com_TouchMemory();
	}

	// clear anything that got printed
	Con_ClearNotify();

	// Ridah, update the memory usage file
	CL_UpdateLevelHunkUsage();
}


/*
====================
CL_GameCommand

See if the current console command is claimed by the cgame
====================
*/
qboolean CL_GameCommand( void ) {
	if ( !cgameExport ) {
		return qfalse;
	}

	return cgameExport->ConsoleCommand();
}



/*
=====================
CL_CGameRendering
=====================
*/
void CL_CGameRendering( stereoFrame_t stereo ) {
	cgameExport->DrawActiveFrame( cl.serverTime, stereo, clc.demoplaying );

}


/*
=================
CL_AdjustTimeDelta

Adjust the clients view of server time.

We attempt to have cl.serverTime exactly equal to the server's view
of time plus the timeNudge, but with variable latencies over the
internet it will often need to drift a bit to match conditions.

Our ideal time would be to have the adjusted time approach, but not pass,
the very latest snapshot.

Adjustments are only made when a new snapshot arrives with a rational
latency, which keeps the adjustment process framerate independent and
prevents massive overadjustment during times of significant packet loss
or bursted delayed packets.
=================
*/

#define RESET_TIME  500

void CL_AdjustTimeDelta( void ) {
	int newDelta;
	int deltaDelta;

	cl.newSnapshots = qfalse;

	// the delta never drifts when replaying a demo
	if ( clc.demoplaying ) {
		return;
	}

	newDelta = cl.snap.serverTime - cls.realtime;
	deltaDelta = abs( newDelta - cl.serverTimeDelta );

	if ( deltaDelta > RESET_TIME ) {
		cl.serverTimeDelta = newDelta;
		cl.oldServerTime = cl.snap.serverTime;  // FIXME: is this a problem for cgame?
		cl.serverTime = cl.snap.serverTime;
		if ( cl_showTimeDelta->integer ) {
			Com_Printf( "<RESET> " );
		}
	} else if ( deltaDelta > 100 ) {
		// fast adjust, cut the difference in half
		if ( cl_showTimeDelta->integer ) {
			Com_Printf( "<FAST> " );
		}
		cl.serverTimeDelta = ( cl.serverTimeDelta + newDelta ) >> 1;
	} else {
		// slow drift adjust, only move 1 or 2 msec

		// if any of the frames between this and the previous snapshot
		// had to be extrapolated, nudge our sense of time back a little
		// the granularity of +1 / -2 is too high for timescale modified frametimes
		if ( com_timescale->value == 0 || com_timescale->value == 1 ) {
			if ( cl.extrapolatedSnapshot ) {
				cl.extrapolatedSnapshot = qfalse;
				cl.serverTimeDelta -= 2;
			} else {
				// otherwise, move our sense of time forward to minimize total latency
				cl.serverTimeDelta++;
			}
		}
	}

	if ( cl_showTimeDelta->integer ) {
		Com_Printf( "%i ", cl.serverTimeDelta );
	}
}


/*
==================
CL_FirstSnapshot
==================
*/
void CL_FirstSnapshot( void ) {
	// ignore snapshots that don't have entities
	if ( cl.snap.snapFlags & SNAPFLAG_NOT_ACTIVE ) {
		return;
	}
	cls.state = CA_ACTIVE;

	// set the timedelta so we are exactly on this first frame
	cl.serverTimeDelta = cl.snap.serverTime - cls.realtime;
	cl.oldServerTime = cl.snap.serverTime;

	clc.timeDemoBaseTime = cl.snap.serverTime;

	// if this is the first frame of active play,
	// execute the contents of activeAction now
	// this is to allow scripting a timedemo to start right
	// after loading
	if ( cl_activeAction->string[0] ) {
		Cbuf_AddText( cl_activeAction->string );
		Cvar_Set( "activeAction", "" );
	}

	Sys_BeginProfiling();
}

/*
==================
CL_SetCGameTime
==================
*/
void CL_SetCGameTime( void ) {
	// getting a valid frame message ends the connection process
	if ( cls.state != CA_ACTIVE ) {
		if ( cls.state != CA_PRIMED ) {
			return;
		}
		if ( clc.demoplaying ) {
			// we shouldn't get the first snapshot on the same frame
			// as the gamestate, because it causes a bad time skip
			if ( !clc.firstDemoFrameSkipped ) {
				clc.firstDemoFrameSkipped = qtrue;
				return;
			}
			CL_ReadDemoMessage();
		}
		if ( cl.newSnapshots ) {
			cl.newSnapshots = qfalse;
			CL_FirstSnapshot();
		}
		if ( cls.state != CA_ACTIVE ) {
			return;
		}
	}

	// if we have gotten to this point, cl.snap is guaranteed to be valid
	if ( !cl.snap.valid ) {
		Com_Error( ERR_DROP, "CL_SetCGameTime: !cl.snap.valid" );
	}

	// allow pause in single player
	if ( sv_paused->integer && cl_paused->integer && com_sv_running->integer ) {
		// paused
		return;
	}

	if ( cl.snap.serverTime < cl.oldFrameServerTime ) {
		// Ridah, if this is a localhost, then we are probably loading a savegame
		if ( !Q_stricmp( cls.servername, "localhost" ) ) {
			// do nothing?
			CL_FirstSnapshot();
		} else {
			Com_Error( ERR_DROP, "cl.snap.serverTime < cl.oldFrameServerTime" );
		}
	}
	cl.oldFrameServerTime = cl.snap.serverTime;


	// get our current view of time

	if ( clc.demoplaying && cl_freezeDemo->integer ) {
		// cl_freezeDemo is used to lock a demo in place for single frame advances

	} else {
		// cl_timeNudge is a user adjustable cvar that allows more
		// or less latency to be added in the interest of better
		// smoothness or better responsiveness.
		int tn;

		tn = cl_timeNudge->integer;
		if ( tn < -30 ) {
			tn = -30;
		} else if ( tn > 30 ) {
			tn = 30;
		}

		cl.serverTime = cls.realtime + cl.serverTimeDelta - tn;

		// guarantee that time will never flow backwards, even if
		// serverTimeDelta made an adjustment or cl_timeNudge was changed
		if ( cl.serverTime < cl.oldServerTime ) {
			cl.serverTime = cl.oldServerTime;
		}
		cl.oldServerTime = cl.serverTime;

		// note if we are almost past the latest frame (without timeNudge),
		// so we will try to adjust back a bit when the next snapshot arrives
		if ( cls.realtime + cl.serverTimeDelta >= cl.snap.serverTime - 5 ) {
			cl.extrapolatedSnapshot = qtrue;
		}
	}

	// if we have gotten new snapshots, drift serverTimeDelta
	// don't do this every frame, or a period of packet loss would
	// make a huge adjustment
	if ( cl.newSnapshots ) {
		CL_AdjustTimeDelta();
	}

	if ( !clc.demoplaying ) {
		return;
	}

	// if we are playing a demo back, we can just keep reading
	// messages from the demo file until the cgame definately
	// has valid snapshots to interpolate between

	// a timedemo will always use a deterministic set of time samples
	// no matter what speed machine it is run on,
	// while a normal demo may have different time samples
	// each time it is played back
	if ( cl_timedemo->integer ) {
		if ( !clc.timeDemoStart ) {
			clc.timeDemoStart = Sys_Milliseconds();
		}
		clc.timeDemoFrames++;
		cl.serverTime = clc.timeDemoBaseTime + clc.timeDemoFrames * 50;
	}

	while ( cl.serverTime >= cl.snap.serverTime ) {
		// feed another messag, which should change
		// the contents of cl.snap
		CL_ReadDemoMessage();
		if ( cls.state != CA_ACTIVE ) {
			return;     // end of demo
		}
	}

}

/*
====================
CL_GetTag
====================
*/
qboolean CL_GetTag( int clientNum, char *tagname, orientation_t *or ) {
	if ( !cgameExport ) {
		return qfalse;
	}

	return cgameExport->GetTag( clientNum, tagname, or );
}


