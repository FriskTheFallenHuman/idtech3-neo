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

#ifndef __SYS_PLATFORM_PUBLIC__
#define __SYS_PLATFORM_PUBLIC__

class idSysPlatform {
public:
	virtual			~idSysPlatform() {}

	// Initialization and shutdown
	virtual bool	Initialize() = 0;
	virtual bool	IsInitialized() = 0;
	virtual void	Shutdown() = 0;

	// Run the platform-specific callbacks, such as Steam API callbacks
	virtual void	RunCallbacks() = 0;

	// platform-specific achievement support
	virtual bool	SetAchievement( const char *achievementName ) = 0;
	virtual void	ResetAchievements() = 0;

	// platform-specific cloud support
	virtual bool	CloudSave( const char* savefile, const char *descFile ) = 0;
	virtual bool	CloudSaveConfig( const char *configFileName ) = 0;
	virtual bool	CloudDeleteFile( const char *filename ) = 0;
	virtual bool	CloudLoad() = 0;

	// platform-specific overlay support
	virtual void	OpenOverlaypage( const char *pageURL ) = 0;
	virtual void	OpenOverlaypageStore() = 0;
	virtual void	OpenOverlayInviteFriend() = 0;
	virtual bool	IsOverlayOpen() = 0;

	// platform-specific workshop support
	virtual int		GetWorkshopAmount() = 0;
	virtual const char *GetWorkshopPathAtIndex( int index ) = 0;

	// platform-specific language support
	virtual const char *GetLanguage() = 0;

	// platform-specific app id support
	virtual const char *GetAppID() = 0;

	// platform-specific rich presence support
	virtual void	SetRichPresence( const char *text ) = 0;

	// platform-specific timeline support ( currently only for Steam's new timeline feature )
	virtual void	SetTimelineEvent( const char *icon ) = 0;

	// platform-specific platform checks (Steam Deck/Rock Ally)
	virtual bool	IsOnHandHeld() = 0;

	// platform-specific big picture mode check (for Steam)
	virtual bool	IsInBigPictureMode() = 0;

	// platform-specific controller support check
	virtual bool	ShouldDefaultToController() { return IsInBigPictureMode() || IsInBigPictureMode(); }
};

extern idSysPlatform *				platform;

#endif /* !__SYS_PLATFORM_PUBLIC__ */