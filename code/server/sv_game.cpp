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

// sv_game.cpp -- interface to the game dll

#include "server.h"

#include "../game/botlib.h"

botlib_export_t *botlib_export;

void SV_GameError( const char *string ) {
	Com_Error( ERR_DROP, "%s", string );
}

void SV_GamePrint( const char *string ) {
	Com_Printf( "%s", string );
}

// these functions must be used instead of pointer arithmetic, because
// the game allocates gentities with private information after the server shared part
int SV_NumForGentity( sharedEntity_t *ent ) {
	int num;

	num = ( (byte *)ent - (byte *)sv.gentities ) / sv.gentitySize;

	return num;
}

sharedEntity_t *SV_GentityNum( int num ) {
	sharedEntity_t *ent;

	ent = ( sharedEntity_t * )( (byte *)sv.gentities + sv.gentitySize * ( num ) );

	return ent;
}

playerState_t *SV_GameClientNum( int num ) {
	playerState_t   *ps;

	ps = ( playerState_t * )( (byte *)sv.gameClients + sv.gameClientSize * ( num ) );

	return ps;
}

svEntity_t  *SV_SvEntityForGentity( sharedEntity_t *gEnt ) {
	if ( !gEnt || gEnt->s.number < 0 || gEnt->s.number >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "SV_SvEntityForGentity: bad gEnt" );
	}
	return &sv.svEntities[ gEnt->s.number ];
}

sharedEntity_t *SV_GEntityForSvEntity( svEntity_t *svEnt ) {
	int num;

	num = svEnt - sv.svEntities;
	return SV_GentityNum( num );
}

/*
===============
SV_GameSendServerCommand

Sends a command string to a client
===============
*/
void SV_GameSendServerCommand( int clientNum, const char *text ) {
	if ( clientNum == -1 ) {
		SV_SendServerCommand( NULL, "%s", text );
	} else {
		if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
			return;
		}
		SV_SendServerCommand( svs.clients + clientNum, "%s", text );
	}
}


/*
===============
SV_GameDropClient

Disconnects the client with a message
===============
*/
void SV_GameDropClient( int clientNum, const char *reason ) {
	if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
		return;
	}
	SV_DropClient( svs.clients + clientNum, reason );
}


/*
=================
SV_SetBrushModel

sets mins and maxs for inline bmodels
=================
*/
void SV_SetBrushModel( sharedEntity_t *ent, const char *name ) {
	clipHandle_t h;
	vec3_t mins, maxs;

	if ( !name ) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: NULL" );
	}

	if ( name[0] != '*' ) {
		Com_Error( ERR_DROP, "SV_SetBrushModel: %s isn't a brush model", name );
	}


	ent->s.modelindex = atoi( name + 1 );

	h = CM_InlineModel( ent->s.modelindex );
	CM_ModelBounds( h, mins, maxs );
	VectorCopy( mins, ent->r.mins );
	VectorCopy( maxs, ent->r.maxs );
	ent->r.bmodel = qtrue;

	ent->r.contents = -1;       // we don't know exactly what is in the brushes

	SV_LinkEntity( ent );       // FIXME: remove
}



/*
=================
SV_inPVS

Also checks portalareas so that doors block sight
=================
*/
qboolean SV_inPVS( const vec3_t p1, const vec3_t p2 ) {
	int leafnum;
	int cluster;
	int area1, area2;
	byte    *mask;

	leafnum = CM_PointLeafnum( p1 );
	cluster = CM_LeafCluster( leafnum );
	area1 = CM_LeafArea( leafnum );
	mask = CM_ClusterPVS( cluster );

	leafnum = CM_PointLeafnum( p2 );
	cluster = CM_LeafCluster( leafnum );
	area2 = CM_LeafArea( leafnum );
	if ( mask && ( !( mask[cluster >> 3] & ( 1 << ( cluster & 7 ) ) ) ) ) {
		return qfalse;
	}
	if ( !CM_AreasConnected( area1, area2 ) ) {
		return qfalse;      // a door blocks sight
	}
	return qtrue;
}


/*
=================
SV_inPVSIgnorePortals

Does NOT check portalareas
=================
*/
qboolean SV_inPVSIgnorePortals( const vec3_t p1, const vec3_t p2 ) {
	int leafnum;
	int cluster;
	byte    *mask;

	leafnum = CM_PointLeafnum( p1 );
	cluster = CM_LeafCluster( leafnum );
	mask = CM_ClusterPVS( cluster );

	leafnum = CM_PointLeafnum( p2 );
	cluster = CM_LeafCluster( leafnum );

	if ( mask && ( !( mask[cluster >> 3] & ( 1 << ( cluster & 7 ) ) ) ) ) {
		return qfalse;
	}

	return qtrue;
}


/*
========================
SV_AdjustAreaPortalState
========================
*/
void SV_AdjustAreaPortalState( sharedEntity_t *ent, qboolean open ) {
	svEntity_t  *svEnt;

	svEnt = SV_SvEntityForGentity( ent );
	if ( svEnt->areanum2 == -1 ) {
		return;
	}
	CM_AdjustAreaPortalState( svEnt->areanum, svEnt->areanum2, open );
}


/*
==================
SV_GameAreaEntities
==================
*/
qboolean    SV_EntityContact( const vec3_t mins, const vec3_t maxs, const sharedEntity_t *gEnt, const int capsule ) {
	const float *origin, *angles;
	clipHandle_t ch;
	trace_t trace;

	// check for exact collision
	origin = gEnt->r.currentOrigin;
	angles = gEnt->r.currentAngles;

	ch = SV_ClipHandleForEntity( gEnt );
	CM_TransformedBoxTrace( &trace, vec3_origin, vec3_origin, mins, maxs,
							ch, -1, origin, angles, capsule );

	return trace.startsolid;
}


/*
===============
SV_GetServerinfo

===============
*/
void SV_GetServerinfo( char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetServerinfo: bufferSize == %i", bufferSize );
	}
	Q_strncpyz( buffer, Cvar_InfoString( CVAR_SERVERINFO ), bufferSize );
}

/*
===============
SV_LocateGameData

===============
*/
void SV_LocateGameData( sharedEntity_t *gEnts, int numGEntities, int sizeofGEntity_t,
						playerState_t *clients, int sizeofGameClient ) {
	sv.gentities = gEnts;
	sv.gentitySize = sizeofGEntity_t;
	sv.num_entities = numGEntities;

	sv.gameClients = clients;
	sv.gameClientSize = sizeofGameClient;
}


/*
===============
SV_GetUsercmd

===============
*/
void SV_GetUsercmd( int clientNum, usercmd_t *cmd ) {
	if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
		Com_Error( ERR_DROP, "SV_GetUsercmd: bad clientNum:%i", clientNum );
	}
	*cmd = svs.clients[clientNum].lastUsercmd;
}


static void G_Printf( const char * a1 ) {
	Com_Printf( "%s", a1 );
}

static void G_Error( const char * a1 ) {
	Com_Error( ERR_DROP, "%s", a1 );
}

static void G_Endgame(  ) {
	Com_Error( ERR_ENDGAME, "endgame" );  // no message, no error print
}

static int G_Milliseconds(  ) {
	return Sys_Milliseconds();
}

static int G_Argc(  ) {
	return Cmd_Argc();
}

static void G_Argv( int a1, char * a2, int a3 ) {
	Cmd_ArgvBuffer( a1,(char *)( a2), a3 );
}

static int G_FS_FOpenFile( const char * a1, fileHandle_t * a2, fsMode_t a3 ) {
	return FS_FOpenFileByMode((const char *)( a1),(fileHandle_t *)( a2),(fsMode_t)( a3 ));
}

static void G_FS_Read( void * a1, int a2, fileHandle_t a3 ) {
	FS_Read( a1, a2, a3 );
}

static int G_FS_Write( const void * a1, int a2, fileHandle_t a3 ) {
	return FS_Write( a1, a2, a3 );
}

static int G_FS_Rename( const char * a1, const char * a2 ) {
	FS_Rename((const char *)( a1),(const char *)( a2 ));
	return 0;
}

static void G_FS_FCloseFile( fileHandle_t a1 ) {
	FS_FCloseFile( a1 );
}

static void G_FS_CopyFile( char * a1, char * a2 ) {
	FS_CopyFileOS((char *)( a1),(char *)( a2 ));    //DAJ
}

static int G_FS_GetFileList( const char * a1, const char * a2, char * a3, int a4 ) {
	return FS_GetFileList((const char *)( a1),(const char *)( a2),(char *)( a3), a4 );
}

static void G_SendConsoleCommand( int a1, const char * a2 ) {
	Cbuf_ExecuteText( a1,(const char *)( a2 ));
}

static void G_Cvar_Register( vmCvar_t * a1, const char * a2, const char * a3, int a4 ) {
	Cvar_Register((vmCvar_t *)( a1),(const char *)( a2),(const char *)( a3), a4 );
}

static void G_Cvar_Update( vmCvar_t * a1 ) {
	Cvar_Update((vmCvar_t *)( a1 ));
}

static void G_Cvar_Set( const char * a1, const char * a2 ) {
	Cvar_Set( (const char *)a1, (const char *)a2 );
}

static int G_Cvar_VariableIntegerValue( const char * a1 ) {
	return Cvar_VariableIntegerValue( (const char *)a1 );
}

static void G_Cvar_VariableStringBuffer( const char * a1, char * a2, int a3 ) {
	Cvar_VariableStringBuffer((const char *)( a1),(char *)( a2), a3 );
}

static void G_LocateGameData( void * a1, int a2, int a3, playerState_t * a4, int a5 ) {
	SV_LocateGameData((sharedEntity_t *)( a1), a2, a3,(playerState_t *)( a4), a5 );
}

static void G_DropClient( int a1, const char * a2 ) {
	SV_GameDropClient( a1,(const char *)( a2 ));
}

static void G_SendServerCommand( int a1, const char * a2 ) {
	SV_GameSendServerCommand( a1,(const char *)( a2 ));
}

static void G_SetConfigstring( int a1, const char * a2 ) {
	SV_SetConfigstring( a1,(const char *)( a2 ));
}

static void G_GetConfigstring( int a1, char * a2, int a3 ) {
	SV_GetConfigstring( a1,(char *)( a2), a3 );
}

static void G_GetUserinfo( int a1, char * a2, int a3 ) {
	SV_GetUserinfo( a1,(char *)( a2), a3 );
}

static void G_SetUserinfo( int a1, const char * a2 ) {
	SV_SetUserinfo( a1,(const char *)( a2 ));
}

static void G_GetServerinfo( char * a1, int a2 ) {
	SV_GetServerinfo((char *)( a1), a2 );
}

static void G_SetBrushModel( void * a1, const char * a2 ) {
	SV_SetBrushModel((sharedEntity_t *)( a1),(const char *)( a2 ));
}

static void G_Trace( trace_t * a1, const vec3_t a2, const vec3_t a3, const vec3_t a4, const vec3_t a5, int a6, int a7 ) {
	SV_Trace((trace_t *)( a1),(const vec_t *)( a2),(const vec_t *)( a3),(const vec_t *)( a4),(const vec_t *)( a5), a6, a7, /* int capsule */ qfalse );
}

static void G_TraceCapsule( trace_t * a1, const vec3_t a2, const vec3_t a3, const vec3_t a4, const vec3_t a5, int a6, int a7 ) {
	SV_Trace((trace_t *)( a1),(const vec_t *)( a2),(const vec_t *)( a3),(const vec_t *)( a4),(const vec_t *)( a5), a6, a7, /* int capsule */ qtrue );
}

static int G_PointContents( const vec3_t a1, int a2 ) {
	return SV_PointContents((const vec_t *)( a1), a2 );
}

static qboolean G_InPVS( const vec3_t a1, const vec3_t a2 ) {
	return SV_inPVS((const vec_t *)( a1),(const vec_t *)( a2 ));
}

static qboolean G_InPVSIgnorePortals( const vec3_t a1, const vec3_t a2 ) {
	return SV_inPVSIgnorePortals((const vec_t *)( a1),(const vec_t *)( a2 ));
}

static void G_AdjustAreaPortalState( void * a1, qboolean a2 ) {
	SV_AdjustAreaPortalState((sharedEntity_t *)( a1), a2 );
}

static qboolean G_AreasConnected( int a1, int a2 ) {
	return CM_AreasConnected( a1, a2 );
}

static void G_LinkEntity( void * a1 ) {
	SV_LinkEntity((sharedEntity_t *)( a1 ));
}

static void G_UnlinkEntity( void * a1 ) {
	SV_UnlinkEntity((sharedEntity_t *)( a1 ));
}

static int G_EntitiesInBox( const vec3_t a1, const vec3_t a2, int * a3, int a4 ) {
	return SV_AreaEntities((const vec_t *)( a1),(const vec_t *)( a2),(int *)( a3), a4 );
}

static qboolean G_EntityContact( const vec3_t a1, const vec3_t a2, const void * a3 ) {
	return SV_EntityContact((const vec_t *)( a1),(const vec_t *)( a2),(const sharedEntity_t *)( a3), /* int capsule */ qfalse );
}

static qboolean G_EntityContactCapsule( const vec3_t a1, const vec3_t a2, const void * a3 ) {
	return SV_EntityContact((const vec_t *)( a1),(const vec_t *)( a2),(const sharedEntity_t *)( a3), /* int capsule */ qtrue );
}

static int G_BotAllocateClient(  ) {
	return SV_BotAllocateClient();
}

static void G_BotFreeClient( int a1 ) {
	SV_BotFreeClient( a1 );
}

static void G_GetUsercmd( int a1, usercmd_t * a2 ) {
	SV_GetUsercmd( a1,(usercmd_t *)( a2 ));
}

static qboolean G_GetEntityToken( char * a1, int a2 ) {
	{
	const char  *s;

	s = COM_Parse( &sv.entityParsePoint );
	Q_strncpyz((char *)( a1), s, a2 );
	if ( !sv.entityParsePoint && !s[0] ) {
	return qfalse;
	} else {
	return qtrue;
	}
	}
}

static int G_DebugPolygonCreate( int a1, int a2, vec3_t * a3 ) {
	return BotImport_DebugPolygonCreate( a1, a2,(vec3_t *)( a3 ));
}

static void G_DebugPolygonDelete( int a1 ) {
	BotImport_DebugPolygonDelete( a1 );
}

static int G_RealTime( qtime_t * a1 ) {
	return Com_RealTime((qtime_t *)( a1 ));
}

static void G_SnapVector( float * a1 ) {
	Sys_SnapVector((float *)( a1 ));
}

static qboolean G_GetTag( int a1, char * a2, orientation_t * a3 ) {
	return SV_GetTag( a1,(char *)( a2),(orientation_t *)( a3 ));
}

static int G_BotLibSetup(  ) {
	return SV_BotLibSetup();
}

static int G_BotLibShutdown(  ) {
	return SV_BotLibShutdown();
}

static int G_BotLibVarSet( char * a1, char * a2 ) {
	return botlib_export->BotLibVarSet( (char *)(a1), (char *)(a2) );
}

static int G_BotLibVarGet( char * a1, char * a2, int a3 ) {
	return botlib_export->BotLibVarGet( (char *)(a1), (char *)(a2), a3 );
}

static int G_BotLibDefine( char * a1 ) {
	return botlib_export->PC_AddGlobalDefine( (char *)(a1) );
}

static int G_BotLibStartFrame( float a1 ) {
	return botlib_export->BotLibStartFrame( a1 );
}

static int G_BotLibLoadMap( const char * a1 ) {
	return botlib_export->BotLibLoadMap( (const char *)(a1) );
}

static int G_BotLibUpdateEntity( int a1, void * a2 ) {
	return botlib_export->BotLibUpdateEntity( a1, (bot_entitystate_t *)(a2) );
}

static int G_BotLibTest( int a1, char * a2, vec3_t a3, vec3_t a4 ) {
	return botlib_export->Test( a1, (char *)(a2), (vec_t *)(a3), (vec_t *)(a4) );
}

static int G_BotGetSnapshotEntity( int a1, int a2 ) {
	return SV_BotGetSnapshotEntity( a1, a2 );
}

static int G_BotGetServerCommand( int a1, char * a2, int a3 ) {
	return SV_BotGetConsoleMessage( a1,(char *)( a2), a3 );
}

static void G_BotUserCommand( int a1, usercmd_t * a2 ) {
	SV_ClientThink( &svs.clients[a1],(usercmd_t *)( a2 ));
}

static void G_AAS_EntityInfo( int a1, void * a2 ) {
	botlib_export->aas.AAS_EntityInfo( a1, (struct aas_entityinfo_s *)(a2) );
}

static int G_AAS_Initialized(  ) {
	return botlib_export->aas.AAS_Initialized();
}

static void G_AAS_PresenceTypeBoundingBox( int a1, vec3_t a2, vec3_t a3 ) {
	botlib_export->aas.AAS_PresenceTypeBoundingBox( a1, (vec_t *)(a2), (vec_t *)(a3) );
}

static float G_AAS_Time(  ) {
	return  botlib_export->aas.AAS_Time() ;
}

static void G_AAS_SetCurrentWorld( int a1 ) {
	botlib_export->aas.AAS_SetCurrentWorld( a1 );
}

static int G_AAS_PointAreaNum( vec3_t a1 ) {
	return botlib_export->aas.AAS_PointAreaNum( (vec_t *)(a1) );
}

static int G_AAS_TraceAreas( vec3_t a1, vec3_t a2, int * a3, vec3_t * a4, int a5 ) {
	return botlib_export->aas.AAS_TraceAreas( (vec_t *)(a1), (vec_t *)(a2), (int *)(a3), (vec3_t *)(a4), a5 );
}

static int G_AAS_PointContents( vec3_t a1 ) {
	return botlib_export->aas.AAS_PointContents( (vec_t *)(a1) );
}

static int G_AAS_NextBSPEntity( int a1 ) {
	return botlib_export->aas.AAS_NextBSPEntity( a1 );
}

static int G_AAS_ValueForBSPEpairKey( int a1, char * a2, char * a3, int a4 ) {
	return botlib_export->aas.AAS_ValueForBSPEpairKey( a1, (char *)(a2), (char *)(a3), a4 );
}

static int G_AAS_VectorForBSPEpairKey( int a1, char * a2, vec3_t a3 ) {
	return botlib_export->aas.AAS_VectorForBSPEpairKey( a1, (char *)(a2), (vec_t *)(a3) );
}

static int G_AAS_FloatForBSPEpairKey( int a1, char * a2, float * a3 ) {
	return botlib_export->aas.AAS_FloatForBSPEpairKey( a1, (char *)(a2), (float *)(a3) );
}

static int G_AAS_IntForBSPEpairKey( int a1, char * a2, int * a3 ) {
	return botlib_export->aas.AAS_IntForBSPEpairKey( a1, (char *)(a2), (int *)(a3) );
}

static int G_AAS_AreaReachability( int a1 ) {
	return botlib_export->aas.AAS_AreaReachability( a1 );
}

static int G_AAS_AreaTravelTimeToGoalArea( int a1, vec3_t a2, int a3, int a4 ) {
	return botlib_export->aas.AAS_AreaTravelTimeToGoalArea( a1, (vec_t *)(a2), a3, a4 );
}

static int G_AAS_Swimming( vec3_t a1 ) {
	return botlib_export->aas.AAS_Swimming( (vec_t *)(a1) );
}

static int G_AAS_PredictClientMovement( void * a1, int a2, vec3_t a3, int a4, int a5, vec3_t a6, vec3_t a7, int a8, int a9, float a10, int a11, int a12, int a13 ) {
	return botlib_export->aas.AAS_PredictClientMovement( (struct aas_clientmove_s *)(a1), a2, (vec_t *)(a3), a4, a5,
	(vec_t *)(a6), (vec_t *)(a7), a8, a9, a10, a11, a12, a13 );
}

static void G_AAS_RT_ShowRoute( vec3_t a1, int a2, int a3 ) {
	botlib_export->aas.AAS_RT_ShowRoute( (vec_t *)(a1), a2, a3 );
}

static qboolean G_AAS_RT_GetHidePos( vec3_t a1, int a2, int a3, vec3_t a4, int a5, int a6, vec3_t a7 ) {
	return botlib_export->aas.AAS_RT_GetHidePos( (vec_t *)(a1), a2, a3, (vec_t *)(a4), a5, a6, (vec_t *)(a7) );
}

static int G_AAS_FindAttackSpotWithinRange( int a1, int a2, int a3, float a4, int a5, float * a6 ) {
	return botlib_export->aas.AAS_FindAttackSpotWithinRange( a1, a2, a3, a4, a5, (float *)(a6) );
}

static qboolean G_AAS_GetRouteFirstVisPos( vec3_t a1, vec3_t a2, int a3, vec3_t a4 ) {
	return botlib_export->aas.AAS_GetRouteFirstVisPos((vec_t *)(a1), (vec_t *)(a2),
	a3, (vec_t *)(a4));
}

static void G_AAS_SetAASBlockingEntity( vec3_t a1, vec3_t a2, qboolean a3 ) {
	botlib_export->aas.AAS_SetAASBlockingEntity( (vec_t *)(a1), (vec_t *)(a2), a3 );
}

static void G_EA_Say( int a1, char * a2 ) {
	botlib_export->ea.EA_Say( a1, (char *)(a2) );
}

static void G_EA_SayTeam( int a1, char * a2 ) {
	botlib_export->ea.EA_SayTeam( a1, (char *)(a2) );
}

static void G_EA_UseItem( int a1, char * a2 ) {
	botlib_export->ea.EA_UseItem( a1, (char *)(a2) );
}

static void G_EA_DropItem( int a1, char * a2 ) {
	botlib_export->ea.EA_DropItem( a1, (char *)(a2) );
}

static void G_EA_UseInv( int a1, char * a2 ) {
	botlib_export->ea.EA_UseInv( a1, (char *)(a2) );
}

static void G_EA_DropInv( int a1, char * a2 ) {
	botlib_export->ea.EA_DropInv( a1, (char *)(a2) );
}

static void G_EA_Gesture( int a1 ) {
	botlib_export->ea.EA_Gesture( a1 );
}

static void G_EA_Command( int a1, char * a2 ) {
	botlib_export->ea.EA_Command( a1, (char *)(a2) );
}

static void G_EA_SelectWeapon( int a1, int a2 ) {
	botlib_export->ea.EA_SelectWeapon( a1, a2 );
}

static void G_EA_Talk( int a1 ) {
	botlib_export->ea.EA_Talk( a1 );
}

static void G_EA_Attack( int a1 ) {
	botlib_export->ea.EA_Attack( a1 );
}

static void G_EA_Reload( int a1 ) {
	botlib_export->ea.EA_Reload( a1 );
}

static void G_EA_Use( int a1 ) {
	botlib_export->ea.EA_Use( a1 );
}

static void G_EA_Respawn( int a1 ) {
	botlib_export->ea.EA_Respawn( a1 );
}

static void G_EA_Jump( int a1 ) {
	botlib_export->ea.EA_Jump( a1 );
}

static void G_EA_DelayedJump( int a1 ) {
	botlib_export->ea.EA_DelayedJump( a1 );
}

static void G_EA_Crouch( int a1 ) {
	botlib_export->ea.EA_Crouch( a1 );
}

static void G_EA_MoveUp( int a1 ) {
	botlib_export->ea.EA_MoveUp( a1 );
}

static void G_EA_MoveDown( int a1 ) {
	botlib_export->ea.EA_MoveDown( a1 );
}

static void G_EA_MoveForward( int a1 ) {
	botlib_export->ea.EA_MoveForward( a1 );
}

static void G_EA_MoveBack( int a1 ) {
	botlib_export->ea.EA_MoveBack( a1 );
}

static void G_EA_MoveLeft( int a1 ) {
	botlib_export->ea.EA_MoveLeft( a1 );
}

static void G_EA_MoveRight( int a1 ) {
	botlib_export->ea.EA_MoveRight( a1 );
}

static void G_EA_Move( int a1, vec3_t a2, float a3 ) {
	botlib_export->ea.EA_Move( a1, (vec_t *)(a2), a3 );
}

static void G_EA_View( int a1, vec3_t a2 ) {
	botlib_export->ea.EA_View( a1, (vec_t *)(a2) );
}

static void G_EA_EndRegular( int a1, float a2 ) {
	botlib_export->ea.EA_EndRegular( a1, a2 );
}

static void G_EA_GetInput( int a1, float a2, void * a3 ) {
	botlib_export->ea.EA_GetInput( a1, a2, (bot_input_t *)(a3) );
}

static void G_EA_ResetInput( int a1, void * a2 ) {
	botlib_export->ea.EA_ResetInput( a1, (bot_input_t *)(a2) );
}

static int G_BotLoadCharacter( char * a1, int a2 ) {
	return botlib_export->ai.BotLoadCharacter( (char *)(a1), a2 );
}

static void G_BotFreeCharacter( int a1 ) {
	botlib_export->ai.BotFreeCharacter( a1 );
}

static float G_Characteristic_Float( int a1, int a2 ) {
	return  botlib_export->ai.Characteristic_Float( a1, a2 ) ;
}

static float G_Characteristic_BFloat( int a1, int a2, float a3, float a4 ) {
	return  botlib_export->ai.Characteristic_BFloat( a1, a2, a3, a4 ) ;
}

static int G_Characteristic_Integer( int a1, int a2 ) {
	return botlib_export->ai.Characteristic_Integer( a1, a2 );
}

static int G_Characteristic_BInteger( int a1, int a2, int a3, int a4 ) {
	return botlib_export->ai.Characteristic_BInteger( a1, a2, a3, a4 );
}

static void G_Characteristic_String( int a1, int a2, char * a3, int a4 ) {
	botlib_export->ai.Characteristic_String( a1, a2, (char *)(a3), a4 );
}

static int G_BotAllocChatState(  ) {
	return botlib_export->ai.BotAllocChatState();
}

static void G_BotFreeChatState( int a1 ) {
	botlib_export->ai.BotFreeChatState( a1 );
}

static void G_BotQueueConsoleMessage( int a1, int a2, char * a3 ) {
	botlib_export->ai.BotQueueConsoleMessage( a1, a2, (char *)(a3) );
}

static void G_BotRemoveConsoleMessage( int a1, int a2 ) {
	botlib_export->ai.BotRemoveConsoleMessage( a1, a2 );
}

static int G_BotNextConsoleMessage( int a1, void * a2 ) {
	return botlib_export->ai.BotNextConsoleMessage( a1, (struct bot_consolemessage_s *)(a2) );
}

static int G_BotNumConsoleMessages( int a1 ) {
	return botlib_export->ai.BotNumConsoleMessages( a1 );
}

static void G_BotInitialChat( int a1, char * a2, int a3, char * a4, char * a5, char * a6, char * a7, char * a8, char * a9, char * a10, char * a11 ) {
	botlib_export->ai.BotInitialChat( a1, (char *)(a2), a3, (char *)(a4), (char *)(a5), (char *)(a6), (char *)(a7), (char *)(a8), (char *)(a9), (char *)(a10), (char *)(a11) );
}

static int G_BotNumInitialChats( int a1, char * a2 ) {
	return botlib_export->ai.BotNumInitialChats( a1, (char *)(a2) );
}

static int G_BotReplyChat( int a1, char * a2, int a3, int a4, char * a5, char * a6, char * a7, char * a8, char * a9, char * a10, char * a11, char * a12 ) {
	return botlib_export->ai.BotReplyChat( a1, (char *)(a2), a3, a4, (char *)(a5), (char *)(a6), (char *)(a7), (char *)(a8), (char *)(a9), (char *)(a10), (char *)(a11), (char *)(a12) );
}

static int G_BotChatLength( int a1 ) {
	return botlib_export->ai.BotChatLength( a1 );
}

static void G_BotEnterChat( int a1, int a2, int a3 ) {
	botlib_export->ai.BotEnterChat( a1, a2, a3 );
}

static void G_BotGetChatMessage( int a1, char * a2, int a3 ) {
	botlib_export->ai.BotGetChatMessage( a1, (char *)(a2), a3 );
}

static int G_StringContains( char * a1, char * a2, int a3 ) {
	return botlib_export->ai.StringContains( (char *)(a1), (char *)(a2), a3 );
}

static int G_BotFindMatch( char * a1, void * a2, unsigned long int a3 ) {
	return botlib_export->ai.BotFindMatch( (char *)(a1), (struct bot_match_s *)(a2), a3 );
}

static void G_BotMatchVariable( void * a1, int a2, char * a3, int a4 ) {
	botlib_export->ai.BotMatchVariable( (struct bot_match_s *)(a1), a2, (char *)(a3), a4 );
}

static void G_UnifyWhiteSpaces( char * a1 ) {
	botlib_export->ai.UnifyWhiteSpaces( (char *)(a1) );
}

static void G_BotReplaceSynonyms( char * a1, unsigned long int a2 ) {
	botlib_export->ai.BotReplaceSynonyms( (char *)(a1), a2 );
}

static int G_BotLoadChatFile( int a1, char * a2, char * a3 ) {
	return botlib_export->ai.BotLoadChatFile( a1, (char *)(a2), (char *)(a3) );
}

static void G_BotSetChatGender( int a1, int a2 ) {
	botlib_export->ai.BotSetChatGender( a1, a2 );
}

static void G_BotSetChatName( int a1, char * a2 ) {
	botlib_export->ai.BotSetChatName( a1, (char *)(a2) );
}

static void G_BotResetGoalState( int a1 ) {
	botlib_export->ai.BotResetGoalState( a1 );
}

static void G_BotResetAvoidGoals( int a1 ) {
	botlib_export->ai.BotResetAvoidGoals( a1 );
}

static void G_BotRemoveFromAvoidGoals( int a1, int a2 ) {
	botlib_export->ai.BotRemoveFromAvoidGoals( a1, a2 );
}

static void G_BotPushGoal( int a1, void * a2 ) {
	botlib_export->ai.BotPushGoal( a1, (struct bot_goal_s *)(a2) );
}

static void G_BotPopGoal( int a1 ) {
	botlib_export->ai.BotPopGoal( a1 );
}

static void G_BotEmptyGoalStack( int a1 ) {
	botlib_export->ai.BotEmptyGoalStack( a1 );
}

static void G_BotDumpAvoidGoals( int a1 ) {
	botlib_export->ai.BotDumpAvoidGoals( a1 );
}

static void G_BotDumpGoalStack( int a1 ) {
	botlib_export->ai.BotDumpGoalStack( a1 );
}

static void G_BotGoalName( int a1, char * a2, int a3 ) {
	botlib_export->ai.BotGoalName( a1, (char *)(a2), a3 );
}

static int G_BotGetTopGoal( int a1, void * a2 ) {
	return botlib_export->ai.BotGetTopGoal( a1, (struct bot_goal_s *)(a2) );
}

static int G_BotGetSecondGoal( int a1, void * a2 ) {
	return botlib_export->ai.BotGetSecondGoal( a1, (struct bot_goal_s *)(a2) );
}

static int G_BotChooseLTGItem( int a1, vec3_t a2, int * a3, int a4 ) {
	return botlib_export->ai.BotChooseLTGItem( a1, (vec_t *)(a2), (int *)(a3), a4 );
}

static int G_BotChooseNBGItem( int a1, vec3_t a2, int * a3, int a4, void * a5, float a6 ) {
	return botlib_export->ai.BotChooseNBGItem( a1, (vec_t *)(a2), (int *)(a3), a4, (struct bot_goal_s *)(a5), a6 );
}

static int G_BotTouchingGoal( vec3_t a1, void * a2 ) {
	return botlib_export->ai.BotTouchingGoal( (vec_t *)(a1), (struct bot_goal_s *)(a2) );
}

static int G_BotItemGoalInVisButNotVisible( int a1, vec3_t a2, vec3_t a3, void * a4 ) {
	return botlib_export->ai.BotItemGoalInVisButNotVisible( a1, (vec_t *)(a2), (vec_t *)(a3), (struct bot_goal_s *)(a4) );
}

static int G_BotGetLevelItemGoal( int a1, char * a2, void * a3 ) {
	return botlib_export->ai.BotGetLevelItemGoal( a1, (char *)(a2), (struct bot_goal_s *)(a3) );
}

static int G_BotGetNextCampSpotGoal( int a1, void * a2 ) {
	return botlib_export->ai.BotGetNextCampSpotGoal( a1, (struct bot_goal_s *)(a2) );
}

static int G_BotGetMapLocationGoal( char * a1, void * a2 ) {
	return botlib_export->ai.BotGetMapLocationGoal( (char *)(a1), (struct bot_goal_s *)(a2) );
}

static float G_BotAvoidGoalTime( int a1, int a2 ) {
	return  botlib_export->ai.BotAvoidGoalTime( a1, a2 ) ;
}

static void G_BotInitLevelItems(  ) {
	botlib_export->ai.BotInitLevelItems();
}

static void G_BotUpdateEntityItems(  ) {
	botlib_export->ai.BotUpdateEntityItems();
}

static int G_BotLoadItemWeights( int a1, char * a2 ) {
	return botlib_export->ai.BotLoadItemWeights( a1, (char *)(a2) );
}

static void G_BotFreeItemWeights( int a1 ) {
	botlib_export->ai.BotFreeItemWeights( a1 );
}

static void G_BotInterbreedGoalFuzzyLogic( int a1, int a2, int a3 ) {
	botlib_export->ai.BotInterbreedGoalFuzzyLogic( a1, a2, a3 );
}

static void G_BotSaveGoalFuzzyLogic( int a1, char * a2 ) {
	botlib_export->ai.BotSaveGoalFuzzyLogic( a1, (char *)(a2) );
}

static void G_BotMutateGoalFuzzyLogic( int a1, float a2 ) {
	botlib_export->ai.BotMutateGoalFuzzyLogic( a1, a2 );
}

static int G_BotAllocGoalState( int a1 ) {
	return botlib_export->ai.BotAllocGoalState( a1 );
}

static void G_BotFreeGoalState( int a1 ) {
	botlib_export->ai.BotFreeGoalState( a1 );
}

static void G_BotResetMoveState( int a1 ) {
	botlib_export->ai.BotResetMoveState( a1 );
}

static void G_BotMoveToGoal( void * a1, int a2, void * a3, int a4 ) {
	botlib_export->ai.BotMoveToGoal( (struct bot_moveresult_s *)(a1), a2, (struct bot_goal_s *)(a3), a4 );
}

static int G_BotMoveInDirection( int a1, vec3_t a2, float a3, int a4 ) {
	return botlib_export->ai.BotMoveInDirection( a1, (vec_t *)(a2), a3, a4 );
}

static void G_BotResetAvoidReach( int a1 ) {
	botlib_export->ai.BotResetAvoidReach( a1 );
}

static void G_BotResetLastAvoidReach( int a1 ) {
	botlib_export->ai.BotResetLastAvoidReach( a1 );
}

static int G_BotReachabilityArea( vec3_t a1, int a2 ) {
	return botlib_export->ai.BotReachabilityArea( (vec_t *)(a1), a2 );
}

static int G_BotMovementViewTarget( int a1, void * a2, int a3, float a4, vec3_t a5 ) {
	return botlib_export->ai.BotMovementViewTarget( a1, (struct bot_goal_s *)(a2), a3, a4, (vec_t *)(a5) );
}

static int G_BotPredictVisiblePosition( vec3_t a1, int a2, void * a3, int a4, vec3_t a5 ) {
	return botlib_export->ai.BotPredictVisiblePosition( (vec_t *)(a1), a2, (struct bot_goal_s *)(a3), a4, (vec_t *)(a5) );
}

static int G_BotAllocMoveState(  ) {
	return botlib_export->ai.BotAllocMoveState();
}

static void G_BotFreeMoveState( int a1 ) {
	botlib_export->ai.BotFreeMoveState( a1 );
}

static void G_BotInitMoveState( int a1, void * a2 ) {
	botlib_export->ai.BotInitMoveState( a1, (struct bot_initmove_s *)(a2) );
}

static void G_BotInitAvoidReach( int a1 ) {
	botlib_export->ai.BotInitAvoidReach( a1 );
}

static int G_BotChooseBestFightWeapon( int a1, int * a2 ) {
	return botlib_export->ai.BotChooseBestFightWeapon( a1, (int *)(a2) );
}

static void G_BotGetWeaponInfo( int a1, int a2, void * a3 ) {
	botlib_export->ai.BotGetWeaponInfo( a1, a2, (struct weaponinfo_s *)(a3) );
}

static int G_BotLoadWeaponWeights( int a1, char * a2 ) {
	return botlib_export->ai.BotLoadWeaponWeights( a1, (char *)(a2) );
}

static int G_BotAllocWeaponState(  ) {
	return botlib_export->ai.BotAllocWeaponState();
}

static void G_BotFreeWeaponState( int a1 ) {
	botlib_export->ai.BotFreeWeaponState( a1 );
}

static void G_BotResetWeaponState( int a1 ) {
	botlib_export->ai.BotResetWeaponState( a1 );
}

static int G_GeneticParentsAndChildSelection( int a1, float * a2, int * a3, int * a4, int * a5 ) {
	return botlib_export->ai.GeneticParentsAndChildSelection( a1, (float *)(a2), (int *)(a3), (int *)(a4), (int *)(a5) );
}

static qboolean G_AAS_Route( vec3_t a1, vec3_t a2, int a3, vec3_t a4 ) {
	return botlib_export->aas.AAS_Route(static_cast<float *>(a1),
	static_cast<float *>(a2), a3,
	static_cast<float *>(a4));
}

static qboolean G_AAS_DoomPlayerRoute( vec3_t a1, vec3_t a2, int a3, void * a4 ) {
	return botlib_export->aas.AAS_DoomPlayerRoute(static_cast<float *>(a1),
	static_cast<float *>(a2), a3, static_cast<doomAasRoute_t *>(a4));
}

static const gameImport_t gameImportTable = {
	GAME_API_VERSION,
	G_Printf,
	G_Error,
	G_Endgame,
	G_Milliseconds,
	G_Argc,
	G_Argv,
	G_FS_FOpenFile,
	G_FS_Read,
	G_FS_Write,
	G_FS_Rename,
	G_FS_FCloseFile,
	G_FS_CopyFile,
	G_FS_GetFileList,
	G_SendConsoleCommand,
	G_Cvar_Register,
	G_Cvar_Update,
	G_Cvar_Set,
	G_Cvar_VariableIntegerValue,
	G_Cvar_VariableStringBuffer,
	G_LocateGameData,
	G_DropClient,
	G_SendServerCommand,
	G_SetConfigstring,
	G_GetConfigstring,
	G_GetUserinfo,
	G_SetUserinfo,
	G_GetServerinfo,
	G_SetBrushModel,
	G_Trace,
	G_TraceCapsule,
	G_PointContents,
	G_InPVS,
	G_InPVSIgnorePortals,
	G_AdjustAreaPortalState,
	G_AreasConnected,
	G_LinkEntity,
	G_UnlinkEntity,
	G_EntitiesInBox,
	G_EntityContact,
	G_EntityContactCapsule,
	G_BotAllocateClient,
	G_BotFreeClient,
	G_GetUsercmd,
	G_GetEntityToken,
	G_DebugPolygonCreate,
	G_DebugPolygonDelete,
	G_RealTime,
	G_SnapVector,
	G_GetTag,
	G_BotLibSetup,
	G_BotLibShutdown,
	G_BotLibVarSet,
	G_BotLibVarGet,
	G_BotLibDefine,
	G_BotLibStartFrame,
	G_BotLibLoadMap,
	G_BotLibUpdateEntity,
	G_BotLibTest,
	G_BotGetSnapshotEntity,
	G_BotGetServerCommand,
	G_BotUserCommand,
	G_AAS_EntityInfo,
	G_AAS_Initialized,
	G_AAS_PresenceTypeBoundingBox,
	G_AAS_Time,
	G_AAS_SetCurrentWorld,
	G_AAS_PointAreaNum,
	G_AAS_TraceAreas,
	G_AAS_PointContents,
	G_AAS_NextBSPEntity,
	G_AAS_ValueForBSPEpairKey,
	G_AAS_VectorForBSPEpairKey,
	G_AAS_FloatForBSPEpairKey,
	G_AAS_IntForBSPEpairKey,
	G_AAS_AreaReachability,
	G_AAS_AreaTravelTimeToGoalArea,
	G_AAS_Swimming,
	G_AAS_PredictClientMovement,
	G_AAS_RT_ShowRoute,
	G_AAS_RT_GetHidePos,
	G_AAS_FindAttackSpotWithinRange,
	G_AAS_GetRouteFirstVisPos,
	G_AAS_SetAASBlockingEntity,
	G_EA_Say,
	G_EA_SayTeam,
	G_EA_UseItem,
	G_EA_DropItem,
	G_EA_UseInv,
	G_EA_DropInv,
	G_EA_Gesture,
	G_EA_Command,
	G_EA_SelectWeapon,
	G_EA_Talk,
	G_EA_Attack,
	G_EA_Reload,
	G_EA_Use,
	G_EA_Respawn,
	G_EA_Jump,
	G_EA_DelayedJump,
	G_EA_Crouch,
	G_EA_MoveUp,
	G_EA_MoveDown,
	G_EA_MoveForward,
	G_EA_MoveBack,
	G_EA_MoveLeft,
	G_EA_MoveRight,
	G_EA_Move,
	G_EA_View,
	G_EA_EndRegular,
	G_EA_GetInput,
	G_EA_ResetInput,
	G_BotLoadCharacter,
	G_BotFreeCharacter,
	G_Characteristic_Float,
	G_Characteristic_BFloat,
	G_Characteristic_Integer,
	G_Characteristic_BInteger,
	G_Characteristic_String,
	G_BotAllocChatState,
	G_BotFreeChatState,
	G_BotQueueConsoleMessage,
	G_BotRemoveConsoleMessage,
	G_BotNextConsoleMessage,
	G_BotNumConsoleMessages,
	G_BotInitialChat,
	G_BotNumInitialChats,
	G_BotReplyChat,
	G_BotChatLength,
	G_BotEnterChat,
	G_BotGetChatMessage,
	G_StringContains,
	G_BotFindMatch,
	G_BotMatchVariable,
	G_UnifyWhiteSpaces,
	G_BotReplaceSynonyms,
	G_BotLoadChatFile,
	G_BotSetChatGender,
	G_BotSetChatName,
	G_BotResetGoalState,
	G_BotResetAvoidGoals,
	G_BotRemoveFromAvoidGoals,
	G_BotPushGoal,
	G_BotPopGoal,
	G_BotEmptyGoalStack,
	G_BotDumpAvoidGoals,
	G_BotDumpGoalStack,
	G_BotGoalName,
	G_BotGetTopGoal,
	G_BotGetSecondGoal,
	G_BotChooseLTGItem,
	G_BotChooseNBGItem,
	G_BotTouchingGoal,
	G_BotItemGoalInVisButNotVisible,
	G_BotGetLevelItemGoal,
	G_BotGetNextCampSpotGoal,
	G_BotGetMapLocationGoal,
	G_BotAvoidGoalTime,
	G_BotInitLevelItems,
	G_BotUpdateEntityItems,
	G_BotLoadItemWeights,
	G_BotFreeItemWeights,
	G_BotInterbreedGoalFuzzyLogic,
	G_BotSaveGoalFuzzyLogic,
	G_BotMutateGoalFuzzyLogic,
	G_BotAllocGoalState,
	G_BotFreeGoalState,
	G_BotResetMoveState,
	G_BotMoveToGoal,
	G_BotMoveInDirection,
	G_BotResetAvoidReach,
	G_BotResetLastAvoidReach,
	G_BotReachabilityArea,
	G_BotMovementViewTarget,
	G_BotPredictVisiblePosition,
	G_BotAllocMoveState,
	G_BotFreeMoveState,
	G_BotInitMoveState,
	G_BotInitAvoidReach,
	G_BotChooseBestFightWeapon,
	G_BotGetWeaponInfo,
	G_BotLoadWeaponWeights,
	G_BotAllocWeaponState,
	G_BotFreeWeaponState,
	G_BotResetWeaponState,
	G_GeneticParentsAndChildSelection,
	G_AAS_Route,
	G_AAS_DoomPlayerRoute,
};
//==============================================

// The loaded game DLL and the exports it provides.
static void *gameDllHandle = NULL;
gameExport_t *gameExport = NULL;

/*
===============
SV_LoadGame

Load the game DLL and get its exports
===============
*/
static void SV_LoadGame( void ) {
	gameExport = (gameExport_t *)Sys_LoadDll( "qagame", &gameDllHandle, "GetGameAPI", (void *)&gameImportTable );
	if ( !gameExport ) {
		Com_Error( ERR_FATAL, "Sys_LoadDll on game failed" );
	}
}

/*
===============
SV_ShutdownGameProgs

Called every time a map changes
===============
*/
void SV_ShutdownGameProgs( void ) {
	if ( !gameExport ) {
		return;
	}
	gameExport->Shutdown( qfalse );
	Sys_UnloadDll( gameDllHandle );
	gameDllHandle = NULL;
	gameExport = NULL;
}

/*
==================
SV_InitGameVM

Called for both a full init and a restart
==================
*/
static void SV_InitGameVM(qboolean restart) {
    int i;

    // Native DLL reloads can relocate game entities. GAME_INIT publishes
    // configstrings, so invalidate client pointers before it can broadcast.
    for (i = 0; i < sv_maxclients->integer; ++i) {
        svs.clients[i].gentity = NULL;
    }

    // start the entity parsing at the beginning
    sv.entityParsePoint = CM_EntityString();

    // use the current msec count for a random seed
    // init for this gamestate
    gameExport->Init(svs.time, Com_Milliseconds(), restart);
}

/*
===================
SV_RestartGameProgs

Called on a map_restart, but not on a normal map change
===================
*/
void SV_RestartGameProgs( void ) {
	if ( !gameExport ) {
		return;
	}
	gameExport->Shutdown( qtrue );

	// do a restart instead of a free: unload the DLL and load it from disk again
	Sys_UnloadDll( gameDllHandle );
	gameDllHandle = NULL;
	gameExport = NULL;
	SV_LoadGame();

	SV_InitGameVM( qtrue );
}


/*
===============
SV_InitGameProgs

Called on a normal map change, not on a map_restart
===============
*/
void SV_InitGameProgs( void ) {
	cvar_t  *var;
	//FIXME these are temp while I make bots run in vm
	extern int bot_enable;

	var = Cvar_Get( "bot_enable", "1", CVAR_LATCH );
	if ( var ) {
		bot_enable = var->integer;
	} else {
		bot_enable = 0;
	}

	SV_LoadGame();

	SV_InitGameVM( qfalse );
}


/*
====================
SV_GameCommand

See if the current console command is claimed by the game
====================
*/
qboolean SV_GameCommand( void ) {
	if ( sv.state != SS_GAME ) {
		return qfalse;
	}

	return gameExport->ConsoleCommand();
}


/*
====================
SV_SendMoveSpeedsToGame
====================
*/
void SV_SendMoveSpeedsToGame( int entnum, char *text ) {
	if ( !gameExport ) {
		return;
	}
	gameExport->RetrieveMoveSpeedsFromClient( entnum, text );
}

/*
====================
SV_GetTag

  return qfalse if unable to retrieve tag information for this client
====================
*/
extern qboolean CL_GetTag( int clientNum, char *tagname, orientation_t *or );

qboolean SV_GetTag( int clientNum, char *tagname, orientation_t *or ) {
	if ( com_dedicated->integer ) {
		return qfalse;
	}

	return CL_GetTag( clientNum, tagname, or );
}

/*
===================
SV_GetModelInfo

  request this modelinfo from the game
===================
*/
qboolean SV_GetModelInfo( int clientNum, char *modelName, animModelInfo_t **modelInfo ) {
	return gameExport->GetModelInfo( clientNum, modelName, (void **)modelInfo );
}

