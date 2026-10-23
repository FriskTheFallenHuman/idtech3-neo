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

// Copyright (C) 1999-2000 Id Software, Inc.
//

#ifndef G_PUBLIC_H
#define G_PUBLIC_H

// g_public.h -- game module information visible to server

#define GAME_API_VERSION 10

// entity->svFlags
// the server does not know how to interpret most of the values
// in entityStates (level eType), so the game must explicitly flag
// special server behaviors
#define SVF_NOCLIENT            0x00000001  // don't send entity to clients, even if it has effects
#define SVF_VISDUMMY            0x00000004  // this ent is a "visibility dummy" and needs it's master to be sent to clients that can see it even if they can't see the master ent
#define SVF_BOT                 0x00000008
// Client-backed server actors; suppress outgoing player messages and snapshots.
#define SVF_AI 0x00000010
// done.
#define SVF_BROADCAST           0x00000020  // send to all connected clients
#define SVF_PORTAL              0x00000040  // merge a second pvs at origin2 into snapshots
#define SVF_USE_CURRENT_ORIGIN  0x00000080  // entity->r.currentOrigin instead of entity->s.origin
											// for link position (missiles and movers)
// Ridah
#define SVF_NOFOOTSTEPS         0x00000100
// done.
// MrE:
#define SVF_CAPSULE             0x00000200  // use capsule for collision detection

#define SVF_VISDUMMY_MULTIPLE   0x00000400  // so that one vis dummy can add to snapshot multiple speakers

// recent id changes
#define SVF_SINGLECLIENT        0x00000800  // only send to a single client (entityShared_t->singleClient)
#define SVF_NOSERVERINFO        0x00001000  // don't send CS_SERVERINFO updates to this client
											// so that it can be updated for ping tools without
											// lagging clients
#define SVF_NOTSINGLECLIENT     0x00002000  // send entity to everyone but one client
											// (entityShared_t->singleClient)

//===============================================================


typedef struct {
	entityState_t s;                // communicated by server to clients

	qboolean linked;                // qfalse if not in any good cluster
	int linkcount;

	int svFlags;                    // SVF_NOCLIENT, SVF_BROADCAST, etc
	int singleClient;               // only send to this client when SVF_SINGLECLIENT is set

	qboolean bmodel;                // if false, assume an explicit mins / maxs bounding box
									// only set by trap_SetBrushModel
	vec3_t mins, maxs;
	int contents;                   // CONTENTS_TRIGGER, CONTENTS_SOLID, CONTENTS_BODY, etc
									// a non-solid entity should set to 0

	vec3_t absmin, absmax;          // derived from mins/maxs and origin + rotation

	// currentOrigin will be used for all collision detection and world linking.
	// it will not necessarily be the same as the trajectory evaluation for the current
	// time, because each entity must be moved one at a time after time is advanced
	// to avoid simultanious collision issues
	vec3_t currentOrigin;
	vec3_t currentAngles;

	// when a trace call is made and passEntityNum != ENTITYNUM_NONE,
	// an ent will be excluded from testing if:
	// ent->s.number == passEntityNum	(don't interact with self)
	// ent->s.ownerNum = passEntityNum	(don't interact with your own missiles)
	// entity[ent->s.ownerNum].ownerNum = passEntityNum	(don't interact with other missiles from owner)
	int ownerNum;
	int eventTime;
} entityShared_t;



// the server looks at a sharedEntity, which is the start of the game's gentity_t structure
typedef struct {
	entityState_t s;                // communicated by server to clients
	entityShared_t r;               // shared by both the server system and game
} sharedEntity_t;



//===============================================================

//
// system traps provided by the main engine
//

typedef struct {
	int apiVersion;
	void (*Printf)( const char * fmt );
	void (*Error)( const char * fmt );
	void (*Endgame)(  );
	int (*Milliseconds)(  );
	int (*Argc)(  );
	void (*Argv)( int n, char * buffer, int bufferLength );
	int (*FS_FOpenFile)( const char * qpath, fileHandle_t * f, fsMode_t mode );
	void (*FS_Read)( void * buffer, int len, fileHandle_t f );
	int (*FS_Write)( const void * buffer, int len, fileHandle_t f );
	int (*FS_Rename)( const char * from, const char * to );
	void (*FS_FCloseFile)( fileHandle_t f );
	void (*FS_CopyFile)( char * from, char * to );
	int (*FS_GetFileList)( const char * path, const char * extension, char * listbuf, int bufsize );
	void (*SendConsoleCommand)( int exec_when, const char * text );
	void (*Cvar_Register)( vmCvar_t * cvar, const char * var_name, const char * value, int flags );
	void (*Cvar_Update)( vmCvar_t * cvar );
	void (*Cvar_Set)( const char * var_name, const char * value );
	int (*Cvar_VariableIntegerValue)( const char * var_name );
	void (*Cvar_VariableStringBuffer)( const char * var_name, char * buffer, int bufsize );
	void (*LocateGameData)( void * gEnts, int numGEntities, int sizeofGEntity_t, playerState_t * clients, int sizeofGClient );
	void (*DropClient)( int clientNum, const char * reason );
	void (*SendServerCommand)( int clientNum, const char * text );
	void (*SetConfigstring)( int num, const char * string );
	void (*GetConfigstring)( int num, char * buffer, int bufferSize );
	void (*GetUserinfo)( int num, char * buffer, int bufferSize );
	void (*SetUserinfo)( int num, const char * buffer );
	void (*GetServerinfo)( char * buffer, int bufferSize );
	void (*SetBrushModel)( void * ent, const char * name );
	void (*Trace)( trace_t * results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask );
	void (*TraceCapsule)( trace_t * results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask );
	int (*PointContents)( const vec3_t point, int passEntityNum );
	qboolean (*InPVS)( const vec3_t p1, const vec3_t p2 );
	qboolean (*InPVSIgnorePortals)( const vec3_t p1, const vec3_t p2 );
	void (*AdjustAreaPortalState)( void * ent, qboolean open );
	qboolean (*AreasConnected)( int area1, int area2 );
	void (*LinkEntity)( void * ent );
	void (*UnlinkEntity)( void * ent );
	int (*EntitiesInBox)( const vec3_t mins, const vec3_t maxs, int * list, int maxcount );
	qboolean (*EntityContact)( const vec3_t mins, const vec3_t maxs, const void * ent );
	qboolean (*EntityContactCapsule)( const vec3_t mins, const vec3_t maxs, const void * ent );
	int (*BotAllocateClient)(  );
	void (*BotFreeClient)( int clientNum );
	void (*GetUsercmd)( int clientNum, usercmd_t * cmd );
	qboolean (*GetEntityToken)( char * buffer, int bufferSize );
	int (*DebugPolygonCreate)( int color, int numPoints, vec3_t * points );
	void (*DebugPolygonDelete)( int id );
	int (*RealTime)( qtime_t * qtime );
	void (*SnapVector)( float * v );
	qboolean (*GetTag)( int clientNum, char * tagName, orientation_t * or );
	int (*BotLibSetup)(  );
	int (*BotLibShutdown)(  );
	int (*BotLibVarSet)( char * var_name, char * value );
	int (*BotLibVarGet)( char * var_name, char * value, int size );
	int (*BotLibDefine)( char * string );
	int (*BotLibStartFrame)( float time );
	int (*BotLibLoadMap)( const char * mapname );
	int (*BotLibUpdateEntity)( int ent, void * bue );
	int (*BotLibTest)( int parm0, char * parm1, vec3_t parm2, vec3_t parm3 );
	int (*BotGetSnapshotEntity)( int clientNum, int sequence );
	int (*BotGetServerCommand)( int clientNum, char * message, int size );
	void (*BotUserCommand)( int clientNum, usercmd_t * ucmd );
	void (*AAS_EntityInfo)( int entnum, void * info );
	int (*AAS_Initialized)(  );
	void (*AAS_PresenceTypeBoundingBox)( int presencetype, vec3_t mins, vec3_t maxs );
	float (*AAS_Time)(  );
	void (*AAS_SetCurrentWorld)( int index );
	int (*AAS_PointAreaNum)( vec3_t point );
	int (*AAS_TraceAreas)( vec3_t start, vec3_t end, int * areas, vec3_t * points, int maxareas );
	int (*AAS_PointContents)( vec3_t point );
	int (*AAS_NextBSPEntity)( int ent );
	int (*AAS_ValueForBSPEpairKey)( int ent, char * key, char * value, int size );
	int (*AAS_VectorForBSPEpairKey)( int ent, char * key, vec3_t v );
	int (*AAS_FloatForBSPEpairKey)( int ent, char * key, float * value );
	int (*AAS_IntForBSPEpairKey)( int ent, char * key, int * value );
	int (*AAS_AreaReachability)( int areanum );
	int (*AAS_AreaTravelTimeToGoalArea)( int areanum, vec3_t origin, int goalareanum, int travelflags );
	int (*AAS_Swimming)( vec3_t origin );
	int (*AAS_PredictClientMovement)( void * move, int entnum, vec3_t origin, int presencetype, int onground, vec3_t velocity, vec3_t cmdmove, int cmdframes, int maxframes, float frametime, int stopevent, int stopareanum, int visualize );
	void (*AAS_RT_ShowRoute)( vec3_t srcpos, int srcnum, int destnum );
	qboolean (*AAS_RT_GetHidePos)( vec3_t srcpos, int srcnum, int srcarea, vec3_t destpos, int destnum, int destarea, vec3_t returnPos );
	int (*AAS_FindAttackSpotWithinRange)( int srcnum, int rangenum, int enemynum, float rangedist, int travelflags, float * outpos );
	qboolean (*AAS_GetRouteFirstVisPos)( vec3_t srcpos, vec3_t destpos, int travelflags, vec3_t retpos );
	void (*AAS_SetAASBlockingEntity)( vec3_t absmin, vec3_t absmax, qboolean blocking );
	void (*EA_Say)( int client, char * str );
	void (*EA_SayTeam)( int client, char * str );
	void (*EA_UseItem)( int client, char * it );
	void (*EA_DropItem)( int client, char * it );
	void (*EA_UseInv)( int client, char * inv );
	void (*EA_DropInv)( int client, char * inv );
	void (*EA_Gesture)( int client );
	void (*EA_Command)( int client, char * command );
	void (*EA_SelectWeapon)( int client, int weapon );
	void (*EA_Talk)( int client );
	void (*EA_Attack)( int client );
	void (*EA_Reload)( int client );
	void (*EA_Use)( int client );
	void (*EA_Respawn)( int client );
	void (*EA_Jump)( int client );
	void (*EA_DelayedJump)( int client );
	void (*EA_Crouch)( int client );
	void (*EA_MoveUp)( int client );
	void (*EA_MoveDown)( int client );
	void (*EA_MoveForward)( int client );
	void (*EA_MoveBack)( int client );
	void (*EA_MoveLeft)( int client );
	void (*EA_MoveRight)( int client );
	void (*EA_Move)( int client, vec3_t dir, float speed );
	void (*EA_View)( int client, vec3_t viewangles );
	void (*EA_EndRegular)( int client, float thinktime );
	void (*EA_GetInput)( int client, float thinktime, void * input );
	void (*EA_ResetInput)( int client, void * init );
	int (*BotLoadCharacter)( char * charfile, int skill );
	void (*BotFreeCharacter)( int character );
	float (*Characteristic_Float)( int character, int index );
	float (*Characteristic_BFloat)( int character, int index, float min, float max );
	int (*Characteristic_Integer)( int character, int index );
	int (*Characteristic_BInteger)( int character, int index, int min, int max );
	void (*Characteristic_String)( int character, int index, char * buf, int size );
	int (*BotAllocChatState)(  );
	void (*BotFreeChatState)( int handle );
	void (*BotQueueConsoleMessage)( int chatstate, int type, char * message );
	void (*BotRemoveConsoleMessage)( int chatstate, int handle );
	int (*BotNextConsoleMessage)( int chatstate, void * cm );
	int (*BotNumConsoleMessages)( int chatstate );
	void (*BotInitialChat)( int chatstate, char * type, int mcontext, char * var0, char * var1, char * var2, char * var3, char * var4, char * var5, char * var6, char * var7 );
	int (*BotNumInitialChats)( int chatstate, char * type );
	int (*BotReplyChat)( int chatstate, char * message, int mcontext, int vcontext, char * var0, char * var1, char * var2, char * var3, char * var4, char * var5, char * var6, char * var7 );
	int (*BotChatLength)( int chatstate );
	void (*BotEnterChat)( int chatstate, int client, int sendto );
	void (*BotGetChatMessage)( int chatstate, char * buf, int size );
	int (*StringContains)( char * str1, char * str2, int casesensitive );
	int (*BotFindMatch)( char * str, void * match, unsigned long int context );
	void (*BotMatchVariable)( void * match, int variable, char * buf, int size );
	void (*UnifyWhiteSpaces)( char * string );
	void (*BotReplaceSynonyms)( char * string, unsigned long int context );
	int (*BotLoadChatFile)( int chatstate, char * chatfile, char * chatname );
	void (*BotSetChatGender)( int chatstate, int gender );
	void (*BotSetChatName)( int chatstate, char * name );
	void (*BotResetGoalState)( int goalstate );
	void (*BotResetAvoidGoals)( int goalstate );
	void (*BotRemoveFromAvoidGoals)( int goalstate, int number );
	void (*BotPushGoal)( int goalstate, void * goal );
	void (*BotPopGoal)( int goalstate );
	void (*BotEmptyGoalStack)( int goalstate );
	void (*BotDumpAvoidGoals)( int goalstate );
	void (*BotDumpGoalStack)( int goalstate );
	void (*BotGoalName)( int number, char * name, int size );
	int (*BotGetTopGoal)( int goalstate, void * goal );
	int (*BotGetSecondGoal)( int goalstate, void * goal );
	int (*BotChooseLTGItem)( int goalstate, vec3_t origin, int * inventory, int travelflags );
	int (*BotChooseNBGItem)( int goalstate, vec3_t origin, int * inventory, int travelflags, void * ltg, float maxtime );
	int (*BotTouchingGoal)( vec3_t origin, void * goal );
	int (*BotItemGoalInVisButNotVisible)( int viewer, vec3_t eye, vec3_t viewangles, void * goal );
	int (*BotGetLevelItemGoal)( int index, char * classname, void * goal );
	int (*BotGetNextCampSpotGoal)( int num, void * goal );
	int (*BotGetMapLocationGoal)( char * name, void * goal );
	float (*BotAvoidGoalTime)( int goalstate, int number );
	void (*BotInitLevelItems)(  );
	void (*BotUpdateEntityItems)(  );
	int (*BotLoadItemWeights)( int goalstate, char * filename );
	void (*BotFreeItemWeights)( int goalstate );
	void (*BotInterbreedGoalFuzzyLogic)( int parent1, int parent2, int child );
	void (*BotSaveGoalFuzzyLogic)( int goalstate, char * filename );
	void (*BotMutateGoalFuzzyLogic)( int goalstate, float range );
	int (*BotAllocGoalState)( int state );
	void (*BotFreeGoalState)( int handle );
	void (*BotResetMoveState)( int movestate );
	void (*BotMoveToGoal)( void * result, int movestate, void * goal, int travelflags );
	int (*BotMoveInDirection)( int movestate, vec3_t dir, float speed, int type );
	void (*BotResetAvoidReach)( int movestate );
	void (*BotResetLastAvoidReach)( int movestate );
	int (*BotReachabilityArea)( vec3_t origin, int testground );
	int (*BotMovementViewTarget)( int movestate, void * goal, int travelflags, float lookahead, vec3_t target );
	int (*BotPredictVisiblePosition)( vec3_t origin, int areanum, void * goal, int travelflags, vec3_t target );
	int (*BotAllocMoveState)(  );
	void (*BotFreeMoveState)( int handle );
	void (*BotInitMoveState)( int handle, void * initmove );
	void (*BotInitAvoidReach)( int handle );
	int (*BotChooseBestFightWeapon)( int weaponstate, int * inventory );
	void (*BotGetWeaponInfo)( int weaponstate, int weapon, void * weaponinfo );
	int (*BotLoadWeaponWeights)( int weaponstate, char * filename );
	int (*BotAllocWeaponState)(  );
	void (*BotFreeWeaponState)( int weaponstate );
	void (*BotResetWeaponState)( int weaponstate );
	int (*GeneticParentsAndChildSelection)( int numranks, float * ranks, int * parent1, int * parent2, int * child );
	qboolean (*AAS_Route)( vec3_t origin, vec3_t goal, int flags, vec3_t point );
	qboolean (*AAS_DoomPlayerRoute)( vec3_t origin, vec3_t goal, int flags, void * route );
} gameImport_t;

// set by the engine right before the module entry point runs
extern gameImport_t *gameImports;

//
// functions exported by the game subsystem
//
typedef struct {
	int apiVersion;
	void (*Init)( int levelTime, int randomSeed, int restart );
	void (*Shutdown)( int restart );
	const char * (*ClientConnect)( int clientNum, qboolean firstTime, qboolean isBot );
	void (*ClientBegin)( int clientNum );
	void (*ClientUserinfoChanged)( int clientNum );
	void (*ClientDisconnect)( int clientNum );
	void (*ClientCommand)( int clientNum );
	void (*ClientThink)( int clientNum );
	void (*RunFrame)( int levelTime );
	qboolean (*ConsoleCommand)( void );
	qboolean (*AnimationFrame)( int time );
	qboolean (*EntityVisibleFromPos)( vec3_t srcpos, int srcnum, vec3_t destpos, int destnum, qboolean updateVisPos );
	qboolean (*EntityCanAttackFromPos)( int entnum, int enemy, vec3_t pos, qboolean ducking, qboolean allowHitWorld );
	void (*RetrieveMoveSpeedsFromClient)( int clientNum, char * buf );
	qboolean (*GetModelInfo)( int clientNum, char * tagName, void ** out );
} gameExport_t;

#endif
