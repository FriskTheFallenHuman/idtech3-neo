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


#include "client.h"

#include "../game/botlib.h"

extern botlib_export_t *botlib_export;

uiExport_t *uiExport = NULL;
static void *uiDllHandle = NULL;

extern char cl_cdkey[34];


/*
====================
GetClientState
====================
*/
static void GetClientState( uiClientState_t *state ) {
	state->connectPacketCount = clc.connectPacketCount;
	state->connState = cls.state;
	Q_strncpyz( state->servername, cls.servername, sizeof( state->servername ) );
	Q_strncpyz( state->updateInfoString, cls.updateInfoString, sizeof( state->updateInfoString ) );
	Q_strncpyz( state->messageString, clc.serverMessage, sizeof( state->messageString ) );
	state->clientNum = cl.snap.ps.clientNum;
}

/*
====================
LAN_LoadCachedServers
====================
*/
void LAN_LoadCachedServers() {
	// TTimo: stub, this is only relevant to MP, SP kills the servercache.dat (and favorites)
	// show_bug.cgi?id=445
	/*
	  int size;
	  fileHandle_t fileIn;
	  cls.numglobalservers = cls.nummplayerservers = cls.numfavoriteservers = 0;
	  cls.numGlobalServerAddresses = 0;
	  if (FS_SV_FOpenFileRead("servercache.dat", &fileIn)) {
		  FS_Read(&cls.numglobalservers, sizeof(int), fileIn);
		  FS_Read(&cls.nummplayerservers, sizeof(int), fileIn);
		  FS_Read(&cls.numfavoriteservers, sizeof(int), fileIn);
		  FS_Read(&size, sizeof(int), fileIn);
		  if (size == sizeof(cls.globalServers) + sizeof(cls.favoriteServers) + sizeof(cls.mplayerServers)) {
			  FS_Read(&cls.globalServers, sizeof(cls.globalServers), fileIn);
			  FS_Read(&cls.mplayerServers, sizeof(cls.mplayerServers), fileIn);
			  FS_Read(&cls.favoriteServers, sizeof(cls.favoriteServers), fileIn);
		  } else {
			  cls.numglobalservers = cls.nummplayerservers = cls.numfavoriteservers = 0;
			  cls.numGlobalServerAddresses = 0;
		  }
		  FS_FCloseFile(fileIn);
	  }
	*/
}

/*
====================
LAN_SaveServersToCache
====================
*/
void LAN_SaveServersToCache() {
	// TTimo: stub, this is only relevant to MP, SP kills the servercache.dat (and favorites)
	// show_bug.cgi?id=445
	/*
	  int size;
	  fileHandle_t fileOut;
  #ifdef __MACOS__	//DAJ MacOS file typing
	  {
		  extern _MSL_IMP_EXP_C long _fcreator, _ftype;
		  _ftype = 'WlfB';
		  _fcreator = 'WlfS';
	  }
  #endif
	  fileOut = FS_SV_FOpenFileWrite("servercache.dat");
	  FS_Write(&cls.numglobalservers, sizeof(int), fileOut);
	  FS_Write(&cls.nummplayerservers, sizeof(int), fileOut);
	  FS_Write(&cls.numfavoriteservers, sizeof(int), fileOut);
	  size = sizeof(cls.globalServers) + sizeof(cls.favoriteServers) + sizeof(cls.mplayerServers);
	  FS_Write(&size, sizeof(int), fileOut);
	  FS_Write(&cls.globalServers, sizeof(cls.globalServers), fileOut);
	  FS_Write(&cls.mplayerServers, sizeof(cls.mplayerServers), fileOut);
	  FS_Write(&cls.favoriteServers, sizeof(cls.favoriteServers), fileOut);
	  FS_FCloseFile(fileOut);
	*/
}


/*
====================
LAN_ResetPings
====================
*/
static void LAN_ResetPings( int source ) {
	int count,i;
	serverInfo_t *servers = NULL;
	count = 0;

	switch ( source ) {
	case AS_LOCAL:
		servers = &cls.localServers[0];
		count = MAX_OTHER_SERVERS;
		break;
	case AS_MPLAYER:
		servers = &cls.mplayerServers[0];
		count = MAX_OTHER_SERVERS;
		break;
	case AS_GLOBAL:
		servers = &cls.globalServers[0];
		count = MAX_GLOBAL_SERVERS;
		break;
	case AS_FAVORITES:
		servers = &cls.favoriteServers[0];
		count = MAX_OTHER_SERVERS;
		break;
	}
	if ( servers ) {
		for ( i = 0; i < count; i++ ) {
			servers[i].ping = -1;
		}
	}
}

/*
====================
LAN_AddServer
====================
*/
static int LAN_AddServer( int source, const char *name, const char *address ) {
	int max, *count, i;
	netadr_t adr;
	serverInfo_t *servers = NULL;
	max = MAX_OTHER_SERVERS;
	count = 0;

	switch ( source ) {
	case AS_LOCAL:
		count = &cls.numlocalservers;
		servers = &cls.localServers[0];
		break;
	case AS_MPLAYER:
		count = &cls.nummplayerservers;
		servers = &cls.mplayerServers[0];
		break;
	case AS_GLOBAL:
		max = MAX_GLOBAL_SERVERS;
		count = &cls.numglobalservers;
		servers = &cls.globalServers[0];
		break;
	case AS_FAVORITES:
		count = &cls.numfavoriteservers;
		servers = &cls.favoriteServers[0];
		break;
	}
	if ( servers && *count < max ) {
		NET_StringToAdr( address, &adr );
		for ( i = 0; i < *count; i++ ) {
			if ( NET_CompareAdr( servers[i].adr, adr ) ) {
				break;
			}
		}
		if ( i >= *count ) {
			servers[*count].adr = adr;
			Q_strncpyz( servers[*count].hostName, name, sizeof( servers[*count].hostName ) );
			servers[*count].visible = qtrue;
			( *count )++;
			return 1;
		}
		return 0;
	}
	return -1;
}

/*
====================
LAN_RemoveServer
====================
*/
static void LAN_RemoveServer( int source, const char *addr ) {
	int *count, i;
	serverInfo_t *servers = NULL;
	count = 0;
	switch ( source ) {
	case AS_LOCAL:
		count = &cls.numlocalservers;
		servers = &cls.localServers[0];
		break;
	case AS_MPLAYER:
		count = &cls.nummplayerservers;
		servers = &cls.mplayerServers[0];
		break;
	case AS_GLOBAL:
		count = &cls.numglobalservers;
		servers = &cls.globalServers[0];
		break;
	case AS_FAVORITES:
		count = &cls.numfavoriteservers;
		servers = &cls.favoriteServers[0];
		break;
	}
	if ( servers ) {
		netadr_t comp;
		NET_StringToAdr( addr, &comp );
		for ( i = 0; i < *count; i++ ) {
			if ( NET_CompareAdr( comp, servers[i].adr ) ) {
				int j = i;
				while ( j < *count - 1 ) {
					Com_Memcpy( &servers[j], &servers[j + 1], sizeof( servers[j] ) );
					j++;
				}
				( *count )--;
				break;
			}
		}
	}
}


/*
====================
LAN_GetServerCount
====================
*/
static int LAN_GetServerCount( int source ) {
	switch ( source ) {
	case AS_LOCAL:
		return cls.numlocalservers;
		break;
	case AS_MPLAYER:
		return cls.nummplayerservers;
		break;
	case AS_GLOBAL:
		return cls.numglobalservers;
		break;
	case AS_FAVORITES:
		return cls.numfavoriteservers;
		break;
	}
	return 0;
}

/*
====================
LAN_GetLocalServerAddressString
====================
*/
static void LAN_GetServerAddressString( int source, int n, char *buf, int buflen ) {
	switch ( source ) {
	case AS_LOCAL:
		if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
			Q_strncpyz( buf, NET_AdrToString( cls.localServers[n].adr ), buflen );
			return;
		}
		break;
	case AS_MPLAYER:
		if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
			Q_strncpyz( buf, NET_AdrToString( cls.mplayerServers[n].adr ), buflen );
			return;
		}
		break;
	case AS_GLOBAL:
		if ( n >= 0 && n < MAX_GLOBAL_SERVERS ) {
			Q_strncpyz( buf, NET_AdrToString( cls.globalServers[n].adr ), buflen );
			return;
		}
		break;
	case AS_FAVORITES:
		if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
			Q_strncpyz( buf, NET_AdrToString( cls.favoriteServers[n].adr ), buflen );
			return;
		}
		break;
	}
	buf[0] = '\0';
}

/*
====================
LAN_GetServerInfo
====================
*/
static void LAN_GetServerInfo( int source, int n, char *buf, int buflen ) {
	char info[MAX_STRING_CHARS];
	serverInfo_t *server = NULL;
	info[0] = '\0';
	switch ( source ) {
	case AS_LOCAL:
		if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
			server = &cls.localServers[n];
		}
		break;
	case AS_MPLAYER:
		if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
			server = &cls.mplayerServers[n];
		}
		break;
	case AS_GLOBAL:
		if ( n >= 0 && n < MAX_GLOBAL_SERVERS ) {
			server = &cls.globalServers[n];
		}
		break;
	case AS_FAVORITES:
		if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
			server = &cls.favoriteServers[n];
		}
		break;
	}
	if ( server && buf ) {
		buf[0] = '\0';
		Info_SetValueForKey( info, "hostname", server->hostName );
		Info_SetValueForKey( info, "mapname", server->mapName );
		Info_SetValueForKey( info, "clients", va( "%i",server->clients ) );
		Info_SetValueForKey( info, "sv_maxclients", va( "%i",server->maxClients ) );
		Info_SetValueForKey( info, "ping", va( "%i",server->ping ) );
		Info_SetValueForKey( info, "minping", va( "%i",server->minPing ) );
		Info_SetValueForKey( info, "maxping", va( "%i",server->maxPing ) );
		Info_SetValueForKey( info, "game", server->game );
		Info_SetValueForKey( info, "gametype", va( "%i",server->gameType ) );
		Info_SetValueForKey( info, "nettype", va( "%i",server->netType ) );
		Info_SetValueForKey( info, "addr", NET_AdrToString( server->adr ) );
		Info_SetValueForKey( info, "sv_allowAnonymous", va( "%i", server->allowAnonymous ) );
		Q_strncpyz( buf, info, buflen );
	} else {
		if ( buf ) {
			buf[0] = '\0';
		}
	}
}

/*
====================
LAN_GetServerPing
====================
*/
static int LAN_GetServerPing( int source, int n ) {
	serverInfo_t *server = NULL;
	switch ( source ) {
	case AS_LOCAL:
		if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
			server = &cls.localServers[n];
		}
		break;
	case AS_MPLAYER:
		if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
			server = &cls.mplayerServers[n];
		}
		break;
	case AS_GLOBAL:
		if ( n >= 0 && n < MAX_GLOBAL_SERVERS ) {
			server = &cls.globalServers[n];
		}
		break;
	case AS_FAVORITES:
		if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
			server = &cls.favoriteServers[n];
		}
		break;
	}
	if ( server ) {
		return server->ping;
	}
	return -1;
}

/*
====================
LAN_GetServerPtr
====================
*/
static serverInfo_t *LAN_GetServerPtr( int source, int n ) {
	switch ( source ) {
	case AS_LOCAL:
		if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
			return &cls.localServers[n];
		}
		break;
	case AS_MPLAYER:
		if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
			return &cls.mplayerServers[n];
		}
		break;
	case AS_GLOBAL:
		if ( n >= 0 && n < MAX_GLOBAL_SERVERS ) {
			return &cls.globalServers[n];
		}
		break;
	case AS_FAVORITES:
		if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
			return &cls.favoriteServers[n];
		}
		break;
	}
	return NULL;
}

/*
====================
LAN_CompareServers
====================
*/
static int LAN_CompareServers( int source, int sortKey, int sortDir, int s1, int s2 ) {
	int res;
	serverInfo_t *server1, *server2;

	server1 = LAN_GetServerPtr( source, s1 );
	server2 = LAN_GetServerPtr( source, s2 );
	if ( !server1 || !server2 ) {
		return 0;
	}

	res = 0;
	switch ( sortKey ) {
	case SORT_HOST:
		res = Q_stricmp( server1->hostName, server2->hostName );
		break;

	case SORT_MAP:
		res = Q_stricmp( server1->mapName, server2->mapName );
		break;
	case SORT_CLIENTS:
		if ( server1->clients < server2->clients ) {
			res = -1;
		} else if ( server1->clients > server2->clients )     {
			res = 1;
		} else {
			res = 0;
		}
		break;
	case SORT_GAME:
		if ( server1->gameType < server2->gameType ) {
			res = -1;
		} else if ( server1->gameType > server2->gameType )     {
			res = 1;
		} else {
			res = 0;
		}
		break;
	case SORT_PING:
		if ( server1->ping < server2->ping ) {
			res = -1;
		} else if ( server1->ping > server2->ping )     {
			res = 1;
		} else {
			res = 0;
		}
		break;
	}

	if ( sortDir ) {
		if ( res < 0 ) {
			return 1;
		}
		if ( res > 0 ) {
			return -1;
		}
		return 0;
	}
	return res;
}

/*
====================
LAN_GetPingQueueCount
====================
*/
static int LAN_GetPingQueueCount( void ) {
	return ( CL_GetPingQueueCount() );
}

/*
====================
LAN_ClearPing
====================
*/
static void LAN_ClearPing( int n ) {
	CL_ClearPing( n );
}

/*
====================
LAN_GetPing
====================
*/
static void LAN_GetPing( int n, char *buf, int buflen, int *pingtime ) {
	CL_GetPing( n, buf, buflen, pingtime );
}

/*
====================
LAN_GetPingInfo
====================
*/
static void LAN_GetPingInfo( int n, char *buf, int buflen ) {
	CL_GetPingInfo( n, buf, buflen );
}

/*
====================
LAN_MarkServerVisible
====================
*/
static void LAN_MarkServerVisible( int source, int n, qboolean visible ) {
	if ( n == -1 ) {
		int count = MAX_OTHER_SERVERS;
		serverInfo_t *server = NULL;
		switch ( source ) {
		case AS_LOCAL:
			server = &cls.localServers[0];
			break;
		case AS_MPLAYER:
			server = &cls.mplayerServers[0];
			break;
		case AS_GLOBAL:
			server = &cls.globalServers[0];
			count = MAX_GLOBAL_SERVERS;
			break;
		case AS_FAVORITES:
			server = &cls.favoriteServers[0];
			break;
		}
		if ( server ) {
			for ( n = 0; n < count; n++ ) {
				server[n].visible = visible;
			}
		}

	} else {
		switch ( source ) {
		case AS_LOCAL:
			if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
				cls.localServers[n].visible = visible;
			}
			break;
		case AS_MPLAYER:
			if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
				cls.mplayerServers[n].visible = visible;
			}
			break;
		case AS_GLOBAL:
			if ( n >= 0 && n < MAX_GLOBAL_SERVERS ) {
				cls.globalServers[n].visible = visible;
			}
			break;
		case AS_FAVORITES:
			if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
				cls.favoriteServers[n].visible = visible;
			}
			break;
		}
	}
}


/*
=======================
LAN_ServerIsVisible
=======================
*/
static int LAN_ServerIsVisible( int source, int n ) {
	switch ( source ) {
	case AS_LOCAL:
		if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
			return cls.localServers[n].visible;
		}
		break;
	case AS_MPLAYER:
		if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
			return cls.mplayerServers[n].visible;
		}
		break;
	case AS_GLOBAL:
		if ( n >= 0 && n < MAX_GLOBAL_SERVERS ) {
			return cls.globalServers[n].visible;
		}
		break;
	case AS_FAVORITES:
		if ( n >= 0 && n < MAX_OTHER_SERVERS ) {
			return cls.favoriteServers[n].visible;
		}
		break;
	}
	return qfalse;
}

/*
=======================
LAN_UpdateVisiblePings
=======================
*/
qboolean LAN_UpdateVisiblePings( int source ) {
	return CL_UpdateVisiblePings_f( source );
}

/*
====================
LAN_GetServerStatus
====================
*/
int LAN_GetServerStatus( char *serverAddress, char *serverStatus, int maxLen ) {
	return CL_ServerStatus( serverAddress, serverStatus, maxLen );
}

/*
====================
CL_GetGlConfig
====================
*/
static void CL_GetGlconfig( glconfig_t *config ) {
	*config = cls.glconfig;
}

/*
====================
GetClipboardData
====================
*/
static void GetClipboardData( char *buf, int buflen ) {
	char    *cbd;

	cbd = Sys_GetClipboardData();

	if ( !cbd ) {
		*buf = 0;
		return;
	}

	Q_strncpyz( buf, cbd, buflen );

	Z_Free( cbd );
}

/*
====================
Key_KeynumToStringBuf
====================
*/
static void Key_KeynumToStringBuf( int keynum, char *buf, int buflen ) {
	Q_strncpyz( buf, Key_KeynumToString( keynum, qtrue ), buflen );
}

/*
====================
Key_GetBindingBuf
====================
*/
static void Key_GetBindingBuf( int keynum, char *buf, int buflen ) {
	char    *value;

	value = Key_GetBinding( keynum );
	if ( value ) {
		Q_strncpyz( buf, value, buflen );
	} else {
		*buf = 0;
	}
}

/*
====================
Key_GetCatcher
====================
*/
int Key_GetCatcher( void ) {
	return cls.keyCatchers;
}

/*
====================
Ket_SetCatcher
====================
*/
void Key_SetCatcher( int catcher ) {
	cls.keyCatchers = catcher;
}


/*
====================
CLUI_GetCDKey
====================
*/
static void CLUI_GetCDKey( char *buf, int buflen ) {
	cvar_t  *fs;
	fs = Cvar_Get( "fs_game", "", CVAR_INIT | CVAR_SYSTEMINFO );
	if ( UI_usesUniqueCDKey() && fs && fs->string[0] != 0 ) {
		memcpy( buf, &cl_cdkey[16], 16 );
		buf[16] = 0;
	} else {
		memcpy( buf, cl_cdkey, 16 );
		buf[16] = 0;
	}
}


/*
====================
CLUI_SetCDKey
====================
*/
static void CLUI_SetCDKey( char *buf ) {
	cvar_t  *fs;
	fs = Cvar_Get( "fs_game", "", CVAR_INIT | CVAR_SYSTEMINFO );
	if ( UI_usesUniqueCDKey() && fs && fs->string[0] != 0 ) {
		memcpy( &cl_cdkey[16], buf, 16 );
		cl_cdkey[32] = 0;
		// set the flag so the fle will be written at the next opportunity
		cvar_modifiedFlags |= CVAR_ARCHIVE;
	} else {
		memcpy( cl_cdkey, buf, 16 );
		// set the flag so the fle will be written at the next opportunity
		cvar_modifiedFlags |= CVAR_ARCHIVE;
	}
}


/*
====================
GetConfigString
====================
*/
static int GetConfigString( int index, char *buf, int size ) {
	int offset;

	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		return qfalse;
	}

	offset = cl.gameState.stringOffsets[index];
	if ( !offset ) {
		if ( size ) {
			buf[0] = 0;
		}
		return qfalse;
	}

	Q_strncpyz( buf, cl.gameState.stringData + offset, size );

	return qtrue;
}

static void UI_Print( const char * a1 ) {
	Com_Printf( "%s", a1 );
}

static void UI_Error( const char * a1 ) {
	Com_Error( ERR_DROP, "%s", a1 );
}

static int UI_Milliseconds(  ) {
	return Sys_Milliseconds();
}

static void UI_Cvar_Register( vmCvar_t * a1, const char * a2, const char * a3, int a4 ) {
	Cvar_Register((vmCvar_t *)( a1),(const char *)( a2),(const char *)( a3), a4 );
}

static void UI_Cvar_Update( vmCvar_t * a1 ) {
	Cvar_Update((vmCvar_t *)( a1 ));
}

static void UI_Cvar_Set( const char * a1, const char * a2 ) {
	Cvar_Set((const char *)( a1),(const char *)( a2 ));
}

static float UI_Cvar_VariableValue( const char * a1 ) {
	return Cvar_VariableValue((const char *)( a1 ) );
}

static void UI_Cvar_VariableStringBuffer( const char * a1, char * a2, int a3 ) {
	Cvar_VariableStringBuffer((const char *)( a1),(char *)( a2), a3 );
}

static void UI_Cvar_SetValue( const char * a1, float a2 ) {
	Cvar_SetValue((const char *)( a1), a2 );
}

static void UI_Cvar_Reset( const char * a1 ) {
	Cvar_Reset((const char *)( a1 ));
}

static void UI_Cvar_Create( const char * a1, const char * a2, int a3 ) {
	Cvar_Get((const char *)( a1),(const char *)( a2), a3 );
}

static void UI_Cvar_InfoStringBuffer( int a1, char * a2, int a3 ) {
	Cvar_InfoStringBuffer( a1,(char *)( a2), a3 );
}

static int UI_Argc(  ) {
	return Cmd_Argc();
}

static void UI_Argv( int a1, char * a2, int a3 ) {
	Cmd_ArgvBuffer( a1,(char *)( a2), a3 );
}

static void UI_Cmd_ExecuteText( int a1, const char * a2 ) {
	Cbuf_ExecuteText( a1,(const char *)( a2 ));
}

static int UI_FS_FOpenFile( const char * a1, fileHandle_t * a2, fsMode_t a3 ) {
	return FS_FOpenFileByMode((const char *)( a1),(fileHandle_t *)( a2),(fsMode_t)( a3 ));
}

static void UI_FS_Read( void * a1, int a2, fileHandle_t a3 ) {
	FS_Read( a1, a2, a3 );
}

static void UI_FS_Seek( fileHandle_t a1, long a2, int a3 ) {
	FS_Seek( a1, a2, a3 );
}

static void UI_FS_Write( const void * a1, int a2, fileHandle_t a3 ) {
	FS_Write( a1, a2, a3 );
}

static void UI_FS_FCloseFile( fileHandle_t a1 ) {
	FS_FCloseFile( a1 );
}

static int UI_FS_GetFileList( const char * a1, const char * a2, char * a3, int a4 ) {
	return FS_GetFileList((const char *)( a1),(const char *)( a2),(char *)( a3), a4 );
}

static int UI_FS_Delete( const char * a1 ) {
	return FS_Delete((char *)( a1 ));
}

static qhandle_t UI_R_RegisterModel( const char * a1 ) {
	return re.RegisterModel( (const char *)(a1) );
}

static qhandle_t UI_R_RegisterSkin( const char * a1 ) {
	return re.RegisterSkin( (const char *)(a1) );
}

static void UI_R_RegisterFont( const char * a1, int a2, fontInfo_t * a3 ) {
	re.RegisterFont( (const char *)(a1), a2, (fontInfo_t *)(a3) );
}

static qhandle_t UI_R_RegisterShaderNoMip( const char * a1 ) {
	return re.RegisterShaderNoMip( (const char *)(a1) );
}

static void UI_R_ClearScene(  ) {
	re.ClearScene();
}

static void UI_R_AddRefEntityToScene( const refEntity_t * a1 ) {
	re.AddRefEntityToScene( (const refEntity_t *)(a1) );
}

static void UI_R_AddPolyToScene( qhandle_t a1, int a2, const polyVert_t * a3 ) {
	re.AddPolyToScene( a1, a2, (const polyVert_t *)(a3) );
}

static void UI_R_AddLightToScene( const vec3_t a1, float a2, float a3, float a4, float a5, int a6 ) {
	re.AddLightToScene( (const vec_t *)(a1), a2, a3, a4, a5, a6 );
}

static void UI_R_AddCoronaToScene( const vec3_t a1, float a2, float a3, float a4, float a5, int a6, int a7 ) {
	re.AddCoronaToScene( (const vec_t *)(a1), a2, a3, a4, a5, a6, a7 );
}

static void UI_R_RenderScene( const refdef_t * a1 ) {
	re.RenderScene( (const refdef_t *)(a1) );
}

static void UI_R_SetColor( const float * a1 ) {
	re.SetColor( (const float *)(a1) );
}

static void UI_R_DrawStretchPic( float a1, float a2, float a3, float a4, float a5, float a6, float a7, float a8, qhandle_t a9 ) {
	re.DrawStretchPic( a1, a2, a3, a4, a5, a6, a7, a8, a9 );
}

static void UI_R_ModelBounds( clipHandle_t a1, vec3_t a2, vec3_t a3 ) {
	re.ModelBounds( a1, (vec_t *)(a2), (vec_t *)(a3) );
}

static void UI_UpdateScreen(  ) {
	SCR_UpdateScreen();
}

static int UI_CM_LerpTag( orientation_t * a1, const refEntity_t * a2, const char * a3, int a4 ) {
	return re.LerpTag( (orientation_t *)(a1), (const refEntity_t *)(a2), (const char *)(a3), a4 );
}

static void UI_S_StartLocalSound( sfxHandle_t a1, int a2 ) {
	S_StartLocalSound( a1, a2 );
}

static sfxHandle_t UI_S_RegisterSound( const char * a1 ) {
	#ifdef DOOMSOUND    ///// (SA) DOOMSOUND
	return S_RegisterSound( a1 );
	#else
	return S_RegisterSound((const char *)( a1), qfalse );
	#endif  ///// (SA) DOOMSOUND
}

static void UI_S_FadeBackgroundTrack( float a1, int a2, int a3 ) {
	S_FadeStreamingSound( a1, a2, a3 );
}

static void UI_S_FadeAllSound( float a1, int a2 ) {
	S_FadeAllSounds( a1, a2 );
}

static void UI_Key_KeynumToStringBuf( int a1, char * a2, int a3 ) {
	Key_KeynumToStringBuf( a1,(char *)( a2), a3 );
}

static void UI_Key_GetBindingBuf( int a1, char * a2, int a3 ) {
	Key_GetBindingBuf( a1,(char *)( a2), a3 );
}

static void UI_Key_SetBinding( int a1, const char * a2 ) {
	Key_SetBinding( a1,(const char *)( a2 ));
}

static qboolean UI_Key_IsDown( int a1 ) {
	return Key_IsDown( a1 );
}

static qboolean UI_Key_GetOverstrikeMode(  ) {
	return Key_GetOverstrikeMode();
}

static void UI_Key_SetOverstrikeMode( qboolean a1 ) {
	Key_SetOverstrikeMode( a1 );
}

static void UI_Key_ClearStates(  ) {
	Key_ClearStates();
}

static int UI_Key_GetCatcher(  ) {
	return Key_GetCatcher();
}

static void UI_Key_SetCatcher( int a1 ) {
	Key_SetCatcher( a1 );
}

static void UI_GetClipboardData( char * a1, int a2 ) {
	GetClipboardData((char *)( a1), a2 );
}

static void UI_GetClientState( uiClientState_t * a1 ) {
	GetClientState((uiClientState_t *)( a1 ));
}

static void UI_GetGlconfig( glconfig_t * a1 ) {
	CL_GetGlconfig((glconfig_t *)( a1 ));
}

static int UI_GetConfigString( int a1, char* a2, int a3 ) {
	return GetConfigString( a1,(char *)( a2), a3 );
}

static int UI_LAN_GetPingQueueCount(  ) {
	return LAN_GetPingQueueCount();
}

static void UI_LAN_ClearPing( int a1 ) {
	LAN_ClearPing( a1 );
}

static void UI_LAN_GetPing( int a1, char * a2, int a3, int * a4 ) {
	LAN_GetPing( a1,(char *)( a2), a3,(int *)( a4 ));
}

static void UI_LAN_GetPingInfo( int a1, char * a2, int a3 ) {
	LAN_GetPingInfo( a1,(char *)( a2), a3 );
}

static qboolean UI_LAN_UpdateVisiblePings( int a1 ) {
	return LAN_UpdateVisiblePings( a1 );
}

static int UI_LAN_GetServerCount( int a1 ) {
	return LAN_GetServerCount( a1 );
}

static int UI_LAN_CompareServers( int a1, int a2, int a3, int a4, int a5 ) {
	return LAN_CompareServers( a1, a2, a3, a4, a5 );
}

static void UI_LAN_GetServerAddressString( int a1, int a2, char * a3, int a4 ) {
	LAN_GetServerAddressString( a1, a2,(char *)( a3), a4 );
}

static void UI_LAN_GetServerInfo( int a1, int a2, char * a3, int a4 ) {
	LAN_GetServerInfo( a1, a2,(char *)( a3), a4 );
}

static int UI_LAN_AddServer( int a1, const char * a2, const char * a3 ) {
	return LAN_AddServer( a1,(const char *)( a2),(const char *)( a3 ));
}

static void UI_LAN_RemoveServer( int a1, const char * a2 ) {
	LAN_RemoveServer( a1,(const char *)( a2 ));
}

static int UI_LAN_GetServerPing( int a1, int a2 ) {
	return LAN_GetServerPing( a1, a2 );
}

static int UI_LAN_ServerIsVisible( int a1, int a2 ) {
	return LAN_ServerIsVisible( a1, a2 );
}

static int UI_LAN_ServerStatus( const char * a1, char * a2, int a3 ) {
	return LAN_GetServerStatus((char *)( a1),(char *)( a2), a3 );
}

static void UI_LAN_SaveCachedServers(  ) {
	LAN_SaveServersToCache();
}

static void UI_LAN_LoadCachedServers(  ) {
	LAN_LoadCachedServers();
}

static void UI_LAN_MarkServerVisible( int a1, int a2, qboolean a3 ) {
	LAN_MarkServerVisible( a1, a2, a3 );
}

static void UI_LAN_ResetPings( int a1 ) {
	LAN_ResetPings( a1 );
}

static int UI_MemoryRemaining(  ) {
	return Hunk_MemoryRemaining();
}

static void UI_GetCDKey( char * a1, int a2 ) {
	CLUI_GetCDKey((char *)( a1), a2 );
}

static void UI_SetCDKey( char * a1 ) {
	CLUI_SetCDKey((char *)( a1 ));
}

static int UI_PC_AddGlobalDefine( char * a1 ) {
	return botlib_export->PC_AddGlobalDefine( (char *)(a1) );
}

static int UI_PC_LoadSource( const char * a1 ) {
	return botlib_export->PC_LoadSourceHandle( (const char *)(a1) );
}

static int UI_PC_FreeSource( int a1 ) {
	return botlib_export->PC_FreeSourceHandle( a1 );
}

static int UI_PC_ReadToken( int a1, pc_token_t * a2 ) {
	return botlib_export->PC_ReadTokenHandle( a1, (pc_token_t *)(a2) );
}

static int UI_PC_SourceFileAndLine( int a1, char * a2, int * a3 ) {
	return botlib_export->PC_SourceFileAndLine( a1, (char *)(a2), (int *)(a3) );
}

static void UI_S_StopBackgroundTrack(  ) {
	S_StopBackgroundTrack();
}

static void UI_S_StartBackgroundTrack( const char * a1, const char * a2, int a3 ) {
	S_StartBackgroundTrack((const char *)( a1),(const char *)( a2), a3 );   //----(SA)	added fadeup time
}

static int UI_RealTime( qtime_t * a1 ) {
	return Com_RealTime((qtime_t *)( a1 ));
}

static int UI_CIN_PlayCinematic( const char * a1, int a2, int a3, int a4, int a5, int a6 ) {
	Com_DPrintf( "UI_CIN_PlayCinematic\n" );
	return CIN_PlayCinematic((const char *)( a1), a2, a3, a4, a5, a6 );
}

static e_status UI_CIN_StopCinematic( int a1 ) {
	return CIN_StopCinematic( a1 );
}

static e_status UI_CIN_RunCinematic( int a1 ) {
	return CIN_RunCinematic( a1 );
}

static void UI_CIN_DrawCinematic( int a1 ) {
	CIN_DrawCinematic( a1 );
}

static void UI_CIN_SetExtents( int a1, int a2, int a3, int a4, int a5 ) {
	CIN_SetExtents( a1, a2, a3, a4, a5 );
}

static void UI_R_RemapShader( const char * a1, const char * a2, const char * a3 ) {
	re.RemapShader( (const char *)(a1), (const char *)(a2), (const char *)(a3) );
}

static qboolean UI_VerifyCDKey( const char * a1, const char * a2 ) {
	return CL_CDKeyValidate((const char *)( a1),(const char *)( a2 ));
}

static qboolean UI_GetLimboString( int a1, char * a2 ) {
	return CL_GetLimboString( a1,(char *)( a2 ));
}

static const uiImport_t uiImportTable = {
	UI_API_VERSION,
	UI_Print,
	UI_Error,
	UI_Milliseconds,
	UI_Cvar_Register,
	UI_Cvar_Update,
	UI_Cvar_Set,
	UI_Cvar_VariableValue,
	UI_Cvar_VariableStringBuffer,
	UI_Cvar_SetValue,
	UI_Cvar_Reset,
	UI_Cvar_Create,
	UI_Cvar_InfoStringBuffer,
	UI_Argc,
	UI_Argv,
	UI_Cmd_ExecuteText,
	UI_FS_FOpenFile,
	UI_FS_Read,
	UI_FS_Seek,
	UI_FS_Write,
	UI_FS_FCloseFile,
	UI_FS_GetFileList,
	UI_FS_Delete,
	UI_R_RegisterModel,
	UI_R_RegisterSkin,
	UI_R_RegisterFont,
	UI_R_RegisterShaderNoMip,
	UI_R_ClearScene,
	UI_R_AddRefEntityToScene,
	UI_R_AddPolyToScene,
	UI_R_AddLightToScene,
	UI_R_AddCoronaToScene,
	UI_R_RenderScene,
	UI_R_SetColor,
	UI_R_DrawStretchPic,
	UI_R_ModelBounds,
	UI_UpdateScreen,
	UI_CM_LerpTag,
	UI_S_StartLocalSound,
	UI_S_RegisterSound,
	UI_S_FadeBackgroundTrack,
	UI_S_FadeAllSound,
	UI_Key_KeynumToStringBuf,
	UI_Key_GetBindingBuf,
	UI_Key_SetBinding,
	UI_Key_IsDown,
	UI_Key_GetOverstrikeMode,
	UI_Key_SetOverstrikeMode,
	UI_Key_ClearStates,
	UI_Key_GetCatcher,
	UI_Key_SetCatcher,
	UI_GetClipboardData,
	UI_GetClientState,
	UI_GetGlconfig,
	UI_GetConfigString,
	UI_LAN_GetPingQueueCount,
	UI_LAN_ClearPing,
	UI_LAN_GetPing,
	UI_LAN_GetPingInfo,
	UI_LAN_UpdateVisiblePings,
	UI_LAN_GetServerCount,
	UI_LAN_CompareServers,
	UI_LAN_GetServerAddressString,
	UI_LAN_GetServerInfo,
	UI_LAN_AddServer,
	UI_LAN_RemoveServer,
	UI_LAN_GetServerPing,
	UI_LAN_ServerIsVisible,
	UI_LAN_ServerStatus,
	UI_LAN_SaveCachedServers,
	UI_LAN_LoadCachedServers,
	UI_LAN_MarkServerVisible,
	UI_LAN_ResetPings,
	UI_MemoryRemaining,
	UI_GetCDKey,
	UI_SetCDKey,
	UI_PC_AddGlobalDefine,
	UI_PC_LoadSource,
	UI_PC_FreeSource,
	UI_PC_ReadToken,
	UI_PC_SourceFileAndLine,
	UI_S_StopBackgroundTrack,
	UI_S_StartBackgroundTrack,
	UI_RealTime,
	UI_CIN_PlayCinematic,
	UI_CIN_StopCinematic,
	UI_CIN_RunCinematic,
	UI_CIN_DrawCinematic,
	UI_CIN_SetExtents,
	UI_R_RemapShader,
	UI_VerifyCDKey,
	UI_GetLimboString,
};

/*
====================
CL_ShutdownUI
====================
*/
void CL_ShutdownUI( void ) {
	cls.keyCatchers &= ~KEYCATCH_UI;
	cls.uiStarted = qfalse;
	if ( !uiExport ) {
		return;
	}
	uiExport->Shutdown();
	Sys_UnloadDll( uiDllHandle );
	uiDllHandle = NULL;
	uiExport = NULL;
}

/*
====================
CL_InitUI
====================
*/

void CL_InitUI( void ) {
	// load the ui DLL
	uiExport = (uiExport_t *)Sys_LoadDll( "ui", &uiDllHandle, "GetUIAPI", (void *)&uiImportTable );
	if ( !uiExport ) {
		Com_Error( ERR_FATAL, "Sys_LoadDll on UI failed" );
	}

	// sanity check
	if ( uiExport->apiVersion != UI_API_VERSION ) {
		Com_Error( ERR_FATAL, "User Interface is version %d, expected %d", uiExport->apiVersion, UI_API_VERSION );
		cls.uiStarted = qfalse;
	}

	// init for this gamestate
	uiExport->Init( ( cls.state >= CA_AUTHORIZING && cls.state < CA_ACTIVE ) );
}


qboolean UI_usesUniqueCDKey() {
	if ( uiExport ) {
		return ( uiExport->HasUniqueCDKey() == qtrue );
	} else {
		return qfalse;
	}
}

/*
====================
UI_GameCommand

See if the current console command is claimed by the ui
====================
*/
qboolean UI_GameCommand( void ) {
	if ( !uiExport ) {
		return qfalse;
	}

	return uiExport->ConsoleCommand( cls.realtime );
}
