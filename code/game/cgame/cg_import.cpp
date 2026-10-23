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

#include "cg_local.h"

cgameImport_t *cgameImports;

void trap_Print( const char * fmt ) {
	cgameImports->Print( fmt );
}

void trap_Error( const char * fmt ) {
	cgameImports->Error( fmt );
}

int trap_Milliseconds(  ) {
	return cgameImports->Milliseconds();
}

void trap_Cvar_Register( vmCvar_t * vmCvar, const char * varName, const char * defaultValue, int flags ) {
	cgameImports->Cvar_Register( vmCvar, varName, defaultValue, flags );
}

void trap_Cvar_Update( vmCvar_t * vmCvar ) {
	cgameImports->Cvar_Update( vmCvar );
}

void trap_Cvar_Set( const char * var_name, const char * value ) {
	cgameImports->Cvar_Set( var_name, value );
}

void trap_Cvar_VariableStringBuffer( const char * var_name, char * buffer, int bufsize ) {
	cgameImports->Cvar_VariableStringBuffer( var_name, buffer, bufsize );
}

int trap_Argc(  ) {
	return cgameImports->Argc();
}

void trap_Argv( int n, char * buffer, int bufferLength ) {
	cgameImports->Argv( n, buffer, bufferLength );
}

void trap_Args( char * buffer, int bufferLength ) {
	cgameImports->Args( buffer, bufferLength );
}

int trap_FS_FOpenFile( const char * qpath, fileHandle_t * f, fsMode_t mode ) {
	return cgameImports->FS_FOpenFile( qpath, f, mode );
}

void trap_FS_Read( void * buffer, int len, fileHandle_t f ) {
	cgameImports->FS_Read( buffer, len, f );
}

void trap_FS_Write( const void * buffer, int len, fileHandle_t f ) {
	cgameImports->FS_Write( buffer, len, f );
}

void trap_FS_FCloseFile( fileHandle_t f ) {
	cgameImports->FS_FCloseFile( f );
}

void trap_SendConsoleCommand( const char * text ) {
	cgameImports->SendConsoleCommand( text );
}

void trap_AddCommand( const char * cmdName ) {
	cgameImports->AddCommand( cmdName );
}

void trap_SendClientCommand( const char * s ) {
	cgameImports->SendClientCommand( s );
}

void trap_UpdateScreen(  ) {
	cgameImports->UpdateScreen();
}

void trap_CM_LoadMap( const char * mapname ) {
	cgameImports->CM_LoadMap( mapname );
}

int trap_CM_NumInlineModels(  ) {
	return cgameImports->CM_NumInlineModels();
}

clipHandle_t trap_CM_InlineModel( int index ) {
	return cgameImports->CM_InlineModel( index );
}

clipHandle_t trap_CM_TempBoxModel( const vec3_t mins, const vec3_t maxs ) {
	return cgameImports->CM_TempBoxModel( mins, maxs );
}

clipHandle_t trap_CM_TempCapsuleModel( const vec3_t mins, const vec3_t maxs ) {
	return cgameImports->CM_TempCapsuleModel( mins, maxs );
}

int trap_CM_PointContents( const vec3_t p, clipHandle_t model ) {
	return cgameImports->CM_PointContents( p, model );
}

int trap_CM_TransformedPointContents( const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles ) {
	return cgameImports->CM_TransformedPointContents( p, model, origin, angles );
}

void trap_CM_BoxTrace( trace_t * results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask ) {
	cgameImports->CM_BoxTrace( results, start, end, mins, maxs, model, brushmask );
}

void trap_CM_TransformedBoxTrace( trace_t * results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles ) {
	cgameImports->CM_TransformedBoxTrace( results, start, end, mins, maxs, model, brushmask, origin, angles );
}

void trap_CM_CapsuleTrace( trace_t * results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask ) {
	cgameImports->CM_CapsuleTrace( results, start, end, mins, maxs, model, brushmask );
}

void trap_CM_TransformedCapsuleTrace( trace_t * results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles ) {
	cgameImports->CM_TransformedCapsuleTrace( results, start, end, mins, maxs, model, brushmask, origin, angles );
}

int trap_CM_MarkFragments( int numPoints, const vec3_t * points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t * fragmentBuffer ) {
	return cgameImports->CM_MarkFragments( numPoints, points, projection, maxPoints, pointBuffer, maxFragments, fragmentBuffer );
}

void trap_S_StartSound( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx ) {
	cgameImports->S_StartSound( origin, entityNum, entchannel, sfx );
}

void trap_S_StartSoundEx( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx, int flags ) {
	cgameImports->S_StartSoundEx( origin, entityNum, entchannel, sfx, flags );
}

void trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum ) {
	cgameImports->S_StartLocalSound( sfx, channelNum );
}

void trap_S_ClearLoopingSounds( qboolean killall ) {
	cgameImports->S_ClearLoopingSounds( killall );
}

void trap_S_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx, int volume ) {
	cgameImports->S_AddLoopingSound( entityNum, origin, velocity, 1250, sfx, volume );
}

void trap_S_AddRangedLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx, int range ) {
	cgameImports->S_AddLoopingSound( entityNum, origin, velocity, range, sfx, 255 );
}

void trap_S_AddRealLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx ) {
	// not in use
}

void trap_S_StopLoopingSound( int entityNum ) {
	cgameImports->S_StopLoopingSound( entityNum );
}

void trap_S_StopStreamingSound( int entityNum ) {
	cgameImports->S_StopStreamingSound( entityNum );
}

void trap_S_UpdateEntityPosition( int entityNum, const vec3_t origin ) {
	cgameImports->S_UpdateEntityPosition( entityNum, origin );
}

int trap_S_GetVoiceAmplitude( int entityNum ) {
	return cgameImports->S_GetVoiceAmplitude( entityNum );
}

void trap_S_Respatialize( int entityNum, const vec3_t origin, vec3_t * axis, int inwater ) {
	cgameImports->S_Respatialize( entityNum, origin, axis, inwater );
}

sfxHandle_t trap_S_RegisterSound( const char * sample ) {
	CG_DrawInformation();
	return cgameImports->S_RegisterSound( sample );
}

void trap_S_StartBackgroundTrack( const char * intro, const char * loop, int fadeupTime ) {
	cgameImports->S_StartBackgroundTrack( intro, loop, fadeupTime );
}

void trap_S_FadeBackgroundTrack( float targetvol, int time, int num ) {
	cgameImports->S_FadeBackgroundTrack( targetvol, time, num );
}

void trap_S_FadeAllSound( float targetvol, int time ) {
	cgameImports->S_FadeAllSound( targetvol, time );
}

void trap_S_StartStreamingSound( const char * intro, const char * loop, int entnum, int channel, int attenuation ) {
	cgameImports->S_StartStreamingSound( intro, loop, entnum, channel, attenuation );
}

void trap_R_LoadWorldMap( const char * mapname ) {
	cgameImports->R_LoadWorldMap( mapname );
}

qhandle_t trap_R_RegisterModel( const char * name ) {
	CG_DrawInformation();
	return cgameImports->R_RegisterModel( name );
}

qboolean trap_R_GetSkinModel( qhandle_t skinid, const char * type, char * name ) {
	return cgameImports->R_GetSkinModel( skinid, type, name );
}

qhandle_t trap_R_GetShaderFromModel( qhandle_t modelid, int surfnum, int withlightmap ) {
	return cgameImports->R_GetShaderFromModel( modelid, surfnum, withlightmap );
}

qhandle_t trap_R_RegisterSkin( const char * name ) {
	CG_DrawInformation();
	return cgameImports->R_RegisterSkin( name );
}

qhandle_t trap_R_RegisterShader( const char * name ) {
	CG_DrawInformation();
	return cgameImports->R_RegisterShader( name );
}

qhandle_t trap_R_RegisterShaderNoMip( const char * name ) {
	CG_DrawInformation();
	return cgameImports->R_RegisterShaderNoMip( name );
}

void trap_R_RegisterFont( const char * fontName, int pointSize, fontInfo_t * font ) {
	cgameImports->R_RegisterFont( fontName, pointSize, font );
}

void trap_R_ClearScene(  ) {
	cgameImports->R_ClearScene();
}

void trap_R_AddRefEntityToScene( const refEntity_t * re ) {
	cgameImports->R_AddRefEntityToScene( re );
}

void trap_R_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t * verts ) {
	cgameImports->R_AddPolyToScene( hShader, numVerts, verts );
}

void trap_R_AddPolysToScene( qhandle_t hShader, int numVerts, const polyVert_t * verts, int numPolys ) {
	cgameImports->R_AddPolysToScene( hShader, numVerts, verts, numPolys );
}

void trap_RB_ZombieFXAddNewHit( int entityNum, const vec3_t hitPos, const vec3_t hitDir ) {
	cgameImports->RB_ZombieFXAddNewHit( entityNum, hitPos, hitDir );
}

void trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b, int overdraw ) {
	cgameImports->R_AddLightToScene( org, intensity, r, g, b, overdraw );
}

void trap_R_AddCoronaToScene( const vec3_t org, float r, float g, float b, float scale, int id, int flags ) {
	cgameImports->R_AddCoronaToScene( org, r, g, b, scale, id, flags );
}

void trap_R_SetFog( int fogvar, int var1, int var2, float r, float g, float b, float density ) {
	cgameImports->R_SetFog( fogvar, var1, var2, r, g, b, density );
}

void trap_R_RenderScene( const refdef_t * fd ) {
	cgameImports->R_RenderScene( fd );
}

void trap_R_SetColor( const float * rgba ) {
	cgameImports->R_SetColor( rgba );
}

void trap_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	cgameImports->R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, hShader );
}

void trap_R_DrawStretchPicGradient( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader, const float * gradientColor, int gradientType ) {
	cgameImports->R_DrawStretchPicGradient( x, y, w, h, s1, t1, s2, t2, hShader, gradientColor, gradientType );
}

void trap_R_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs ) {
	cgameImports->R_ModelBounds( model, mins, maxs );
}

int trap_R_LerpTag( orientation_t * tag, const refEntity_t * refent, const char * tagName, int startIndex ) {
	return cgameImports->R_LerpTag( tag, refent, tagName, startIndex );
}

void trap_R_RemapShader( const char * oldShader, const char * newShader, const char * timeOffset ) {
	cgameImports->R_RemapShader( oldShader, newShader, timeOffset );
}

void trap_GetGlconfig( glconfig_t * glconfig ) {
	cgameImports->GetGlconfig( glconfig );
}

void trap_GetGameState( gameState_t * gamestate ) {
	cgameImports->GetGameState( gamestate );
}

void trap_GetCurrentSnapshotNumber( int * snapshotNumber, int * serverTime ) {
	cgameImports->GetCurrentSnapshotNumber( snapshotNumber, serverTime );
}

qboolean trap_GetSnapshot( int snapshotNumber, snapshot_t * snapshot ) {
	return cgameImports->GetSnapshot( snapshotNumber, snapshot );
}

qboolean trap_GetServerCommand( int serverCommandNumber ) {
	return cgameImports->GetServerCommand( serverCommandNumber );
}

int trap_GetCurrentCmdNumber(  ) {
	return cgameImports->GetCurrentCmdNumber();
}

qboolean trap_GetUserCmd( int cmdNumber, usercmd_t * ucmd ) {
	return cgameImports->GetUserCmd( cmdNumber, ucmd );
}

void trap_SetUserCmdValue( int stateValue, int holdableValue, float sensitivityScale, int cld ) {
	cgameImports->SetUserCmdValue( stateValue, holdableValue, sensitivityScale, cld );
}

int trap_MemoryRemaining(  ) {
	return cgameImports->MemoryRemaining();
}

qboolean trap_loadCamera( int camNum, const char * name ) {
	return cgameImports->loadCamera( camNum, name );
}

void trap_startCamera( int camNum, int time ) {
	cgameImports->startCamera( camNum, time );
}

void trap_stopCamera( int camNum ) {
	cgameImports->stopCamera( camNum );
}

qboolean trap_getCameraInfo( int camNum, int time, vec3_t * origin, vec3_t * angles, float * fov ) {
	return cgameImports->getCameraInfo( camNum, time, origin, angles, fov );
}

qboolean trap_Key_IsDown( int keynum ) {
	return cgameImports->Key_IsDown( keynum );
}

int trap_Key_GetCatcher(  ) {
	return cgameImports->Key_GetCatcher();
}

void trap_Key_SetCatcher( int catcher ) {
	cgameImports->Key_SetCatcher( catcher );
}

int trap_Key_GetKey( const char * binding ) {
	return cgameImports->Key_GetKey( binding );
}

int trap_PC_AddGlobalDefine( char * define ) {
	return cgameImports->PC_AddGlobalDefine( define );
}

int trap_PC_LoadSource( const char * filename ) {
	return cgameImports->PC_LoadSource( filename );
}

int trap_PC_FreeSource( int handle ) {
	return cgameImports->PC_FreeSource( handle );
}

int trap_PC_ReadToken( int handle, pc_token_t * pc_token ) {
	return cgameImports->PC_ReadToken( handle, pc_token );
}

int trap_PC_SourceFileAndLine( int handle, char * filename, int * line ) {
	return cgameImports->PC_SourceFileAndLine( handle, filename, line );
}

void trap_S_StopBackgroundTrack(  ) {
	cgameImports->S_StopBackgroundTrack();
}

int trap_RealTime( qtime_t * qtime ) {
	return cgameImports->RealTime( qtime );
}

void trap_SnapVector( float * v ) {
	(cgameImports->SnapVector)( v );
}

void trap_SendMoveSpeedsToGame( int entnum, char * movespeeds ) {
	cgameImports->SendMoveSpeedsToGame( entnum, movespeeds );
}

int trap_CIN_PlayCinematic( const char * arg0, int xpos, int ypos, int width, int height, int bits ) {
	return cgameImports->CIN_PlayCinematic( arg0, xpos, ypos, width, height, bits );
}

e_status trap_CIN_StopCinematic( int handle ) {
	return cgameImports->CIN_StopCinematic( handle );
}

e_status trap_CIN_RunCinematic( int handle ) {
	return cgameImports->CIN_RunCinematic( handle );
}

void trap_CIN_DrawCinematic( int handle ) {
	cgameImports->CIN_DrawCinematic( handle );
}

void trap_CIN_SetExtents( int handle, int x, int y, int w, int h ) {
	cgameImports->CIN_SetExtents( handle, x, y, w, h );
}

qboolean trap_GetEntityToken( char * buffer, int bufferSize ) {
	return cgameImports->GetEntityToken( buffer, bufferSize );
}

void trap_UI_Popup( const char * arg0 ) {
	cgameImports->UI_Popup( arg0 );
}

void trap_UI_LimboChat( const char * arg0 ) {
	cgameImports->UI_LimboChat( arg0 );
}

void trap_UI_ClosePopup( const char * arg0 ) {
	cgameImports->UI_ClosePopup( arg0 );
}

qboolean trap_GetModelInfo( int clientNum, char * modelName, animModelInfo_t ** modelInfo ) {
	return cgameImports->GetModelInfo( clientNum, modelName, (void **)( modelInfo ) );
}

void trap_R_SetDoomSectorLight( int sector, int light ) {
	cgameImports->R_SetDoomSectorLight( sector, light );
}

void trap_R_SetDoomSwitches( const char * state ) {
	cgameImports->R_SetDoomSwitches( state );
}

int trap_R_LightForPoint( vec3_t point, vec3_t ambient, vec3_t directed, vec3_t lightDirection ) {
	return cgameImports->R_LightForPoint( point, ambient, directed, lightDirection );
}

void trap_R_DrawDoomMap( int x, int y, int width, int height, const byte * pixels ) {
	cgameImports->R_DrawDoomMap( x, y, width, height, pixels );
}


