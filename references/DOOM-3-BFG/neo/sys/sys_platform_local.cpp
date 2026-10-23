/*
===========================================================================

Doom 3 BFG Edition GPL Source Code
Copyright (C) 1993-2012 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 BFG Edition GPL Source Code ("Doom 3 BFG Edition Source Code").

Doom 3 BFG Edition Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 BFG Edition Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 BFG Edition Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 BFG Edition Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 BFG Edition Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include "sys_platform_local.h"

idSysPlatformLocal	platformLocal;
idSysPlatform *		platform = &platformLocal;

/*
================
idSysPlatformLocal::Initialize
================
*/
bool idSysPlatformLocal::Initialize() {
	common->Printf ("\n------- Platform Initialization -------\n");

	common->Printf( "Platform initialized successfully.\n" );

	common->Printf ("------------------------------------\n");

	return true;
}

/*
================
idSysPlatformLocal::IsInitialized
================
*/
bool idSysPlatformLocal::IsInitialized() {
	return initialized;
}

/*
================
idSysPlatformLocal::Shutdown
================
*/
void idSysPlatformLocal::Shutdown() {
}

/*
================
idSysPlatformLocal::IsOnHandHeld
================
*/
bool idSysPlatformLocal::IsOnHandHeld() {
	return false;
}

/*
================
idSysPlatformLocal::IsInBigPictureMode
================
*/
bool idSysPlatformLocal::IsInBigPictureMode() {
	return false;
}

/*
================
idSysPlatformLocal::RunCallbacks
================
*/
void idSysPlatformLocal::RunCallbacks() {
}

/*
================
idSysPlatformLocal::CloudSave
================
*/
bool idSysPlatformLocal::CloudSave( const char *savefile, const char *descFile ) {
	return false;
}

/*
================
idSysPlatformLocal::CloudSaveConfig
================
*/
bool idSysPlatformLocal::CloudSaveConfig( const char *configFileName ) {
	return false;
}

/*
================
idSysPlatformLocal::CloudDeleteFile
================
*/
bool idSysPlatformLocal::CloudDeleteFile( const char *filename ) {
	return false;
}

/*
================
idSysPlatformLocal::CloudLoad
================
*/
bool idSysPlatformLocal::CloudLoad() {
	return false;
}

/*
================
idSysPlatformLocal::OpenOverlaypage
================
*/
void idSysPlatformLocal::OpenOverlaypage( const char *pageURL ) {
}

/*
================
idSysPlatformLocal::OpenOverlaypage
================
*/
void idSysPlatformLocal::OpenOverlaypageStore() {
}

/*
================
idSysPlatformLocal::OpenOverlayInviteFriend
================
*/
void idSysPlatformLocal::OpenOverlayInviteFriend() {
}

/*
================
idSysPlatformLocal::IsOverlayOpen
================
*/
bool idSysPlatformLocal::IsOverlayOpen() {
	return false;
}

/*
================
idSysPlatformLocal::SetAchievement
================
*/
bool idSysPlatformLocal::SetAchievement( const char *achievementName ) {
	return false;
}

/*
================
idSysPlatformLocal::ResetAchievements
================
*/
void idSysPlatformLocal::ResetAchievements() {

}

/*
================
idSysPlatformLocal::GetWorkshopAmount
================
*/
int idSysPlatformLocal::GetWorkshopAmount() {
	return 0;
}

/*
================
idSysPlatformLocal::GetWorkshopAmount
================
*/
const char *idSysPlatformLocal::GetWorkshopPathAtIndex( int index ) {
	return "";
}

/*
================
idSysPlatformLocal::GetLanguage
================
*/
const char *idSysPlatformLocal::GetLanguage() {
	return "english";
}

/*
================
idSysPlatformLocal::GetAppID
================
*/
const char *idSysPlatformLocal::GetAppID() {
	return "12456";
}

/*
================
idSysPlatformLocal::SetRichPresence
================
*/
void idSysPlatformLocal::SetRichPresence( const char *text ) {

}

/*
================
idSysPlatformLocal::SetTimelineEvent
================
*/
void idSysPlatformLocal::SetTimelineEvent( const char *icon ) {

}
