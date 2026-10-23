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
/*
** QGL.H
*/

#ifndef __QGL_H__
#define __QGL_H__


#include <gl/gl.h>


#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef WINAPI
#define WINAPI
#endif

// only use local glext.h if we are not using the system one already
// http://oss.sgi.com/projects/ogl-sample/ABI/
#ifndef GL_GLEXT_VERSION

#include "glext.h"

#endif

typedef void (*GLExtension_t)(void);

#ifdef __cplusplus
	extern "C" {
#endif

GLExtension_t GLimp_ExtensionPointer( const char *name );

#ifdef __cplusplus
	}
#endif

// GL_EXT_direct_state_access
extern PFNGLBINDMULTITEXTUREEXTPROC			qglBindMultiTextureEXT;

// GL_ARB_texture_compression
extern PFNGLCOMPRESSEDTEXIMAGE2DARBPROC		qglCompressedTexImage2DARB;
extern PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC	qglCompressedTexSubImage2DARB;
extern PFNGLGETCOMPRESSEDTEXIMAGEARBPROC	qglGetCompressedTexImageARB;

// GL_ARB_vertex_buffer_object
extern PFNGLBINDBUFFERARBPROC				qglBindBufferARB;
extern PFNGLBINDBUFFERRANGEPROC				qglBindBufferRange;
extern PFNGLDELETEBUFFERSARBPROC			qglDeleteBuffersARB;
extern PFNGLGENBUFFERSARBPROC				qglGenBuffersARB;
extern PFNGLISBUFFERARBPROC					qglIsBufferARB;
extern PFNGLBUFFERDATAARBPROC				qglBufferDataARB;
extern PFNGLBUFFERSUBDATAARBPROC			qglBufferSubDataARB;
extern PFNGLGETBUFFERSUBDATAARBPROC			qglGetBufferSubDataARB;
extern PFNGLMAPBUFFERARBPROC				qglMapBufferARB;
extern PFNGLUNMAPBUFFERARBPROC				qglUnmapBufferARB;
extern PFNGLGETBUFFERPARAMETERIVARBPROC		qglGetBufferParameterivARB;
extern PFNGLGETBUFFERPOINTERVARBPROC		qglGetBufferPointervARB;
extern PFNGLCOPYBUFFERSUBDATAPROC			qglCopyBufferSubDataARB;

// GL_ARB_map_buffer_Range
extern PFNGLMAPBUFFERRANGEPROC				qglMapBufferRange;

// GL_ARB_draw_elements_base_vertex
extern PFNGLDRAWELEMENTSBASEVERTEXPROC		qglDrawElementsBaseVertex;

// GL_ARB_vertex_array_object
extern PFNGLGENVERTEXARRAYSPROC				qglGenVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC				qglBindVertexArray;
extern PFNGLDELETEVERTEXARRAYSPROC			qglDeleteVertexArrays;

// GL_ARB_vertex_program / GL_ARB_fragment_program
extern PFNGLVERTEXATTRIBPOINTERARBPROC		qglVertexAttribPointerARB;
extern PFNGLENABLEVERTEXATTRIBARRAYARBPROC	qglEnableVertexAttribArrayARB;
extern PFNGLDISABLEVERTEXATTRIBARRAYARBPROC	qglDisableVertexAttribArrayARB;

// GLSL / OpenGL 2.0
extern PFNGLCREATESHADERPROC				qglCreateShader;
extern PFNGLDELETESHADERPROC				qglDeleteShader;
extern PFNGLSHADERSOURCEPROC				qglShaderSource;
extern PFNGLCOMPILESHADERPROC				qglCompileShader;
extern PFNGLGETSHADERIVPROC					qglGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC			qglGetShaderInfoLog;
extern PFNGLCREATEPROGRAMPROC				qglCreateProgram;
extern PFNGLDELETEPROGRAMPROC				qglDeleteProgram;
extern PFNGLATTACHSHADERPROC				qglAttachShader;
extern PFNGLDETACHSHADERPROC				qglDetachShader;
extern PFNGLLINKPROGRAMPROC					qglLinkProgram;
extern PFNGLUSEPROGRAMPROC					qglUseProgram;
extern PFNGLGETPROGRAMIVPROC				qglGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC			qglGetProgramInfoLog;
extern PFNGLPROGRAMPARAMETERIPROC			qglProgramParameteri;
extern PFNGLBINDATTRIBLOCATIONPROC			qglBindAttribLocation;
extern PFNGLGETUNIFORMLOCATIONPROC			qglGetUniformLocation;
extern PFNGLUNIFORM1IPROC					qglUniform1i;
extern PFNGLUNIFORM4FVPROC					qglUniform4fv;
extern PFNGLBINDFRAGDATALOCATIONPROC		qglBindFragDataLocation;

// GL_ARB_uniform_buffer_object
extern PFNGLGETUNIFORMBLOCKINDEXPROC		qglGetUniformBlockIndex;
extern PFNGLUNIFORMBLOCKBINDINGPROC			qglUniformBlockBinding;

// GL_ATI_separate_stencil / OpenGL 2.0 separate stencil
extern PFNGLSTENCILOPSEPARATEATIPROC		qglStencilOpSeparate;
extern PFNGLSTENCILFUNCSEPARATEATIPROC		qglStencilFuncSeparate;

// GL_EXT_depth_bounds_test
extern PFNGLDEPTHBOUNDSEXTPROC              qglDepthBoundsEXT;

// GL_ARB_sync
extern PFNGLFENCESYNCPROC					qglFenceSync;
extern PFNGLISSYNCPROC						qglIsSync;
extern PFNGLCLIENTWAITSYNCPROC				qglClientWaitSync;
extern PFNGLDELETESYNCPROC					qglDeleteSync;

// GL_ARB_occlusion_query
extern PFNGLGENQUERIESARBPROC				qglGenQueriesARB;
extern PFNGLDELETEQUERIESARBPROC			qglDeleteQueriesARB;
extern PFNGLISQUERYARBPROC					qglIsQueryARB;
extern PFNGLBEGINQUERYARBPROC				qglBeginQueryARB;
extern PFNGLENDQUERYARBPROC					qglEndQueryARB;
extern PFNGLGETQUERYIVARBPROC				qglGetQueryivARB;
extern PFNGLGETQUERYOBJECTIVARBPROC			qglGetQueryObjectivARB;
extern PFNGLGETQUERYOBJECTUIVARBPROC		qglGetQueryObjectuivARB;

// GL_ARB_timer_query / GL_EXT_timer_query
extern PFNGLGETQUERYOBJECTUI64VEXTPROC		qglGetQueryObjectui64vEXT;

// GL_ARB_debug_output
extern PFNGLDEBUGMESSAGECONTROLARBPROC		qglDebugMessageControlARB;
extern PFNGLDEBUGMESSAGEINSERTARBPROC		qglDebugMessageInsertARB;
extern PFNGLDEBUGMESSAGECALLBACKARBPROC		qglDebugMessageCallbackARB;
extern PFNGLGETDEBUGMESSAGELOGARBPROC		qglGetDebugMessageLogARB;

//===========================================================================

#include "qgl_linked.h"

#endif
