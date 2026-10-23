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
#ifndef __SWF_DIALOGS_H__
#define __SWF_DIALOGS_H__

#include "../framework/Common_dialog.h"
#include "../swf/SWF.h"

/*
================================================
idSWFDialog
================================================
*/
class idSWFDialog : public idCommonDialog {
public:
					idSWFDialog();
	virtual         ~idSWFDialog();

	virtual void    Init();
	virtual void    Shutdown();

	virtual void    ShowSaveIndicator( bool show );
	virtual bool    HandleDialogEvent( const sysEvent_t *sev );
	virtual bool    IsDialogActive() const;

protected:

	virtual bool    IsRendererLoaded() const;
	virtual bool    IsRendererActive() const;
	virtual void    ActivateRenderer( bool active );
	virtual bool    IsSaveIndicatorActive()  const;
	virtual void    RenderDialog( int timeMicroseconds );
	virtual void    RenderSaveIndicator( int timeMicroseconds );

	virtual void    SetRendererGlobalInt( const char *name, int val );
	virtual void    SetRendererGlobalString( const char *name, const char *val );

	virtual void    AddRefCallback( void *cb );
	virtual void    ReleaseCallback( void *cb );
	virtual void    InvokeCallback( void *cb ) ;

	virtual void    BindDialogToRenderer( const idDialogInfo &info );

private:

	static idSWFScriptFunction *AsSWF( void *cb ) { return static_cast< idSWFScriptFunction * >( cb ); }

	idSWF *dialog;
	idSWF *saveIndicator;
};

#endif /* !__SWF_DIALOGS_H__ */