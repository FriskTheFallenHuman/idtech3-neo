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

/*
========================
idSWFDialog::idSWFDialog
========================
*/
idSWFDialog::idSWFDialog() {
	dialog = NULL;
	saveIndicator = NULL;
}

/*
========================
idSWFDialog::~idSWFDialog
========================
*/
idSWFDialog::~idSWFDialog() {
	Shutdown();
}

/*
========================
idSWFDialog::Init
========================
*/
void idSWFDialog::Init() {
	idLib::PrintfIf( popupDialog_debug.GetBool(), "[%s]\n", __FUNCTION__ );

	Shutdown();

	dialog = new (TAG_SWF) idSWF("dialog");
	saveIndicator = new (TAG_SWF) idSWF("save_indicator");

	// Bind all dialogType_t enum values as globals so the SWF
	// ActionScript can reference them by name
#define BIND_DIALOG_CONSTANT( x ) dialog->SetGlobal( #x, x )
	BIND_DIALOG_CONSTANT(DIALOG_ACCEPT);
	BIND_DIALOG_CONSTANT(DIALOG_CONTINUE);
	BIND_DIALOG_CONSTANT(DIALOG_ACCEPT_CANCEL);
	BIND_DIALOG_CONSTANT(DIALOG_YES_NO);
	BIND_DIALOG_CONSTANT(DIALOG_CANCEL);
	BIND_DIALOG_CONSTANT(DIALOG_WAIT);
	BIND_DIALOG_CONSTANT(DIALOG_WAIT_BLACKOUT);
	BIND_DIALOG_CONSTANT(DIALOG_WAIT_CANCEL);
	BIND_DIALOG_CONSTANT(DIALOG_DYNAMIC);
	BIND_DIALOG_CONSTANT(DIALOG_QUICK_SAVE);
	BIND_DIALOG_CONSTANT(DIALOG_TIMER_ACCEPT_REVERT);
	BIND_DIALOG_CONSTANT(DIALOG_CRAWL_SAVE);
	BIND_DIALOG_CONSTANT(DIALOG_CONTINUE_LARGE);
	BIND_DIALOG_CONSTANT(DIALOG_BENCHMARK);
#undef BIND_DIALOG_CONSTANT
}

/*
========================
idSWFDialog::Shutdown
========================
*/
void idSWFDialog::Shutdown() {
	idLib::PrintfIf( popupDialog_debug.GetBool(), "[%s]\n", __FUNCTION__ );

	ClearDialogs();

	delete dialog;
	dialog = NULL;

	delete saveIndicator;
	saveIndicator = NULL;
}

/*
========================
idSWFDialog::IsRendererLoaded
========================
*/
bool idSWFDialog::IsRendererLoaded() const {
	return (dialog != NULL);
}

/*
========================
idSWFDialog::IsRendererActive
========================
*/
bool idSWFDialog::IsRendererActive() const {
	return (dialog != NULL && dialog->IsActive());
}

/*
========================
idSWFDialog::ActivateRenderer
========================
*/
void idSWFDialog::ActivateRenderer( bool active ) {
	if ( dialog != NULL ) {
		dialog->Activate( active );
	}
}

/*
========================
idSWFDialog::IsSaveIndicatorActive
========================
*/
bool idSWFDialog::IsSaveIndicatorActive() const {
	return (saveIndicator != NULL && saveIndicator->IsActive());
}

/*
========================
idSWFDialog::RenderDialog
========================
*/
void idSWFDialog::RenderDialog( int timeMicroseconds ) {
	if (dialog != NULL) {
		dialog->Render( renderSystem, timeMicroseconds );
	}
}

/*
========================
idSWFDialog::RenderSaveIndicator
========================
*/
void idSWFDialog::RenderSaveIndicator( int timeMicroseconds ) {
	if ( saveIndicator != NULL ) {
		saveIndicator->Render( renderSystem, timeMicroseconds );
	}
}

/*
========================
idSWFDialog::SetRendererGlobalInt
========================
*/
void idSWFDialog::SetRendererGlobalInt( const char *name, int val ) {
	if ( dialog != NULL ) {
		dialog->SetGlobal( name, val );
	}
}

/*
========================
idSWFDialog::SetRendererGlobalString
========================
*/
void idSWFDialog::SetRendererGlobalString( const char *name, const char *val ) {
	if (dialog != NULL) {
		dialog->SetGlobal( name, val );
	}
}

/*
========================
idSWFDialog::AddRefCallback
========================
*/
void idSWFDialog::AddRefCallback( void *cb ) {
	if ( cb != NULL ) {
		AsSWF(cb)->AddRef();
	}
}

/*
========================
idSWFDialog::ReleaseCallback
========================
*/
void idSWFDialog::ReleaseCallback( void *cb ) {
	if ( cb != NULL ) {
		AsSWF(cb)->Release();
	}
}

/*
========================
idSWFDialog::InvokeCallback
========================
*/
void idSWFDialog::InvokeCallback( void *cb ) {
	if ( cb != NULL ) {
		idSWFParmList parms;
		AsSWF(cb)->Call(NULL, parms);
	}
}

/*
========================
idSWFDialog::InvokeCallback
========================
*/
void idSWFDialog::BindDialogToRenderer( const idDialogInfo &info ) {
	if ( dialog == NULL ) {
		return;
	}

	class idSWFScriptFunction_CallbackAdapter : public idSWFScriptFunction_RefCounted {
	public:
		explicit idSWFScriptFunction_CallbackAdapter( idDialogCallback * cb )
			: callback( cb ) {
			if ( callback )
				callback->AddRef();
		}
		~idSWFScriptFunction_CallbackAdapter() {
			if ( callback )
				callback->Release();
		}
		idSWFScriptVar Call( idSWFScriptObject *, const idSWFParmList & ) override {
			if ( callback )
				callback->Call();
			return idSWFScriptVar();
		}
	private:
		idDialogCallback * callback;
	};

	auto Wrap = []( idDialogCallback * cb ) -> idSWFScriptFunction * {
		if ( cb == NULL )
			return NULL;
		return new (TAG_SWF) idSWFScriptFunction_CallbackAdapter( cb );
	};

	idStr message, title;
	GetDialogMsg( info.msg, message, title );

	dialog->SetGlobal( "titleVal", title );
	if ( info.overrideMsg.IsEmpty() ) {
		dialog->SetGlobal( "messageInfo", message );
	} else {
		dialog->SetGlobal( "messageInfo", info.overrideMsg );
	}
	dialog->SetGlobal( "Infotype", info.type );

	if ( info.acceptCB == NULL &&
		 info.type != DIALOG_WAIT &&
		 info.type != DIALOG_WAIT_BLACKOUT ) {

		// Default accept: just clears this dialog
		class idSWFScriptFunction_DefaultAccept : public idSWFScriptFunction_RefCounted {
		public:
			explicit idSWFScriptFunction_DefaultAccept( gameDialogMessages_t m ) : msg( m ) {}
			idSWFScriptVar Call( idSWFScriptObject *, const idSWFParmList & ) override {
				common->Dialog().ClearDialog( msg );
				return idSWFScriptVar();
			}
		private:
			gameDialogMessages_t msg;
		};
		dialog->SetGlobal( "acceptCallBack", new (TAG_SWF) idSWFScriptFunction_DefaultAccept( info.msg ) );
	} else {
		dialog->SetGlobal( "acceptCallBack", Wrap(info.acceptCB) );
	}

	dialog->SetGlobal( "cancelCallBack", Wrap(info.cancelCB) );
	dialog->SetGlobal( "altCBOne",	Wrap(info.altCBOne) );
	dialog->SetGlobal( "altCBTwo",	Wrap(info.altCBTwo) );

	dialog->SetGlobal( "opt1Txt", info.txt1.GetLocalizedString() );
	dialog->SetGlobal( "opt2Txt", info.txt2.GetLocalizedString() );
	dialog->SetGlobal( "opt3Txt", info.txt3.GetLocalizedString() );
	dialog->SetGlobal( "opt4Txt", info.txt4.GetLocalizedString() );
}

/*
========================
idSWFDialog::IsDialogActive
========================
*/
bool idSWFDialog::IsDialogActive() const {
	if ( dialog != NULL ) {
		return dialog->IsActive();
	}

	return false;
}

/*
================================================
idSWFDialog::HandleDialogEvent
================================================
*/
bool idSWFDialog::HandleDialogEvent( const sysEvent_t *sev ) {
	if ( dialog != NULL && dialog->IsLoaded() && dialog->IsActive() ) {
		if ( IsSaveIndicatorActive() ) {
			return false;
		} else {
			if ( dialog->HandleEvent( sev ) ) {
				idKeyInput::ClearStates();
				Sys_ClearEvents();
			}
		}

		return true;
	}

	return false;
}

/*
================================================
idSWFDialog::ShowSaveIndicator
================================================
*/
void idSWFDialog::ShowSaveIndicator( bool show ) {
	idLib::PrintfIf( popupDialog_debug.GetBool(), "[%s]\n", __FUNCTION__ );

	if ( show ) {
		idStr msg = idStrId( "#str_dlg_pc_saving" ).GetLocalizedString();

		AddDialog( GDM_SAVING, DIALOG_WAIT, NULL, NULL, true, "", 0, false, true, true );
	} else {
		ClearDialog( GDM_SAVING );
	}
}

CONSOLE_COMMAND( testShowDynamicDialog, "show a dynamic dialog", 0 ) {
    class idDialogContinueCallback : public idDialogCallback {
    public:
        void Call() override {
            common->Dialog().ClearDialog( GDM_INSUFFICENT_STORAGE_SPACE );
        }
    };

    idStaticList< idDialogCallback *, 4 > callbacks;
    idStaticList< idStrId, 4 > optionText;
    callbacks.Append( new idDialogContinueCallback() );
    optionText.Append( idStrId( "#str_swf_continue" ) );

	// build custom space required string
	// #str_dlg_space_required ~= "There is insufficient storage available.  Please free %s and try again."
	idStr format = idStrId( "#str_dlg_space_required" ).GetLocalizedString();
	idStr size;
	int requiredSpaceInBytes = 150000;
	if ( requiredSpaceInBytes > ( 1024 * 1024 ) ) {
		size = va( "%.1f MB", (float) requiredSpaceInBytes / ( 1024.0f * 1024.0f ) );
	} else {
		size = va( "%.0f KB", (float) requiredSpaceInBytes / 1024.0f );
	}
	idStr msg = va( format.c_str(), size.c_str() );

	ADD_DYNAMIC_DIALOG( GDM_INSUFFICENT_STORAGE_SPACE, callbacks, optionText, true, msg );
}

CONSOLE_COMMAND( testShowDialogBug, "show a dynamic dialog", 0 ) {
	common->Dialog().ShowSaveIndicator( true );
	common->Dialog().ShowSaveIndicator( false );

	// This locks the game because it thinks it's paused because we're passing in pause = true but the
	// dialog isn't ever added because of the abuse of dialog->isActive when the save indicator is shown.
	int dialogId = atoi( args.Argv( 1 ) );
	ADD_DIALOG( (gameDialogMessages_t)dialogId, DIALOG_ACCEPT, NULL, NULL, true );
}