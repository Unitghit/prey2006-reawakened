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

static bool rbDrawingGlow = false;

extern idCVar r_useCarmacksReverse;
extern idCVar r_useStencilOpSeparate;

/*
=====================
RB_BakeTextureMatrixIntoTexgen
=====================
*/
void RB_BakeTextureMatrixIntoTexgen( idPlane lightProject[3], const float *textureMatrix ) {
	float	genMatrix[16];
	float	final[16];

	genMatrix[0] = lightProject[0][0];
	genMatrix[4] = lightProject[0][1];
	genMatrix[8] = lightProject[0][2];
	genMatrix[12] = lightProject[0][3];

	genMatrix[1] = lightProject[1][0];
	genMatrix[5] = lightProject[1][1];
	genMatrix[9] = lightProject[1][2];
	genMatrix[13] = lightProject[1][3];

	genMatrix[2] = 0;
	genMatrix[6] = 0;
	genMatrix[10] = 0;
	genMatrix[14] = 0;

	genMatrix[3] = lightProject[2][0];
	genMatrix[7] = lightProject[2][1];
	genMatrix[11] = lightProject[2][2];
	genMatrix[15] = lightProject[2][3];

	myGlMultMatrix( genMatrix, backEnd.lightTextureMatrix, final );

	lightProject[0][0] = final[0];
	lightProject[0][1] = final[4];
	lightProject[0][2] = final[8];
	lightProject[0][3] = final[12];

	lightProject[1][0] = final[1];
	lightProject[1][1] = final[5];
	lightProject[1][2] = final[9];
	lightProject[1][3] = final[13];
}

/*
================
RB_PrepareStageTexturing
================
*/
void RB_PrepareStageTexturing( const shaderStage_t *pStage,  const drawSurf_t *surf, idDrawVert *ac ) {
	// set privatePolygonOffset if necessary
	if ( pStage->privatePolygonOffset ) {
		qglEnable( GL_POLYGON_OFFSET_FILL );
		qglPolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * pStage->privatePolygonOffset );
	}

	// set the texture matrix if needed
	if ( pStage->texture.hasMatrix ) {
		RB_LoadShaderTextureMatrix( surf->shaderRegisters, &pStage->texture );
	}

	// texgens
	if ( pStage->texture.texgen == TG_DIFFUSE_CUBE ) {
		qglTexCoordPointer( 3, GL_FLOAT, sizeof( idDrawVert ), ac->normal.ToFloatPtr() );
	}
	if ( pStage->texture.texgen == TG_SKYBOX_CUBE || pStage->texture.texgen == TG_WOBBLESKY_CUBE ) {
		qglTexCoordPointer( 3, GL_FLOAT, 0, vertexCache.Position( surf->dynamicTexCoords ) );
	}
	if ( pStage->texture.texgen == TG_SCREEN ) {
		qglEnable( GL_TEXTURE_GEN_S );
		qglEnable( GL_TEXTURE_GEN_T );
		qglEnable( GL_TEXTURE_GEN_Q );

		float	mat[16], plane[4];
		myGlMultMatrix( surf->space->modelViewMatrix, backEnd.viewDef->projectionMatrix, mat );

		plane[0] = mat[0];
		plane[1] = mat[4];
		plane[2] = mat[8];
		plane[3] = mat[12];
		qglTexGenfv( GL_S, GL_OBJECT_PLANE, plane );

		plane[0] = mat[1];
		plane[1] = mat[5];
		plane[2] = mat[9];
		plane[3] = mat[13];
		qglTexGenfv( GL_T, GL_OBJECT_PLANE, plane );

		plane[0] = mat[3];
		plane[1] = mat[7];
		plane[2] = mat[11];
		plane[3] = mat[15];
		qglTexGenfv( GL_Q, GL_OBJECT_PLANE, plane );
	}

	if ( pStage->texture.texgen == TG_GLASSWARP ) {
		qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, FPROG_GLASSWARP );
		qglEnable( GL_FRAGMENT_PROGRAM_ARB );

		GL_SelectTexture( 2 );
		globalImages->scratchImage->Bind();

		GL_SelectTexture( 1 );
		globalImages->scratchImage2->Bind();

		qglEnable( GL_TEXTURE_GEN_S );
		qglEnable( GL_TEXTURE_GEN_T );
		qglEnable( GL_TEXTURE_GEN_Q );

		float	mat[16], plane[4];
		myGlMultMatrix( surf->space->modelViewMatrix, backEnd.viewDef->projectionMatrix, mat );

		plane[0] = mat[0];
		plane[1] = mat[4];
		plane[2] = mat[8];
		plane[3] = mat[12];
		qglTexGenfv( GL_S, GL_OBJECT_PLANE, plane );

		plane[0] = mat[1];
		plane[1] = mat[5];
		plane[2] = mat[9];
		plane[3] = mat[13];
		qglTexGenfv( GL_T, GL_OBJECT_PLANE, plane );

		plane[0] = mat[3];
		plane[1] = mat[7];
		plane[2] = mat[11];
		plane[3] = mat[15];
		qglTexGenfv( GL_Q, GL_OBJECT_PLANE, plane );

		GL_SelectTexture( 0 );
	}

	if ( pStage->texture.texgen == TG_REFLECT_CUBE ) {
		// see if there is also a bump map specified
		const shaderStage_t *bumpStage = surf->material->GetBumpStage();
		if ( bumpStage ) {
			// per-pixel reflection mapping with bump mapping
			GL_SelectTexture( 1 );
			bumpStage->texture.image->Bind();
			GL_SelectTexture( 0 );

			qglNormalPointer( GL_FLOAT, sizeof( idDrawVert ), ac->normal.ToFloatPtr() );
			qglVertexAttribPointerARB( 10, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->tangents[1].ToFloatPtr() );
			qglVertexAttribPointerARB( 9, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->tangents[0].ToFloatPtr() );

			qglEnableVertexAttribArrayARB( 9 );
			qglEnableVertexAttribArrayARB( 10 );
			qglEnableClientState( GL_NORMAL_ARRAY );

			// Program env 5, 6, 7, 8 have been set in RB_SetProgramEnvironmentSpace

			qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, FPROG_BUMPY_ENVIRONMENT );
			qglEnable( GL_FRAGMENT_PROGRAM_ARB );
			qglBindProgramARB( GL_VERTEX_PROGRAM_ARB, VPROG_BUMPY_ENVIRONMENT );
			qglEnable( GL_VERTEX_PROGRAM_ARB );
		} else {
			// per-pixel reflection mapping without a normal map
			qglNormalPointer( GL_FLOAT, sizeof( idDrawVert ), ac->normal.ToFloatPtr() );
			qglEnableClientState( GL_NORMAL_ARRAY );

			qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, FPROG_ENVIRONMENT );
			qglEnable( GL_FRAGMENT_PROGRAM_ARB );
			qglBindProgramARB( GL_VERTEX_PROGRAM_ARB, VPROG_ENVIRONMENT );
			qglEnable( GL_VERTEX_PROGRAM_ARB );
		}
	}
}

/*
================
RB_FinishStageTexturing
================
*/
void RB_FinishStageTexturing( const shaderStage_t *pStage, const drawSurf_t *surf, idDrawVert *ac ) {
	// unset privatePolygonOffset if necessary
	if ( pStage->privatePolygonOffset && !surf->material->TestMaterialFlag(MF_POLYGONOFFSET) ) {
		qglDisable( GL_POLYGON_OFFSET_FILL );
	}

	if ( pStage->texture.texgen == TG_DIFFUSE_CUBE || pStage->texture.texgen == TG_SKYBOX_CUBE
		|| pStage->texture.texgen == TG_WOBBLESKY_CUBE ) {
		qglTexCoordPointer( 2, GL_FLOAT, sizeof( idDrawVert ), (void *)&ac->st );
	}

	if ( pStage->texture.texgen == TG_SCREEN ) {
		qglDisable( GL_TEXTURE_GEN_S );
		qglDisable( GL_TEXTURE_GEN_T );
		qglDisable( GL_TEXTURE_GEN_Q );
	}

	if ( pStage->texture.texgen == TG_GLASSWARP ) {
		GL_SelectTexture( 2 );
		globalImages->BindNull();

		GL_SelectTexture( 1 );
		if ( pStage->texture.hasMatrix ) {
			RB_LoadShaderTextureMatrix( surf->shaderRegisters, &pStage->texture );
		}
		qglDisable( GL_TEXTURE_GEN_S );
		qglDisable( GL_TEXTURE_GEN_T );
		qglDisable( GL_TEXTURE_GEN_Q );
		qglDisable( GL_FRAGMENT_PROGRAM_ARB );
		globalImages->BindNull();
		GL_SelectTexture( 0 );
	}

	if ( pStage->texture.texgen == TG_REFLECT_CUBE ) {
		// see if there is also a bump map specified
		const shaderStage_t *bumpStage = surf->material->GetBumpStage();
		if ( bumpStage ) {
			// per-pixel reflection mapping with bump mapping
			GL_SelectTexture( 1 );
			globalImages->BindNull();
			GL_SelectTexture( 0 );

			qglDisableVertexAttribArrayARB( 9 );
			qglDisableVertexAttribArrayARB( 10 );
		} else {
			// per-pixel reflection mapping without bump mapping
		}

		qglDisableClientState( GL_NORMAL_ARRAY );
		qglDisable( GL_FRAGMENT_PROGRAM_ARB );
		qglDisable( GL_VERTEX_PROGRAM_ARB );
		// Fixme: Hack to get around an apparent bug in ATI drivers.  Should remove as soon as it gets fixed.
		qglBindProgramARB( GL_VERTEX_PROGRAM_ARB, 0 ); // FIXME ...
	}

	if ( pStage->texture.hasMatrix ) {
		qglMatrixMode( GL_TEXTURE );
		qglLoadIdentity();
		qglMatrixMode( GL_MODELVIEW );
	}
}

/*
=============================================================================================

FILL DEPTH BUFFER

=============================================================================================
*/


/*
==================
RB_T_FillDepthBuffer
==================
*/
void RB_T_FillDepthBuffer( const drawSurf_t *surf ) {
	int			stage;
	const idMaterial	*shader;
	const shaderStage_t *pStage;
	const float	*regs;
	float		color[4];
	const srfTriangles_t	*tri;

	tri = surf->geo;
	shader = surf->material;

	// update the clip plane if needed
	if ( backEnd.viewDef->numClipPlanes && surf->space != backEnd.currentSpace ) {
		GL_SelectTexture( 1 );

		idPlane	plane;

		R_GlobalPlaneToLocal( surf->space->modelMatrix, backEnd.viewDef->clipPlanes[0], plane );
		plane[3] += 0.5;	// the notch is in the middle
		qglTexGenfv( GL_S, GL_OBJECT_PLANE, plane.ToFloatPtr() );
		GL_SelectTexture( 0 );
	}

	if ( !shader->IsDrawn() ) {
		return;
	}

	// some deforms may disable themselves by setting numIndexes = 0
	if ( !tri->numIndexes ) {
		return;
	}

	// translucent surfaces don't put anything in the depth buffer and don't
	// test against it, which makes them fail the mirror clip plane operation
	if ( shader->Coverage() == MC_TRANSLUCENT ) {
		return;
	}

	if ( !tri->ambientCache ) {
		common->Printf( "RB_T_FillDepthBuffer: !tri->ambientCache\n" );
		return;
	}

	// get the expressions for conditionals / color / texcoords
	regs = surf->shaderRegisters;

	// if all stages of a material have been conditioned off, don't do anything
	for ( stage = 0; stage < shader->GetNumStages() ; stage++ ) {
		pStage = shader->GetStage(stage);
		// check the stage enable condition
		if ( regs[ pStage->conditionRegister ] != 0 ) {
			break;
		}
	}
	if ( stage == shader->GetNumStages() ) {
		return;
	}

	// set polygon offset if necessary
	if ( shader->TestMaterialFlag(MF_POLYGONOFFSET) ) {
		qglEnable( GL_POLYGON_OFFSET_FILL );
		qglPolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset() );
	}

	// subviews will just down-modulate the color buffer by overbright
	const bool clampPortalDepth = r_portalDepthClampAvailable &&
		shader->GetSort() == SS_SUBVIEW && shader->GetSubviewClass() == SC_PORTAL;
	// Preserve the planar aperture when it lies between the eye and zNear.
	// Without this, local geometry behind the portal flashes before teleport.
	if (clampPortalDepth) qglEnable(GL_DEPTH_CLAMP);
	if ( shader->GetSort() == SS_SUBVIEW ) {
		GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO | GLS_DEPTHFUNC_LESS );
		color[0] =
		color[1] =
		color[2] = ( 1.0 / backEnd.overBright );
		color[3] = 1;
	} else {
		// others just draw black
		color[0] = 0;
		color[1] = 0;
		color[2] = 0;
		color[3] = 1;
	}

	idDrawVert *ac = (idDrawVert *)vertexCache.Position( tri->ambientCache );
	qglVertexPointer( 3, GL_FLOAT, sizeof( idDrawVert ), ac->xyz.ToFloatPtr() );
	qglTexCoordPointer( 2, GL_FLOAT, sizeof( idDrawVert ), reinterpret_cast<void *>(&ac->st) );

	bool drawSolid = false;

	if ( shader->Coverage() == MC_OPAQUE ) {
		drawSolid = true;
	}

	// we may have multiple alpha tested stages
	if ( shader->Coverage() == MC_PERFORATED ) {
		// if the only alpha tested stages are condition register omitted,
		// draw a normal opaque surface
		bool	didDraw = false;

		qglEnable( GL_ALPHA_TEST );
		// perforated surfaces may have multiple alpha tested stages
		for ( stage = 0; stage < shader->GetNumStages() ; stage++ ) {
			pStage = shader->GetStage(stage);

			if ( !pStage->hasAlphaTest ) {
				continue;
			}

			// check the stage enable condition
			if ( regs[ pStage->conditionRegister ] == 0 ) {
				continue;
			}

			// if we at least tried to draw an alpha tested stage,
			// we won't draw the opaque surface
			didDraw = true;

			// set the alpha modulate
			color[3] = regs[ pStage->color.registers[3] ];

			// skip the entire stage if alpha would be black
			if ( color[3] <= 0 ) {
				continue;
			}
			qglColor4fv( color );

			qglAlphaFunc( GL_GREATER, regs[ pStage->alphaTestRegister ] );

			// bind the texture
			pStage->texture.image->Bind();

			// set texture matrix and texGens
			RB_PrepareStageTexturing( pStage, surf, ac );

			// draw it
			RB_DrawElementsWithCounters( tri );

			RB_FinishStageTexturing( pStage, surf, ac );
		}
		qglDisable( GL_ALPHA_TEST );
		if ( !didDraw ) {
			drawSolid = true;
		}
	}

	// draw the entire surface solid
	if ( drawSolid ) {
		qglColor4fv( color );
		globalImages->whiteImage->Bind();
		// Unit 1's alpha notch is the subview clip plane. Opaque geometry
		// must test it too, otherwise walls between the virtual eye and the
		// destination portal write depth and obscure the remote scene.
		if ( backEnd.viewDef->numClipPlanes ) {
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GREATER, 0.5f );
		}

		// draw it
		RB_DrawElementsWithCounters( tri );
		if ( backEnd.viewDef->numClipPlanes ) {
			qglDisable( GL_ALPHA_TEST );
		}
	}


	// reset polygon offset
	if (clampPortalDepth) qglDisable(GL_DEPTH_CLAMP);
	if ( shader->TestMaterialFlag(MF_POLYGONOFFSET) ) {
		qglDisable( GL_POLYGON_OFFSET_FILL );
	}

	// reset blending
	if ( shader->GetSort() == SS_SUBVIEW ) {
		GL_State( GLS_DEPTHFUNC_LESS );
	}

}

/*
=====================
RB_STD_FillDepthBuffer

If we are rendering a subview with a near clip plane, use a second texture
to force the alpha test to fail when behind that clip plane
=====================
*/
void RB_STD_FillDepthBuffer( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	// if we are just doing 2D rendering, no need to fill the depth buffer
	if ( !backEnd.viewDef->viewEntitys ) {
		return;
	}

	// enable the second texture for mirror plane clipping if needed
	if ( backEnd.viewDef->numClipPlanes ) {
		GL_SelectTexture( 1 );
		globalImages->alphaNotchImage->Bind();
		qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		qglEnable( GL_TEXTURE_GEN_S );
		qglTexCoord2f( 1, 0.5 );
	}

	// the first texture will be used for alpha tested surfaces
	GL_SelectTexture( 0 );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );

	// decal surfaces may enable polygon offset
	qglPolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() );

	GL_State( GLS_DEPTHFUNC_LESS );

	// Enable stencil test if we are going to be using it for shadows.
	// If we didn't do this, it would be legal behavior to get z fighting
	// from the ambient pass and the light passes.
	qglEnable( GL_STENCIL_TEST );
	qglStencilFunc( GL_ALWAYS, 1, 255 );

	RB_RenderDrawSurfListWithFunction( drawSurfs, numDrawSurfs, RB_T_FillDepthBuffer );

	if ( backEnd.viewDef->numClipPlanes ) {
		GL_SelectTexture( 1 );
		globalImages->BindNull();
		qglDisable( GL_TEXTURE_GEN_S );
		GL_SelectTexture( 0 );
	}

}

/*
=============================================================================================

SHADER PASSES

=============================================================================================
*/

/*
==================
RB_SetProgramEnvironment

Sets variables that can be used by all vertex programs
==================
*/
void RB_SetProgramEnvironment( bool isPostProcess ) {
	float	parm[4];
	int		pot;

	if ( !glConfig.ARBVertexProgramAvailable ) {
		return;
	}

	// screen power of two correction factor, assuming the copy to _currentRender
	// also copied an extra row and column for the bilerp
	int	 w = backEnd.viewDef->viewport.x2 - backEnd.viewDef->viewport.x1 + 1;
	pot = globalImages->currentRenderImage->uploadWidth;
	parm[0] = (float)w / pot;

	int	 h = backEnd.viewDef->viewport.y2 - backEnd.viewDef->viewport.y1 + 1;
	pot = globalImages->currentRenderImage->uploadHeight;
	parm[1] = (float)h / pot;

	parm[2] = 0;
	parm[3] = 1;
	qglProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, 0, parm );

	qglProgramEnvParameter4fvARB( GL_FRAGMENT_PROGRAM_ARB, 0, parm );

	// window coord to 0.0 to 1.0 conversion
	parm[0] = 1.0 / w;
	parm[1] = 1.0 / h;
	parm[2] = 0;
	parm[3] = 1;
	qglProgramEnvParameter4fvARB( GL_FRAGMENT_PROGRAM_ARB, 1, parm );

	// DG: brightness and gamma in shader as program.env[4]
	if ( r_gammaInShader.GetBool() ) {
		// program.env[4].xyz are all r_brightness, program.env[4].w is 1.0/r_gamma
		if ( !isPostProcess ) {
			parm[0] = parm[1] = parm[2] = r_brightness.GetFloat();
			parm[3] = 1.0/r_gamma.GetFloat(); // 1.0/gamma so the shader doesn't have to do this calculation
		} else {
			// don't apply gamma/brightness in postprocess passes to avoid applying them twice
			// (setting them to 1.0 makes them no-ops)
			parm[0] = parm[1] = parm[2] = parm[3] = 1.0f;
		}
		qglProgramEnvParameter4fvARB( GL_FRAGMENT_PROGRAM_ARB, PP_GAMMA_BRIGHTNESS, parm );
	}

	//
	// set eye position in global space
	//
	parm[0] = backEnd.viewDef->renderView.vieworg[0];
	parm[1] = backEnd.viewDef->renderView.vieworg[1];
	parm[2] = backEnd.viewDef->renderView.vieworg[2];
	parm[3] = 1.0;
	qglProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, 1, parm );
}

/*
==================
RB_SetProgramEnvironmentSpace

Sets variables related to the current space that can be used by all vertex programs
==================
*/
void RB_SetProgramEnvironmentSpace( void ) {
	if ( !glConfig.ARBVertexProgramAvailable ) {
		return;
	}

	const struct viewEntity_s *space = backEnd.currentSpace;
	float	parm[4];

	// set eye position in local space
	R_GlobalPointToLocal( space->modelMatrix, backEnd.viewDef->renderView.vieworg, *(idVec3 *)parm );
	parm[3] = 1.0;
	qglProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, 5, parm );

	// we need the model matrix without it being combined with the view matrix
	// so we can transform local vectors to global coordinates
	parm[0] = space->modelMatrix[0];
	parm[1] = space->modelMatrix[4];
	parm[2] = space->modelMatrix[8];
	parm[3] = space->modelMatrix[12];
	qglProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, 6, parm );
	parm[0] = space->modelMatrix[1];
	parm[1] = space->modelMatrix[5];
	parm[2] = space->modelMatrix[9];
	parm[3] = space->modelMatrix[13];
	qglProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, 7, parm );
	parm[0] = space->modelMatrix[2];
	parm[1] = space->modelMatrix[6];
	parm[2] = space->modelMatrix[10];
	parm[3] = space->modelMatrix[14];
	qglProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, 8, parm );
}

/*
==================
RB_STD_T_RenderShaderPasses

This is also called for the generated 2D rendering
==================
*/
void RB_STD_T_RenderShaderPasses( const drawSurf_t *surf ) {
	int			stage;
	const idMaterial	*shader;
	const shaderStage_t *pStage;
	const float	*regs;
	float		color[4];
	const srfTriangles_t	*tri;

	tri = surf->geo;
	shader = surf->material;

	if ( !shader->HasAmbient() ) {
		return;
	}

	if ( shader->IsPortalSky() ) {
		return;
	}

	// change the matrix if needed
	if ( surf->space != backEnd.currentSpace ) {
		qglLoadMatrixf( surf->space->modelViewMatrix );
		backEnd.currentSpace = surf->space;
		RB_SetProgramEnvironmentSpace();
	}

	// change the scissor if needed
	if ( r_useScissor.GetBool() && !backEnd.currentScissor.Equals( surf->scissorRect ) ) {
		backEnd.currentScissor = surf->scissorRect;
		qglScissor( backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1,
			backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
			backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
			backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1 );
	}

	// some deforms may disable themselves by setting numIndexes = 0
	if ( !tri->numIndexes ) {
		return;
	}

	if ( !tri->ambientCache ) {
		common->Printf( "RB_T_RenderShaderPasses: !tri->ambientCache\n" );
		return;
	}

	// get the expressions for conditionals / color / texcoords
	regs = surf->shaderRegisters;

	// set face culling appropriately
	GL_Cull( shader->GetCullType() );

	// set polygon offset if necessary
	if ( shader->TestMaterialFlag(MF_POLYGONOFFSET) ) {
		qglEnable( GL_POLYGON_OFFSET_FILL );
		qglPolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset() );
	}

	if ( surf->space->weaponDepthHack ) {
		RB_EnterWeaponDepthHack();
	}

	if ( surf->space->modelDepthHack != 0.0f ) {
		RB_EnterModelDepthHack( surf->space->modelDepthHack );
	}

	idDrawVert *ac = (idDrawVert *)vertexCache.Position( tri->ambientCache );
	qglVertexPointer( 3, GL_FLOAT, sizeof( idDrawVert ), ac->xyz.ToFloatPtr() );
	qglTexCoordPointer( 2, GL_FLOAT, sizeof( idDrawVert ), reinterpret_cast<void *>(&ac->st) );

	for ( stage = 0; stage < shader->GetNumStages() ; stage++ ) {
		pStage = shader->GetStage(stage);

		if ( tr.IsScopeView() ) {
			if ( pStage->isNotScopeView ) {
				continue;
			}
		} else {
			if ( pStage->isScopeView ) {
				continue;
			}
		}

		if ( !tr.IsShuttleView() ) {
			if ( pStage->isShuttleView ) {
				continue;
			}
		}

		if ( backEnd.viewDef->renderView.viewSpiritEntities ) {
			if ( pStage->isNotSpiritWalk ) {
				continue;
			}
		} else {
			if ( pStage->isSpiritWalk ) {
				continue;
			}
		}

		if ( ( rbDrawingGlow && !pStage->isGlow ) || ( pStage->isGlow && r_skipGlowOverlay.GetBool() ) ) {
			continue;
		}

		// check the enable condition
		if ( regs[ pStage->conditionRegister ] == 0 ) {
			continue;
		}

		// skip the stages involved in lighting
		if ( pStage->lighting != SL_AMBIENT ) {
			continue;
		}

		// skip if the stage is ( GL_ZERO, GL_ONE ), which is used for some alpha masks
		if ( ( pStage->drawStateBits & (GLS_SRCBLEND_BITS|GLS_DSTBLEND_BITS) ) == ( GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE ) ) {
			continue;
		}

		// see if we are a new-style stage
		newShaderStage_t *newStage = pStage->newStage;
		if ( newStage ) {
			//--------------------------
			//
			// new style stages
			//
			//--------------------------

			if ( r_skipNewAmbient.GetBool() || r_shaderlevel.GetInteger() < 1 ) {
				continue;
			}

			qglColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( idDrawVert ), (void *)&ac->color );
			qglVertexAttribPointerARB( 9, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->tangents[0].ToFloatPtr() );
			qglVertexAttribPointerARB( 10, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->tangents[1].ToFloatPtr() );
			qglNormalPointer( GL_FLOAT, sizeof( idDrawVert ), ac->normal.ToFloatPtr() );

			qglEnableClientState( GL_COLOR_ARRAY );
			qglEnableVertexAttribArrayARB( 9 );
			qglEnableVertexAttribArrayARB( 10 );
			qglEnableClientState( GL_NORMAL_ARRAY );

			GL_State( pStage->drawStateBits );

			qglBindProgramARB( GL_VERTEX_PROGRAM_ARB, newStage->vertexProgram );
			qglEnable( GL_VERTEX_PROGRAM_ARB );
            qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, newStage->fragmentProgram );
            qglEnable( GL_FRAGMENT_PROGRAM_ARB );

			// megaTextures bind a lot of images and set a lot of parameters
			if ( newStage->megaTexture ) {
				newStage->megaTexture->SetMappingForSurface( tri );
				idVec3	localViewer;
				R_GlobalPointToLocal( surf->space->modelMatrix, backEnd.viewDef->renderView.vieworg, localViewer );
				newStage->megaTexture->BindForViewOrigin( localViewer );
			}

			for ( int i = 0 ; i < newStage->numVertexParms ; i++ ) {
				float	parm[4];
				parm[0] = regs[ newStage->vertexParms[i][0] ];
				parm[1] = regs[ newStage->vertexParms[i][1] ];
				parm[2] = regs[ newStage->vertexParms[i][2] ];
				parm[3] = regs[ newStage->vertexParms[i][3] ];
				qglProgramLocalParameter4fvARB( GL_VERTEX_PROGRAM_ARB, i, parm );
			}
            for ( int i = 0; i < newStage->numFragmentParms; i++ ) {
                float	parm[4];
                parm[0] = regs[ newStage->fragmentParms[i][0] ];
                parm[1] = regs[ newStage->fragmentParms[i][1] ];
                parm[2] = regs[ newStage->fragmentParms[i][2] ];
                parm[3] = regs[ newStage->fragmentParms[i][3] ];
                qglProgramLocalParameter4fvARB( GL_FRAGMENT_PROGRAM_ARB, i, parm );
            }

			for ( int i = 0 ; i < newStage->numFragmentProgramImages ; i++ ) {
				if ( newStage->fragmentProgramImages[i] ) {
					GL_SelectTexture( i );
					newStage->fragmentProgramImages[i]->Bind();
				}
			}

			// draw it
			RB_DrawElementsWithCounters( tri );

			for ( int i = 1 ; i < newStage->numFragmentProgramImages ; i++ ) {
				if ( newStage->fragmentProgramImages[i] ) {
					GL_SelectTexture( i );
					globalImages->BindNull();
				}
			}
			if ( newStage->megaTexture ) {
				newStage->megaTexture->Unbind();
			}

			GL_SelectTexture( 0 );

			qglDisable( GL_VERTEX_PROGRAM_ARB );
			qglDisable( GL_FRAGMENT_PROGRAM_ARB );
			// Fixme: Hack to get around an apparent bug in ATI drivers.  Should remove as soon as it gets fixed.
			qglBindProgramARB( GL_VERTEX_PROGRAM_ARB, 0 ); // FIXME: ...

			qglDisableClientState( GL_COLOR_ARRAY );
			qglDisableVertexAttribArrayARB( 9 );
			qglDisableVertexAttribArrayARB( 10 );
			qglDisableClientState( GL_NORMAL_ARRAY );
			continue;
		}

		//--------------------------
		//
		// old style stages
		//
		//--------------------------

		// set the color
		color[0] = regs[ pStage->color.registers[0] ];
		color[1] = regs[ pStage->color.registers[1] ];
		color[2] = regs[ pStage->color.registers[2] ];
		color[3] = regs[ pStage->color.registers[3] ];

		// skip the entire stage if an add would be black
		if ( ( pStage->drawStateBits & (GLS_SRCBLEND_BITS|GLS_DSTBLEND_BITS) ) == ( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE )
			&& color[0] <= 0 && color[1] <= 0 && color[2] <= 0 ) {
			continue;
		}

		// skip the entire stage if a blend would be completely transparent
		if ( ( pStage->drawStateBits & (GLS_SRCBLEND_BITS|GLS_DSTBLEND_BITS) ) == ( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA )
			&& color[3] <= 0 ) {
			continue;
		}

		// select the vertex color source
		if ( pStage->vertexColor == SVC_IGNORE ) {
			qglColor4fv( color );
		} else {
			qglColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( idDrawVert ), (void *)&ac->color );
			qglEnableClientState( GL_COLOR_ARRAY );

			if ( pStage->vertexColor == SVC_INVERSE_MODULATE ) {
				GL_TexEnv( GL_COMBINE_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PRIMARY_COLOR_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_ONE_MINUS_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1 );
			}

			// for vertex color and modulated color, we need to enable a second
			// texture stage
			if ( color[0] != 1 || color[1] != 1 || color[2] != 1 || color[3] != 1 ) {
				GL_SelectTexture( 1 );

				globalImages->whiteImage->Bind();
				GL_TexEnv( GL_COMBINE_ARB );

				qglTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color );

				qglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_CONSTANT_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR );
				qglTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1 );

				qglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PREVIOUS_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_CONSTANT_ARB );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA );
				qglTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA );
				qglTexEnvi( GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1 );

				GL_SelectTexture( 0 );
			}
		}

		// bind the texture
		RB_BindVariableStageImage( &pStage->texture, regs );

		// set the state
		GL_State( pStage->drawStateBits );

		RB_PrepareStageTexturing( pStage, surf, ac );

		// draw it
		RB_DrawElementsWithCounters( tri );

		RB_FinishStageTexturing( pStage, surf, ac );

		if ( pStage->vertexColor != SVC_IGNORE ) {
			qglDisableClientState( GL_COLOR_ARRAY );

			GL_SelectTexture( 1 );
			GL_TexEnv( GL_MODULATE );
			globalImages->BindNull();
			GL_SelectTexture( 0 );
			GL_TexEnv( GL_MODULATE );
		}
	}

	// reset polygon offset
	if ( shader->TestMaterialFlag(MF_POLYGONOFFSET) ) {
		qglDisable( GL_POLYGON_OFFSET_FILL );
	}
	if ( surf->space->weaponDepthHack || surf->space->modelDepthHack != 0.0f ) {
		RB_LeaveDepthHack();
	}
}

/*
=====================
RB_STD_DrawShaderPasses

Draw non-light dependent passes
=====================
*/
// Depth clipping alone cannot reject translucent/additive geometry. Define the
// subview plane in eye space once, independent of each surface's model matrix.
// Keep it out of screen-space bloom and stencil shadow volumes. World-space
// postprocess surfaces (glass/refraction) must still obey the portal plane.
void RB_SetSubviewClipPlane( bool enable ) {
	if ( !enable || !backEnd.viewDef->numClipPlanes ) {
		qglDisable( GL_CLIP_PLANE0 );
		return;
	}
	const idPlane &plane = backEnd.viewDef->clipPlanes[0];
	const GLdouble equation[4] = { plane[0], plane[1], plane[2], plane[3] };
	qglMatrixMode( GL_MODELVIEW );
	qglPushMatrix();
	qglLoadMatrixf( backEnd.viewDef->worldSpace.modelViewMatrix );
	qglClipPlane( GL_CLIP_PLANE0, equation );
	qglPopMatrix();
	qglEnable( GL_CLIP_PLANE0 );
}

int RB_STD_DrawShaderPasses( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	int				i;

	// only obey skipAmbient if we are rendering a view
	if ( backEnd.viewDef->viewEntitys && r_skipAmbient.GetBool() ) {
		return numDrawSurfs;
	}

	bool isPostProcess = false;

	// if we are about to draw the first surface that needs
	// the rendering in a texture, copy it over
	if ( drawSurfs[0]->material->GetSort() >= SS_POST_PROCESS ) {
		if ( r_skipPostProcess.GetBool() ) {
			return 0;
		}
		isPostProcess = true;

		// only dump if in a 3d view
#if 0 //karin: 2D spiritwalk and deathwalk on Prey
		if ( backEnd.viewDef->viewEntitys )
#endif
        {
			globalImages->currentRenderImage->CopyFramebuffer( backEnd.viewDef->viewport.x1,
				backEnd.viewDef->viewport.y1,  backEnd.viewDef->viewport.x2 -  backEnd.viewDef->viewport.x1 + 1,
				backEnd.viewDef->viewport.y2 -  backEnd.viewDef->viewport.y1 + 1, true );
		}
		backEnd.currentRenderCopied = true;
	}

	GL_SelectTexture( 1 );
	globalImages->BindNull();

	GL_SelectTexture( 0 );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );

	RB_SetProgramEnvironment( isPostProcess );

	// we don't use RB_RenderDrawSurfListWithFunction()
	// because we want to defer the matrix load because many
	// surfaces won't draw any ambient passes
	backEnd.currentSpace = NULL;
	RB_SetSubviewClipPlane( true );
	for (i = 0  ; i < numDrawSurfs ; i++ ) {
		if ( drawSurfs[i]->material->SuppressInSubview() ) {
			continue;
		}

		if ( backEnd.viewDef->isXraySubview && drawSurfs[i]->space->entityDef ) {
			if ( drawSurfs[i]->space->xrayIndex != 2 ) {
				continue;
			}
		}

		// we need to draw the post process shaders after we have drawn the fog lights
		if ( drawSurfs[i]->material->GetSort() >= SS_POST_PROCESS
			&& !backEnd.currentRenderCopied ) {
			break;
		}

		RB_STD_T_RenderShaderPasses( drawSurfs[i] );
	}

	RB_SetSubviewClipPlane( false );
	GL_Cull( CT_FRONT_SIDED );
	qglColor3f( 1, 1, 1 );

	return i;
}



/*
==============================================================================

BACK END RENDERING OF STENCIL SHADOWS

==============================================================================
*/

/*
=====================
RB_T_Shadow

the shadow volumes face INSIDE
=====================
*/
static void RB_T_Shadow( const drawSurf_t *surf ) {
	const srfTriangles_t	*tri;

	// set the light position if we are using a vertex program to project the rear surfaces
	if ( r_useShadowVertexProgram.GetBool()
		&& surf->space != backEnd.currentSpace ) {
		idVec4 localLight;

		R_GlobalPointToLocal( surf->space->modelMatrix, backEnd.vLight->globalLightOrigin, localLight.ToVec3() );
		localLight.w = 0.0f;
		qglProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_LIGHT_ORIGIN, localLight.ToFloatPtr() );
	}

	tri = surf->geo;

	if ( !tri->shadowCache ) {
		return;
	}

	qglVertexPointer( 4, GL_FLOAT, sizeof( shadowCache_t ), vertexCache.Position(tri->shadowCache) );

	// we always draw the sil planes, but we may not need to draw the front or rear caps
	int	numIndexes;
	bool external = false;

	// A clipped subview can expose shadow volumes behind its destination plane.
	// The ordinary outside-volume tests do not account for that extra clipping
	// boundary. Keep closed volumes and use z-fail instead of the capless path.
	if ( !r_useExternalShadows.GetInteger() || backEnd.viewDef->numClipPlanes > 0 ) {
		numIndexes = tri->numIndexes;
	} else if ( r_useExternalShadows.GetInteger() == 2 ) { // force to no caps for testing
		numIndexes = tri->numShadowIndexesNoCaps;
	} else if ( !(surf->dsFlags & DSF_VIEW_INSIDE_SHADOW) ) {
		// if we aren't inside the shadow projection, no caps are ever needed needed
		numIndexes = tri->numShadowIndexesNoCaps;
		external = true;
	} else if ( !backEnd.vLight->viewInsideLight && !(surf->geo->shadowCapPlaneBits & SHADOW_CAP_INFINITE) ) {
		// if we are inside the shadow projection, but outside the light, and drawing
		// a non-infinite shadow, we can skip some caps
		if ( backEnd.vLight->viewSeesShadowPlaneBits & surf->geo->shadowCapPlaneBits ) {
			// we can see through a rear cap, so we need to draw it, but we can skip the
			// caps on the actual surface
			numIndexes = tri->numShadowIndexesNoFrontCaps;
		} else {
			// we don't need to draw any caps
			numIndexes = tri->numShadowIndexesNoCaps;
		}
		external = true;
	} else {
		// must draw everything
		numIndexes = tri->numIndexes;
	}

	// set depth bounds
	if( glConfig.depthBoundsTestAvailable && r_useDepthBoundsTest.GetBool() ) {
		const float nearDepth = backEnd.worldDepthNear;
		qglDepthBoundsEXT( nearDepth + (1.0f - nearDepth) * surf->scissorRect.zmin,
			nearDepth + (1.0f - nearDepth) * surf->scissorRect.zmax );
	}

	// debug visualization
	if ( r_showShadows.GetInteger() ) {
		if ( r_showShadows.GetInteger() == 3 ) {
			if ( external ) {
				qglColor3f( 0.1/backEnd.overBright, 1/backEnd.overBright, 0.1/backEnd.overBright );
			} else {
				// these are the surfaces that require the reverse
				qglColor3f( 1/backEnd.overBright, 0.1/backEnd.overBright, 0.1/backEnd.overBright );
			}
		} else {
			// draw different color for turboshadows
			if ( surf->geo->shadowCapPlaneBits & SHADOW_CAP_INFINITE ) {
				if ( numIndexes == tri->numIndexes ) {
					qglColor3f( 1/backEnd.overBright, 0.1/backEnd.overBright, 0.1/backEnd.overBright );
				} else {
					qglColor3f( 1/backEnd.overBright, 0.4/backEnd.overBright, 0.1/backEnd.overBright );
				}
			} else {
				if ( numIndexes == tri->numIndexes ) {
					qglColor3f( 0.1/backEnd.overBright, 1/backEnd.overBright, 0.1/backEnd.overBright );
				} else if ( numIndexes == tri->numShadowIndexesNoFrontCaps ) {
					qglColor3f( 0.1/backEnd.overBright, 1/backEnd.overBright, 0.6/backEnd.overBright );
				} else {
					qglColor3f( 0.6/backEnd.overBright, 1/backEnd.overBright, 0.1/backEnd.overBright );
				}
			}
		}

		qglStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		qglDisable( GL_STENCIL_TEST );
		GL_Cull( CT_TWO_SIDED );
		RB_DrawShadowElementsWithCounters( tri, numIndexes );
		GL_Cull( CT_FRONT_SIDED );
		qglEnable( GL_STENCIL_TEST );

		return;
	}

	// DG: that bloody patent on depth-fail stencil shadows has finally expired on 2019-10-13,
	//     so use them (see https://patents.google.com/patent/US6384822B1/en for expiration status)
	bool useStencilOpSeperate = r_useStencilOpSeparate.GetBool() && qglStencilOpSeparate != NULL;
	if( !r_useCarmacksReverse.GetBool() ) {
		if( useStencilOpSeperate ) {
			// not using z-fail, but using qglStencilOpSeparate()
			GLenum firstFace = backEnd.viewDef->isMirror ? GL_FRONT : GL_BACK;
			GLenum secondFace = backEnd.viewDef->isMirror ? GL_BACK : GL_FRONT;
			GL_Cull( CT_TWO_SIDED );
			if ( !external ) {
				qglStencilOpSeparate( firstFace, GL_KEEP, tr.stencilDecr, tr.stencilDecr );
				qglStencilOpSeparate( secondFace, GL_KEEP, tr.stencilIncr, tr.stencilIncr );
				RB_DrawShadowElementsWithCounters( tri, numIndexes );
			}

			qglStencilOpSeparate( firstFace, GL_KEEP, GL_KEEP, tr.stencilIncr );
			qglStencilOpSeparate( secondFace, GL_KEEP, GL_KEEP, tr.stencilDecr );

			RB_DrawShadowElementsWithCounters( tri, numIndexes );

		} else { // DG: this is the original code:
			// patent-free work around
			if ( !external ) {
				// "preload" the stencil buffer with the number of volumes
				// that get clipped by the near or far clip plane
				qglStencilOp( GL_KEEP, tr.stencilDecr, tr.stencilDecr );
				GL_Cull( CT_FRONT_SIDED );
				RB_DrawShadowElementsWithCounters( tri, numIndexes );
				qglStencilOp( GL_KEEP, tr.stencilIncr, tr.stencilIncr );
				GL_Cull( CT_BACK_SIDED );
				RB_DrawShadowElementsWithCounters( tri, numIndexes );
			}

			// traditional depth-pass stencil shadows
			qglStencilOp( GL_KEEP, GL_KEEP, tr.stencilIncr );
			GL_Cull( CT_FRONT_SIDED );
			RB_DrawShadowElementsWithCounters( tri, numIndexes );

			qglStencilOp( GL_KEEP, GL_KEEP, tr.stencilDecr );
			GL_Cull( CT_BACK_SIDED );
			RB_DrawShadowElementsWithCounters( tri, numIndexes );
		}
	} else { // use the formerly patented "Carmack's Reverse" Z-Fail code
		if( useStencilOpSeperate ) {
			// Z-Fail with glStencilOpSeparate() which will reduce draw calls
			GLenum firstFace = backEnd.viewDef->isMirror ? GL_FRONT : GL_BACK;
			GLenum secondFace = backEnd.viewDef->isMirror ? GL_BACK : GL_FRONT;
			if ( !external ) { // z-fail
				qglStencilOpSeparate( firstFace, GL_KEEP, tr.stencilDecr, GL_KEEP );
				qglStencilOpSeparate( secondFace, GL_KEEP, tr.stencilIncr, GL_KEEP );
			} else { // depth-pass
				qglStencilOpSeparate( firstFace, GL_KEEP, GL_KEEP, tr.stencilIncr );
				qglStencilOpSeparate( secondFace, GL_KEEP, GL_KEEP, tr.stencilDecr );
			}
			GL_Cull( CT_TWO_SIDED );
			RB_DrawShadowElementsWithCounters( tri, numIndexes );

		} else { // Z-Fail without glStencilOpSeparate()

			// LEITH: the (formerly patented) "Carmack's Reverse" code

			// depth-fail/Z-Fail stencil shadows
			if ( !external ) {
				qglStencilOp( GL_KEEP, tr.stencilDecr, GL_KEEP );
				GL_Cull( CT_FRONT_SIDED );
				RB_DrawShadowElementsWithCounters( tri, numIndexes );
				qglStencilOp( GL_KEEP, tr.stencilIncr, GL_KEEP );
				GL_Cull( CT_BACK_SIDED );
				RB_DrawShadowElementsWithCounters( tri, numIndexes );
			}
			// traditional depth-pass stencil shadows
			else {
				qglStencilOp( GL_KEEP, GL_KEEP, tr.stencilIncr );
				GL_Cull( CT_FRONT_SIDED );
				RB_DrawShadowElementsWithCounters( tri, numIndexes );

				qglStencilOp( GL_KEEP, GL_KEEP, tr.stencilDecr );
				GL_Cull( CT_BACK_SIDED );
				RB_DrawShadowElementsWithCounters( tri, numIndexes );
			}
		}
	}
}

/*
=====================
RB_StencilShadowPass

Stencil test should already be enabled, and the stencil buffer should have
been set to 128 on any surfaces that might receive shadows
=====================
*/
void RB_StencilShadowPass( const drawSurf_t *drawSurfs ) {
	if ( r_shadows.GetInteger() <= 0 ) {
		return;
	}

	if ( !drawSurfs ) {
		return;
	}

	globalImages->BindNull();
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	// for visualizing the shadows
	if ( r_showShadows.GetInteger() ) {
		if ( r_showShadows.GetInteger() == 2 ) {
			// draw filled in
			GL_State( GLS_DEPTHMASK | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_LESS  );
		} else {
			// draw as lines, filling the depth buffer
			GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO | GLS_POLYMODE_LINE | GLS_DEPTHFUNC_ALWAYS  );
		}
	} else {
		// don't write to the color buffer, just the stencil buffer
		GL_State( GLS_DEPTHMASK | GLS_COLORMASK | GLS_ALPHAMASK | GLS_DEPTHFUNC_LESS );
	}

	if ( r_shadowPolygonFactor.GetFloat() || r_shadowPolygonOffset.GetFloat() ) {
		qglPolygonOffset( r_shadowPolygonFactor.GetFloat(), -r_shadowPolygonOffset.GetFloat() );
		qglEnable( GL_POLYGON_OFFSET_FILL );
	}

	qglStencilFunc( GL_ALWAYS, 1, 255 );

	if ( glConfig.depthBoundsTestAvailable && r_useDepthBoundsTest.GetBool() ) {
		qglEnable( GL_DEPTH_BOUNDS_TEST_EXT );
	}

	RB_RenderDrawSurfChainWithFunction( drawSurfs, RB_T_Shadow );

	GL_Cull( CT_FRONT_SIDED );

	if ( r_shadowPolygonFactor.GetFloat() || r_shadowPolygonOffset.GetFloat() ) {
		qglDisable( GL_POLYGON_OFFSET_FILL );
	}

	if ( glConfig.depthBoundsTestAvailable && r_useDepthBoundsTest.GetBool() ) {
		qglDisable( GL_DEPTH_BOUNDS_TEST_EXT );
	}

	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );

	qglStencilFunc( GL_GEQUAL, 128, 255 );
	qglStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
}



/*
=============================================================================================

BLEND LIGHT PROJECTION

=============================================================================================
*/

/*
=====================
RB_T_BlendLight

=====================
*/
static void RB_T_BlendLight( const drawSurf_t *surf ) {
	const srfTriangles_t *tri;

	tri = surf->geo;

	if ( backEnd.currentSpace != surf->space ) {
		idPlane	lightProject[4];
		int		i;

		for ( i = 0 ; i < 4 ; i++ ) {
			R_GlobalPlaneToLocal( surf->space->modelMatrix, backEnd.vLight->lightProject[i], lightProject[i] );
		}

		GL_SelectTexture( 0 );
		qglTexGenfv( GL_S, GL_OBJECT_PLANE, lightProject[0].ToFloatPtr() );
		qglTexGenfv( GL_T, GL_OBJECT_PLANE, lightProject[1].ToFloatPtr() );
		qglTexGenfv( GL_Q, GL_OBJECT_PLANE, lightProject[2].ToFloatPtr() );

		GL_SelectTexture( 1 );
		qglTexGenfv( GL_S, GL_OBJECT_PLANE, lightProject[3].ToFloatPtr() );
	}

	// this gets used for both blend lights and shadow draws
	if ( tri->ambientCache ) {
		idDrawVert	*ac = (idDrawVert *)vertexCache.Position( tri->ambientCache );
		qglVertexPointer( 3, GL_FLOAT, sizeof( idDrawVert ), ac->xyz.ToFloatPtr() );
	} else if ( tri->shadowCache ) {
		shadowCache_t	*sc = (shadowCache_t *)vertexCache.Position( tri->shadowCache );
		qglVertexPointer( 3, GL_FLOAT, sizeof( shadowCache_t ), sc->xyz.ToFloatPtr() );
	}

	RB_DrawElementsWithCounters( tri );
}


/*
=====================
RB_BlendLight

Dual texture together the falloff and projection texture with a blend
mode to the framebuffer, instead of interacting with the surface texture
=====================
*/
static void RB_BlendLight( const drawSurf_t *drawSurfs,  const drawSurf_t *drawSurfs2 ) {
	const idMaterial	*lightShader;
	const shaderStage_t	*stage;
	int					i;
	const float	*regs;

	if ( !drawSurfs ) {
		return;
	}
	if ( r_skipBlendLights.GetBool() ) {
		return;
	}

	lightShader = backEnd.vLight->lightShader;
	regs = backEnd.vLight->shaderRegisters;

	// texture 1 will get the falloff texture
	GL_SelectTexture( 1 );
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	qglEnable( GL_TEXTURE_GEN_S );
	qglTexCoord2f( 0, 0.5 );
	backEnd.vLight->falloffImage->Bind();

	// texture 0 will get the projected texture
	GL_SelectTexture( 0 );
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	qglEnable( GL_TEXTURE_GEN_S );
	qglEnable( GL_TEXTURE_GEN_T );
	qglEnable( GL_TEXTURE_GEN_Q );

	for ( i = 0 ; i < lightShader->GetNumStages() ; i++ ) {
		stage = lightShader->GetStage(i);

		if ( !regs[ stage->conditionRegister ] ) {
			continue;
		}

		GL_State( GLS_DEPTHMASK | stage->drawStateBits | GLS_DEPTHFUNC_EQUAL );

		GL_SelectTexture( 0 );
		stage->texture.image->Bind();

		if ( stage->texture.hasMatrix ) {
			RB_LoadShaderTextureMatrix( regs, &stage->texture );
		}

		// get the modulate values from the light, including alpha, unlike normal lights
		backEnd.lightColor[0] = regs[ stage->color.registers[0] ];
		backEnd.lightColor[1] = regs[ stage->color.registers[1] ];
		backEnd.lightColor[2] = regs[ stage->color.registers[2] ];
		backEnd.lightColor[3] = regs[ stage->color.registers[3] ];
		qglColor4fv( backEnd.lightColor );

		RB_RenderDrawSurfChainWithFunction( drawSurfs, RB_T_BlendLight );
		RB_RenderDrawSurfChainWithFunction( drawSurfs2, RB_T_BlendLight );

		if ( stage->texture.hasMatrix ) {
			GL_SelectTexture( 0 );
			qglMatrixMode( GL_TEXTURE );
			qglLoadIdentity();
			qglMatrixMode( GL_MODELVIEW );
		}
	}

	GL_SelectTexture( 1 );
	qglDisable( GL_TEXTURE_GEN_S );
	globalImages->BindNull();

	GL_SelectTexture( 0 );
	qglDisable( GL_TEXTURE_GEN_S );
	qglDisable( GL_TEXTURE_GEN_T );
	qglDisable( GL_TEXTURE_GEN_Q );
}


//========================================================================

static idPlane	fogPlanes[4];

/*
=====================
RB_T_BasicFog

=====================
*/
static void RB_T_BasicFog( const drawSurf_t *surf ) {
	if ( backEnd.currentSpace != surf->space ) {
		idPlane	local;

		GL_SelectTexture( 0 );

		R_GlobalPlaneToLocal( surf->space->modelMatrix, fogPlanes[0], local );
		local[3] += 0.5;
		qglTexGenfv( GL_S, GL_OBJECT_PLANE, local.ToFloatPtr() );

//		R_GlobalPlaneToLocal( surf->space->modelMatrix, fogPlanes[1], local );
//		local[3] += 0.5;
local[0] = local[1] = local[2] = 0; local[3] = 0.5;
		qglTexGenfv( GL_T, GL_OBJECT_PLANE, local.ToFloatPtr() );

		GL_SelectTexture( 1 );

		// GL_S is constant per viewer
		R_GlobalPlaneToLocal( surf->space->modelMatrix, fogPlanes[2], local );
		local[3] += FOG_ENTER;
		qglTexGenfv( GL_T, GL_OBJECT_PLANE, local.ToFloatPtr() );

		R_GlobalPlaneToLocal( surf->space->modelMatrix, fogPlanes[3], local );
		qglTexGenfv( GL_S, GL_OBJECT_PLANE, local.ToFloatPtr() );
	}

	RB_T_RenderTriangleSurface( surf );
}



/*
==================
RB_FogPass
==================
*/
static void RB_FogPass( const drawSurf_t *drawSurfs,  const drawSurf_t *drawSurfs2 ) {
	const srfTriangles_t*frustumTris;
	drawSurf_t			ds;
	const idMaterial	*lightShader;
	const shaderStage_t	*stage;
	const float			*regs;

	// create a surface for the light frustom triangles, which are oriented drawn side out
	frustumTris = backEnd.vLight->frustumTris;

	// if we ran out of vertex cache memory, skip it
	if ( !frustumTris->ambientCache ) {
		return;
	}
	memset( &ds, 0, sizeof( ds ) );
	ds.space = &backEnd.viewDef->worldSpace;
	ds.geo = frustumTris;
	ds.scissorRect = backEnd.viewDef->scissor;

	// find the current color and density of the fog
	lightShader = backEnd.vLight->lightShader;
	regs = backEnd.vLight->shaderRegisters;
	// assume fog shaders have only a single stage
	stage = lightShader->GetStage(0);

	backEnd.lightColor[0] = regs[ stage->color.registers[0] ];
	backEnd.lightColor[1] = regs[ stage->color.registers[1] ];
	backEnd.lightColor[2] = regs[ stage->color.registers[2] ];
	backEnd.lightColor[3] = regs[ stage->color.registers[3] ];

	qglColor3fv( backEnd.lightColor );

	// calculate the falloff planes
	float	a;

	// if they left the default value on, set a fog distance of 500
	if ( backEnd.lightColor[3] <= 1.0 ) {
		a = -0.5f / DEFAULT_FOG_DISTANCE;
	} else {
		// otherwise, distance = alpha color
		a = -0.5f / backEnd.lightColor[3];
	}

	GL_State( GLS_DEPTHMASK | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL );

	// texture 0 is the falloff image
	GL_SelectTexture( 0 );
	globalImages->fogImage->Bind();
	//GL_Bind( tr.whiteImage );
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	qglEnable( GL_TEXTURE_GEN_S );
	qglEnable( GL_TEXTURE_GEN_T );
	qglTexCoord2f( 0.5f, 0.5f );		// make sure Q is set

	fogPlanes[0][0] = a * backEnd.viewDef->worldSpace.modelViewMatrix[2];
	fogPlanes[0][1] = a * backEnd.viewDef->worldSpace.modelViewMatrix[6];
	fogPlanes[0][2] = a * backEnd.viewDef->worldSpace.modelViewMatrix[10];
	fogPlanes[0][3] = a * backEnd.viewDef->worldSpace.modelViewMatrix[14];

	fogPlanes[1][0] = a * backEnd.viewDef->worldSpace.modelViewMatrix[0];
	fogPlanes[1][1] = a * backEnd.viewDef->worldSpace.modelViewMatrix[4];
	fogPlanes[1][2] = a * backEnd.viewDef->worldSpace.modelViewMatrix[8];
	fogPlanes[1][3] = a * backEnd.viewDef->worldSpace.modelViewMatrix[12];


	// texture 1 is the entering plane fade correction
	GL_SelectTexture( 1 );
	globalImages->fogEnterImage->Bind();
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	qglEnable( GL_TEXTURE_GEN_S );
	qglEnable( GL_TEXTURE_GEN_T );

	// T will get a texgen for the fade plane, which is always the "top" plane on unrotated lights
	fogPlanes[2][0] = 0.001f * backEnd.vLight->fogPlane[0];
	fogPlanes[2][1] = 0.001f * backEnd.vLight->fogPlane[1];
	fogPlanes[2][2] = 0.001f * backEnd.vLight->fogPlane[2];
	fogPlanes[2][3] = 0.001f * backEnd.vLight->fogPlane[3];

	// S is based on the view origin
	float s = backEnd.viewDef->renderView.vieworg * fogPlanes[2].Normal() + fogPlanes[2][3];

	fogPlanes[3][0] = 0;
	fogPlanes[3][1] = 0;
	fogPlanes[3][2] = 0;
	fogPlanes[3][3] = FOG_ENTER + s;

	qglTexCoord2f( FOG_ENTER + s, FOG_ENTER );


	// draw it
	RB_RenderDrawSurfChainWithFunction( drawSurfs, RB_T_BasicFog );
	RB_RenderDrawSurfChainWithFunction( drawSurfs2, RB_T_BasicFog );

	// the light frustum bounding planes aren't in the depth buffer, so use depthfunc_less instead
	// of depthfunc_equal
	GL_State( GLS_DEPTHMASK | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_LESS );
	GL_Cull( CT_BACK_SIDED );
	RB_RenderDrawSurfChainWithFunction( &ds, RB_T_BasicFog );
	GL_Cull( CT_FRONT_SIDED );

	GL_SelectTexture( 1 );
	qglDisable( GL_TEXTURE_GEN_S );
	qglDisable( GL_TEXTURE_GEN_T );
	globalImages->BindNull();

	GL_SelectTexture( 0 );
	qglDisable( GL_TEXTURE_GEN_S );
	qglDisable( GL_TEXTURE_GEN_T );
}


/*
==================
RB_STD_FogAllLights
==================
*/
void RB_STD_FogAllLights( void ) {
	viewLight_t	*vLight;

	if ( r_skipFogLights.GetBool() || r_showOverDraw.GetInteger() != 0
		 || backEnd.viewDef->isXraySubview /* dont fog in xray mode*/
		 ) {
		return;
	}

	qglDisable( GL_STENCIL_TEST );

	for ( vLight = backEnd.viewDef->viewLights ; vLight ; vLight = vLight->next ) {
		backEnd.vLight = vLight;

		if ( !vLight->lightShader->IsFogLight() && !vLight->lightShader->IsBlendLight() ) {
			continue;
		}

#if 0 // _D3XP disabled that
		if ( r_ignore.GetInteger() ) {
			// we use the stencil buffer to guarantee that no pixels will be
			// double fogged, which happens in some areas that are thousands of
			// units from the origin
			backEnd.currentScissor = vLight->scissorRect;
			if ( r_useScissor.GetBool() ) {
				qglScissor( backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1,
					backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
					backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
					backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1 );
			}
			qglClear( GL_STENCIL_BUFFER_BIT );

			qglEnable( GL_STENCIL_TEST );

			// only pass on the cleared stencil values
			qglStencilFunc( GL_EQUAL, 128, 255 );

			// when we pass the stencil test and depth test and are going to draw,
			// increment the stencil buffer so we don't ever draw on that pixel again
			qglStencilOp( GL_KEEP, GL_KEEP, GL_INCR );
		}
#endif

		if ( vLight->lightShader->IsFogLight() ) {
			RB_FogPass( vLight->globalInteractions, vLight->localInteractions );
		} else if ( vLight->lightShader->IsBlendLight() ) {
			RB_BlendLight( vLight->globalInteractions, vLight->localInteractions );
		}
		qglDisable( GL_STENCIL_TEST );
	}

	qglEnable( GL_STENCIL_TEST );
}

//=========================================================================================

/*
==================
RB_STD_LightScale

Perform extra blending passes to multiply the entire buffer by
a floating point value
==================
*/
void RB_STD_LightScale( void ) {
	float	v, f;

	if ( backEnd.overBright == 1.0f ) {
		return;
	}

	if ( r_skipLightScale.GetBool() ) {
		return;
	}

	// the scissor may be smaller than the viewport for subviews
	if ( r_useScissor.GetBool() ) {
		qglScissor( backEnd.viewDef->viewport.x1 + backEnd.viewDef->scissor.x1,
			backEnd.viewDef->viewport.y1 + backEnd.viewDef->scissor.y1,
			backEnd.viewDef->scissor.x2 - backEnd.viewDef->scissor.x1 + 1,
			backEnd.viewDef->scissor.y2 - backEnd.viewDef->scissor.y1 + 1 );
		backEnd.currentScissor = backEnd.viewDef->scissor;
	}

	// full screen blends
	qglLoadIdentity();
	qglMatrixMode( GL_PROJECTION );
	qglPushMatrix();
	qglLoadIdentity();
	qglOrtho( 0, 1, 0, 1, -1, 1 );

	GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_SRC_COLOR );
	GL_Cull( CT_TWO_SIDED );	// so mirror views also get it
	globalImages->BindNull();
	qglDisable( GL_DEPTH_TEST );
	qglDisable( GL_STENCIL_TEST );

	v = 1;
	while ( idMath::Fabs( v - backEnd.overBright ) > 0.01 ) {	// a little extra slop
		f = backEnd.overBright / v;
		f /= 2;
		if ( f > 1 ) {
			f = 1;
		}
		qglColor3f( f, f, f );
		v = v * f * 2;

		qglBegin( GL_QUADS );
		qglVertex2f( 0,0 );
		qglVertex2f( 0,1 );
		qglVertex2f( 1,1 );
		qglVertex2f( 1,0 );
		qglEnd();
	}


	qglPopMatrix();
	qglEnable( GL_DEPTH_TEST );
	qglMatrixMode( GL_MODELVIEW );
	GL_Cull( CT_FRONT_SIDED );
}

//=========================================================================================

/*
=============
RB_STD_DrawView

=============
*/
// Glow passes adapted from openPREY; see docs/OPENPREY_PORTS.md.
static bool RB_GlowEnabledForCurrentView( void ) {
	if ( backEnd.viewDef == NULL ) {
		return false;
	}

	if ( backEnd.viewDef->isEditor || rbDrawingGlow ) {
		return false;
	}

	if ( backEnd.viewDef->isSubview ) {
		// Portal views finish before their parent is drawn. Bloom their scene
		// here; the parent's depth pass then masks the actual portal outline.
		const drawSurf_t *surface = backEnd.viewDef->subviewSurface;
		bool portal = false;
		if ( surface ) {
			const idMaterial *material = surface->material;
			portal = material->GetSort() == SS_SUBVIEW && material->GetSubviewClass() == SC_PORTAL;
			for ( int i = 0; material->GetSort() != SS_SUBVIEW && i < material->GetNumStages(); ++i ) {
				portal |= material->GetStage( i )->texture.dynamic == DI_PORTAL_RENDER;
			}
		}
		// Sky glow is included in its parent's mask, then composited once.
		if ( r_glowMode.GetInteger() != 2 || !portal || !r_glowPortals.GetBool() ) {
			return false;
		}
	}

	if ( !backEnd.viewDef->viewEntitys ) {
		return false;
	}

	if ( r_skipGlowOverlay.GetBool() ) {
		return false;
	}

	if ( r_glowStrength.GetFloat() <= 0.0f ) {
		return false;
	}

	if ( globalImages->glowSceneImage == NULL || globalImages->glowScreenImage == NULL || globalImages->glowCompositeImage == NULL ) {
		return false;
	}

	return true;
}

static void RB_GlowSetScreenRect( void ) {
	const int viewportWidth = backEnd.viewDef->viewport.x2 - backEnd.viewDef->viewport.x1 + 1;
	const int viewportHeight = backEnd.viewDef->viewport.y2 - backEnd.viewDef->viewport.y1 + 1;

	qglViewport(
		backEnd.viewDef->viewport.x1,
		backEnd.viewDef->viewport.y1,
		viewportWidth,
		viewportHeight );

	qglScissor(
		backEnd.viewDef->viewport.x1 + backEnd.viewDef->scissor.x1,
		backEnd.viewDef->viewport.y1 + backEnd.viewDef->scissor.y1,
		backEnd.viewDef->scissor.x2 - backEnd.viewDef->scissor.x1 + 1,
		backEnd.viewDef->scissor.y2 - backEnd.viewDef->scissor.y1 + 1 );
	backEnd.currentScissor = backEnd.viewDef->scissor;
}

static void RB_GlowBeginScreenPass( void ) {
	RB_GlowSetScreenRect();

	qglMatrixMode( GL_MODELVIEW );
	qglPushMatrix();
	qglLoadIdentity();
	qglMatrixMode( GL_PROJECTION );
	qglPushMatrix();
	qglLoadIdentity();
	qglOrtho( 0, 1, 0, 1, -4, 1 );

	GL_Cull( CT_TWO_SIDED );
	qglDisableClientState( GL_COLOR_ARRAY );
	qglDisable( GL_DEPTH_TEST );
	qglDisable( GL_STENCIL_TEST );

	GL_SelectTexture( 1 );
	globalImages->BindNull();
	GL_SelectTexture( 0 );
	GL_TexEnv( GL_MODULATE );
    qglMatrixMode( GL_TEXTURE );
    qglPushMatrix();
    qglLoadIdentity();
    qglMatrixMode( GL_MODELVIEW );
    qglDisable( GL_ALPHA_TEST );
}

static void RB_GlowEndScreenPass( void ) {
    qglMatrixMode( GL_TEXTURE );
    qglPopMatrix();
	qglMatrixMode( GL_PROJECTION );
	qglPopMatrix();
	qglEnable( GL_DEPTH_TEST );
	qglEnable( GL_STENCIL_TEST );
	qglMatrixMode( GL_MODELVIEW );
	qglPopMatrix();
	backEnd.currentSpace = NULL;
	GL_Cull( CT_FRONT_SIDED );
}

static void RB_GlowDrawScreenQuad( idImage *image, float offsetX, float offsetY, int imageWidth = 0, int imageHeight = 0 ) {
    // CopyFramebuffer pads non-power-of-two views. Sample only the copied area.
    const float w = imageWidth ? imageWidth : backEnd.viewDef->viewport.x2 - backEnd.viewDef->viewport.x1 + 1;
    const float h = imageHeight ? imageHeight : backEnd.viewDef->viewport.y2 - backEnd.viewDef->viewport.y1 + 1;
    const float sMax = w / image->uploadWidth;
    const float tMax = h / image->uploadHeight;
    const float s0 = idMath::ClampFloat( 0.0f, sMax, offsetX );
    const float s1 = idMath::ClampFloat( 0.0f, sMax, sMax + offsetX );
    const float t0 = idMath::ClampFloat( 0.0f, tMax, offsetY );
    const float t1 = idMath::ClampFloat( 0.0f, tMax, tMax + offsetY );
    qglBegin( GL_QUADS );
    qglTexCoord2f( s0, t0 ); qglVertex2f( 0, 0 );
    qglTexCoord2f( s1, t0 ); qglVertex2f( 1, 0 );
    qglTexCoord2f( s1, t1 ); qglVertex2f( 1, 1 );
    qglTexCoord2f( s0, t1 ); qglVertex2f( 0, 1 );
    qglEnd();
}

static void RB_GlowCopyViewToImage( idImage *image ) {
	if ( image == NULL ) {
		return;
	}

	const int viewportWidth = backEnd.viewDef->viewport.x2 - backEnd.viewDef->viewport.x1 + 1;
	const int viewportHeight = backEnd.viewDef->viewport.y2 - backEnd.viewDef->viewport.y1 + 1;
	if ( viewportWidth <= 0 || viewportHeight <= 0 ) {
		return;
	}

	image->CopyFramebuffer(
		backEnd.viewDef->viewport.x1,
		backEnd.viewDef->viewport.y1,
		viewportWidth,
		viewportHeight, false, false );
}

static void RB_GlowDrawImage( idImage *image, int stateBits, float colorScale ) {
	if ( image == NULL ) {
		return;
	}

	GL_State( stateBits );
	GL_SelectTexture( 0 );
	image->Bind();
	qglColor4f( colorScale, colorScale, colorScale, colorScale );
	RB_GlowDrawScreenQuad( image, 0.0f, 0.0f );
}

static void RB_GlowBlurPass( idImage *sourceImage, idImage *captureImage, bool horizontal ) {
	if ( sourceImage == NULL || captureImage == NULL ) {
		return;
	}

	const int steps = idMath::ClampInt( 0, 256, r_glowSteps.GetInteger() );
	const float startAlpha = idMath::ClampFloat( 0.0f, 8.0f, r_glowAlpha.GetFloat() );
	const float alphaChange = idMath::ClampFloat( 0.0f, 8.0f, r_glowAlphaChange.GetFloat() );
	const float blurStep = 1.0f / static_cast<float>( Max( 1, ( horizontal ? sourceImage->uploadWidth : sourceImage->uploadHeight ) ) );

	RB_GlowBeginScreenPass();

	GL_SelectTexture( 0 );
	sourceImage->Bind();

	GL_State( GLS_DEPTHFUNC_ALWAYS | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ZERO );
	qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	RB_GlowDrawScreenQuad( sourceImage, 0.0f, 0.0f );

	if ( steps > 1 && startAlpha > 0.0f ) {
		float alpha = startAlpha;
		GL_State( GLS_DEPTHFUNC_ALWAYS | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE );
		for ( int step = 1; step < steps; ++step ) {
			if ( alpha <= 0.001f ) {
				break;
			}

			const float offset = blurStep * static_cast<float>( step );
			const float offsetX = horizontal ? offset : 0.0f;
			const float offsetY = horizontal ? 0.0f : offset;
			qglColor4f( 1.0f, 1.0f, 1.0f, alpha );
			RB_GlowDrawScreenQuad( sourceImage, offsetX, offsetY );
			RB_GlowDrawScreenQuad( sourceImage, -offsetX, -offsetY );
			alpha *= alphaChange;
		}
	}

	qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	RB_GlowCopyViewToImage( captureImage );
	RB_GlowEndScreenPass();
}



// Portable reconstruction of the recovered shader, not an emulation of all
// Xbox render-target/state quirks. See docs/BLOOM_RECONSTRUCTION.md.
static const int GLOW_KERNEL_SIZE = 256;

static void RB_GlowKernelRect( void ) {
    qglViewport( backEnd.viewDef->viewport.x1, backEnd.viewDef->viewport.y1,
        GLOW_KERNEL_SIZE, GLOW_KERNEL_SIZE );
    qglScissor( backEnd.viewDef->viewport.x1, backEnd.viewDef->viewport.y1,
        GLOW_KERNEL_SIZE, GLOW_KERNEL_SIZE );
}

static void RB_GlowKernelCopy( idImage *image ) {
    image->CopyFramebuffer( backEnd.viewDef->viewport.x1, backEnd.viewDef->viewport.y1,
        GLOW_KERNEL_SIZE, GLOW_KERNEL_SIZE, false, false );
}

static void RB_GlowRecoveredKernel( void ) {
    RB_GlowBeginScreenPass();
    RB_GlowKernelRect();
    // Downsample the visible glow mask; the saved scene and its depth survive.
    RB_GlowDrawImage( globalImages->glowScreenImage,
        GLS_DEPTHFUNC_ALWAYS | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO, 1.0f );
    RB_GlowKernelCopy( globalImages->glowKernelImage );

    float weights[8];
    const float alpha = idMath::ClampFloat( 0.0f, 4.0f, r_glowAlpha.GetFloat() );
    weights[0] = alpha;
    for ( int i = 1; i < 8; ++i ) { weights[i] = weights[i-1] * alpha; }
    qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, FPROG_GLOW );
    qglProgramLocalParameter4fvARB( GL_FRAGMENT_PROGRAM_ARB, 0, weights );
    qglProgramLocalParameter4fvARB( GL_FRAGMENT_PROGRAM_ARB, 1, weights + 4 );
    qglEnable( GL_FRAGMENT_PROGRAM_ARB );
    for ( int axis = 0; axis < 2; ++axis ) {
        idHitchScope hitch("bloom_blur", axis == 0 ? "horizontal" : "vertical");
        idImage *source = axis == 0 ? globalImages->glowKernelImage : globalImages->glowCompositeImage;
        source->Bind();
        // Half a texel on the recovered 256-square intermediate, in both axes.
        const float direction[4] = { axis == 0 ? 1.0f / 512.0f : 0.0f,
            axis == 1 ? 1.0f / 512.0f : 0.0f, 0.0f, 0.0f };
        qglProgramLocalParameter4fvARB( GL_FRAGMENT_PROGRAM_ARB, 2, direction );
        RB_GlowDrawScreenQuad( source, 0.0f, 0.0f, GLOW_KERNEL_SIZE, GLOW_KERNEL_SIZE );
        // Copy only after the draw completes; source and destination may alias.
        RB_GlowKernelCopy( globalImages->glowCompositeImage );
    }
    qglDisable( GL_FRAGMENT_PROGRAM_ARB );
    qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, 0 );
    RB_GlowEndScreenPass();
}

// Retail Windows uses shifted geometry, not stretched/clamped UV endpoints.
// RGB and alpha are both modulated by c, then blended with SRC_ALPHA, ONE.
// With the RGB8 input this gives c*c weights and fixed-point rounding per draw.
struct retailGlowProgram_t {
    bool valid, supported;
    int size, steps;
    float alpha, change;
};
static const int RETAIL_GLOW_CACHE_SIZE = 8;
static retailGlowProgram_t retailGlowPrograms[2][RETAIL_GLOW_CACHE_SIZE];
static int retailGlowNextSlot[2];
static int retailGlowActiveProgram[2];

void RB_InvalidateRetailGlowPrograms( void ) {
    memset( retailGlowPrograms, 0, sizeof( retailGlowPrograms ) );
    memset( retailGlowNextSlot, 0, sizeof( retailGlowNextSlot ) );
    memset( retailGlowActiveProgram, 0, sizeof( retailGlowActiveProgram ) );
}

// A larger mask with only the original sparse taps produces separated copies
// of thin neon tubes. Resample the retail kernel onto the actual texel grid,
// preserving its total gain and normalized radius. Adjacent taps share one
// bilinear fetch. The 256-square reference still uses the captured GL draws.
static bool RB_GlowRetailProgram( int axis, int size ) {
    if ( !glConfig.ARBFragmentProgramAvailable ) {
        return false;
    }
    const int steps = idMath::ClampInt( 0, 256, r_glowSteps.GetInteger() );
    const float alpha = r_glowAlpha.GetFloat(), change = r_glowAlphaChange.GetFloat();
    // Retain the exact programs for recurring viewport sizes (portals,
    // thumbnails, main view). Fixed reserved IDs bound driver resource use.
    for ( int slot = 0; slot < RETAIL_GLOW_CACHE_SIZE; ++slot ) {
        const retailGlowProgram_t &entry = retailGlowPrograms[axis][slot];
        if ( entry.valid && entry.size == size && entry.steps == steps &&
             entry.alpha == alpha && entry.change == change ) {
            retailGlowActiveProgram[axis] = FPROG_RETAIL_GLOW_CACHE + axis * RETAIL_GLOW_CACHE_SIZE + slot;
            return entry.supported;
        }
    }
    const int slot = retailGlowNextSlot[axis];
    retailGlowNextSlot[axis] = (slot + 1) % RETAIL_GLOW_CACHE_SIZE;
    retailGlowProgram_t &cached = retailGlowPrograms[axis][slot];
    retailGlowActiveProgram[axis] = FPROG_RETAIL_GLOW_CACHE + axis * RETAIL_GLOW_CACHE_SIZE + slot;
    idHitchScope compileHitch("bloom_program_build", axis == 0 ? "horizontal" : "vertical");
    cached.valid = true;
    cached.supported = false;
    cached.size = size; cached.steps = steps; cached.alpha = alpha; cached.change = change;

    float retailWeights[257] = { 1.0f };
    float color = alpha, gain = 1.0f;
    for ( int i = 1; i < steps; ++i ) {
        const float clamped = idMath::ClampFloat( 0, 1, color );
        retailWeights[i] = clamped * clamped;
        gain += 2.0f * retailWeights[i];
        color *= change;
    }
    const int radius = int( ceilf( Max( 1, steps ) * float( size ) / GLOW_KERNEL_SIZE ) );
    const int samples = radius + 1;
    GLint maxInstructions = 0, maxTextures = 0, maxParameters = 0;
    const PFNGLGETPROGRAMIVARBPROC getProgram = reinterpret_cast<PFNGLGETPROGRAMIVARBPROC>( GLimp_ExtensionPointer( "glGetProgramivARB" ) );
    if ( !getProgram ) {
        return false;
    }
    getProgram( GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_INSTRUCTIONS_ARB, &maxInstructions );
    getProgram( GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_TEX_INSTRUCTIONS_ARB, &maxTextures );
    getProgram( GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_PARAMETERS_ARB, &maxParameters );
    if ( 5 * samples + 2 > maxInstructions || samples > maxTextures || samples + 2 > maxParameters ) {
        common->Warning( "Native glow exceeds fragment-program limits; using 256-square retail glow" );
        return false;
    }

    idList<float> weights;
    weights.SetNum( 2 * radius + 1 );
    float total = 0.0f;
    for ( int i = -radius; i <= radius; ++i ) {
        const float distance = idMath::Fabs( float( i ) ) * GLOW_KERNEL_SIZE / size;
        const int index = int( distance );
        const float fraction = distance - index;
        const float weight = index < Max( 1, steps ) ?
            retailWeights[index] * ( 1.0f - fraction ) + retailWeights[index + 1] * fraction : 0.0f;
        weights[i + radius] = weight;
        total += weight;
    }
    const float scale = gain / total;
    const float textureSize = MakePowerOfTwo( size );
    idStr program = "!!ARBfp1.0\nTEMP uv, sample, sum;\nPARAM bounds = program.local[0];\n";
    idStr instructions = "MOV sum, 0.0;\n";
    int tap = 0;
    for ( int i = 0; i < weights.Num(); i += 2 ) {
        const float a = weights[i];
        const float b = i + 1 < weights.Num() ? weights[i + 1] : 0.0f;
        if ( a + b <= 0.0f ) {
            continue;
        }
        const float offset = ( i - radius + b / ( a + b ) ) / textureSize;
        program += va( "PARAM tap%d = { %.9g, %.9g, %.9g, 0 };\n", tap,
            axis == 0 ? offset : 0.0f, axis == 1 ? offset : 0.0f, ( a + b ) * scale );
        instructions += va( "ADD uv, fragment.texcoord[0], tap%d;\n"
            "MAX uv.xy, uv, bounds;\nMIN uv.xy, uv, bounds.zwzw;\n"
            "TEX sample, uv, texture[0], 2D;\nMAD sum, sample, tap%d.z, sum;\n", tap, tap );
        ++tap;
    }
    program += instructions;
    program += "MOV result.color, sum;\nEND\n";
    qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, retailGlowActiveProgram[axis] );
    qglGetError();
    qglProgramStringARB( GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, program.Length(), program.c_str() );
    const GLenum error = qglGetError();
    GLint position = -1;
    qglGetIntegerv( GL_PROGRAM_ERROR_POSITION_ARB, &position );
    cached.supported = error == GL_NO_ERROR && position == -1;
    if ( !cached.supported ) {
        common->Warning( "Native glow program rejected; using 256-square retail glow (%s)",
            reinterpret_cast<const char *>( qglGetString( GL_PROGRAM_ERROR_STRING_ARB ) ) );
    }
    qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, 0 );
    return cached.supported;
}

static void RB_GlowRetailQuad( idImage *image, int width, int height, float x, float y ) {
    const float s = float( width ) / image->uploadWidth;
    const float t = float( height ) / image->uploadHeight;
    qglBegin( GL_QUADS );
    qglTexCoord2f( 0, 0 ); qglVertex2f( x, y );
    qglTexCoord2f( 0, t ); qglVertex2f( x, y + 1 );
    qglTexCoord2f( s, t ); qglVertex2f( x + 1, y + 1 );
    qglTexCoord2f( s, 0 ); qglVertex2f( x + 1, y );
    qglEnd();
}

static void RB_GlowRetailScreenRect( bool fullViewport = false ) {
    const viewDef_t *view = backEnd.viewDef;
    qglViewport( tr.viewportOffset[0] + view->viewport.x1,
        tr.viewportOffset[1] + view->viewport.y1,
        view->viewport.x2 - view->viewport.x1 + 1,
        view->viewport.y2 - view->viewport.y1 + 1 );
    qglScissor( tr.viewportOffset[0] + view->viewport.x1 + view->scissor.x1,
        tr.viewportOffset[1] + view->viewport.y1 + view->scissor.y1,
        view->scissor.x2 - view->scissor.x1 + 1,
        view->scissor.y2 - view->scissor.y1 + 1 );
    backEnd.currentScissor = view->scissor;
    if ( fullViewport ) {
        backEnd.currentScissor.x1 = backEnd.currentScissor.y1 = 0;
        backEnd.currentScissor.x2 = view->viewport.x2 - view->viewport.x1;
        backEnd.currentScissor.y2 = view->viewport.y2 - view->viewport.y1;
        qglScissor( tr.viewportOffset[0] + view->viewport.x1,
            tr.viewportOffset[1] + view->viewport.y1,
            backEnd.currentScissor.x2 + 1, backEnd.currentScissor.y2 + 1 );
    }
}

static void RB_GlowRetailCopy( idImage *image ) {
    idHitchScope hitch("bloom_copy", image->imgName.c_str());
    const viewDef_t *view = backEnd.viewDef;
    image->CopyFramebuffer( tr.viewportOffset[0] + view->viewport.x1,
        tr.viewportOffset[1] + view->viewport.y1,
        view->viewport.x2 - view->viewport.x1 + 1,
        view->viewport.y2 - view->viewport.y1 + 1, false, false );
}

static idScreenRect RB_GlowRetailScissor( const idScreenRect &source, int width, int height, int targetWidth, int targetHeight ) {
    idScreenRect result = source;
    if ( source.IsEmpty() ) {
        result.Clear();
        return result;
    }
    // Round outwards so portal-area scissors cannot cut off a low-resolution texel.
    result.x1 = idMath::ClampInt( 0, targetWidth, source.x1 * targetWidth / width );
    result.y1 = idMath::ClampInt( 0, targetHeight, source.y1 * targetHeight / height );
    result.x2 = idMath::ClampInt( -1, targetWidth - 1,
        ( ( source.x2 + 1 ) * targetWidth + width - 1 ) / width - 1 );
    result.y2 = idMath::ClampInt( -1, targetHeight - 1,
        ( ( source.y2 + 1 ) * targetHeight + height - 1 ) / height - 1 );
    return result;
}

static bool RB_GlowHasStages( drawSurf_t **drawSurfs, int numDrawSurfs ) {
    for ( int i = 0; i < numDrawSurfs; ++i ) {
        for ( int j = 0; j < drawSurfs[i]->material->GetNumStages(); ++j ) {
            const shaderStage_t *stage = drawSurfs[i]->material->GetStage( j );
            if ( stage->isGlow && drawSurfs[i]->shaderRegisters[stage->conditionRegister] != 0 ) {
                return true;
            }
        }
    }
    return false;
}

// Build the mask before the normal view clears/fills its depth buffer. Clone
// only render descriptions: no second scene traversal, animation or game tick.
static bool RB_GlowPrepareRetail( drawSurf_t **drawSurfs, int numDrawSurfs, int &glowWidth, int &glowHeight ) {
    if ( r_glowMode.GetInteger() != 2 || !RB_GlowEnabledForCurrentView() ) {
        return false;
    }
    const viewDef_t *savedView = backEnd.viewDef;
    const int width = savedView->viewport.x2 - savedView->viewport.x1 + 1;
    const int height = savedView->viewport.y2 - savedView->viewport.y1 + 1;
    if ( width <= 0 || height <= 0 ) {
        return false;
    }
    const int resolution = r_glowResolution.GetInteger();
    glowWidth = resolution > 0 ? Min( resolution, width ) : width;
    glowHeight = resolution > 0 ? Min( resolution, height ) : height;
    bool smoothKernel = glowWidth != GLOW_KERNEL_SIZE || glowHeight != GLOW_KERNEL_SIZE;
    if ( smoothKernel && ( !RB_GlowRetailProgram( 0, glowWidth ) || !RB_GlowRetailProgram( 1, glowHeight ) ) ) {
        glowWidth = Min( GLOW_KERNEL_SIZE, width );
        glowHeight = Min( GLOW_KERNEL_SIZE, height );
        smoothKernel = false;
    }
    int count = 0;
    while ( count < numDrawSurfs && drawSurfs[count]->material->GetSort() < SS_POST_PROCESS ) {
        ++count;
    }
    const viewDef_t *sky = savedView->glowSkyView;
    if ( !RB_GlowHasStages( drawSurfs, count ) &&
         !( sky && RB_GlowHasStages( sky->drawSurfs, sky->numDrawSurfs ) ) ) {
        return false;
    }

    viewDef_t glowView = *savedView;
    glowView.viewport.x2 = glowView.viewport.x1 + glowWidth - 1;
    glowView.viewport.y2 = glowView.viewport.y1 + glowHeight - 1;
    glowView.scissor = RB_GlowRetailScissor( savedView->scissor, width, height, glowWidth, glowHeight );
    idList<drawSurf_t> surfaces;
    idList<drawSurf_t *> surfacePointers;
    surfaces.SetNum( count );
    surfacePointers.SetNum( count );
    int visibleCount = 0;
    for ( int i = 0; i < count; ++i ) {
        surfaces[i] = *drawSurfs[i];
        surfaces[i].scissorRect = RB_GlowRetailScissor( drawSurfs[i]->scissorRect, width, height, glowWidth, glowHeight );
        surfaces[i].scissorRect.Intersect( glowView.scissor );
        if ( !surfaces[i].scissorRect.IsEmpty() ) {
            surfacePointers[visibleCount++] = &surfaces[i];
        }
    }
    if ( visibleCount == 0 ) {
        return false;
    }
    count = visibleCount;
    glowView.drawSurfs = surfacePointers.Ptr();
    glowView.numDrawSurfs = count;
    glowView.maxDrawSurfs = count;

    // A previously rendered direct subview may already occupy this framebuffer.
    // Preserve its color; the main view will rebuild depth after this prepass.
    GL_SelectTexture( 0 );
    RB_GlowRetailCopy( globalImages->glowSceneImage );
    const int savedDepthFunc = backEnd.depthFunc;
    const bool savedRenderCopied = backEnd.currentRenderCopied;
    backEnd.viewDef = &glowView;
    backEnd.currentRenderCopied = false;
    rbDrawingGlow = true;
    RB_BeginDrawingView();
    // The blur samples outside the portal scissor. Those pixels must be black,
    // not another portal or the preceding frame. Preserve/restore the full color
    // viewport around this scratch work; geometry keeps its original scissors.
    RB_GlowRetailScreenRect( true );
    qglClearColor( 0, 0, 0, 1 );
    qglClear( GL_COLOR_BUFFER_BIT );
    RB_GlowRetailScreenRect();
    // Render the sky into the same emission mask as the parent. Opaque parent
    // geometry masks it before blur, allowing the final halo to cross sky edges.
    if ( sky ) {
        viewDef_t skyMask = *sky;
        skyMask.viewport = glowView.viewport;
        skyMask.scissor = glowView.scissor;
        idList<drawSurf_t> skySurfaces;
        idList<drawSurf_t *> skyPointers;
        skySurfaces.SetNum( sky->numDrawSurfs );
        skyPointers.SetNum( sky->numDrawSurfs );
        int skyCount = 0;
        for ( int i = 0; i < sky->numDrawSurfs; ++i ) {
            if ( sky->drawSurfs[i]->material->GetSort() >= SS_POST_PROCESS ) break;
            skySurfaces[i] = *sky->drawSurfs[i];
            skySurfaces[i].scissorRect = RB_GlowRetailScissor( skySurfaces[i].scissorRect, width, height, glowWidth, glowHeight );
            skySurfaces[i].scissorRect.Intersect( skyMask.scissor );
            if ( !skySurfaces[i].scissorRect.IsEmpty() ) skyPointers[skyCount++] = &skySurfaces[i];
        }
        skyMask.drawSurfs = skyPointers.Ptr();
        skyMask.numDrawSurfs = skyCount;
        backEnd.viewDef = &skyMask;
        RB_BeginDrawingView();
        RB_STD_FillDepthBuffer( skyMask.drawSurfs, skyCount );
        qglStencilFunc( GL_ALWAYS, 128, 255 );
        RB_STD_DrawShaderPasses( skyMask.drawSurfs, skyCount );
        backEnd.viewDef = &glowView;
        RB_BeginDrawingView();
    }
    {
        idHitchScope hitch("bloom_mask_depth", "mask");
        RB_STD_FillDepthBuffer( glowView.drawSurfs, count );
    }
    qglStencilFunc( GL_ALWAYS, 128, 255 );
    {
        idHitchScope hitch("bloom_mask_materials", "mask");
        RB_STD_DrawShaderPasses( glowView.drawSurfs, count );
    }
    rbDrawingGlow = false;
    RB_GlowRetailCopy( globalImages->glowScreenImage );

    RB_GlowBeginScreenPass();
    RB_GlowRetailScreenRect( true );
    GL_State( GLS_DEPTHFUNC_ALWAYS | GLS_DEPTHMASK | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );
    globalImages->glowScreenImage->Bind();
    qglColor4f( 1, 1, 1, 1 );
    RB_GlowRetailQuad( globalImages->glowScreenImage, glowWidth, glowHeight, 0, 0 );
    const int steps = idMath::ClampInt( 0, 256, r_glowSteps.GetInteger() );
    const float startAlpha = r_glowAlpha.GetFloat();
    const float alphaChange = r_glowAlphaChange.GetFloat();
    for ( int axis = 0; axis < 2; ++axis ) {
        idHitchScope hitch("bloom_blur", axis == 0 ? "horizontal" : "vertical");
        idImage *source = axis == 0 ? globalImages->glowScreenImage : globalImages->glowCompositeImage;
        if ( smoothKernel ) {
            GL_State( GLS_DEPTHFUNC_ALWAYS | GLS_DEPTHMASK | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );
            qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, retailGlowActiveProgram[axis] );
            const float bounds[4] = { 0.5f / source->uploadWidth, 0.5f / source->uploadHeight,
                ( glowWidth - 0.5f ) / source->uploadWidth, ( glowHeight - 0.5f ) / source->uploadHeight };
            qglProgramLocalParameter4fvARB( GL_FRAGMENT_PROGRAM_ARB, 0, bounds );
            qglEnable( GL_FRAGMENT_PROGRAM_ARB );
            RB_GlowRetailQuad( source, glowWidth, glowHeight, 0, 0 );
            qglDisable( GL_FRAGMENT_PROGRAM_ARB );
            qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, 0 );
        } else {
            GL_State( GLS_DEPTHFUNC_ALWAYS | GLS_DEPTHMASK | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE );
            float color = startAlpha;
            for ( int step = 1; step < steps; ++step ) {
                const float offset = float( step ) / GLOW_KERNEL_SIZE;
                qglColor4f( color, color, color, color );
                RB_GlowRetailQuad( source, glowWidth, glowHeight, axis == 0 ? offset : 0, axis == 1 ? offset : 0 );
                RB_GlowRetailQuad( source, glowWidth, glowHeight, axis == 0 ? -offset : 0, axis == 1 ? -offset : 0 );
                color *= alphaChange;
            }
        }
        // CopyFramebuffer binds the result as the next source. Keep the center
        // already in the framebuffer when adding the vertical pairs.
        RB_GlowRetailCopy( globalImages->glowCompositeImage );
    }
    RB_GlowEndScreenPass();

    backEnd.viewDef = savedView;
    backEnd.depthFunc = savedDepthFunc;
    backEnd.currentRenderCopied = savedRenderCopied;
    RB_GlowBeginScreenPass();
    RB_GlowRetailScreenRect( true );
    RB_GlowDrawImage( globalImages->glowSceneImage,
        GLS_DEPTHFUNC_ALWAYS | GLS_DEPTHMASK | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO, 1.0f );
    RB_GlowEndScreenPass();
    RB_GlowRetailScreenRect();
    return true;
}

static void RB_GlowOverlayRetail( int glowWidth, int glowHeight ) {
    RB_GlowBeginScreenPass();
    RB_GlowRetailScreenRect();
    GL_State( GLS_DEPTHFUNC_ALWAYS | GLS_DEPTHMASK | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );
    globalImages->glowCompositeImage->Bind();
    const float strength = r_glowStrength.GetFloat();
    qglColor3f( strength, strength, strength );
    RB_GlowRetailQuad( globalImages->glowCompositeImage, glowWidth, glowHeight, 0, 0 );
    qglColor4f( 1, 1, 1, 1 );
    RB_GlowEndScreenPass();
}

static void RB_STD_GlowOverlay( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	if ( numDrawSurfs <= 0 || !RB_GlowEnabledForCurrentView() ) {
		return;
	}

	const int viewportWidth = backEnd.viewDef->viewport.x2 - backEnd.viewDef->viewport.x1 + 1;
	const int viewportHeight = backEnd.viewDef->viewport.y2 - backEnd.viewDef->viewport.y1 + 1;
	if ( viewportWidth <= 0 || viewportHeight <= 0 ) {
		return;
	}


	// Reuse the completed depth buffer so occluded glow stays occluded.
    // No second simulation, scene traversal or depth/stencil clear is needed.
    bool hasGlow = false;
    for ( int i = 0; i < numDrawSurfs && !hasGlow; ++i ) {
        for ( int j = 0; j < drawSurfs[i]->material->GetNumStages(); ++j ) {
            const shaderStage_t *stage = drawSurfs[i]->material->GetStage( j );
            if ( stage->isGlow && drawSurfs[i]->shaderRegisters[stage->conditionRegister] != 0 ) {
                hasGlow = true;
                break;
            }
        }
    }
    if ( !hasGlow ) { return; }

	RB_GlowCopyViewToImage( globalImages->glowSceneImage );
	GL_State( GLS_DEPTHFUNC_EQUAL | GLS_DEPTHMASK );
    RB_GlowSetScreenRect();
    qglClearColor( 0, 0, 0, 1 );
    qglClear( GL_COLOR_BUFFER_BIT );
    rbDrawingGlow = true;
    RB_STD_DrawShaderPasses( drawSurfs, numDrawSurfs );
    rbDrawingGlow = false;

	RB_GlowCopyViewToImage( globalImages->glowScreenImage );

	const bool recoveredKernel = r_glowMode.GetInteger() == 1 && glConfig.ARBFragmentProgramAvailable
        && viewportWidth >= GLOW_KERNEL_SIZE && viewportHeight >= GLOW_KERNEL_SIZE;
    if ( recoveredKernel ) {
        RB_GlowRecoveredKernel();
    } else {
        RB_GlowBlurPass( globalImages->glowScreenImage, globalImages->glowCompositeImage, true );
        RB_GlowBlurPass( globalImages->glowCompositeImage, globalImages->glowCompositeImage, false );
    }

	RB_GlowBeginScreenPass();
	RB_GlowDrawImage( globalImages->glowSceneImage, GLS_DEPTHFUNC_ALWAYS | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO, 1.0f );
	if ( recoveredKernel ) {
        GL_State( GLS_DEPTHFUNC_ALWAYS | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );
        globalImages->glowCompositeImage->Bind();
        const float strength = r_glowStrength.GetFloat();
        qglColor4f( strength, strength, strength, 1.0f );
        RB_GlowDrawScreenQuad( globalImages->glowCompositeImage, 0.0f, 0.0f, GLOW_KERNEL_SIZE, GLOW_KERNEL_SIZE );
    } else {
        RB_GlowDrawImage( globalImages->glowCompositeImage, GLS_DEPTHFUNC_ALWAYS | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE, r_glowStrength.GetFloat() );
    }
	qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	RB_GlowEndScreenPass();
}

void	RB_STD_DrawView( void ) {
	drawSurf_t	 **drawSurfs;
	int			numDrawSurfs;

	backEnd.depthFunc = GLS_DEPTHFUNC_EQUAL;

	drawSurfs = (drawSurf_t **)&backEnd.viewDef->drawSurfs[0];
	numDrawSurfs = backEnd.viewDef->numDrawSurfs;

	const char *hitchView = !backEnd.viewDef->viewEntitys ? "gui" :
		(backEnd.viewDef->isSubview ? "subview" : "main");
	int glowWidth = 0, glowHeight = 0;
	bool retailGlow;
	{
		idHitchScope hitch("draw_bloom_prepare", hitchView);
		retailGlow = RB_GlowPrepareRetail( drawSurfs, numDrawSurfs, glowWidth, glowHeight );
	}

	// clear the z buffer, set the projection matrix, etc
	{
		idHitchScope hitch("draw_setup", hitchView);
		RB_BeginDrawingView();
	}

	// decide how much overbrighting we are going to do
	{
		idHitchScope hitch("draw_light_setup", hitchView);
		RB_DetermineLightScale();
	}

	// fill the depth buffer and clear color buffer to black except on
	// subviews
	{
		idHitchScope hitch("draw_depth", hitchView);
		RB_STD_FillDepthBuffer( drawSurfs, numDrawSurfs );
	}

	// main light renderer
	{
		idHitchScope hitch("draw_lighting_shadows", hitchView);
		RB_ARB2_DrawInteractions();
	}

	// disable stencil shadow test
	qglStencilFunc( GL_ALWAYS, 128, 255 );

	// uplight the entire screen to crutch up not having better blending range
	{
		idHitchScope hitch("draw_light_scale", hitchView);
		RB_STD_LightScale();
	}

	// now draw any non-light dependent shading passes
	int processed;
	{
		idHitchScope hitch("draw_materials", hitchView);
		processed = RB_STD_DrawShaderPasses( drawSurfs, numDrawSurfs );
	}

	// fob and blend lights
	{
		idHitchScope hitch("draw_fog", hitchView);
		RB_STD_FogAllLights();
	}

	if ( !retailGlow ) {
		{
			idHitchScope hitch("draw_bloom_legacy", hitchView);
			RB_STD_GlowOverlay( drawSurfs, processed );
		}
	}

	// now draw any post-processing effects using _currentRender
	if ( processed < numDrawSurfs ) {
		{
			idHitchScope hitch("draw_postprocess", hitchView);
			RB_STD_DrawShaderPasses( drawSurfs+processed, numDrawSurfs-processed );
		}
	}

	if ( retailGlow ) {
		{
			idHitchScope hitch("draw_bloom_composite", hitchView);
			RB_GlowOverlayRetail( glowWidth, glowHeight );
		}
	}

	{
		idHitchScope hitch("draw_debug", hitchView);
		RB_RenderDebugTools( drawSurfs, numDrawSurfs );
	}

}
