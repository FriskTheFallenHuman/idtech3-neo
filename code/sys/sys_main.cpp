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

// sys_main.cpp: the engine entry point and the OS-level helpers, on SDL3.
// SDL provides the entry point through SDL_main.h, the process launching,
// the URL opening, the clipboard, the directory scanning and the DLL
// loading. The Win32 console window is reached through the Win_* bridges
// declared in sys_local.h.

#include "../client/client.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

#include <SDL3/SDL_main.h>

#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// less than this much RAM means the low physical memory mode kicks in
#define MEM_THRESHOLD_MB 96

static char sys_cmdline[MAX_STRING_CHARS];

/*
==================
Sys_LowPhysicalMemory()
==================
*/
qboolean Sys_LowPhysicalMemory() {
	// SDL reports the RAM size in megabytes
	return ( SDL_GetSystemRAM() <= MEM_THRESHOLD_MB ) ? qtrue : qfalse;
}

//----(SA)	from from NERVE MP codebase (10/15/01)  (checkins at time of this file should be related)
/*
==================
Sys_StartProcess
==================
*/
void Sys_StartProcess( char *exeName, qboolean doexit ) {           // NERVE - SMF
	char cmd[MAX_OSPATH];
	SDL_Process *process;
	const char *args[2];

	Com_sprintf( cmd, sizeof( cmd ), "%s\\%s", Sys_Cwd(), exeName );

	args[0] = cmd;
	args[1] = NULL;
	process = SDL_CreateProcess( args, false );
	if ( !process ) {
		// couldn't start it, popup error box
		Com_Error( ERR_DROP, "Could not start process: '%s' ", cmd );
		return;
	}

	// the handle is not needed, the child keeps running on its own
	SDL_DestroyProcess( process );

	// TTimo: similar way of exiting as used in Sys_OpenURL below
	if ( doexit ) {
		Cbuf_ExecuteText( EXEC_APPEND, "quit" );
	}
}

/*
==================
Sys_OpenURL
==================
*/
void Sys_OpenURL( char *url, qboolean doexit ) {                // NERVE - SMF
	if ( !SDL_OpenURL( url ) ) {
		// couldn't start it, popup error box
		Com_Error( ERR_DROP, "Could not open url: '%s' ", url );
		return;
	}

	if ( doexit ) {
		Cbuf_ExecuteText( EXEC_APPEND, "quit" );
	}
}
//----(SA)	end

/*
==================
Sys_BeginProfiling
==================
*/
void Sys_BeginProfiling( void ) {
	// this is just used on the mac build
}

/*
=============
Sys_Error

Show the early console as an error dialog
=============
*/
void QDECL Sys_Error( const char *error, ... ) {
	va_list argptr;
	char text[4096];

	va_start( argptr, error );
	vsprintf( text, error, argptr );
	va_end( argptr );

	Conbuf_AppendText( text );
	Conbuf_AppendText( "\n" );

	Sys_SetErrorText( text );
	Sys_ShowConsole( 1, qtrue );

	Win_SetTimerResolution( qfalse );

	IN_Shutdown();

	// wait for the user to quit
	while ( 1 ) {
		if ( !Win_PumpConsoleMessages() ) {
			Com_Quit_f();
		}
	}

	Sys_DestroyConsole();

	exit( 1 );
}

/*
==============
Sys_Quit
==============
*/
void Sys_Quit( void ) {
	Win_SetTimerResolution( qfalse );
	IN_Shutdown();
	Sys_DestroyConsole();

	exit( 0 );
}

/*
==============
Sys_Print
==============
*/
void Sys_Print( const char *msg ) {
	Conbuf_AppendText( msg );
}


/*
==============
Sys_Mkdir
==============
*/
void Sys_Mkdir( const char *path ) {
	SDL_CreateDirectory( path );
}

/*
==============
Sys_Cwd
==============
*/
char *Sys_Cwd( void ) {
	static char cwd[MAX_OSPATH];
	char *s;
	int length;

	s = SDL_GetCurrentDirectory();
	if ( !s ) {
		cwd[0] = 0;
		return cwd;
	}

	// SDL returns the directory with a trailing separator
	Q_strncpyz( cwd, s, sizeof( cwd ) );
	SDL_free( s );

	length = strlen( cwd );
	while ( length > 0 && ( cwd[length - 1] == '/' || cwd[length - 1] == '\\' ) ) {
		cwd[--length] = 0;
	}

	return cwd;
}

/*
==============
Sys_DefaultCDPath
==============
*/
char *Sys_DefaultCDPath( void ) {
	return "";
}

/*
==============
Sys_DefaultBasePath
==============
*/
char *Sys_DefaultBasePath( void ) {
	return Sys_Cwd();
}

/*
==============================================================

DIRECTORY SCANNING

==============================================================
*/

#define MAX_FOUND_FILES 0x1000

// the filtered scan recurses into subdirectories through this function
void Sys_ListFilteredFiles( const char *basedir, const char *subdirs, const char *filter, char **list, int *numfiles );

// state shared between Sys_ListFilteredFiles and its enumeration callback
typedef struct {
	const char *basedir;
	const char *subdirs;
	const char *filter;
	char **list;
	int *numfiles;
} filterScan_t;

/*
==================
FilteredFilesCallback

One directory entry during a filtered scan: recurse into
subdirectories and add everything that matches the filter, the same
walk the old _findfirst code did.
==================
*/
static SDL_EnumerationResult SDLCALL FilteredFilesCallback( void *userdata, const char *dirname, const char *fname ) {
	filterScan_t *scan = (filterScan_t *)userdata;
	char filename[MAX_OSPATH];
	char newsubdirs[MAX_OSPATH];
	char path[MAX_OSPATH];
	SDL_PathInfo info;

	if ( !Q_stricmp( fname, "." ) || !Q_stricmp( fname, ".." ) ) {
		return SDL_ENUM_CONTINUE;
	}

	if ( *scan->numfiles >= MAX_FOUND_FILES - 1 ) {
		return SDL_ENUM_FAILURE;
	}

	// full OS path of the entry, for the directory test below
	Com_sprintf( path, sizeof( path ), "%s%s", dirname, fname );
	if ( !SDL_GetPathInfo( path, &info ) ) {
		return SDL_ENUM_CONTINUE;
	}

	if ( info.type == SDL_PATHTYPE_DIRECTORY ) {
		if ( scan->subdirs[0] ) {
			Com_sprintf( newsubdirs, sizeof( newsubdirs ), "%s\\%s", scan->subdirs, fname );
		} else {
			Com_sprintf( newsubdirs, sizeof( newsubdirs ), "%s", fname );
		}
		Sys_ListFilteredFiles( scan->basedir, newsubdirs, scan->filter, scan->list, scan->numfiles );

		if ( *scan->numfiles >= MAX_FOUND_FILES - 1 ) {
			return SDL_ENUM_FAILURE;
		}
	}

	// keep the same entry naming as the old code, including the leading
	// separator when scanning the base directory itself
	Com_sprintf( filename, sizeof( filename ), "%s\\%s", scan->subdirs, fname );
	if ( !Com_FilterPath( (char *)scan->filter, filename, qfalse ) ) {
		return SDL_ENUM_CONTINUE;
	}

	scan->list[ *scan->numfiles ] = CopyString( filename );
	( *scan->numfiles )++;

	return SDL_ENUM_CONTINUE;
}

void Sys_ListFilteredFiles( const char *basedir, const char *subdirs, const char *filter, char **list, int *numfiles ) {
	char search[MAX_OSPATH];
	filterScan_t scan;

	if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
		return;
	}

	if ( strlen( subdirs ) ) {
		Com_sprintf( search, sizeof( search ), "%s\\%s", basedir, subdirs );
	} else {
		Com_sprintf( search, sizeof( search ), "%s", basedir );
	}

	scan.basedir = basedir;
	scan.subdirs = subdirs;
	scan.filter = filter;
	scan.list = list;
	scan.numfiles = numfiles;

	SDL_EnumerateDirectory( search, FilteredFilesCallback, &scan );
}

static qboolean strgtr( const char *s0, const char *s1 ) {
	int l0, l1, i;

	l0 = strlen( s0 );
	l1 = strlen( s1 );

	if ( l1 < l0 ) {
		l0 = l1;
	}

	for ( i = 0; i < l0; i++ ) {
		if ( s1[i] > s0[i] ) {
			return qtrue;
		}
		if ( s1[i] < s0[i] ) {
			return qfalse;
		}
	}
	return qfalse;
}

// state shared between Sys_ListFiles and its enumeration callback
typedef struct {
	char **list;
	int nfiles;
	const char *extension;
	qboolean dirsOnly;
	qboolean wantsubs;
} listFilesScan_t;

/*
==================
ListFilesCallback

One directory entry during a plain listing.
==================
*/
static SDL_EnumerationResult SDLCALL ListFilesCallback( void *userdata, const char *dirname, const char *fname ) {
	listFilesScan_t *scan = (listFilesScan_t *)userdata;
	char path[MAX_OSPATH];
	SDL_PathInfo info;
	qboolean isDir;
	qboolean want;
	int nameLen, extLen;

	if ( !Q_stricmp( fname, "." ) || !Q_stricmp( fname, ".." ) ) {
		return SDL_ENUM_CONTINUE;
	}

	if ( scan->nfiles == MAX_FOUND_FILES - 1 ) {
		return SDL_ENUM_FAILURE;
	}

	Com_sprintf( path, sizeof( path ), "%s%s", dirname, fname );
	if ( !SDL_GetPathInfo( path, &info ) ) {
		return SDL_ENUM_CONTINUE;
	}
	isDir = ( info.type == SDL_PATHTYPE_DIRECTORY );

	// passing a slash as extension finds directories, otherwise
	// wantsubs picks files or directories
	if ( scan->dirsOnly ) {
		want = isDir;
	} else {
		want = scan->wantsubs ? isDir : !isDir;
	}
	if ( !want ) {
		return SDL_ENUM_CONTINUE;
	}

	// match the trailing extension the way the old "*ext" search did
	if ( scan->extension[0] ) {
		nameLen = strlen( fname );
		extLen = strlen( scan->extension );
		if ( nameLen < extLen || Q_stricmp( fname + nameLen - extLen, scan->extension ) ) {
			return SDL_ENUM_CONTINUE;
		}
	}

	scan->list[ scan->nfiles ] = CopyString( fname );
	scan->nfiles++;

	return SDL_ENUM_CONTINUE;
}

char **Sys_ListFiles( const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs ) {
	int nfiles;
	char        **listCopy;
	char        *list[MAX_FOUND_FILES];
	listFilesScan_t scan;
	int i;

	if ( filter ) {

		nfiles = 0;
		Sys_ListFilteredFiles( directory, "", filter, list, &nfiles );

		list[ nfiles ] = 0;
		*numfiles = nfiles;

		if ( !nfiles ) {
			return NULL;
		}

		listCopy = (char **)Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
		for ( i = 0 ; i < nfiles ; i++ ) {
			listCopy[i] = list[i];
		}
		listCopy[i] = NULL;

		return listCopy;
	}

	if ( !extension ) {
		extension = "";
	}

	// passing a slash as extension will find directories
	if ( extension[0] == '/' && extension[1] == 0 ) {
		extension = "";
		scan.dirsOnly = qtrue;
	} else {
		scan.dirsOnly = qfalse;
	}

	// search
	nfiles = 0;

	scan.list = list;
	scan.nfiles = 0;
	scan.extension = extension;
	scan.wantsubs = wantsubs;

	SDL_EnumerateDirectory( directory, ListFilesCallback, &scan );
	nfiles = scan.nfiles;

	list[ nfiles ] = 0;

	// return a copy of the list
	*numfiles = nfiles;

	if ( !nfiles ) {
		return NULL;
	}

	listCopy = (char **)Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
	for ( i = 0 ; i < nfiles ; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	do {
		int flag = 0;

		for ( i = 1; i < nfiles; i++ ) {
			if ( strgtr( listCopy[i - 1], listCopy[i] ) ) {
				char *temp = listCopy[i];
				listCopy[i] = listCopy[i - 1];
				listCopy[i - 1] = temp;
				flag = 1;
			}
		}
		if ( !flag ) {
			break;
		}
	} while ( 1 );

	return listCopy;
}

void    Sys_FreeFileList( char **list ) {
	int i;

	if ( !list ) {
		return;
	}

	for ( i = 0 ; list[i] ; i++ ) {
		Z_Free( list[i] );
	}

	Z_Free( list );
}

//========================================================


/*
================
Sys_CheckCD

Return true if the proper CD is in the drive. The CD scan itself is
dead, nothing can ship on a CD any more.
================
*/
qboolean    Sys_CheckCD( void ) {
	// FIXME: mission pack
	return qtrue;
}


/*
================
Sys_GetClipboardData

================
*/
char *Sys_GetClipboardData( void ) {
	char *data = NULL;
	char *cliptext;

	cliptext = SDL_GetClipboardText();
	if ( cliptext && cliptext[0] ) {
		data = (char *)Z_Malloc( strlen( cliptext ) + 1 );
		Q_strncpyz( data, cliptext, strlen( cliptext ) + 1 );
		strtok( data, "\n\r\b" );
	}

	if ( cliptext ) {
		SDL_free( cliptext );
	}

	return data;
}


/*
========================================================================

LOAD/UNLOAD DLL

========================================================================
*/

/*
=================
Sys_UnloadDll

=================
*/
void Sys_UnloadDll( void *dllHandle ) {
	if ( !dllHandle ) {
		return;
	}
	SDL_UnloadObject( (SDL_SharedObject *)dllHandle );
}

/*
=================
Sys_LoadDll

Used to load a game module DLL.
The entry point is looked up by name and called with the import table;
it returns the module's export table.
=================
*/
extern char *FS_BuildOSPath( const char *base, const char *game, const char *qpath );

void * QDECL Sys_LoadDll( const char *name, void **dllHandle, const char *entryName, void *import ) {
	static int lastWarning = 0;
	SDL_SharedObject *libHandle;
	void * ( QDECL * entry )( void * );
	char    *basepath;
	char    *cdpath;
	char    *gamedir;
	char    *fn;
	char filename[MAX_QPATH];
#ifdef NDEBUG
	int timestamp;
	int buttonID;
#endif

#ifdef WOLF_SP_DEMO
	Com_sprintf( filename, sizeof( filename ), "%sx86_d.dll", name );
#else
	Com_sprintf( filename, sizeof( filename ), "%sx86.dll", name );
#endif


#ifdef NDEBUG
	timestamp = Sys_Milliseconds();
	if ( ( ( timestamp - lastWarning ) > ( 5 * 60000 ) ) && !Cvar_VariableIntegerValue( "dedicated" )
		 && !Cvar_VariableIntegerValue( "com_blindlyLoadDLLs" ) ) {
		if ( FS_FileExists( filename ) ) {
			// Enter and Escape both pick Cancel, like the old MB_DEFBUTTON2 box
			const SDL_MessageBoxButtonData buttons[] = {
				{ 0, 0, "OK" },
				{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT |
				  SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Cancel" },
			};
			const SDL_MessageBoxData messageBox = {
				SDL_MESSAGEBOX_WARNING,       // SDL_MessageBoxFlags flags
				NULL,                         // SDL_Window *window
				"Security Warning",           // const char *title
				"You are about to load a .DLL executable that\n"
				"has not been verified for use with Wolfenstein.\n"
				"This type of file can compromise the security of\n"
				"your computer.\n\n"
				"Select 'OK' if you choose to load it anyway.",  // const char *message
				SDL_arraysize( buttons ),     // int numbuttons
				buttons,                      // const SDL_MessageBoxButtonData *buttons
				NULL                          // SDL_MessageBoxColorScheme *colorScheme
			};

			lastWarning = timestamp;
			buttonID = 0;
			SDL_ShowMessageBox( &messageBox, &buttonID );
			if ( buttonID != 0 ) {
				return NULL;
			}
		}
	}
#endif

	// check current folder only if we are a developer
	if ( 1 ) { //----(SA)	always dll
//	if (Cvar_VariableIntegerValue( "devdll" )) {
		libHandle = SDL_LoadObject( filename );
		if ( libHandle ) {
			goto found_dll;
		}
	}

	// the build puts the modules into bin/base, next to the engine
	{
		const char *exeDir = SDL_GetBasePath();

		if ( exeDir ) {
			char exePath[MAX_OSPATH];

			Com_sprintf( exePath, sizeof( exePath ), "%sbase\\%s", exeDir, filename );
			SDL_free( (void *)exeDir );
			libHandle = SDL_LoadObject( exePath );
			if ( libHandle ) {
				goto found_dll;
			}
		}
	}

	basepath = Cvar_VariableString( "fs_basepath" );
	cdpath = Cvar_VariableString( "fs_cdpath" );
	gamedir = Cvar_VariableString( "fs_game" );

	fn = FS_BuildOSPath( basepath, gamedir, filename );
	libHandle = SDL_LoadObject( fn );

	if ( !libHandle ) {
		if ( cdpath[0] ) {
			fn = FS_BuildOSPath( cdpath, gamedir, filename );
			libHandle = SDL_LoadObject( fn );
		}

		if ( !libHandle ) {
			return NULL;
		}
	}

found_dll:

	entry = ( void * ( QDECL * )( void * ) )SDL_LoadFunction( libHandle, entryName );
	if ( !entry ) {
		SDL_UnloadObject( libHandle );
		return NULL;
	}

	*dllHandle = libHandle;
	return entry( import );
}


/*
========================================================================

BACKGROUND FILE STREAMING

========================================================================
*/

void Sys_InitStreamThread( void ) {
}

void Sys_ShutdownStreamThread( void ) {
}

void Sys_BeginStreamedFile( fileHandle_t f, int readAhead ) {
}

void Sys_EndStreamedFile( fileHandle_t f ) {
}

int Sys_StreamedRead( void *buffer, int size, int count, fileHandle_t f ) {
	return FS_Read( buffer, size * count, f );
}

void Sys_StreamSeek( fileHandle_t f, int offset, int origin ) {
	FS_Seek( f, offset, origin );
}


void *Sys_InitializeCriticalSection() {
	return (void*)-1;
}

void Sys_EnterCriticalSection( void *ptr ) {
}

void Sys_LeaveCriticalSection( void *ptr ) {
}

/*
========================================================================

EVENT LOOP

========================================================================
*/

#define MAX_QUED_EVENTS     256
#define MASK_QUED_EVENTS    ( MAX_QUED_EVENTS - 1 )

sysEvent_t eventQue[MAX_QUED_EVENTS];
int eventHead, eventTail;
byte sys_packetReceived[MAX_MSGLEN];

/*
================
Sys_QueEvent

A time of 0 will get the current time
Ptr should either be null, or point to a block of data that can
be freed by the game later.
================
*/
void Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr ) {
	sysEvent_t  *ev;

	ev = &eventQue[ eventHead & MASK_QUED_EVENTS ];
	if ( eventHead - eventTail >= MAX_QUED_EVENTS ) {
		Com_Printf( "Sys_QueEvent: overflow\n" );
		// we are discarding an event, but don't leak memory
		if ( ev->evPtr ) {
			Z_Free( ev->evPtr );
		}
		eventTail++;
	}

	eventHead++;

	if ( time == 0 ) {
		time = Sys_Milliseconds();
	}

	ev->evTime = time;
	ev->evType = type;
	ev->evValue = value;
	ev->evValue2 = value2;
	ev->evPtrLength = ptrLength;
	ev->evPtr = ptr;
}

/*
================
Sys_GetEvent
================
*/
sysEvent_t Sys_GetEvent( void ) {
	sysEvent_t ev;
	char        *s;
	msg_t netmsg;
	netadr_t adr;

	// return if we have data
	if ( eventHead > eventTail ) {
		eventTail++;
		return eventQue[ ( eventTail - 1 ) & MASK_QUED_EVENTS ];
	}

	// pump the SDL event loop
	Sys_EventLoop();

	// check for console commands
	s = Sys_ConsoleInput();
	if ( s ) {
		char    *b;
		int len;

		len = strlen( s ) + 1;
		b = (char *)Z_Malloc( len );
		Q_strncpyz( b, s, len - 1 );
		Sys_QueEvent( 0, SE_CONSOLE, 0, 0, len, b );
	}

	// check for network packets
	MSG_Init( &netmsg, sys_packetReceived, sizeof( sys_packetReceived ) );
	if ( Sys_GetPacket( &adr, &netmsg ) ) {
		netadr_t        *buf;
		int len;

		// copy out to a seperate buffer for qeueing
		// the readcount stepahead is for SOCKS support
		len = sizeof( netadr_t ) + netmsg.cursize - netmsg.readcount;
		buf = (netadr_t *)Z_Malloc( len );
		*buf = adr;
		memcpy( buf + 1, &netmsg.data[netmsg.readcount], netmsg.cursize - netmsg.readcount );
		Sys_QueEvent( 0, SE_PACKET, 0, 0, len, buf );
	}

	// return if we have data
	if ( eventHead > eventTail ) {
		eventTail++;
		return eventQue[ ( eventTail - 1 ) & MASK_QUED_EVENTS ];
	}

	// create an empty event to return

	memset( &ev, 0, sizeof( ev ) );
	ev.evTime = Sys_Milliseconds();

	return ev;
}

//================================================================

/*
=================
Sys_In_Restart_f

Restart the input subsystem
=================
*/
void Sys_In_Restart_f( void ) {
	IN_Shutdown();
	IN_Init();
}


/*
=================
Sys_Net_Restart_f

Restart the network subsystem
=================
*/
void Sys_Net_Restart_f( void ) {
	NET_Restart();
}


/*
================
Sys_Init

Called after the common systems (cvars, files, etc)
are initialized
================
*/
void Sys_Init( void ) {
	int cpuid;

	// make sure the timer is high precision, otherwise
	// NT gets 18ms resolution
	Win_SetTimerResolution( qtrue );

	Cmd_AddCommand( "in_restart", Sys_In_Restart_f );
	Cmd_AddCommand( "net_restart", Sys_Net_Restart_f );

	Cvar_Set( "arch", "winnt" );

	//
	// figure out our CPU
	//
	Cvar_Get( "sys_cpustring", "detect", 0 );
	if ( !Q_stricmp( Cvar_VariableString( "sys_cpustring" ), "detect" ) ) {
		Com_Printf( "...detecting CPU, found " );

		cpuid = Sys_GetProcessorId();

		switch ( cpuid )
		{
		case CPUID_GENERIC:
			Cvar_Set( "sys_cpustring", "generic" );
			break;
		case CPUID_INTEL_UNSUPPORTED:
			Cvar_Set( "sys_cpustring", "x86 (pre-Pentium)" );
			break;
		case CPUID_INTEL_PENTIUM:
			Cvar_Set( "sys_cpustring", "x86 (P5/PPro, non-MMX)" );
			break;
		case CPUID_INTEL_MMX:
			Cvar_Set( "sys_cpustring", "x86 (P5/Pentium2, MMX)" );
			break;
		case CPUID_INTEL_KATMAI:
			Cvar_Set( "sys_cpustring", "Intel Pentium III" );
			break;
		case CPUID_AMD_3DNOW:
			Cvar_Set( "sys_cpustring", "AMD w/ 3DNow!" );
			break;
		case CPUID_AXP:
			Cvar_Set( "sys_cpustring", "Alpha AXP" );
			break;
		default:
			Com_Error( ERR_FATAL, "Unknown cpu type %d\n", cpuid );
			break;
		}
	} else
	{
		Com_Printf( "...forcing CPU type to " );
		if ( !Q_stricmp( Cvar_VariableString( "sys_cpustring" ), "generic" ) ) {
			cpuid = CPUID_GENERIC;
		} else if ( !Q_stricmp( Cvar_VariableString( "sys_cpustring" ), "x87" ) )     {
			cpuid = CPUID_INTEL_PENTIUM;
		} else if ( !Q_stricmp( Cvar_VariableString( "sys_cpustring" ), "mmx" ) )     {
			cpuid = CPUID_INTEL_MMX;
		} else if ( !Q_stricmp( Cvar_VariableString( "sys_cpustring" ), "3dnow" ) )     {
			cpuid = CPUID_AMD_3DNOW;
		} else if ( !Q_stricmp( Cvar_VariableString( "sys_cpustring" ), "PentiumIII" ) )     {
			cpuid = CPUID_INTEL_KATMAI;
		} else if ( !Q_stricmp( Cvar_VariableString( "sys_cpustring" ), "axp" ) )     {
			cpuid = CPUID_AXP;
		} else
		{
			Com_Printf( "WARNING: unknown sys_cpustring '%s'\n", Cvar_VariableString( "sys_cpustring" ) );
			cpuid = CPUID_GENERIC;
		}
	}
	Cvar_SetValue( "sys_cpuid", cpuid );
	Com_Printf( "%s\n", Cvar_VariableString( "sys_cpustring" ) );

	Cvar_Set( "username", Sys_GetCurrentUser() );

	IN_Init();      // FIXME: not in dedicated?
}


//=======================================================================

int totalMsec, countMsec;

/*
==================
main

SDL provides the entry point: SDL_main.h renames main to SDL_main and
the WinMain inside SDL3.dll parses the command line and calls it.
==================
*/
int main( int argc, char *argv[] ) {
	int startTime, endTime;
	int i;

	// rebuild the command line string Com_Init expects
	sys_cmdline[0] = 0;
	for ( i = 1; i < argc; i++ ) {
		if ( i > 1 ) {
			Q_strcat( sys_cmdline, sizeof( sys_cmdline ), " " );
		}
		Q_strcat( sys_cmdline, sizeof( sys_cmdline ), argv[i] );
	}

	// done before Com/Sys_Init since we need this for error output
	Sys_CreateConsole();

	// no abort/retry/fail errors
	Win_SetProcessErrorMode();

	// SDL owns the window, the GL context and the input from here on
	if ( !SDL_Init( SDL_INIT_VIDEO | SDL_INIT_GAMEPAD ) ) {
		Sys_Error( "SDL_Init failed: %s", SDL_GetError() );
	}

	// get the initial time base
	Sys_Milliseconds();

	Sys_InitStreamThread();

	Com_Init( sys_cmdline );
	NET_Init();

	Com_Printf( "Working directory: %s\n", Sys_Cwd() );

	// hide the early console since we've reached the point where we
	// have a working graphics subsystems
	if ( !com_dedicated->integer && !com_viewlog->integer ) {
		Sys_ShowConsole( 0, qfalse );
	}

	if ( glw_state.window ) {
		SDL_RaiseWindow( glw_state.window );
	}

	// main game loop
	while ( 1 ) {
		// if not running as a game client, sleep a bit
		if ( g_sv.isMinimized || ( com_dedicated && com_dedicated->integer ) ) {
			SDL_Delay( 5 );
		}

		// set low precision every frame, because some system calls
		// reset it arbitrarily
//		_controlfp( _PC_24, _MCW_PC );
//    _controlfp( -1, _MCW_EM  ); // no exceptions, even if some crappy
		// syscall turns them back on!

		startTime = Sys_Milliseconds();

		// make sure mouse and joystick are only called once a frame
		IN_Frame();

		// run the game
		Com_Frame();

		endTime = Sys_Milliseconds();
		totalMsec += endTime - startTime;
		countMsec++;
	}

	// never gets here
}
