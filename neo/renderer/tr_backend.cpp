/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/
#include "precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "../framework/HitchTrace.h"

static idCVar r_fillWindowAlphaChan( "r_fillWindowAlphaChan", "-1", CVAR_SYSTEM | CVAR_NOCHEAT | CVAR_ARCHIVE | CVAR_NEW, "Make sure alpha channel of windows default framebuffer is completely opaque at the end of each frame. Needed at least when using Wayland with older drivers.\n 1: do this, 0: don't do it, -1: let the engine decide (default)" );

frameData_t		*frameData;
backEndState_t	backEnd;

/*
======================
RB_SetDefaultGLState

This should initialize all GL state that any part of the entire program
may touch, including the editor.
======================
*/
void RB_SetDefaultGLState( void ) {
	int		i;

	qglClearDepth( 1.0f );
	qglColor4f (1,1,1,1);

	// the vertex array is always enabled
	qglEnableClientState( GL_VERTEX_ARRAY );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	qglDisableClientState( GL_COLOR_ARRAY );

	//
	// make sure our GL state vector is set correctly
	//
	memset( &backEnd.glState, 0, sizeof( backEnd.glState ) );
	backEnd.glState.forceGlState = true;

	qglColorMask( 1, 1, 1, 1 );

	qglEnable( GL_DEPTH_TEST );
	qglEnable( GL_BLEND );
	qglEnable( GL_SCISSOR_TEST );
	qglEnable( GL_CULL_FACE );
	qglDisable( GL_LIGHTING );
	qglDisable( GL_LINE_STIPPLE );
	qglDisable( GL_STENCIL_TEST );

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qglDepthMask( GL_TRUE );
	qglDepthFunc( GL_ALWAYS );

	qglCullFace( GL_FRONT_AND_BACK );
	qglShadeModel( GL_SMOOTH );

	if ( r_useScissor.GetBool() ) {
		qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	}

	for ( i = glConfig.maxTextureUnits - 1 ; i >= 0 ; i-- ) {
		GL_SelectTexture( i );

		// object linear texgen is our default
		qglTexGenf( GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
		qglTexGenf( GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
		qglTexGenf( GL_R, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
		qglTexGenf( GL_Q, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );

		GL_TexEnv( GL_MODULATE );
		qglDisable( GL_TEXTURE_2D );
		if ( glConfig.texture3DAvailable ) {
			qglDisable( GL_TEXTURE_3D );
		}
		if ( glConfig.cubeMapAvailable ) {
			qglDisable( GL_TEXTURE_CUBE_MAP_EXT );
		}
	}
}




//=============================================================================



/*
====================
GL_SelectTexture
====================
*/
void GL_SelectTexture( int unit ) {
	if ( backEnd.glState.currenttmu == unit ) {
		return;
	}

	if ( unit < 0 || (unit >= glConfig.maxTextureUnits && unit >= glConfig.maxTextureImageUnits) ) {
		common->Warning( "GL_SelectTexture: unit = %i", unit );
		return;
	}

	qglActiveTextureARB( GL_TEXTURE0_ARB + unit );
	qglClientActiveTextureARB( GL_TEXTURE0_ARB + unit );

	backEnd.glState.currenttmu = unit;
}


/*
====================
GL_Cull

This handles the flipping needed when the view being
rendered is a mirored view.
====================
*/
void GL_Cull( int cullType ) {
	if ( backEnd.glState.faceCulling == cullType ) {
		return;
	}

	if ( cullType == CT_TWO_SIDED ) {
		qglDisable( GL_CULL_FACE );
	} else  {
		if ( backEnd.glState.faceCulling == CT_TWO_SIDED ) {
			qglEnable( GL_CULL_FACE );
		}

		if ( cullType == CT_BACK_SIDED ) {
			if ( backEnd.viewDef->isMirror ) {
				qglCullFace( GL_FRONT );
			} else {
				qglCullFace( GL_BACK );
			}
		} else {
			if ( backEnd.viewDef->isMirror ) {
				qglCullFace( GL_BACK );
			} else {
				qglCullFace( GL_FRONT );
			}
		}
	}

	backEnd.glState.faceCulling = cullType;
}

/*
====================
GL_TexEnv
====================
*/
void GL_TexEnv( int env ) {
	tmu_t	*tmu;

	tmu = &backEnd.glState.tmu[backEnd.glState.currenttmu];
	if ( env == tmu->texEnv ) {
		return;
	}

	tmu->texEnv = env;

	switch ( env ) {
	case GL_COMBINE_EXT:
	case GL_MODULATE:
	case GL_REPLACE:
	case GL_DECAL:
	case GL_ADD:
		qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, env );
		break;
	default:
		common->Error( "GL_TexEnv: invalid env '%d' passed\n", env );
		break;
	}
}

/*
=================
GL_ClearStateDelta

Clears the state delta bits, so the next GL_State
will set every item
=================
*/
void GL_ClearStateDelta( void ) {
	backEnd.glState.forceGlState = true;
}

/*
====================
GL_State

This routine is responsible for setting the most commonly changed state
====================
*/
void GL_State( int stateBits ) {
	int	diff;

	if ( !r_useStateCaching.GetBool() || backEnd.glState.forceGlState ) {
		// make sure everything is set all the time, so we
		// can see if our delta checking is screwing up
		diff = -1;
		backEnd.glState.forceGlState = false;
	} else {
		diff = stateBits ^ backEnd.glState.glStateBits;
		if ( !diff ) {
			return;
		}
	}

	//
	// check depthFunc bits
	//
	if ( diff & ( GLS_DEPTHFUNC_EQUAL | GLS_DEPTHFUNC_LESS | GLS_DEPTHFUNC_ALWAYS ) ) {
		if ( stateBits & GLS_DEPTHFUNC_EQUAL ) {
			qglDepthFunc( GL_EQUAL );
		} else if ( stateBits & GLS_DEPTHFUNC_ALWAYS ) {
			qglDepthFunc( GL_ALWAYS );
		} else {
			qglDepthFunc( GL_LEQUAL );
		}
	}


	//
	// check blend bits
	//
	if ( diff & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) {
		GLenum srcFactor, dstFactor;

		switch ( stateBits & GLS_SRCBLEND_BITS ) {
		case GLS_SRCBLEND_ZERO:
			srcFactor = GL_ZERO;
			break;
		case GLS_SRCBLEND_ONE:
			srcFactor = GL_ONE;
			break;
		case GLS_SRCBLEND_DST_COLOR:
			srcFactor = GL_DST_COLOR;
			break;
		case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
			srcFactor = GL_ONE_MINUS_DST_COLOR;
			break;
		case GLS_SRCBLEND_SRC_ALPHA:
			srcFactor = GL_SRC_ALPHA;
			break;
		case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
			srcFactor = GL_ONE_MINUS_SRC_ALPHA;
			break;
		case GLS_SRCBLEND_DST_ALPHA:
			srcFactor = GL_DST_ALPHA;
			break;
		case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
			srcFactor = GL_ONE_MINUS_DST_ALPHA;
			break;
		case GLS_SRCBLEND_ALPHA_SATURATE:
			srcFactor = GL_SRC_ALPHA_SATURATE;
			break;
		default:
			srcFactor = GL_ONE;		// to get warning to shut up
			common->Error( "GL_State: invalid src blend state bits\n" );
			break;
		}

		switch ( stateBits & GLS_DSTBLEND_BITS ) {
		case GLS_DSTBLEND_ZERO:
			dstFactor = GL_ZERO;
			break;
		case GLS_DSTBLEND_ONE:
			dstFactor = GL_ONE;
			break;
		case GLS_DSTBLEND_SRC_COLOR:
			dstFactor = GL_SRC_COLOR;
			break;
		case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
			dstFactor = GL_ONE_MINUS_SRC_COLOR;
			break;
		case GLS_DSTBLEND_SRC_ALPHA:
			dstFactor = GL_SRC_ALPHA;
			break;
		case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
			dstFactor = GL_ONE_MINUS_SRC_ALPHA;
			break;
		case GLS_DSTBLEND_DST_ALPHA:
			dstFactor = GL_DST_ALPHA;
			break;
		case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
			dstFactor = GL_ONE_MINUS_DST_ALPHA;
			break;
		default:
			dstFactor = GL_ONE;		// to get warning to shut up
			common->Error( "GL_State: invalid dst blend state bits\n" );
			break;
		}

		qglBlendFunc( srcFactor, dstFactor );
	}

	//
	// check depthmask
	//
	if ( diff & GLS_DEPTHMASK ) {
		if ( stateBits & GLS_DEPTHMASK ) {
			qglDepthMask( GL_FALSE );
		} else {
			qglDepthMask( GL_TRUE );
		}
	}

	//
	// check colormask
	//
	if ( diff & (GLS_REDMASK|GLS_GREENMASK|GLS_BLUEMASK|GLS_ALPHAMASK) ) {
		GLboolean		r, g, b, a;
		r = ( stateBits & GLS_REDMASK ) ? 0 : 1;
		g = ( stateBits & GLS_GREENMASK ) ? 0 : 1;
		b = ( stateBits & GLS_BLUEMASK ) ? 0 : 1;
		a = ( stateBits & GLS_ALPHAMASK ) ? 0 : 1;
		qglColorMask( r, g, b, a );
	}

	//
	// fill/line mode
	//
	if ( diff & GLS_POLYMODE_LINE ) {
		if ( stateBits & GLS_POLYMODE_LINE ) {
			qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		} else {
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	//
	// alpha test
	//
	if ( diff & GLS_ATEST_BITS ) {
		switch ( stateBits & GLS_ATEST_BITS ) {
		case 0:
			qglDisable( GL_ALPHA_TEST );
			break;
		case GLS_ATEST_EQ_255:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_EQUAL, 1 );
			break;
		case GLS_ATEST_LT_128:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_LESS, 0.5 );
			break;
		case GLS_ATEST_GE_128:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GEQUAL, 0.5 );
			break;
		default:
			assert( 0 );
			break;
		}
	}

	backEnd.glState.glStateBits = stateBits;
}




/*
============================================================================

RENDER BACK END THREAD FUNCTIONS

============================================================================
*/

/*
=============
RB_SetGL2D

This is not used by the normal game paths, just by some tools
=============
*/
void RB_SetGL2D( void ) {
	// set 2D virtual screen size
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	if ( r_useScissor.GetBool() ) {
		qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	}
	qglMatrixMode( GL_PROJECTION );
	qglLoadIdentity();
	qglOrtho( 0, 640, 480, 0, 0, 1 );		// always assume 640x480 virtual coordinates
	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity();

	GL_State( GLS_DEPTHFUNC_ALWAYS |
			  GLS_SRCBLEND_SRC_ALPHA |
			  GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	GL_Cull( CT_TWO_SIDED );

	qglDisable( GL_DEPTH_TEST );
	qglDisable( GL_STENCIL_TEST );
}



/*
=============
RB_SetBuffer

=============
*/
static void	RB_SetBuffer( const void *data ) {
	const setBufferCommand_t	*cmd;

	// see which draw buffer we want to render the frame to

	cmd = (const setBufferCommand_t *)data;

	backEnd.frameCount = cmd->frameCount;

	qglDrawBuffer( cmd->buffer );

	// clear screen for debugging
	// automatically enable this with several other debug tools
	// that might leave unrendered portions of the screen
	if ( r_clear.GetFloat() || idStr::Length( r_clear.GetString() ) != 1 || r_lockSurfaces.GetBool() || r_singleArea.GetBool() || r_showOverDraw.GetBool() ) {
		float c[3];
		if ( sscanf( r_clear.GetString(), "%f %f %f", &c[0], &c[1], &c[2] ) == 3 ) {
			qglClearColor( c[0], c[1], c[2], 1 );
		} else if ( r_clear.GetInteger() == 2 ) {
			qglClearColor( 0.0f, 0.0f,  0.0f, 1.0f );
		} else if ( r_showOverDraw.GetBool() ) {
			qglClearColor( 1.0f, 1.0f, 1.0f, 1.0f );
		} else {
			qglClearColor( 0.4f, 0.0f, 0.25f, 1.0f );
		}
		qglClear( GL_COLOR_BUFFER_BIT );
	}
}

/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.
===============
*/
void RB_ShowImages( void ) {
	int		i;
	idImage	*image;
	float	x, y, w, h;
	int		start, end;

	RB_SetGL2D();

	//qglClearColor( 0.2, 0.2, 0.2, 1 );
	//qglClear( GL_COLOR_BUFFER_BIT );

	qglFinish();

	start = Sys_Milliseconds();

	for ( i = 0 ; i < globalImages->images.Num() ; i++ ) {
		image = globalImages->images[i];

		if ( image->texnum == idImage::TEXTURE_NOT_LOADED && image->partialImage == NULL ) {
			continue;
		}

		w = glConfig.vidWidth / 20;
		h = glConfig.vidHeight / 15;
		x = i % 20 * w;
		y = i / 20 * h;

		// show in proportional size in mode 2
		if ( r_showImages.GetInteger() == 2 ) {
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		image->Bind();
		qglBegin (GL_QUADS);
		qglTexCoord2f( 0, 0 );
		qglVertex2f( x, y );
		qglTexCoord2f( 1, 0 );
		qglVertex2f( x + w, y );
		qglTexCoord2f( 1, 1 );
		qglVertex2f( x + w, y + h );
		qglTexCoord2f( 0, 1 );
		qglVertex2f( x, y + h );
		qglEnd();
	}

	qglFinish();

	end = Sys_Milliseconds();
	common->Printf( "%i msec to draw all images\n", end - start );
}


/*
=============
RB_SwapBuffers

=============
*/
const void	RB_SwapBuffers( const void *data ) {
	// texture swapping test
	if ( r_showImages.GetInteger() != 0 ) {
		RB_ShowImages();
	}

	D3::ImGuiHooks::EndFrame();

	int fillAlpha = r_fillWindowAlphaChan.GetInteger();
	if ( fillAlpha == 1 || (fillAlpha == -1 && glConfig.shouldFillWindowAlpha) )
	{
		// make sure the whole alpha chan of the (default) framebuffer is opaque.
		// at least Wayland needs this, see also the big comment in GLimp_Init()

		bool blendEnabled = qglIsEnabled( GL_BLEND );
		if ( !blendEnabled )
			qglEnable( GL_BLEND );

		// TODO: GL_DEPTH_TEST ? (should be disabled, if it needs changing at all)

		bool scissorEnabled = qglIsEnabled( GL_SCISSOR_TEST );
		if( scissorEnabled )
			qglDisable( GL_SCISSOR_TEST );

		bool tex2Denabled = qglIsEnabled( GL_TEXTURE_2D );
		if( tex2Denabled )
			qglDisable( GL_TEXTURE_2D );

		qglDisable( GL_VERTEX_PROGRAM_ARB );
		qglDisable( GL_FRAGMENT_PROGRAM_ARB );

		qglBlendEquation( GL_FUNC_ADD );

		qglBlendFunc( GL_ONE, GL_ONE );

		// setup transform matrices so we can easily/reliably draw a fullscreen quad
		qglMatrixMode( GL_MODELVIEW );
		qglPushMatrix();
		qglLoadIdentity();

		qglMatrixMode( GL_PROJECTION );
		qglPushMatrix();
		qglLoadIdentity();
		qglOrtho( 0, 1, 0, 1, -1, 1 );

		// draw screen-sized quad with color (0.0, 0.0, 0.0, 1.0)
		const float x=0, y=0, w=1, h=1;
		qglColor4f( 0.0f, 0.0f, 0.0f, 1.0f );
		// debug values:
		//const float x = 0.1, y = 0.1, w = 0.8, h = 0.8;
		//qglColor4f( 0.0f, 0.0f, 0.5f, 1.0f );

		qglBegin( GL_QUADS );
			qglVertex2f( x,   y   ); // ( 0,0 );
			qglVertex2f( x,   y+h ); // ( 0,1 );
			qglVertex2f( x+w, y+h ); // ( 1,1 );
			qglVertex2f( x+w, y   ); // ( 1,0 );
		qglEnd();

		// restore previous transform matrix states
		qglPopMatrix(); // for projection
		qglMatrixMode( GL_MODELVIEW );
		qglPopMatrix(); // for modelview

		// restore default or previous states
		qglBlendEquation( GL_FUNC_ADD );
		if ( !blendEnabled )
			qglDisable( GL_BLEND );
		if( tex2Denabled )
			qglEnable( GL_TEXTURE_2D );
		if( scissorEnabled )
			qglEnable( GL_SCISSOR_TEST );
	}

	// force a gl sync if requested
	if ( r_finish.GetBool() ) {
		idHitchScope hitch("render_finish", "explicit_gpu_wait");
		qglFinish();
	}

	// don't flip if drawing to front buffer
	if ( !r_frontBuffer.GetBool() ) {
		idHitchScope hitch("render_present", "swap_buffers");
		GLimp_SwapBuffers();
	}
}

/*
=============
RB_CopyRender

Copy part of the current framebuffer to an image
=============
*/
const void	RB_CopyRender( const void *data ) {
	const copyRenderCommand_t	*cmd;

	cmd = (const copyRenderCommand_t *)data;

	if ( r_skipCopyTexture.GetBool() ) {
		return;
	}

	if (cmd->image) {
		cmd->image->CopyFramebuffer( cmd->x, cmd->y, cmd->imageWidth, cmd->imageHeight, false );
	}
}

// A direct subview owns only its aperture, not its rectangular scissor.
static void RB_PortalAperture(const portalApertureCommand_t *cmd) {
    const int x = tr.viewportOffset[0] + cmd->parent->viewport.x1 + cmd->scissor.x1;
    const int y = tr.viewportOffset[1] + cmd->parent->viewport.y1 + cmd->scissor.y1;
    const int width = cmd->scissor.x2 - cmd->scissor.x1 + 1;
    const int height = cmd->scissor.y2 - cmd->scissor.y1 + 1;
    if (!cmd->restore) {
        GL_SelectTexture(0);
        cmd->image->CopyFramebuffer(x, y, width, height, true, false, false);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        return;
    }
    const drawSurf_t *surf = cmd->surface;
    qglDisable(GL_VERTEX_PROGRAM_ARB); qglDisable(GL_FRAGMENT_PROGRAM_ARB);
    qglDisable(GL_CLIP_PLANE0); qglDisable(GL_ALPHA_TEST);
    qglEnable(GL_SCISSOR_TEST); qglScissor(x, y, width, height);
    qglViewport(tr.viewportOffset[0] + cmd->parent->viewport.x1,
        tr.viewportOffset[1] + cmd->parent->viewport.y1,
        cmd->parent->viewport.x2 - cmd->parent->viewport.x1 + 1,
        cmd->parent->viewport.y2 - cmd->parent->viewport.y1 + 1);
    GL_SelectTexture(1); globalImages->BindNull();
    GL_SelectTexture(0); globalImages->whiteImage->Bind();
    qglDisable(GL_TEXTURE_GEN_S); qglDisable(GL_TEXTURE_GEN_T);
    qglDisable(GL_TEXTURE_GEN_R); qglDisable(GL_TEXTURE_GEN_Q);
    qglMatrixMode(GL_TEXTURE); qglPushMatrix(); qglLoadIdentity();
    qglMatrixMode(GL_PROJECTION); qglPushMatrix(); qglLoadMatrixf(cmd->parent->projectionMatrix);
    qglMatrixMode(GL_MODELVIEW); qglPushMatrix();
    if (surf) qglLoadMatrixf(surf->space->modelViewMatrix); else qglLoadIdentity();
    GL_Cull(CT_TWO_SIDED);
    qglDisable(GL_DEPTH_TEST); qglEnable(GL_STENCIL_TEST);
    qglStencilMask(0xff); qglClearStencil(0); qglClear(GL_STENCIL_BUFFER_BIT);
    qglStencilFunc(GL_ALWAYS, 1, 0xff); qglStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    if (cmd->composite != 1) {
    GL_State(GLS_COLORMASK | GLS_ALPHAMASK | GLS_DEPTHMASK);
    qglDisableClientState(GL_COLOR_ARRAY);
    qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
    const idDrawVert *verts = (const idDrawVert *)vertexCache.Position(surf->geo->ambientCache);
    qglVertexPointer(3, GL_FLOAT, sizeof(idDrawVert), verts->xyz.ToFloatPtr());
    if (r_portalDepthClampAvailable) qglEnable(GL_DEPTH_CLAMP);
    RB_DrawElementsWithCounters(surf->geo);
    if (r_portalDepthClampAvailable) qglDisable(GL_DEPTH_CLAMP);

    }
    // Restore exact pixels, or composite a filtered deep view inside the aperture.
    qglStencilFunc(GL_EQUAL, cmd->composite >= 2 ? 1 : 0, 0xff); qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    if (cmd->composite == 1) qglDisable(GL_STENCIL_TEST);
    GL_State(GLS_DEPTHMASK);
    if (cmd->composite == 2) {
        qglViewport(tr.viewportOffset[0] + cmd->parent->viewport.x1, tr.viewportOffset[1] + cmd->parent->viewport.y1,
            cmd->parent->viewport.x2 - cmd->parent->viewport.x1 + 1, cmd->parent->viewport.y2 - cmd->parent->viewport.y1 + 1);
    } else qglViewport(x, y, width, height);
    qglMatrixMode(GL_PROJECTION); qglLoadIdentity(); qglOrtho(0, 1, 0, 1, -1, 1);
    qglMatrixMode(GL_MODELVIEW); qglLoadIdentity();
    cmd->image->Bind(); GL_TexEnv(GL_REPLACE);
    const float u = float(cmd->composite == 2 ? cmd->textureWidth : width) / cmd->image->uploadWidth;
    const float v = float(cmd->composite == 2 ? cmd->textureHeight : height) / cmd->image->uploadHeight;
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, cmd->composite >= 2 ? GL_LINEAR : GL_NEAREST);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, cmd->composite >= 2 ? GL_LINEAR : GL_NEAREST);
    float u0 = 0, v0 = 0, u1 = u, v1 = v;
    if (cmd->composite == 3) {
        u0 = float(cmd->textureRect.x1 + 0.5f) / cmd->image->uploadWidth;
        v0 = float(cmd->textureRect.y1 + 0.5f) / cmd->image->uploadHeight;
        u1 = float(cmd->textureRect.x2 + 0.5f) / cmd->image->uploadWidth;
        v1 = float(cmd->textureRect.y2 + 0.5f) / cmd->image->uploadHeight;
    }
    qglBegin(GL_QUADS);
    qglTexCoord2f(u0, v0); qglVertex2f(0, 0);
    qglTexCoord2f(u1, v0); qglVertex2f(1, 0);
    qglTexCoord2f(u1, v1); qglVertex2f(1, 1);
    qglTexCoord2f(u0, v1); qglVertex2f(0, 1);
    qglEnd();
    GL_TexEnv(GL_MODULATE);
    qglPopMatrix(); qglMatrixMode(GL_PROJECTION); qglPopMatrix();
    qglMatrixMode(GL_TEXTURE); qglPopMatrix(); qglMatrixMode(GL_MODELVIEW);
    qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
    qglDisable(GL_STENCIL_TEST); qglEnable(GL_DEPTH_TEST);
    GL_State(GLS_DEFAULT);
}

// Isolated color/depth/stencil targets avoid copying the parent's color buffer.
// Functions are optional: the existing scratch/restore path remains available.
static PFNGLGENFRAMEBUFFERSPROC portalGenFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC portalBindFramebuffer;
static PFNGLDELETEFRAMEBUFFERSPROC portalDeleteFramebuffers;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC portalCheckFramebufferStatus;
static PFNGLGENRENDERBUFFERSPROC portalGenRenderbuffers;
static PFNGLBINDRENDERBUFFERPROC portalBindRenderbuffer;
static PFNGLDELETERENDERBUFFERSPROC portalDeleteRenderbuffers;
static PFNGLRENDERBUFFERSTORAGEPROC portalRenderbufferStorage;
static PFNGLFRAMEBUFFERRENDERBUFFERPROC portalFramebufferRenderbuffer;
static bool portalTargetsChecked, portalTargetsAvailable;
static GLuint portalActiveTarget;
static idCVar r_portalDeepTargets("r_portalDeepTargets", "1", CVAR_RENDERER | CVAR_BOOL,
    "use isolated framebuffer targets for reduced-resolution portal views when supported");
struct portalTarget_t { GLuint frame, color, depth, previous; int width, height; bool active; };
static portalTarget_t portalTargets[32];
bool R_PortalTargetsAvailable() {
    if (!portalTargetsChecked) {
        portalTargetsChecked = true;
#define PORTAL_PROC(name, type) portal##name = (type)GLimp_ExtensionPointer("gl" #name)
        PORTAL_PROC(GenFramebuffers, PFNGLGENFRAMEBUFFERSPROC);
        PORTAL_PROC(BindFramebuffer, PFNGLBINDFRAMEBUFFERPROC);
        PORTAL_PROC(DeleteFramebuffers, PFNGLDELETEFRAMEBUFFERSPROC);
        PORTAL_PROC(CheckFramebufferStatus, PFNGLCHECKFRAMEBUFFERSTATUSPROC);
        PORTAL_PROC(GenRenderbuffers, PFNGLGENRENDERBUFFERSPROC);
        PORTAL_PROC(BindRenderbuffer, PFNGLBINDRENDERBUFFERPROC);
        PORTAL_PROC(DeleteRenderbuffers, PFNGLDELETERENDERBUFFERSPROC);
        PORTAL_PROC(RenderbufferStorage, PFNGLRENDERBUFFERSTORAGEPROC);
        PORTAL_PROC(FramebufferRenderbuffer, PFNGLFRAMEBUFFERRENDERBUFFERPROC);
#undef PORTAL_PROC
        portalTargetsAvailable = portalGenFramebuffers && portalBindFramebuffer && portalDeleteFramebuffers &&
            portalCheckFramebufferStatus && portalGenRenderbuffers && portalBindRenderbuffer && portalDeleteRenderbuffers &&
            portalRenderbufferStorage && portalFramebufferRenderbuffer;
    }
    return portalTargetsAvailable && r_portalDeepTargets.GetBool();
}
bool R_PortalTargetActive() { return portalActiveTarget != 0; }
void R_ShutdownPortalTargets() {
    for (int i = 0; i < 32; ++i) {
        portalTarget_t &t = portalTargets[i];
        if (t.frame && portalDeleteFramebuffers) portalDeleteFramebuffers(1, &t.frame);
        if (t.color && portalDeleteRenderbuffers) portalDeleteRenderbuffers(1, &t.color);
        if (t.depth && portalDeleteRenderbuffers) portalDeleteRenderbuffers(1, &t.depth);
    }
    memset(portalTargets, 0, sizeof(portalTargets));
    portalActiveTarget = 0; portalTargetsChecked = portalTargetsAvailable = false;
}
static void RB_PortalTarget(const portalTargetCommand_t *cmd) {
    portalTarget_t &t = portalTargets[cmd->level];
    portalApertureCommand_t backup = {};
    backup.parent = cmd->parent; backup.image = cmd->background;
    backup.scissor.Clear(); backup.scissor.x1 = backup.scissor.y1 = 0;
    backup.scissor.x2 = cmd->parent->viewport.x2 - cmd->parent->viewport.x1;
    backup.scissor.y2 = cmd->parent->viewport.y2 - cmd->parent->viewport.y1;
    backup.composite = 1;
    if (!cmd->begin) {
        if (t.active) {
            portalBindFramebuffer(GL_FRAMEBUFFER, t.previous);
            portalActiveTarget = t.previous;
        } else {
            backup.restore = true; RB_PortalAperture(&backup);
        }
        return;
    }
    t.previous = portalActiveTarget;
    if (!t.frame) {
        portalGenFramebuffers(1, &t.frame);
        portalGenRenderbuffers(1, &t.color);
        portalGenRenderbuffers(1, &t.depth);
    }
    portalBindFramebuffer(GL_FRAMEBUFFER, t.frame);
    if (t.width < cmd->width || t.height < cmd->height) {
        t.width = Max(t.width, cmd->width); t.height = Max(t.height, cmd->height);
        portalBindRenderbuffer(GL_RENDERBUFFER, t.color);
        portalRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, t.width, t.height);
        portalBindRenderbuffer(GL_RENDERBUFFER, t.depth);
        portalRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, t.width, t.height);
        portalFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, t.color);
        portalFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, t.depth);
        portalFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, t.depth);
        portalBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    t.active = portalCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!t.active) {
        portalBindFramebuffer(GL_FRAMEBUFFER, t.previous);
        RB_PortalAperture(&backup);
        return;
    }
    portalActiveTarget = t.frame;
    qglDrawBuffer(GL_COLOR_ATTACHMENT0); qglReadBuffer(GL_COLOR_ATTACHMENT0);
    qglDisable(GL_SCISSOR_TEST);
    GL_State(GLS_DEFAULT);
    qglClearColor(0, 0, 0, 0); qglClear(GL_COLOR_BUFFER_BIT);
    qglEnable(GL_SCISSOR_TEST);
}

/*
====================
RB_ExecuteBackEndCommands

This function will be called syncronously if running without
smp extensions, or asyncronously by another thread.
====================
*/
int		backEndStartTime, backEndFinishTime;
void RB_ExecuteBackEndCommands( const emptyCommand_t *cmds ) {
	// r_debugRenderToTexture
	int	c_draw3d = 0, c_draw2d = 0, c_setBuffers = 0, c_swapBuffers = 0, c_copyRenders = 0;

	if ( cmds->commandId == RC_NOP && !cmds->next ) {
		return;
	}

	backEndStartTime = Sys_Milliseconds();

	// needed for editor rendering
	RB_SetDefaultGLState();

	// upload any image loads that have completed
	globalImages->CompleteBackgroundImageLoads();

	for ( ; cmds ; cmds = (const emptyCommand_t *)cmds->next ) {
		switch ( cmds->commandId ) {
		case RC_NOP:
			break;
		case RC_DRAW_VIEW:
			{
				idHitchScope hitch("render_view", "draw_view");
				RB_DrawView( cmds );
			}
			if ( ((const drawSurfsCommand_t *)cmds)->viewDef->viewEntitys ) {
				c_draw3d++;
			}
			else {
				c_draw2d++;
			}
			break;
		case RC_SET_BUFFER:
			RB_SetBuffer( cmds );
			c_setBuffers++;
			break;
		case RC_SWAP_BUFFERS:
			RB_SwapBuffers( cmds );
			c_swapBuffers++;
			break;
		case RC_PORTAL_TARGET:
            RB_PortalTarget((const portalTargetCommand_t *)cmds);
            break;
		case RC_PORTAL_APERTURE:
            RB_PortalAperture((const portalApertureCommand_t *)cmds);
            break;
		case RC_COPY_RENDER:
			{
				idHitchScope hitch("render_copy", "copy_render");
				RB_CopyRender( cmds );
			}
			c_copyRenders++;
			break;
		default:
			common->Error( "RB_ExecuteBackEndCommands: bad commandId" );
			break;
		}
	}

	// go back to the default texture so the editor doesn't mess up a bound image
	qglBindTexture( GL_TEXTURE_2D, 0 );
	backEnd.glState.tmu[0].current2DMap = -1;

	// stop rendering on this thread
	backEndFinishTime = Sys_Milliseconds();
	backEnd.pc.msec = backEndFinishTime - backEndStartTime;

	if ( r_debugRenderToTexture.GetInteger() == 1 ) {
		common->Printf( "3d: %i, 2d: %i, SetBuf: %i, SwpBuf: %i, CpyRenders: %i, CpyFrameBuf: %i\n", c_draw3d, c_draw2d, c_setBuffers, c_swapBuffers, c_copyRenders, backEnd.c_copyFrameBuffer );
		backEnd.c_copyFrameBuffer = 0;
	}
}
