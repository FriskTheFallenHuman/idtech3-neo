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

#ifndef CG_PUBLIC_H
#define CG_PUBLIC_H

#define CMD_BACKUP          64
#define CMD_MASK            ( CMD_BACKUP - 1 )
// allow a lot of command backups for very fast systems
// multiple commands may be combined in a single packet, so this
// needs to be larger than PACKET_BACKUP

#define MAX_ENTITIES_IN_SNAPSHOT MAX_GENTITIES

// snapshots are a view of the server at a given time

// Snapshots are generated at regular time intervals by the server,
// but they may not be sent if a client's rate level is exceeded, or
// they may be dropped by the network.
typedef struct {
	int snapFlags;                      // SNAPFLAG_RATE_DELAYED, etc
	int ping;

	int serverTime;                 // server time the message is valid for (in msec)

	byte areamask[MAX_MAP_AREA_BYTES];                  // portalarea visibility bits

	playerState_t ps;                       // complete information about the current player at this time

	int numEntities;                        // all of the entities that need to be presented
	entityState_t entities[MAX_ENTITIES_IN_SNAPSHOT];   // at the time of this snapshot

	int numServerCommands;                 // text based server commands to execute when this
	int serverCommandSequence;             // snapshot becomes current
} snapshot_t;

enum {
	CGAME_EVENT_NONE,
	CGAME_EVENT_TEAMMENU,
	CGAME_EVENT_SCOREBOARD,
	CGAME_EVENT_EDITHUD
};


/*
==================================================================

functions imported from the main executable

==================================================================
*/

#define CGAME_IMPORT_API_VERSION 5 // shared refEntity_t VFX layout

typedef struct {
	int apiVersion;
	void (*Print)( const char * fmt );
	void (*Error)( const char * fmt );
	int (*Milliseconds)(  );
	void (*Cvar_Register)( vmCvar_t * vmCvar, const char * varName, const char * defaultValue, int flags );
	void (*Cvar_Update)( vmCvar_t * vmCvar );
	void (*Cvar_Set)( const char * var_name, const char * value );
	void (*Cvar_VariableStringBuffer)( const char * var_name, char * buffer, int bufsize );
	int (*Argc)(  );
	void (*Argv)( int n, char * buffer, int bufferLength );
	void (*Args)( char * buffer, int bufferLength );
	int (*FS_FOpenFile)( const char * qpath, fileHandle_t * f, fsMode_t mode );
	void (*FS_Read)( void * buffer, int len, fileHandle_t f );
	void (*FS_Write)( const void * buffer, int len, fileHandle_t f );
	void (*FS_FCloseFile)( fileHandle_t f );
	void (*SendConsoleCommand)( const char * text );
	void (*AddCommand)( const char * cmdName );
	void (*SendClientCommand)( const char * s );
	void (*UpdateScreen)(  );
	void (*CM_LoadMap)( const char * mapname );
	int (*CM_NumInlineModels)(  );
	clipHandle_t (*CM_InlineModel)( int index );
	clipHandle_t (*CM_TempBoxModel)( const vec3_t mins, const vec3_t maxs );
	clipHandle_t (*CM_TempCapsuleModel)( const vec3_t mins, const vec3_t maxs );
	int (*CM_PointContents)( const vec3_t p, clipHandle_t model );
	int (*CM_TransformedPointContents)( const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles );
	void (*CM_BoxTrace)( trace_t * results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask );
	void (*CM_TransformedBoxTrace)( trace_t * results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles );
	void (*CM_CapsuleTrace)( trace_t * results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask );
	void (*CM_TransformedCapsuleTrace)( trace_t * results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles );
	int (*CM_MarkFragments)( int numPoints, const vec3_t * points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t * fragmentBuffer );
	void (*S_StartSound)( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx );
	void (*S_StartSoundEx)( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx, int flags );
	void (*S_StartLocalSound)( sfxHandle_t sfx, int channelNum );
	void (*S_ClearLoopingSounds)( qboolean killall );
	void (*S_AddLoopingSound)( int entityNum, const vec3_t origin, const vec3_t velocity, int range, sfxHandle_t sfx, int volume );
	void (*S_StopLoopingSound)( int entityNum );
	void (*S_StopStreamingSound)( int entityNum );
	void (*S_UpdateEntityPosition)( int entityNum, const vec3_t origin );
	int (*S_GetVoiceAmplitude)( int entityNum );
	void (*S_Respatialize)( int entityNum, const vec3_t origin, vec3_t * axis, int inwater );
	sfxHandle_t (*S_RegisterSound)( const char * sample );
	void (*S_StartBackgroundTrack)( const char * intro, const char * loop, int fadeupTime );
	void (*S_FadeBackgroundTrack)( float targetvol, int time, int num );
	void (*S_FadeAllSound)( float targetvol, int time );
	void (*S_StartStreamingSound)( const char * intro, const char * loop, int entnum, int channel, int attenuation );
	void (*R_LoadWorldMap)( const char * mapname );
	qhandle_t (*R_RegisterModel)( const char * name );
	qboolean (*R_GetSkinModel)( qhandle_t skinid, const char * type, char * name );
	qhandle_t (*R_GetShaderFromModel)( qhandle_t modelid, int surfnum, int withlightmap );
	qhandle_t (*R_RegisterSkin)( const char * name );
	qhandle_t (*R_RegisterShader)( const char * name );
	qhandle_t (*R_RegisterShaderNoMip)( const char * name );
	void (*R_RegisterFont)( const char * fontName, int pointSize, fontInfo_t * font );
	void (*R_ClearScene)(  );
	void (*R_AddRefEntityToScene)( const refEntity_t * re );
	void (*R_AddPolyToScene)( qhandle_t hShader, int numVerts, const polyVert_t * verts );
	void (*R_AddPolysToScene)( qhandle_t hShader, int numVerts, const polyVert_t * verts, int numPolys );
	void (*RB_ZombieFXAddNewHit)( int entityNum, const vec3_t hitPos, const vec3_t hitDir );
	void (*R_AddLightToScene)( const vec3_t org, float intensity, float r, float g, float b, int overdraw );
	void (*R_AddCoronaToScene)( const vec3_t org, float r, float g, float b, float scale, int id, int flags );
	void (*R_SetFog)( int fogvar, int var1, int var2, float r, float g, float b, float density );
	void (*R_RenderScene)( const refdef_t * fd );
	void (*R_SetColor)( const float * rgba );
	void (*R_DrawStretchPic)( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader );
	void (*R_DrawStretchPicGradient)( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader, const float * gradientColor, int gradientType );
	void (*R_ModelBounds)( clipHandle_t model, vec3_t mins, vec3_t maxs );
	int (*R_LerpTag)( orientation_t * tag, const refEntity_t * refent, const char * tagName, int startIndex );
	void (*R_RemapShader)( const char * oldShader, const char * newShader, const char * timeOffset );
	void (*GetGlconfig)( glconfig_t * glconfig );
	void (*GetGameState)( gameState_t * gamestate );
	void (*GetCurrentSnapshotNumber)( int * snapshotNumber, int * serverTime );
	qboolean (*GetSnapshot)( int snapshotNumber, snapshot_t * snapshot );
	qboolean (*GetServerCommand)( int serverCommandNumber );
	int (*GetCurrentCmdNumber)(  );
	qboolean (*GetUserCmd)( int cmdNumber, usercmd_t * ucmd );
	void (*SetUserCmdValue)( int stateValue, int holdableValue, float sensitivityScale, int cld );
	int (*MemoryRemaining)(  );
	qboolean (*loadCamera)( int camNum, const char * name );
	void (*startCamera)( int camNum, int time );
	void (*stopCamera)( int camNum );
	qboolean (*getCameraInfo)( int camNum, int time, vec3_t * origin, vec3_t * angles, float * fov );
	qboolean (*Key_IsDown)( int keynum );
	int (*Key_GetCatcher)(  );
	void (*Key_SetCatcher)( int catcher );
	int (*Key_GetKey)( const char * binding );
	int (*PC_AddGlobalDefine)( char * define );
	int (*PC_LoadSource)( const char * filename );
	int (*PC_FreeSource)( int handle );
	int (*PC_ReadToken)( int handle, pc_token_t * pc_token );
	int (*PC_SourceFileAndLine)( int handle, char * filename, int * line );
	void (*S_StopBackgroundTrack)(  );
	int (*RealTime)( qtime_t * qtime );
	void (*SnapVector)( float * v );
	void (*SendMoveSpeedsToGame)( int entnum, char * movespeeds );
	int (*CIN_PlayCinematic)( const char * arg0, int xpos, int ypos, int width, int height, int bits );
	e_status (*CIN_StopCinematic)( int handle );
	e_status (*CIN_RunCinematic)( int handle );
	void (*CIN_DrawCinematic)( int handle );
	void (*CIN_SetExtents)( int handle, int x, int y, int w, int h );
	qboolean (*GetEntityToken)( char * buffer, int bufferSize );
	void (*UI_Popup)( const char * arg0 );
	void (*UI_LimboChat)( const char * arg0 );
	void (*UI_ClosePopup)( const char * arg0 );
	qboolean (*GetModelInfo)( int clientNum, char * modelName, void ** modelInfo );
	void (*R_SetDoomSectorLight)( int sector, int light );
	void (*R_SetDoomSwitches)( const char * state );
	int (*R_LightForPoint)( vec3_t point, vec3_t ambient, vec3_t directed, vec3_t lightDirection );
	void (*R_DrawDoomMap)( int x, int y, int width, int height, const byte * pixels );
} cgameImport_t;

// set by the engine right before the module entry point runs
extern cgameImport_t *cgameImports;

/*
==================================================================

functions exported to the main executable

==================================================================
*/

typedef struct {
	int apiVersion;
	void (*Init)( int serverMessageNum, int serverCommandSequence );
	void (*Shutdown)( void );
	qboolean (*ConsoleCommand)( void );
	void (*DrawActiveFrame)( int serverTime, stereoFrame_t stereoView, qboolean demoPlayback );
	int (*CrosshairPlayer)( void );
	int (*LastAttacker)( void );
	void (*KeyEvent)( int key, qboolean down );
	void (*MouseEvent)( int dx, int dy );
	void (*EventHandling)( int type );
	qboolean (*GetTag)( int clientNum, char * tagName, orientation_t * or );
} cgameExport_t;

//----------------------------------------------

#endif


