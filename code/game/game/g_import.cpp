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

===========================================================================
*/

// Replaces the old syscall table. Each trap_ function now forwards to a
// function pointer in the import table that the engine fills in.

#include "g_save_callbacks.h"
#include "g_local.h"
#include "../qcommon/doom_aas_route.h"

gameImport_t *gameImports;

void trap_Printf( const char * fmt ) {
	gameImports->Printf( fmt );
}
DOOM_SAVE_CALLBACK("trap_Printf", trap_Printf);

void trap_Error( const char * fmt ) {
	gameImports->Error( fmt );
}
DOOM_SAVE_CALLBACK("trap_Error", trap_Error);

void trap_Endgame(  ) {
	gameImports->Endgame();
}
DOOM_SAVE_CALLBACK("trap_Endgame", trap_Endgame);

int trap_Milliseconds(  ) {
	return gameImports->Milliseconds();
}
DOOM_SAVE_CALLBACK("trap_Milliseconds", trap_Milliseconds);

int trap_Argc(  ) {
	return gameImports->Argc();
}
DOOM_SAVE_CALLBACK("trap_Argc", trap_Argc);

void trap_Argv( int n, char * buffer, int bufferLength ) {
	gameImports->Argv( n, buffer, bufferLength );
}
DOOM_SAVE_CALLBACK("trap_Argv", trap_Argv);

int trap_FS_FOpenFile( const char * qpath, fileHandle_t * f, fsMode_t mode ) {
	return gameImports->FS_FOpenFile( qpath, f, mode );
}
DOOM_SAVE_CALLBACK("trap_FS_FOpenFile", trap_FS_FOpenFile);

void trap_FS_Read( void * buffer, int len, fileHandle_t f ) {
	gameImports->FS_Read( buffer, len, f );
}
DOOM_SAVE_CALLBACK("trap_FS_Read", trap_FS_Read);

int trap_FS_Write( const void * buffer, int len, fileHandle_t f ) {
	return gameImports->FS_Write( buffer, len, f );
}
DOOM_SAVE_CALLBACK("trap_FS_Write", trap_FS_Write);

int trap_FS_Rename( const char * from, const char * to ) {
	return gameImports->FS_Rename( from, to );
}
DOOM_SAVE_CALLBACK("trap_FS_Rename", trap_FS_Rename);

void trap_FS_FCloseFile( fileHandle_t f ) {
	gameImports->FS_FCloseFile( f );
}
DOOM_SAVE_CALLBACK("trap_FS_FCloseFile", trap_FS_FCloseFile);

void trap_FS_CopyFile( char * from, char * to ) {
	gameImports->FS_CopyFile( from, to );
}
DOOM_SAVE_CALLBACK("trap_FS_CopyFile", trap_FS_CopyFile);

int trap_FS_GetFileList( const char * path, const char * extension, char * listbuf, int bufsize ) {
	return gameImports->FS_GetFileList( path, extension, listbuf, bufsize );
}
DOOM_SAVE_CALLBACK("trap_FS_GetFileList", trap_FS_GetFileList);

void trap_SendConsoleCommand( int exec_when, const char * text ) {
	gameImports->SendConsoleCommand( exec_when, text );
}
DOOM_SAVE_CALLBACK("trap_SendConsoleCommand", trap_SendConsoleCommand);

void trap_Cvar_Register( vmCvar_t * cvar, const char * var_name, const char * value, int flags ) {
	gameImports->Cvar_Register( cvar, var_name, value, flags );
}
DOOM_SAVE_CALLBACK("trap_Cvar_Register", trap_Cvar_Register);

void trap_Cvar_Update( vmCvar_t * cvar ) {
	gameImports->Cvar_Update( cvar );
}
DOOM_SAVE_CALLBACK("trap_Cvar_Update", trap_Cvar_Update);

void trap_Cvar_Set( const char * var_name, const char * value ) {
	gameImports->Cvar_Set( var_name, value );
}
DOOM_SAVE_CALLBACK("trap_Cvar_Set", trap_Cvar_Set);

int trap_Cvar_VariableIntegerValue( const char * var_name ) {
	return gameImports->Cvar_VariableIntegerValue( var_name );
}
DOOM_SAVE_CALLBACK("trap_Cvar_VariableIntegerValue", trap_Cvar_VariableIntegerValue);

void trap_Cvar_VariableStringBuffer( const char * var_name, char * buffer, int bufsize ) {
	gameImports->Cvar_VariableStringBuffer( var_name, buffer, bufsize );
}
DOOM_SAVE_CALLBACK("trap_Cvar_VariableStringBuffer", trap_Cvar_VariableStringBuffer);

void trap_LocateGameData( gentity_t * gEnts, int numGEntities, int sizeofGEntity_t, playerState_t * clients, int sizeofGClient ) {
	gameImports->LocateGameData( gEnts, numGEntities, sizeofGEntity_t, clients, sizeofGClient );
}
DOOM_SAVE_CALLBACK("trap_LocateGameData", trap_LocateGameData);

void trap_DropClient( int clientNum, const char * reason ) {
	gameImports->DropClient( clientNum, reason );
}
DOOM_SAVE_CALLBACK("trap_DropClient", trap_DropClient);

void trap_SendServerCommand( int clientNum, const char * text ) {
	gameImports->SendServerCommand( clientNum, text );
}
DOOM_SAVE_CALLBACK("trap_SendServerCommand", trap_SendServerCommand);

void trap_SetConfigstring( int num, const char * string ) {
	gameImports->SetConfigstring( num, string );
}
DOOM_SAVE_CALLBACK("trap_SetConfigstring", trap_SetConfigstring);

void trap_GetConfigstring( int num, char * buffer, int bufferSize ) {
	gameImports->GetConfigstring( num, buffer, bufferSize );
}
DOOM_SAVE_CALLBACK("trap_GetConfigstring", trap_GetConfigstring);

void trap_GetUserinfo( int num, char * buffer, int bufferSize ) {
	gameImports->GetUserinfo( num, buffer, bufferSize );
}
DOOM_SAVE_CALLBACK("trap_GetUserinfo", trap_GetUserinfo);

void trap_SetUserinfo( int num, const char * buffer ) {
	gameImports->SetUserinfo( num, buffer );
}
DOOM_SAVE_CALLBACK("trap_SetUserinfo", trap_SetUserinfo);

void trap_GetServerinfo( char * buffer, int bufferSize ) {
	gameImports->GetServerinfo( buffer, bufferSize );
}
DOOM_SAVE_CALLBACK("trap_GetServerinfo", trap_GetServerinfo);

void trap_SetBrushModel( gentity_t * ent, const char * name ) {
	gameImports->SetBrushModel( ent, name );
}
DOOM_SAVE_CALLBACK("trap_SetBrushModel", trap_SetBrushModel);

void trap_Trace( trace_t * results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask ) {
	gameImports->Trace( results, start, mins, maxs, end, passEntityNum, contentmask );
}
DOOM_SAVE_CALLBACK("trap_Trace", trap_Trace);

void trap_TraceCapsule( trace_t * results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask ) {
	gameImports->TraceCapsule( results, start, mins, maxs, end, passEntityNum, contentmask );
}
DOOM_SAVE_CALLBACK("trap_TraceCapsule", trap_TraceCapsule);

int trap_PointContents( const vec3_t point, int passEntityNum ) {
	return gameImports->PointContents( point, passEntityNum );
}
DOOM_SAVE_CALLBACK("trap_PointContents", trap_PointContents);

qboolean trap_InPVS( const vec3_t p1, const vec3_t p2 ) {
	return gameImports->InPVS( p1, p2 );
}
DOOM_SAVE_CALLBACK("trap_InPVS", trap_InPVS);

qboolean trap_InPVSIgnorePortals( const vec3_t p1, const vec3_t p2 ) {
	return gameImports->InPVSIgnorePortals( p1, p2 );
}
DOOM_SAVE_CALLBACK("trap_InPVSIgnorePortals", trap_InPVSIgnorePortals);

void trap_AdjustAreaPortalState( gentity_t * ent, qboolean open ) {
	gameImports->AdjustAreaPortalState( ent, open );
}
DOOM_SAVE_CALLBACK("trap_AdjustAreaPortalState", trap_AdjustAreaPortalState);

qboolean trap_AreasConnected( int area1, int area2 ) {
	return gameImports->AreasConnected( area1, area2 );
}
DOOM_SAVE_CALLBACK("trap_AreasConnected", trap_AreasConnected);

void trap_LinkEntity( gentity_t * ent ) {
	gameImports->LinkEntity( ent );
}
DOOM_SAVE_CALLBACK("trap_LinkEntity", trap_LinkEntity);

void trap_UnlinkEntity( gentity_t * ent ) {
	gameImports->UnlinkEntity( ent );
}
DOOM_SAVE_CALLBACK("trap_UnlinkEntity", trap_UnlinkEntity);

int trap_EntitiesInBox( const vec3_t mins, const vec3_t maxs, int * list, int maxcount ) {
	return gameImports->EntitiesInBox( mins, maxs, list, maxcount );
}
DOOM_SAVE_CALLBACK("trap_EntitiesInBox", trap_EntitiesInBox);

qboolean trap_EntityContact( const vec3_t mins, const vec3_t maxs, const gentity_t * ent ) {
	return gameImports->EntityContact( mins, maxs, ent );
}
DOOM_SAVE_CALLBACK("trap_EntityContact", trap_EntityContact);

qboolean trap_EntityContactCapsule( const vec3_t mins, const vec3_t maxs, const gentity_t * ent ) {
	return gameImports->EntityContactCapsule( mins, maxs, ent );
}
DOOM_SAVE_CALLBACK("trap_EntityContactCapsule", trap_EntityContactCapsule);

int trap_BotAllocateClient(  ) {
	return gameImports->BotAllocateClient();
}
DOOM_SAVE_CALLBACK("trap_BotAllocateClient", trap_BotAllocateClient);

void trap_BotFreeClient( int clientNum ) {
	gameImports->BotFreeClient( clientNum );
}
DOOM_SAVE_CALLBACK("trap_BotFreeClient", trap_BotFreeClient);

void trap_GetUsercmd( int clientNum, usercmd_t * cmd ) {
	gameImports->GetUsercmd( clientNum, cmd );
}
DOOM_SAVE_CALLBACK("trap_GetUsercmd", trap_GetUsercmd);

qboolean trap_GetEntityToken( char * buffer, int bufferSize ) {
	return gameImports->GetEntityToken( buffer, bufferSize );
}
DOOM_SAVE_CALLBACK("trap_GetEntityToken", trap_GetEntityToken);

int trap_DebugPolygonCreate( int color, int numPoints, vec3_t * points ) {
	return gameImports->DebugPolygonCreate( color, numPoints, points );
}
DOOM_SAVE_CALLBACK("trap_DebugPolygonCreate", trap_DebugPolygonCreate);

void trap_DebugPolygonDelete( int id ) {
	gameImports->DebugPolygonDelete( id );
}
DOOM_SAVE_CALLBACK("trap_DebugPolygonDelete", trap_DebugPolygonDelete);

int trap_RealTime( qtime_t * qtime ) {
	return gameImports->RealTime( qtime );
}
DOOM_SAVE_CALLBACK("trap_RealTime", trap_RealTime);

void trap_SnapVector( float * v ) {
	(gameImports->SnapVector)( v );
}
DOOM_SAVE_CALLBACK("trap_SnapVector", trap_SnapVector);

qboolean trap_GetTag( int clientNum, char * tagName, orientation_t * or ) {
	return gameImports->GetTag( clientNum, tagName, or );
}
DOOM_SAVE_CALLBACK("trap_GetTag", trap_GetTag);

int trap_BotLibSetup(  ) {
	return gameImports->BotLibSetup();
}
DOOM_SAVE_CALLBACK("trap_BotLibSetup", trap_BotLibSetup);

int trap_BotLibShutdown(  ) {
	return gameImports->BotLibShutdown();
}
DOOM_SAVE_CALLBACK("trap_BotLibShutdown", trap_BotLibShutdown);

int trap_BotLibVarSet( char * var_name, char * value ) {
	return gameImports->BotLibVarSet( var_name, value );
}
DOOM_SAVE_CALLBACK("trap_BotLibVarSet", trap_BotLibVarSet);

int trap_BotLibVarGet( char * var_name, char * value, int size ) {
	return gameImports->BotLibVarGet( var_name, value, size );
}
DOOM_SAVE_CALLBACK("trap_BotLibVarGet", trap_BotLibVarGet);

int trap_BotLibDefine( char * string ) {
	return gameImports->BotLibDefine( string );
}
DOOM_SAVE_CALLBACK("trap_BotLibDefine", trap_BotLibDefine);

int trap_BotLibStartFrame( float time ) {
	return gameImports->BotLibStartFrame( time );
}
DOOM_SAVE_CALLBACK("trap_BotLibStartFrame", trap_BotLibStartFrame);

int trap_BotLibLoadMap( const char * mapname ) {
	return gameImports->BotLibLoadMap( mapname );
}
DOOM_SAVE_CALLBACK("trap_BotLibLoadMap", trap_BotLibLoadMap);

int trap_BotLibUpdateEntity( int ent, void * bue ) {
	return gameImports->BotLibUpdateEntity( ent, bue );
}
DOOM_SAVE_CALLBACK("trap_BotLibUpdateEntity", trap_BotLibUpdateEntity);

int trap_BotLibTest( int parm0, char * parm1, vec3_t parm2, vec3_t parm3 ) {
	return gameImports->BotLibTest( parm0, parm1, parm2, parm3 );
}
DOOM_SAVE_CALLBACK("trap_BotLibTest", trap_BotLibTest);

int trap_BotGetSnapshotEntity( int clientNum, int sequence ) {
	return gameImports->BotGetSnapshotEntity( clientNum, sequence );
}
DOOM_SAVE_CALLBACK("trap_BotGetSnapshotEntity", trap_BotGetSnapshotEntity);

int trap_BotGetServerCommand( int clientNum, char * message, int size ) {
	return gameImports->BotGetServerCommand( clientNum, message, size );
}
DOOM_SAVE_CALLBACK("trap_BotGetServerCommand", trap_BotGetServerCommand);

void trap_BotUserCommand( int clientNum, usercmd_t * ucmd ) {
	gameImports->BotUserCommand( clientNum, ucmd );
}
DOOM_SAVE_CALLBACK("trap_BotUserCommand", trap_BotUserCommand);

void trap_AAS_EntityInfo( int entnum, void * info ) {
	gameImports->AAS_EntityInfo( entnum, info );
}
DOOM_SAVE_CALLBACK("trap_AAS_EntityInfo", trap_AAS_EntityInfo);

int trap_AAS_Initialized(  ) {
	return gameImports->AAS_Initialized();
}
DOOM_SAVE_CALLBACK("trap_AAS_Initialized", trap_AAS_Initialized);

void trap_AAS_PresenceTypeBoundingBox( int presencetype, vec3_t mins, vec3_t maxs ) {
	gameImports->AAS_PresenceTypeBoundingBox( presencetype, mins, maxs );
}
DOOM_SAVE_CALLBACK("trap_AAS_PresenceTypeBoundingBox", trap_AAS_PresenceTypeBoundingBox);

float trap_AAS_Time(  ) {
	return gameImports->AAS_Time();
}
DOOM_SAVE_CALLBACK("trap_AAS_Time", trap_AAS_Time);

void trap_AAS_SetCurrentWorld( int index ) {
	gameImports->AAS_SetCurrentWorld( index );
}
DOOM_SAVE_CALLBACK("trap_AAS_SetCurrentWorld", trap_AAS_SetCurrentWorld);

int trap_AAS_PointAreaNum( vec3_t point ) {
	return gameImports->AAS_PointAreaNum( point );
}
DOOM_SAVE_CALLBACK("trap_AAS_PointAreaNum", trap_AAS_PointAreaNum);

int trap_AAS_TraceAreas( vec3_t start, vec3_t end, int * areas, vec3_t * points, int maxareas ) {
	return gameImports->AAS_TraceAreas( start, end, areas, points, maxareas );
}
DOOM_SAVE_CALLBACK("trap_AAS_TraceAreas", trap_AAS_TraceAreas);

int trap_AAS_PointContents( vec3_t point ) {
	return gameImports->AAS_PointContents( point );
}
DOOM_SAVE_CALLBACK("trap_AAS_PointContents", trap_AAS_PointContents);

int trap_AAS_NextBSPEntity( int ent ) {
	return gameImports->AAS_NextBSPEntity( ent );
}
DOOM_SAVE_CALLBACK("trap_AAS_NextBSPEntity", trap_AAS_NextBSPEntity);

int trap_AAS_ValueForBSPEpairKey( int ent, char * key, char * value, int size ) {
	return gameImports->AAS_ValueForBSPEpairKey( ent, key, value, size );
}
DOOM_SAVE_CALLBACK("trap_AAS_ValueForBSPEpairKey", trap_AAS_ValueForBSPEpairKey);

int trap_AAS_VectorForBSPEpairKey( int ent, char * key, vec3_t v ) {
	return gameImports->AAS_VectorForBSPEpairKey( ent, key, v );
}
DOOM_SAVE_CALLBACK("trap_AAS_VectorForBSPEpairKey", trap_AAS_VectorForBSPEpairKey);

int trap_AAS_FloatForBSPEpairKey( int ent, char * key, float * value ) {
	return gameImports->AAS_FloatForBSPEpairKey( ent, key, value );
}
DOOM_SAVE_CALLBACK("trap_AAS_FloatForBSPEpairKey", trap_AAS_FloatForBSPEpairKey);

int trap_AAS_IntForBSPEpairKey( int ent, char * key, int * value ) {
	return gameImports->AAS_IntForBSPEpairKey( ent, key, value );
}
DOOM_SAVE_CALLBACK("trap_AAS_IntForBSPEpairKey", trap_AAS_IntForBSPEpairKey);

int trap_AAS_AreaReachability( int areanum ) {
	return gameImports->AAS_AreaReachability( areanum );
}
DOOM_SAVE_CALLBACK("trap_AAS_AreaReachability", trap_AAS_AreaReachability);

int trap_AAS_AreaTravelTimeToGoalArea( int areanum, vec3_t origin, int goalareanum, int travelflags ) {
	return gameImports->AAS_AreaTravelTimeToGoalArea( areanum, origin, goalareanum, travelflags );
}
DOOM_SAVE_CALLBACK("trap_AAS_AreaTravelTimeToGoalArea", trap_AAS_AreaTravelTimeToGoalArea);

int trap_AAS_Swimming( vec3_t origin ) {
	return gameImports->AAS_Swimming( origin );
}
DOOM_SAVE_CALLBACK("trap_AAS_Swimming", trap_AAS_Swimming);

int trap_AAS_PredictClientMovement( void * move, int entnum, vec3_t origin, int presencetype, int onground, vec3_t velocity, vec3_t cmdmove, int cmdframes, int maxframes, float frametime, int stopevent, int stopareanum, int visualize ) {
	return gameImports->AAS_PredictClientMovement( move, entnum, origin, presencetype, onground, velocity, cmdmove, cmdframes, maxframes, frametime, stopevent, stopareanum, visualize );
}
DOOM_SAVE_CALLBACK("trap_AAS_PredictClientMovement", trap_AAS_PredictClientMovement);

void trap_AAS_RT_ShowRoute( vec3_t srcpos, int srcnum, int destnum ) {
	gameImports->AAS_RT_ShowRoute( srcpos, srcnum, destnum );
}
DOOM_SAVE_CALLBACK("trap_AAS_RT_ShowRoute", trap_AAS_RT_ShowRoute);

qboolean trap_AAS_RT_GetHidePos( vec3_t srcpos, int srcnum, int srcarea, vec3_t destpos, int destnum, int destarea, vec3_t returnPos ) {
	return gameImports->AAS_RT_GetHidePos( srcpos, srcnum, srcarea, destpos, destnum, destarea, returnPos );
}
DOOM_SAVE_CALLBACK("trap_AAS_RT_GetHidePos", trap_AAS_RT_GetHidePos);

int trap_AAS_FindAttackSpotWithinRange( int srcnum, int rangenum, int enemynum, float rangedist, int travelflags, float * outpos ) {
	return gameImports->AAS_FindAttackSpotWithinRange( srcnum, rangenum, enemynum, rangedist, travelflags, outpos );
}
DOOM_SAVE_CALLBACK("trap_AAS_FindAttackSpotWithinRange", trap_AAS_FindAttackSpotWithinRange);

qboolean trap_AAS_GetRouteFirstVisPos( vec3_t srcpos, vec3_t destpos, int travelflags, vec3_t retpos ) {
	return gameImports->AAS_GetRouteFirstVisPos( srcpos, destpos, travelflags, retpos );
}
DOOM_SAVE_CALLBACK("trap_AAS_GetRouteFirstVisPos", trap_AAS_GetRouteFirstVisPos);

void trap_AAS_SetAASBlockingEntity( vec3_t absmin, vec3_t absmax, qboolean blocking ) {
	gameImports->AAS_SetAASBlockingEntity( absmin, absmax, blocking );
}
DOOM_SAVE_CALLBACK("trap_AAS_SetAASBlockingEntity", trap_AAS_SetAASBlockingEntity);

void trap_EA_Say( int client, char * str ) {
	gameImports->EA_Say( client, str );
}
DOOM_SAVE_CALLBACK("trap_EA_Say", trap_EA_Say);

void trap_EA_SayTeam( int client, char * str ) {
	gameImports->EA_SayTeam( client, str );
}
DOOM_SAVE_CALLBACK("trap_EA_SayTeam", trap_EA_SayTeam);

void trap_EA_UseItem( int client, char * it ) {
	gameImports->EA_UseItem( client, it );
}
DOOM_SAVE_CALLBACK("trap_EA_UseItem", trap_EA_UseItem);

void trap_EA_DropItem( int client, char * it ) {
	gameImports->EA_DropItem( client, it );
}
DOOM_SAVE_CALLBACK("trap_EA_DropItem", trap_EA_DropItem);

void trap_EA_UseInv( int client, char * inv ) {
	gameImports->EA_UseInv( client, inv );
}
DOOM_SAVE_CALLBACK("trap_EA_UseInv", trap_EA_UseInv);

void trap_EA_DropInv( int client, char * inv ) {
	gameImports->EA_DropInv( client, inv );
}
DOOM_SAVE_CALLBACK("trap_EA_DropInv", trap_EA_DropInv);

void trap_EA_Gesture( int client ) {
	gameImports->EA_Gesture( client );
}
DOOM_SAVE_CALLBACK("trap_EA_Gesture", trap_EA_Gesture);

void trap_EA_Command( int client, char * command ) {
	gameImports->EA_Command( client, command );
}
DOOM_SAVE_CALLBACK("trap_EA_Command", trap_EA_Command);

void trap_EA_SelectWeapon( int client, int weapon ) {
	gameImports->EA_SelectWeapon( client, weapon );
}
DOOM_SAVE_CALLBACK("trap_EA_SelectWeapon", trap_EA_SelectWeapon);

void trap_EA_Talk( int client ) {
	gameImports->EA_Talk( client );
}
DOOM_SAVE_CALLBACK("trap_EA_Talk", trap_EA_Talk);

void trap_EA_Attack( int client ) {
	gameImports->EA_Attack( client );
}
DOOM_SAVE_CALLBACK("trap_EA_Attack", trap_EA_Attack);

void trap_EA_Reload( int client ) {
	gameImports->EA_Reload( client );
}
DOOM_SAVE_CALLBACK("trap_EA_Reload", trap_EA_Reload);

void trap_EA_Use( int client ) {
	gameImports->EA_Use( client );
}
DOOM_SAVE_CALLBACK("trap_EA_Use", trap_EA_Use);

void trap_EA_Respawn( int client ) {
	gameImports->EA_Respawn( client );
}
DOOM_SAVE_CALLBACK("trap_EA_Respawn", trap_EA_Respawn);

void trap_EA_Jump( int client ) {
	gameImports->EA_Jump( client );
}
DOOM_SAVE_CALLBACK("trap_EA_Jump", trap_EA_Jump);

void trap_EA_DelayedJump( int client ) {
	gameImports->EA_DelayedJump( client );
}
DOOM_SAVE_CALLBACK("trap_EA_DelayedJump", trap_EA_DelayedJump);

void trap_EA_Crouch( int client ) {
	gameImports->EA_Crouch( client );
}
DOOM_SAVE_CALLBACK("trap_EA_Crouch", trap_EA_Crouch);

void trap_EA_MoveUp( int client ) {
	gameImports->EA_MoveUp( client );
}
DOOM_SAVE_CALLBACK("trap_EA_MoveUp", trap_EA_MoveUp);

void trap_EA_MoveDown( int client ) {
	gameImports->EA_MoveDown( client );
}
DOOM_SAVE_CALLBACK("trap_EA_MoveDown", trap_EA_MoveDown);

void trap_EA_MoveForward( int client ) {
	gameImports->EA_MoveForward( client );
}
DOOM_SAVE_CALLBACK("trap_EA_MoveForward", trap_EA_MoveForward);

void trap_EA_MoveBack( int client ) {
	gameImports->EA_MoveBack( client );
}
DOOM_SAVE_CALLBACK("trap_EA_MoveBack", trap_EA_MoveBack);

void trap_EA_MoveLeft( int client ) {
	gameImports->EA_MoveLeft( client );
}
DOOM_SAVE_CALLBACK("trap_EA_MoveLeft", trap_EA_MoveLeft);

void trap_EA_MoveRight( int client ) {
	gameImports->EA_MoveRight( client );
}
DOOM_SAVE_CALLBACK("trap_EA_MoveRight", trap_EA_MoveRight);

void trap_EA_Move( int client, vec3_t dir, float speed ) {
	gameImports->EA_Move( client, dir, speed );
}
DOOM_SAVE_CALLBACK("trap_EA_Move", trap_EA_Move);

void trap_EA_View( int client, vec3_t viewangles ) {
	gameImports->EA_View( client, viewangles );
}
DOOM_SAVE_CALLBACK("trap_EA_View", trap_EA_View);

void trap_EA_EndRegular( int client, float thinktime ) {
	gameImports->EA_EndRegular( client, thinktime );
}
DOOM_SAVE_CALLBACK("trap_EA_EndRegular", trap_EA_EndRegular);

void trap_EA_GetInput( int client, float thinktime, void * input ) {
	gameImports->EA_GetInput( client, thinktime, input );
}
DOOM_SAVE_CALLBACK("trap_EA_GetInput", trap_EA_GetInput);

void trap_EA_ResetInput( int client, void * init ) {
	gameImports->EA_ResetInput( client, init );
}
DOOM_SAVE_CALLBACK("trap_EA_ResetInput", trap_EA_ResetInput);

int trap_BotLoadCharacter( char * charfile, int skill ) {
	return gameImports->BotLoadCharacter( charfile, skill );
}
DOOM_SAVE_CALLBACK("trap_BotLoadCharacter", trap_BotLoadCharacter);

void trap_BotFreeCharacter( int character ) {
	gameImports->BotFreeCharacter( character );
}
DOOM_SAVE_CALLBACK("trap_BotFreeCharacter", trap_BotFreeCharacter);

float trap_Characteristic_Float( int character, int index ) {
	return gameImports->Characteristic_Float( character, index );
}
DOOM_SAVE_CALLBACK("trap_Characteristic_Float", trap_Characteristic_Float);

float trap_Characteristic_BFloat( int character, int index, float min, float max ) {
	return gameImports->Characteristic_BFloat( character, index, min, max );
}
DOOM_SAVE_CALLBACK("trap_Characteristic_BFloat", trap_Characteristic_BFloat);

int trap_Characteristic_Integer( int character, int index ) {
	return gameImports->Characteristic_Integer( character, index );
}
DOOM_SAVE_CALLBACK("trap_Characteristic_Integer", trap_Characteristic_Integer);

int trap_Characteristic_BInteger( int character, int index, int min, int max ) {
	return gameImports->Characteristic_BInteger( character, index, min, max );
}
DOOM_SAVE_CALLBACK("trap_Characteristic_BInteger", trap_Characteristic_BInteger);

void trap_Characteristic_String( int character, int index, char * buf, int size ) {
	gameImports->Characteristic_String( character, index, buf, size );
}
DOOM_SAVE_CALLBACK("trap_Characteristic_String", trap_Characteristic_String);

int trap_BotAllocChatState(  ) {
	return gameImports->BotAllocChatState();
}
DOOM_SAVE_CALLBACK("trap_BotAllocChatState", trap_BotAllocChatState);

void trap_BotFreeChatState( int handle ) {
	gameImports->BotFreeChatState( handle );
}
DOOM_SAVE_CALLBACK("trap_BotFreeChatState", trap_BotFreeChatState);

void trap_BotQueueConsoleMessage( int chatstate, int type, char * message ) {
	gameImports->BotQueueConsoleMessage( chatstate, type, message );
}
DOOM_SAVE_CALLBACK("trap_BotQueueConsoleMessage", trap_BotQueueConsoleMessage);

void trap_BotRemoveConsoleMessage( int chatstate, int handle ) {
	gameImports->BotRemoveConsoleMessage( chatstate, handle );
}
DOOM_SAVE_CALLBACK("trap_BotRemoveConsoleMessage", trap_BotRemoveConsoleMessage);

int trap_BotNextConsoleMessage( int chatstate, void * cm ) {
	return gameImports->BotNextConsoleMessage( chatstate, cm );
}
DOOM_SAVE_CALLBACK("trap_BotNextConsoleMessage", trap_BotNextConsoleMessage);

int trap_BotNumConsoleMessages( int chatstate ) {
	return gameImports->BotNumConsoleMessages( chatstate );
}
DOOM_SAVE_CALLBACK("trap_BotNumConsoleMessages", trap_BotNumConsoleMessages);

void trap_BotInitialChat( int chatstate, char * type, int mcontext, char * var0, char * var1, char * var2, char * var3, char * var4, char * var5, char * var6, char * var7 ) {
	gameImports->BotInitialChat( chatstate, type, mcontext, var0, var1, var2, var3, var4, var5, var6, var7 );
}
DOOM_SAVE_CALLBACK("trap_BotInitialChat", trap_BotInitialChat);

int trap_BotNumInitialChats( int chatstate, char * type ) {
	return gameImports->BotNumInitialChats( chatstate, type );
}
DOOM_SAVE_CALLBACK("trap_BotNumInitialChats", trap_BotNumInitialChats);

int trap_BotReplyChat( int chatstate, char * message, int mcontext, int vcontext, char * var0, char * var1, char * var2, char * var3, char * var4, char * var5, char * var6, char * var7 ) {
	return gameImports->BotReplyChat( chatstate, message, mcontext, vcontext, var0, var1, var2, var3, var4, var5, var6, var7 );
}
DOOM_SAVE_CALLBACK("trap_BotReplyChat", trap_BotReplyChat);

int trap_BotChatLength( int chatstate ) {
	return gameImports->BotChatLength( chatstate );
}
DOOM_SAVE_CALLBACK("trap_BotChatLength", trap_BotChatLength);

void trap_BotEnterChat( int chatstate, int client, int sendto ) {
	gameImports->BotEnterChat( chatstate, client, sendto );
}
DOOM_SAVE_CALLBACK("trap_BotEnterChat", trap_BotEnterChat);

void trap_BotGetChatMessage( int chatstate, char * buf, int size ) {
	gameImports->BotGetChatMessage( chatstate, buf, size );
}
DOOM_SAVE_CALLBACK("trap_BotGetChatMessage", trap_BotGetChatMessage);

int trap_StringContains( char * str1, char * str2, int casesensitive ) {
	return gameImports->StringContains( str1, str2, casesensitive );
}
DOOM_SAVE_CALLBACK("trap_StringContains", trap_StringContains);

int trap_BotFindMatch( char * str, void * match, unsigned long int context ) {
	return gameImports->BotFindMatch( str, match, context );
}
DOOM_SAVE_CALLBACK("trap_BotFindMatch", trap_BotFindMatch);

void trap_BotMatchVariable( void * match, int variable, char * buf, int size ) {
	gameImports->BotMatchVariable( match, variable, buf, size );
}
DOOM_SAVE_CALLBACK("trap_BotMatchVariable", trap_BotMatchVariable);

void trap_UnifyWhiteSpaces( char * string ) {
	gameImports->UnifyWhiteSpaces( string );
}
DOOM_SAVE_CALLBACK("trap_UnifyWhiteSpaces", trap_UnifyWhiteSpaces);

void trap_BotReplaceSynonyms( char * string, unsigned long int context ) {
	gameImports->BotReplaceSynonyms( string, context );
}
DOOM_SAVE_CALLBACK("trap_BotReplaceSynonyms", trap_BotReplaceSynonyms);

int trap_BotLoadChatFile( int chatstate, char * chatfile, char * chatname ) {
	return gameImports->BotLoadChatFile( chatstate, chatfile, chatname );
}
DOOM_SAVE_CALLBACK("trap_BotLoadChatFile", trap_BotLoadChatFile);

void trap_BotSetChatGender( int chatstate, int gender ) {
	gameImports->BotSetChatGender( chatstate, gender );
}
DOOM_SAVE_CALLBACK("trap_BotSetChatGender", trap_BotSetChatGender);

void trap_BotSetChatName( int chatstate, char * name ) {
	gameImports->BotSetChatName( chatstate, name );
}
DOOM_SAVE_CALLBACK("trap_BotSetChatName", trap_BotSetChatName);

void trap_BotResetGoalState( int goalstate ) {
	gameImports->BotResetGoalState( goalstate );
}
DOOM_SAVE_CALLBACK("trap_BotResetGoalState", trap_BotResetGoalState);

void trap_BotResetAvoidGoals( int goalstate ) {
	gameImports->BotResetAvoidGoals( goalstate );
}
DOOM_SAVE_CALLBACK("trap_BotResetAvoidGoals", trap_BotResetAvoidGoals);

void trap_BotRemoveFromAvoidGoals( int goalstate, int number ) {
	gameImports->BotRemoveFromAvoidGoals( goalstate, number );
}
DOOM_SAVE_CALLBACK("trap_BotRemoveFromAvoidGoals", trap_BotRemoveFromAvoidGoals);

void trap_BotPushGoal( int goalstate, void * goal ) {
	gameImports->BotPushGoal( goalstate, goal );
}
DOOM_SAVE_CALLBACK("trap_BotPushGoal", trap_BotPushGoal);

void trap_BotPopGoal( int goalstate ) {
	gameImports->BotPopGoal( goalstate );
}
DOOM_SAVE_CALLBACK("trap_BotPopGoal", trap_BotPopGoal);

void trap_BotEmptyGoalStack( int goalstate ) {
	gameImports->BotEmptyGoalStack( goalstate );
}
DOOM_SAVE_CALLBACK("trap_BotEmptyGoalStack", trap_BotEmptyGoalStack);

void trap_BotDumpAvoidGoals( int goalstate ) {
	gameImports->BotDumpAvoidGoals( goalstate );
}
DOOM_SAVE_CALLBACK("trap_BotDumpAvoidGoals", trap_BotDumpAvoidGoals);

void trap_BotDumpGoalStack( int goalstate ) {
	gameImports->BotDumpGoalStack( goalstate );
}
DOOM_SAVE_CALLBACK("trap_BotDumpGoalStack", trap_BotDumpGoalStack);

void trap_BotGoalName( int number, char * name, int size ) {
	gameImports->BotGoalName( number, name, size );
}
DOOM_SAVE_CALLBACK("trap_BotGoalName", trap_BotGoalName);

int trap_BotGetTopGoal( int goalstate, void * goal ) {
	return gameImports->BotGetTopGoal( goalstate, goal );
}
DOOM_SAVE_CALLBACK("trap_BotGetTopGoal", trap_BotGetTopGoal);

int trap_BotGetSecondGoal( int goalstate, void * goal ) {
	return gameImports->BotGetSecondGoal( goalstate, goal );
}
DOOM_SAVE_CALLBACK("trap_BotGetSecondGoal", trap_BotGetSecondGoal);

int trap_BotChooseLTGItem( int goalstate, vec3_t origin, int * inventory, int travelflags ) {
	return gameImports->BotChooseLTGItem( goalstate, origin, inventory, travelflags );
}
DOOM_SAVE_CALLBACK("trap_BotChooseLTGItem", trap_BotChooseLTGItem);

int trap_BotChooseNBGItem( int goalstate, vec3_t origin, int * inventory, int travelflags, void * ltg, float maxtime ) {
	return gameImports->BotChooseNBGItem( goalstate, origin, inventory, travelflags, ltg, maxtime );
}
DOOM_SAVE_CALLBACK("trap_BotChooseNBGItem", trap_BotChooseNBGItem);

int trap_BotTouchingGoal( vec3_t origin, void * goal ) {
	return gameImports->BotTouchingGoal( origin, goal );
}
DOOM_SAVE_CALLBACK("trap_BotTouchingGoal", trap_BotTouchingGoal);

int trap_BotItemGoalInVisButNotVisible( int viewer, vec3_t eye, vec3_t viewangles, void * goal ) {
	return gameImports->BotItemGoalInVisButNotVisible( viewer, eye, viewangles, goal );
}
DOOM_SAVE_CALLBACK("trap_BotItemGoalInVisButNotVisible", trap_BotItemGoalInVisButNotVisible);

int trap_BotGetLevelItemGoal( int index, char * classname, void * goal ) {
	return gameImports->BotGetLevelItemGoal( index, classname, goal );
}
DOOM_SAVE_CALLBACK("trap_BotGetLevelItemGoal", trap_BotGetLevelItemGoal);

int trap_BotGetNextCampSpotGoal( int num, void * goal ) {
	return gameImports->BotGetNextCampSpotGoal( num, goal );
}
DOOM_SAVE_CALLBACK("trap_BotGetNextCampSpotGoal", trap_BotGetNextCampSpotGoal);

int trap_BotGetMapLocationGoal( char * name, void * goal ) {
	return gameImports->BotGetMapLocationGoal( name, goal );
}
DOOM_SAVE_CALLBACK("trap_BotGetMapLocationGoal", trap_BotGetMapLocationGoal);

float trap_BotAvoidGoalTime( int goalstate, int number ) {
	return gameImports->BotAvoidGoalTime( goalstate, number );
}
DOOM_SAVE_CALLBACK("trap_BotAvoidGoalTime", trap_BotAvoidGoalTime);

void trap_BotInitLevelItems(  ) {
	gameImports->BotInitLevelItems();
}
DOOM_SAVE_CALLBACK("trap_BotInitLevelItems", trap_BotInitLevelItems);

void trap_BotUpdateEntityItems(  ) {
	gameImports->BotUpdateEntityItems();
}
DOOM_SAVE_CALLBACK("trap_BotUpdateEntityItems", trap_BotUpdateEntityItems);

int trap_BotLoadItemWeights( int goalstate, char * filename ) {
	return gameImports->BotLoadItemWeights( goalstate, filename );
}
DOOM_SAVE_CALLBACK("trap_BotLoadItemWeights", trap_BotLoadItemWeights);

void trap_BotFreeItemWeights( int goalstate ) {
	gameImports->BotFreeItemWeights( goalstate );
}
DOOM_SAVE_CALLBACK("trap_BotFreeItemWeights", trap_BotFreeItemWeights);

void trap_BotInterbreedGoalFuzzyLogic( int parent1, int parent2, int child ) {
	gameImports->BotInterbreedGoalFuzzyLogic( parent1, parent2, child );
}
DOOM_SAVE_CALLBACK("trap_BotInterbreedGoalFuzzyLogic", trap_BotInterbreedGoalFuzzyLogic);

void trap_BotSaveGoalFuzzyLogic( int goalstate, char * filename ) {
	gameImports->BotSaveGoalFuzzyLogic( goalstate, filename );
}
DOOM_SAVE_CALLBACK("trap_BotSaveGoalFuzzyLogic", trap_BotSaveGoalFuzzyLogic);

void trap_BotMutateGoalFuzzyLogic( int goalstate, float range ) {
	gameImports->BotMutateGoalFuzzyLogic( goalstate, range );
}
DOOM_SAVE_CALLBACK("trap_BotMutateGoalFuzzyLogic", trap_BotMutateGoalFuzzyLogic);

int trap_BotAllocGoalState( int state ) {
	return gameImports->BotAllocGoalState( state );
}
DOOM_SAVE_CALLBACK("trap_BotAllocGoalState", trap_BotAllocGoalState);

void trap_BotFreeGoalState( int handle ) {
	gameImports->BotFreeGoalState( handle );
}
DOOM_SAVE_CALLBACK("trap_BotFreeGoalState", trap_BotFreeGoalState);

void trap_BotResetMoveState( int movestate ) {
	gameImports->BotResetMoveState( movestate );
}
DOOM_SAVE_CALLBACK("trap_BotResetMoveState", trap_BotResetMoveState);

void trap_BotMoveToGoal( void * result, int movestate, void * goal, int travelflags ) {
	gameImports->BotMoveToGoal( result, movestate, goal, travelflags );
}
DOOM_SAVE_CALLBACK("trap_BotMoveToGoal", trap_BotMoveToGoal);

int trap_BotMoveInDirection( int movestate, vec3_t dir, float speed, int type ) {
	return gameImports->BotMoveInDirection( movestate, dir, speed, type );
}
DOOM_SAVE_CALLBACK("trap_BotMoveInDirection", trap_BotMoveInDirection);

void trap_BotResetAvoidReach( int movestate ) {
	gameImports->BotResetAvoidReach( movestate );
}
DOOM_SAVE_CALLBACK("trap_BotResetAvoidReach", trap_BotResetAvoidReach);

void trap_BotResetLastAvoidReach( int movestate ) {
	gameImports->BotResetLastAvoidReach( movestate );
}
DOOM_SAVE_CALLBACK("trap_BotResetLastAvoidReach", trap_BotResetLastAvoidReach);

int trap_BotReachabilityArea( vec3_t origin, int testground ) {
	return gameImports->BotReachabilityArea( origin, testground );
}
DOOM_SAVE_CALLBACK("trap_BotReachabilityArea", trap_BotReachabilityArea);

int trap_BotMovementViewTarget( int movestate, void * goal, int travelflags, float lookahead, vec3_t target ) {
	return gameImports->BotMovementViewTarget( movestate, goal, travelflags, lookahead, target );
}
DOOM_SAVE_CALLBACK("trap_BotMovementViewTarget", trap_BotMovementViewTarget);

int trap_BotPredictVisiblePosition( vec3_t origin, int areanum, void * goal, int travelflags, vec3_t target ) {
	return gameImports->BotPredictVisiblePosition( origin, areanum, goal, travelflags, target );
}
DOOM_SAVE_CALLBACK("trap_BotPredictVisiblePosition", trap_BotPredictVisiblePosition);

int trap_BotAllocMoveState(  ) {
	return gameImports->BotAllocMoveState();
}
DOOM_SAVE_CALLBACK("trap_BotAllocMoveState", trap_BotAllocMoveState);

void trap_BotFreeMoveState( int handle ) {
	gameImports->BotFreeMoveState( handle );
}
DOOM_SAVE_CALLBACK("trap_BotFreeMoveState", trap_BotFreeMoveState);

void trap_BotInitMoveState( int handle, void * initmove ) {
	gameImports->BotInitMoveState( handle, initmove );
}
DOOM_SAVE_CALLBACK("trap_BotInitMoveState", trap_BotInitMoveState);

void trap_BotInitAvoidReach( int handle ) {
	gameImports->BotInitAvoidReach( handle );
}
DOOM_SAVE_CALLBACK("trap_BotInitAvoidReach", trap_BotInitAvoidReach);

int trap_BotChooseBestFightWeapon( int weaponstate, int * inventory ) {
	return gameImports->BotChooseBestFightWeapon( weaponstate, inventory );
}
DOOM_SAVE_CALLBACK("trap_BotChooseBestFightWeapon", trap_BotChooseBestFightWeapon);

void trap_BotGetWeaponInfo( int weaponstate, int weapon, void * weaponinfo ) {
	gameImports->BotGetWeaponInfo( weaponstate, weapon, weaponinfo );
}
DOOM_SAVE_CALLBACK("trap_BotGetWeaponInfo", trap_BotGetWeaponInfo);

int trap_BotLoadWeaponWeights( int weaponstate, char * filename ) {
	return gameImports->BotLoadWeaponWeights( weaponstate, filename );
}
DOOM_SAVE_CALLBACK("trap_BotLoadWeaponWeights", trap_BotLoadWeaponWeights);

int trap_BotAllocWeaponState(  ) {
	return gameImports->BotAllocWeaponState();
}
DOOM_SAVE_CALLBACK("trap_BotAllocWeaponState", trap_BotAllocWeaponState);

void trap_BotFreeWeaponState( int weaponstate ) {
	gameImports->BotFreeWeaponState( weaponstate );
}
DOOM_SAVE_CALLBACK("trap_BotFreeWeaponState", trap_BotFreeWeaponState);

void trap_BotResetWeaponState( int weaponstate ) {
	gameImports->BotResetWeaponState( weaponstate );
}
DOOM_SAVE_CALLBACK("trap_BotResetWeaponState", trap_BotResetWeaponState);

int trap_GeneticParentsAndChildSelection( int numranks, float * ranks, int * parent1, int * parent2, int * child ) {
	return gameImports->GeneticParentsAndChildSelection( numranks, ranks, parent1, parent2, child );
}
DOOM_SAVE_CALLBACK("trap_GeneticParentsAndChildSelection", trap_GeneticParentsAndChildSelection);

qboolean trap_AAS_Route( vec3_t origin, vec3_t goal, int flags, vec3_t point ) {
	return gameImports->AAS_Route( origin, goal, flags, point );
}
DOOM_SAVE_CALLBACK("trap_AAS_Route", trap_AAS_Route);

qboolean trap_AAS_DoomPlayerRoute( vec3_t origin, vec3_t goal, int flags, doomAasRoute_t * route ) {
	return gameImports->AAS_DoomPlayerRoute( origin, goal, flags, route );
}
DOOM_SAVE_CALLBACK("trap_AAS_DoomPlayerRoute", trap_AAS_DoomPlayerRoute);


