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

#ifndef __UI_PUBLIC_H__
#define __UI_PUBLIC_H__

#define UI_API_VERSION  4

typedef struct {
	connstate_t connState;
	int connectPacketCount;
	int clientNum;
	char servername[MAX_STRING_CHARS];
	char updateInfoString[MAX_STRING_CHARS];
	char messageString[MAX_STRING_CHARS];
} uiClientState_t;

typedef struct {
	int apiVersion;
	void (*Print)( const char * string );
	void (*Error)( const char * string );
	int (*Milliseconds)(  );
	void (*Cvar_Register)( vmCvar_t * cvar, const char * var_name, const char * value, int flags );
	void (*Cvar_Update)( vmCvar_t * cvar );
	void (*Cvar_Set)( const char * var_name, const char * value );
	float (*Cvar_VariableValue)( const char * var_name );
	void (*Cvar_VariableStringBuffer)( const char * var_name, char * buffer, int bufsize );
	void (*Cvar_SetValue)( const char * var_name, float value );
	void (*Cvar_Reset)( const char * name );
	void (*Cvar_Create)( const char * var_name, const char * var_value, int flags );
	void (*Cvar_InfoStringBuffer)( int bit, char * buffer, int bufsize );
	int (*Argc)(  );
	void (*Argv)( int n, char * buffer, int bufferLength );
	void (*Cmd_ExecuteText)( int exec_when, const char * text );
	int (*FS_FOpenFile)( const char * qpath, fileHandle_t * f, fsMode_t mode );
	void (*FS_Read)( void * buffer, int len, fileHandle_t f );
	void (*FS_Seek)( fileHandle_t f, long offset, int origin );
	void (*FS_Write)( const void * buffer, int len, fileHandle_t f );
	void (*FS_FCloseFile)( fileHandle_t f );
	int (*FS_GetFileList)( const char * path, const char * extension, char * listbuf, int bufsize );
	int (*FS_Delete)( const char * filename );
	qhandle_t (*R_RegisterModel)( const char * name );
	qhandle_t (*R_RegisterSkin)( const char * name );
	void (*R_RegisterFont)( const char * fontName, int pointSize, fontInfo_t * font );
	qhandle_t (*R_RegisterShaderNoMip)( const char * name );
	void (*R_ClearScene)(  );
	void (*R_AddRefEntityToScene)( const refEntity_t * re );
	void (*R_AddPolyToScene)( qhandle_t hShader, int numVerts, const polyVert_t * verts );
	void (*R_AddLightToScene)( const vec3_t org, float intensity, float r, float g, float b, int overdraw );
	void (*R_AddCoronaToScene)( const vec3_t org, float r, float g, float b, float scale, int id, int flags );
	void (*R_RenderScene)( const refdef_t * fd );
	void (*R_SetColor)( const float * rgba );
	void (*R_DrawStretchPic)( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader );
	void (*R_ModelBounds)( clipHandle_t model, vec3_t mins, vec3_t maxs );
	void (*UpdateScreen)(  );
	int (*CM_LerpTag)( orientation_t * tag, const refEntity_t * refent, const char * tagName, int startIndex );
	void (*S_StartLocalSound)( sfxHandle_t sfx, int channelNum );
	sfxHandle_t (*S_RegisterSound)( const char * sample );
	void (*S_FadeBackgroundTrack)( float targetvol, int time, int num );
	void (*S_FadeAllSound)( float targetvol, int time );
	void (*Key_KeynumToStringBuf)( int keynum, char * buf, int buflen );
	void (*Key_GetBindingBuf)( int keynum, char * buf, int buflen );
	void (*Key_SetBinding)( int keynum, const char * binding );
	qboolean (*Key_IsDown)( int keynum );
	qboolean (*Key_GetOverstrikeMode)(  );
	void (*Key_SetOverstrikeMode)( qboolean state );
	void (*Key_ClearStates)(  );
	int (*Key_GetCatcher)(  );
	void (*Key_SetCatcher)( int catcher );
	void (*GetClipboardData)( char * buf, int bufsize );
	void (*GetClientState)( uiClientState_t * state );
	void (*GetGlconfig)( glconfig_t * glconfig );
	int (*GetConfigString)( int index, char* buff, int buffsize );
	int (*LAN_GetPingQueueCount)(  );
	void (*LAN_ClearPing)( int n );
	void (*LAN_GetPing)( int n, char * buf, int buflen, int * pingtime );
	void (*LAN_GetPingInfo)( int n, char * buf, int buflen );
	qboolean (*LAN_UpdateVisiblePings)( int source );
	int (*LAN_GetServerCount)( int source );
	int (*LAN_CompareServers)( int source, int sortKey, int sortDir, int s1, int s2 );
	void (*LAN_GetServerAddressString)( int source, int n, char * buf, int buflen );
	void (*LAN_GetServerInfo)( int source, int n, char * buf, int buflen );
	int (*LAN_AddServer)( int source, const char * name, const char * addr );
	void (*LAN_RemoveServer)( int source, const char * addr );
	int (*LAN_GetServerPing)( int source, int n );
	int (*LAN_ServerIsVisible)( int source, int n );
	int (*LAN_ServerStatus)( const char * serverAddress, char * serverStatus, int maxLen );
	void (*LAN_SaveCachedServers)(  );
	void (*LAN_LoadCachedServers)(  );
	void (*LAN_MarkServerVisible)( int source, int n, qboolean visible );
	void (*LAN_ResetPings)( int n );
	int (*MemoryRemaining)(  );
	void (*GetCDKey)( char * buf, int buflen );
	void (*SetCDKey)( char * buf );
	int (*PC_AddGlobalDefine)( char * define );
	int (*PC_LoadSource)( const char * filename );
	int (*PC_FreeSource)( int handle );
	int (*PC_ReadToken)( int handle, pc_token_t * pc_token );
	int (*PC_SourceFileAndLine)( int handle, char * filename, int * line );
	void (*S_StopBackgroundTrack)(  );
	void (*S_StartBackgroundTrack)( const char * intro, const char * loop, int fadeupTime );
	int (*RealTime)( qtime_t * qtime );
	int (*CIN_PlayCinematic)( const char * arg0, int xpos, int ypos, int width, int height, int bits );
	e_status (*CIN_StopCinematic)( int handle );
	e_status (*CIN_RunCinematic)( int handle );
	void (*CIN_DrawCinematic)( int handle );
	void (*CIN_SetExtents)( int handle, int x, int y, int w, int h );
	void (*R_RemapShader)( const char * oldShader, const char * newShader, const char * timeOffset );
	qboolean (*VerifyCDKey)( const char * key, const char * chksum );
	qboolean (*GetLimboString)( int index, char * buf );
} uiImport_t;

// set by the engine right before the module entry point runs
extern uiImport_t *uiImports;

typedef enum {
	UIMENU_NONE,
	UIMENU_MAIN,
	UIMENU_INGAME,
	UIMENU_NEED_CD,
	UIMENU_ENDGAME, //----(SA)	added
	UIMENU_BAD_CD_KEY,
	UIMENU_TEAM,
	UIMENU_PREGAME, //----(SA)	added
	UIMENU_POSTGAME,
	UIMENU_NOTEBOOK,
	UIMENU_CLIPBOARD,
	UIMENU_HELP,
	UIMENU_BOOK1,           //----(SA)	added
	UIMENU_BOOK2,           //----(SA)	added
	UIMENU_BOOK3,           //----(SA)	added
	UIMENU_WM_PICKTEAM,     // NERVE - SMF - for multiplayer only
	UIMENU_WM_PICKPLAYER,   // NERVE - SMF - for multiplayer only
	UIMENU_WM_QUICKMESSAGE, // NERVE - SMF
	UIMENU_WM_LIMBO,        // NERVE - SMF
	UIMENU_BRIEFING         //----(SA)	added
} uiMenuCommand_t;

#define SORT_HOST           0
#define SORT_MAP            1
#define SORT_CLIENTS        2
#define SORT_GAME           3
#define SORT_PING           4

#define SORT_SAVENAME       0
#define SORT_SAVETIME       1

typedef struct {
	int apiVersion;
	void (*Init)( qboolean fullscreen );
	void (*Shutdown)( void );
	void (*KeyEvent)( int key, qboolean down );
	void (*MouseEvent)( int dx, int dy );
	void (*Refresh)( int time );
	qboolean (*IsFullscreen)( void );
	void (*SetActiveMenu)( uiMenuCommand_t menu );
	uiMenuCommand_t (*GetActiveMenu)( void );
	qboolean (*ConsoleCommand)( int realtime );
	void (*DrawConnectScreen)( qboolean overlay );
	qboolean (*HasUniqueCDKey)( void );
} uiExport_t;

#endif

