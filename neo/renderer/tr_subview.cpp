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


typedef struct {
	idVec3		origin;
	idMat3		axis;
} orientation_t;


/*
=================
R_MirrorPoint
=================
*/
static void R_MirrorPoint( const idVec3 in, orientation_t *surface, orientation_t *camera, idVec3 &out ) {
	int		i;
	idVec3	local;
	idVec3	transformed;
	float	d;

	local = in - surface->origin;

	transformed = vec3_origin;
	for ( i = 0 ; i < 3 ; i++ ) {
		d = local * surface->axis[i];
		transformed += d * camera->axis[i];
	}

	out = transformed + camera->origin;
}

/*
=================
R_MirrorVector
=================
*/
static void R_MirrorVector( const idVec3 in, orientation_t *surface, orientation_t *camera, idVec3 &out ) {
	int		i;
	float	d;

	out = vec3_origin;
	for ( i = 0 ; i < 3 ; i++ ) {
		d = in * surface->axis[i];
		out += d * camera->axis[i];
	}
}

// Adapted from openPREY; see docs/OPENPREY_PORTS.md.
static void R_BuildPortalOrientations( const renderEntity_t &portalEntity, const renderView_t &remoteRenderView,
	orientation_t &surface, orientation_t &camera ) {
	surface.origin = portalEntity.origin;
	surface.axis = portalEntity.axis;

	camera.origin = remoteRenderView.vieworg;
	camera.axis[0] = -remoteRenderView.viewaxis[0];
	camera.axis[1] = -remoteRenderView.viewaxis[1];
	camera.axis[2] = remoteRenderView.viewaxis[2];
}

static void R_TransformPortalSubview( const renderEntity_t &portalEntity, const renderView_t &remoteRenderView, viewDef_t &parms ) {
	orientation_t surface, camera;

	R_BuildPortalOrientations( portalEntity, remoteRenderView, surface, camera );

	R_MirrorPoint( tr.viewDef->renderView.vieworg, &surface, &camera, parms.renderView.vieworg );
	R_MirrorVector( tr.viewDef->renderView.viewaxis[0], &surface, &camera, parms.renderView.viewaxis[0] );
	R_MirrorVector( tr.viewDef->renderView.viewaxis[1], &surface, &camera, parms.renderView.viewaxis[1] );
	R_MirrorVector( tr.viewDef->renderView.viewaxis[2], &surface, &camera, parms.renderView.viewaxis[2] );
}

static bool R_PortalViewerPassesRetailGate( const renderEntity_t &portalEntity ) {
	const idVec3 planePoint = tr.viewDef->isSubview
		? ( portalEntity.origin + portalEntity.axis[0] * 4.0f )
		: ( portalEntity.origin - portalEntity.axis[0] * 16.0f );

	return ( ( tr.viewDef->renderView.vieworg - planePoint ) * portalEntity.axis[0] ) > 0.0f;
}

static idCVar r_portalTrace("r_portalTrace", "0", CVAR_RENDERER | CVAR_BOOL, "log portal subview decisions");

static idCVar r_portalMaxDepth("r_portalMaxDepth", "3", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER,
    "maximum portal view layers, including the first portal", 1, 8);
static idCVar r_portalDeepViews("r_portalDeepViews", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER,
    "deep portal rendering: 0 legacy, 1 reduced resolution, 2 reduced resolution and guarded repeating fallback", 0, 2);
static int portalDeepFrame = -1, portalDeepViews = 0;
static int R_PortalDepth() {
    int depth = 0;
    for (const viewDef_t *v = tr.viewDef; v; v = v->superView)
        if (v->subviewSurface && v->subviewSurface->material->GetSubviewClass() == SC_PORTAL) ++depth;
    return depth;
}

static viewDef_t *R_PortalSubviewBySurface( drawSurf_t *drawSurf ) {
	int depth = 0;
	for (const viewDef_t *view = tr.viewDef; view; view = view->superView) {
		const drawSurf_t *surface = view->subviewSurface;
		if (surface && surface->material->GetSubviewClass() == SC_PORTAL) ++depth;
	}
	const renderView_t *remoteRenderView = drawSurf->space->entityDef->parms.remoteRenderView;
	if (r_portalTrace.GetBool()) common->Printf("PORTAL_TRACE entity %d depth %d remote %d gate %d\n", drawSurf->space->entityDef->parms.entityNum, depth, remoteRenderView ? 1 : 0, R_PortalViewerPassesRetailGate(drawSurf->space->entityDef->parms) ? 1 : 0);
	if ( !remoteRenderView ) {
		return NULL;
	}

	const renderEntity_t &portalEntity = drawSurf->space->entityDef->parms;
	if (r_portalTrace.GetBool()) common->Printf("PORTAL_PLANE entity %d origin %s normal %s eye_distance %.4f\n", portalEntity.entityNum,
		portalEntity.origin.ToString(), portalEntity.axis[0].ToString(),
		(tr.viewDef->renderView.vieworg - portalEntity.origin) * portalEntity.axis[0]);
	const int index = drawSurf->material->GetDirectPortalDistance();
	if ( index >= 0 && index < drawSurf->material->GetNumRegisters() && drawSurf->shaderRegisters != NULL ) {
		const float maxPortalDistanceLimit = drawSurf->shaderRegisters[index];
		if ( maxPortalDistanceLimit < 0 ) {
			return NULL;
		}
		if ( maxPortalDistanceLimit > 0 &&
			( tr.viewDef->renderView.vieworg - portalEntity.origin ).LengthSqr() > Square( maxPortalDistanceLimit * r_portalDistanceScale.GetFloat() ) ) {
			return NULL;
		}
	}

	if ( !R_PortalViewerPassesRetailGate( portalEntity ) ) {
		return NULL;
	}

	viewDef_t *parms = (viewDef_t *)R_FrameAlloc( sizeof( *parms ) );
	if ( !parms ) {
		return NULL;
	}

	*parms = *tr.viewDef;
    parms->portalCacheSlot = -1;
	parms->isSubview = true;
	parms->isMirror = false;
	parms->renderView.viewID = 0;	// clear to allow player bodies to show up, and suppress view weapons

	parms->numClipPlanes = 1;
	parms->clipPlanes[0] = remoteRenderView->viewaxis[0];
	parms->clipPlanes[0][3] = -( remoteRenderView->vieworg * parms->clipPlanes[0].Normal() );
    // A decal-style portal is mounted on solid world geometry. Exclude its
    // coplanar backing wall from the remote view without shifting the eye or
    // teleport transform. Free-standing retail portals retain their exact plane.
    if (drawSurf->material->TestMaterialFlag(MF_POLYGONOFFSET)) parms->clipPlanes[0][3] -= 0.25f;


	R_TransformPortalSubview( portalEntity, *remoteRenderView, *parms );
	// Select the area just inside the exit without displacing either the
	// projected eye or the clip plane from the physical portal transform.
	parms->initialViewAreaOrigin = remoteRenderView->vieworg + remoteRenderView->viewaxis[0] * 4.0f;

	return parms;
}

/*
=============
R_PlaneForSurface

Returns the plane for the first triangle in the surface
FIXME: check for degenerate triangle?
=============
*/
static void R_PlaneForSurface( const srfTriangles_t *tri, idPlane &plane ) {
	idDrawVert		*v1, *v2, *v3;

	v1 = tri->verts + tri->indexes[0];
	v2 = tri->verts + tri->indexes[1];
	v3 = tri->verts + tri->indexes[2];
	plane.FromPoints( v1->xyz, v2->xyz, v3->xyz );
}

/*
=========================
R_PreciseCullSurface

Check the surface for visibility on a per-triangle basis
for cases when it is going to be VERY expensive to draw (subviews)

If not culled, also returns the bounding box of the surface in
Normalized Device Coordinates, so it can be used to crop the scissor rect.

OPTIMIZE: we could also take exact portal passing into consideration
=========================
*/
bool R_PreciseCullSurface( const drawSurf_t *drawSurf, idBounds &ndcBounds ) {
	const srfTriangles_t *tri;
	idPlane clip, eye;
	int i, j;
	unsigned int pointOr;
	unsigned int pointAnd;
	idVec3 localView;
	idFixedWinding w;

	tri = drawSurf->geo;

	pointOr = 0;
	pointAnd = (unsigned int)~0;

	// get an exact bounds of the triangles for scissor cropping
	ndcBounds.Clear();

	for ( i = 0; i < tri->numVerts; i++ ) {
		int j;
		unsigned int pointFlags;

		R_TransformModelToClip( tri->verts[i].xyz, drawSurf->space->modelViewMatrix,
			tr.viewDef->projectionMatrix, eye, clip );

		pointFlags = 0;
		// A portal aperture must survive the last few units before the eye
		// crosses it. Its depth pass clamps Z rather than clipping the opening.
		const int clipAxes = r_portalDepthClampAvailable &&
			drawSurf->material->GetSubviewClass() == SC_PORTAL ? 2 : 3;
		for ( j = 0; j < clipAxes; j++ ) {
			if ( clip[j] >= clip[3] ) {
				pointFlags |= (1 << (j*2));
			} else if ( clip[j] <= -clip[3] ) {
				pointFlags |= ( 1 << (j*2+1));
			}
		}

		pointAnd &= pointFlags;
		pointOr |= pointFlags;
	}

	// trivially reject
	if ( pointAnd ) {
		return true;
	}

	// backface and frustum cull
	R_GlobalPointToLocal( drawSurf->space->modelMatrix, tr.viewDef->renderView.vieworg, localView );

	for ( i = 0; i < tri->numIndexes; i += 3 ) {
		idVec3	dir, normal;
		float	dot;
		idVec3	d1, d2;

		const idVec3 &v1 = tri->verts[tri->indexes[i]].xyz;
		const idVec3 &v2 = tri->verts[tri->indexes[i+1]].xyz;
		const idVec3 &v3 = tri->verts[tri->indexes[i+2]].xyz;

		// this is a hack, because R_GlobalPointToLocal doesn't work with the non-normalized
		// axis that we get from the gui view transform.  It doesn't hurt anything, because
		// we know that all gui generated surfaces are front facing
		if ( tr.guiRecursionLevel == 0 ) {
			// we don't care that it isn't normalized,
			// all we want is the sign
			d1 = v2 - v1;
			d2 = v3 - v1;
			normal = d2.Cross( d1 );

			dir = v1 - localView;

			dot = normal * dir;
			if ( dot >= 0.0f ) {
				// Portal apertures are not necessarily one planar polygon. A
				// back-facing edge must not discard the other visible triangles.
				if (drawSurf->material->GetSubviewClass() == SC_PORTAL) continue;
				return true;
			}
		}

		// now find the exact screen bounds of the clipped triangle
		w.SetNumPoints( 3 );
		R_LocalPointToGlobal( drawSurf->space->modelMatrix, v1, w[0].ToVec3() );
		R_LocalPointToGlobal( drawSurf->space->modelMatrix, v2, w[1].ToVec3() );
		R_LocalPointToGlobal( drawSurf->space->modelMatrix, v3, w[2].ToVec3() );
		w[0].s = w[0].t = w[1].s = w[1].t = w[2].s = w[2].t = 0.0f;


        // The usual 0.1-unit tolerance can exceed the eye-to-portal distance.
        // Keeping those off-frustum vertices can project behind the eye and
        // underbound the visible opening, leaving a black strip in the subview.
        const float clipEpsilon = drawSurf->material->GetSubviewClass() == SC_PORTAL ? 0.0f : 0.1f;
		for ( j = 0; j < 4; j++ ) {
			if ( !w.ClipInPlace( -tr.viewDef->frustum[j], clipEpsilon ) ) {
				break;
			}
		}
		for ( j = 0; j < w.GetNumPoints(); j++ ) {
			idVec3	screen;

			R_GlobalToNormalizedDeviceCoordinates( w[j].ToVec3(), screen );
			ndcBounds.AddPoint( screen );
		}
	}

	// if we don't enclose any area, return
	if ( ndcBounds.IsCleared() ) {
		return true;
	}

	return false;
}

/*
========================
R_MirrorViewBySurface
========================
*/
static viewDef_t *R_MirrorViewBySurface( drawSurf_t *drawSurf ) {
	viewDef_t		*parms;
	orientation_t	surface, camera;
	idPlane			originalPlane, plane;

	// copy the viewport size from the original
	parms = (viewDef_t *)R_FrameAlloc( sizeof( *parms ) );
	*parms = *tr.viewDef;
	parms->renderView.viewID = 0;	// clear to allow player bodies to show up, and suppress view weapons

	parms->isSubview = true;
	parms->isMirror = true;

	// create plane axis for the portal we are seeing
	R_PlaneForSurface( drawSurf->geo, originalPlane );
	R_LocalPlaneToGlobal( drawSurf->space->modelMatrix, originalPlane, plane );

	surface.origin = plane.Normal() * -plane[3];
	surface.axis[0] = plane.Normal();
	surface.axis[0].NormalVectors( surface.axis[1], surface.axis[2] );
	surface.axis[2] = -surface.axis[2];

	camera.origin = surface.origin;
	camera.axis[0] = -surface.axis[0];
	camera.axis[1] = surface.axis[1];
	camera.axis[2] = surface.axis[2];

	// set the mirrored origin and axis
	R_MirrorPoint( tr.viewDef->renderView.vieworg, &surface, &camera, parms->renderView.vieworg );

	R_MirrorVector( tr.viewDef->renderView.viewaxis[0], &surface, &camera, parms->renderView.viewaxis[0] );
	R_MirrorVector( tr.viewDef->renderView.viewaxis[1], &surface, &camera, parms->renderView.viewaxis[1] );
	R_MirrorVector( tr.viewDef->renderView.viewaxis[2], &surface, &camera, parms->renderView.viewaxis[2] );

	// make the view origin 16 units away from the center of the surface
	idVec3	viewOrigin = ( drawSurf->geo->bounds[0] + drawSurf->geo->bounds[1] ) * 0.5;
	viewOrigin += ( originalPlane.Normal() * 16 );

	R_LocalPointToGlobal( drawSurf->space->modelMatrix, viewOrigin, parms->initialViewAreaOrigin );

	// set the mirror clip plane
	parms->numClipPlanes = 1;
	parms->clipPlanes[0] = -camera.axis[0];

	parms->clipPlanes[0][3] = -( camera.origin * parms->clipPlanes[0].Normal() );

	return parms;
}

/*
========================
R_XrayViewBySurface
========================
*/
static viewDef_t *R_XrayViewBySurface( drawSurf_t *drawSurf ) {
	viewDef_t		*parms;
	idPlane			originalPlane, plane;

	// copy the viewport size from the original
	parms = (viewDef_t *)R_FrameAlloc( sizeof( *parms ) );
	*parms = *tr.viewDef;
	parms->renderView.viewID = 0;	// clear to allow player bodies to show up, and suppress view weapons

	parms->isSubview = true;
	parms->isXraySubview = true;

	return parms;
}

/*
===============
R_RemoteRender
===============
*/
static void R_TransformPortalSkyboxSubview( const renderView_t &remoteRenderView, viewDef_t &parms ) {
	parms.renderView.vieworg = remoteRenderView.vieworg;
	parms.renderView.viewaxis = tr.viewDef->renderView.viewaxis;
}

static viewDef_t *R_PortalSkyboxSubviewBySurface( drawSurf_t *drawSurf ) {
	const renderView_t *remoteRenderView = drawSurf->space->entityDef->parms.remoteRenderView;
	if ( !remoteRenderView ) {
		return NULL;
	}

	viewDef_t *parms = (viewDef_t *)R_FrameAlloc( sizeof( *parms ) );
	if ( !parms ) {
		return NULL;
	}

	*parms = *tr.viewDef;
	parms->isSubview = true;
	parms->isMirror = false;
	parms->renderView.viewID = 0;	// clear to allow player bodies to show up, and suppress view weapons
	R_TransformPortalSkyboxSubview( *remoteRenderView, *parms );
	parms->initialViewAreaOrigin = remoteRenderView->vieworg;

	return parms;
}

static void R_PortalRender( drawSurf_t *surf, textureStage_t *stage ) {
    if (R_PortalDepth() >= r_portalMaxDepth.GetInteger()) return;
	viewDef_t *parms;

	if ( stage->dynamicFrameCount == tr.frameCount ) {
		return;
	}

	parms = R_PortalSubviewBySurface( surf );
	if ( !parms ) {
		return;
	}

	tr.CropRenderSize( stage->width, stage->height, true );

	parms->renderView.x = 0;
	parms->renderView.y = 0;
	parms->renderView.width = SCREEN_WIDTH;
	parms->renderView.height = SCREEN_HEIGHT;

	tr.RenderViewToViewport( &parms->renderView, &parms->viewport );

	parms->scissor.x1 = 0;
	parms->scissor.y1 = 0;
	parms->scissor.x2 = parms->viewport.x2 - parms->viewport.x1;
	parms->scissor.y2 = parms->viewport.y2 - parms->viewport.y1;

	parms->superView = tr.viewDef;
	parms->subviewSurface = surf;

	R_RenderView( parms );

	stage->dynamicFrameCount = tr.frameCount;
	if ( !stage->image ) {
		stage->image = globalImages->scratchImage;
	}

	tr.CaptureRenderToImage( stage->image->imgName );
	tr.UnCrop();
}

static void R_SkyboxRender( drawSurf_t *surf, textureStage_t *stage ) {
	viewDef_t *parms;

	if ( stage->dynamicFrameCount == tr.frameCount ) {
		return;
	}

	parms = R_PortalSkyboxSubviewBySurface( surf );
	if ( !parms ) {
		return;
	}

	tr.CropRenderSize( stage->width, stage->height, true );

	parms->renderView.x = 0;
	parms->renderView.y = 0;
	parms->renderView.width = SCREEN_WIDTH;
	parms->renderView.height = SCREEN_HEIGHT;

	tr.RenderViewToViewport( &parms->renderView, &parms->viewport );

	parms->scissor.x1 = 0;
	parms->scissor.y1 = 0;
	parms->scissor.x2 = parms->viewport.x2 - parms->viewport.x1;
	parms->scissor.y2 = parms->viewport.y2 - parms->viewport.y1;

	parms->superView = tr.viewDef;
	parms->subviewSurface = surf;

	R_RenderView( parms );

	stage->dynamicFrameCount = tr.frameCount;
	if ( !stage->image ) {
		stage->image = globalImages->scratchImage;
	}

	tr.CaptureRenderToImage( stage->image->imgName );
	tr.UnCrop();
}

static void R_RemoteRender( drawSurf_t *surf, textureStage_t *stage ) {
	viewDef_t		*parms;

	// remote views can be reused in a single frame
	if ( stage->dynamicFrameCount == tr.frameCount ) {
		return;
	}

	// if the entity doesn't have a remoteRenderView, do nothing
	if ( !surf->space->entityDef->parms.remoteRenderView ) {
		return;
	}

	// copy the viewport size from the original
	parms = (viewDef_t *)R_FrameAlloc( sizeof( *parms ) );
	*parms = *tr.viewDef;

	parms->isSubview = true;
	parms->isMirror = false;

	parms->renderView = *surf->space->entityDef->parms.remoteRenderView;
	parms->renderView.viewID = 0;	// clear to allow player bodies to show up, and suppress view weapons
	parms->initialViewAreaOrigin = parms->renderView.vieworg;

	tr.CropRenderSize( stage->width, stage->height, true );

	parms->renderView.x = 0;
	parms->renderView.y = 0;
	parms->renderView.width = SCREEN_WIDTH;
	parms->renderView.height = SCREEN_HEIGHT;

	tr.RenderViewToViewport( &parms->renderView, &parms->viewport );

	parms->scissor.x1 = 0;
	parms->scissor.y1 = 0;
	parms->scissor.x2 = parms->viewport.x2 - parms->viewport.x1;
	parms->scissor.y2 = parms->viewport.y2 - parms->viewport.y1;

	parms->superView = tr.viewDef;
	parms->subviewSurface = surf;

	// generate render commands for it
	R_RenderView(parms);

	// copy this rendering to the image
	stage->dynamicFrameCount = tr.frameCount;
	if (!stage->image) {
		stage->image = globalImages->scratchImage;
	}

	tr.CaptureRenderToImage( stage->image->imgName );
	tr.UnCrop();
}

/*
=================
R_MirrorRender
=================
*/
void R_MirrorRender( drawSurf_t *surf, textureStage_t *stage, idScreenRect scissor ) {
	viewDef_t		*parms;

	// remote views can be reused in a single frame
	if ( stage->dynamicFrameCount == tr.frameCount ) {
		return;
	}

	// issue a new view command
	parms = R_MirrorViewBySurface( surf );
	if ( !parms ) {
		return;
	}

	tr.CropRenderSize( stage->width, stage->height, true );

	parms->renderView.x = 0;
	parms->renderView.y = 0;
	parms->renderView.width = SCREEN_WIDTH;
	parms->renderView.height = SCREEN_HEIGHT;

	tr.RenderViewToViewport( &parms->renderView, &parms->viewport );

	parms->scissor.x1 = 0;
	parms->scissor.y1 = 0;
	parms->scissor.x2 = parms->viewport.x2 - parms->viewport.x1;
	parms->scissor.y2 = parms->viewport.y2 - parms->viewport.y1;

	parms->superView = tr.viewDef;
	parms->subviewSurface = surf;

	// triangle culling order changes with mirroring
	parms->isMirror = ( ( (int)parms->isMirror ^ (int)tr.viewDef->isMirror ) != 0 );

	// generate render commands for it
	R_RenderView( parms );

	// copy this rendering to the image
	stage->dynamicFrameCount = tr.frameCount;
	stage->image = globalImages->scratchImage;

	tr.CaptureRenderToImage( stage->image->imgName );
	tr.UnCrop();
}

/*
=================
R_XrayRender
=================
*/
void R_XrayRender( drawSurf_t *surf, textureStage_t *stage, idScreenRect scissor ) {
	viewDef_t		*parms;

	// remote views can be reused in a single frame
	if ( stage->dynamicFrameCount == tr.frameCount ) {
		return;
	}

	// issue a new view command
	parms = R_XrayViewBySurface( surf );
	if ( !parms ) {
		return;
	}

	tr.CropRenderSize( stage->width, stage->height, true );

	parms->renderView.x = 0;
	parms->renderView.y = 0;
	parms->renderView.width = SCREEN_WIDTH;
	parms->renderView.height = SCREEN_HEIGHT;

	tr.RenderViewToViewport( &parms->renderView, &parms->viewport );

	parms->scissor.x1 = 0;
	parms->scissor.y1 = 0;
	parms->scissor.x2 = parms->viewport.x2 - parms->viewport.x1;
	parms->scissor.y2 = parms->viewport.y2 - parms->viewport.y1;

	parms->superView = tr.viewDef;
	parms->subviewSurface = surf;

	// triangle culling order changes with mirroring
	parms->isMirror = ( ( (int)parms->isMirror ^ (int)tr.viewDef->isMirror ) != 0 );

	// generate render commands for it
	R_RenderView( parms );

	// copy this rendering to the image
	stage->dynamicFrameCount = tr.frameCount;
	stage->image = globalImages->scratchImage2;

	tr.CaptureRenderToImage( stage->image->imgName );
	tr.UnCrop();
}

// Subtract the portal's projected opening from biased decal geometry. A depth
// bias may pull a decal in front of the portal in screen space despite the
// actual decal being on its backing surface. Preserve real foreground pieces.
// Keep depth/ambient and lighting on identical clipped triangles. Mutate only
// this view's draw surfaces, never cached interactions shared with other views.
static void R_ClipPortalDecalInteractions(const viewDef_t *view, const drawSurf_t *surface,
    const srfTriangles_t *source, srfTriangles_t *clipped, const idList<int> &parents) {
    for (viewLight_t *light = view->viewLights; light; light = light->next) {
        const drawSurf_t *chains[] = { light->localInteractions, light->globalInteractions, light->translucentInteractions };
        for (int chain = 0; chain < 3; ++chain) {
            for (const drawSurf_t *interaction = chains[chain]; interaction; interaction = interaction->nextOnLight) {
                const srfTriangles_t *old = interaction->geo;
                if (interaction->space != surface->space || interaction->material != surface->material ||
                    !old || old->ambientSurface != source) continue;
                idHashIndex hash;
                for (int i = 0; i < old->numIndexes; i += 3)
                    hash.Add(hash.GenerateKey(old->indexes[i], old->indexes[i+1]), i/3);
                idList<glIndex_t> indexes;
                for (int t = 0; t < parents.Num(); ++t) {
                    const glIndex_t *original = source->indexes + parents[t];
                    for (int candidate = hash.First(hash.GenerateKey(original[0], original[1])); candidate != -1; candidate = hash.Next(candidate)) {
                        const glIndex_t *lit = old->indexes + candidate*3;
                        if (lit[0] != original[0] || lit[1] != original[1] || lit[2] != original[2]) continue;
                        for (int k = 0; k < 3; ++k) indexes.Append(clipped->indexes[t*3+k]);
                        break;
                    }
                }
                srfTriangles_t *replacement = (srfTriangles_t *)R_FrameAlloc(sizeof(*replacement));
                *replacement = *clipped;
                replacement->ambientSurface = clipped;
                replacement->numIndexes = indexes.Num();
                replacement->indexes = (glIndex_t *)R_FrameAlloc(indexes.Num()*sizeof(glIndex_t));
                if (indexes.Num()) memcpy(replacement->indexes, indexes.Ptr(), indexes.Num()*sizeof(glIndex_t));
                replacement->indexCache = replacement->lightingCache = NULL;
                const_cast<drawSurf_t *>(interaction)->geo = replacement;
            }
        }
    }
}
static void R_ClipDecalsBehindPortal(const viewDef_t *view, const drawSurf_t *portal) {
    const srfTriangles_t *aperture = portal->geo;
    if (!aperture || !aperture->verts || aperture->numVerts < 3) return;
    const idVec3 normal = portal->space->entityDef->parms.axis[0];
    idVec3 point;
    R_LocalPointToGlobal(portal->space->modelMatrix, aperture->verts[0].xyz, point);
    idPlane opening; opening.SetNormal(normal); opening.FitThroughPoint(point);
    if (opening.Distance(view->renderView.vieworg) <= 0.001f) return;
    idWinding hull(aperture->numVerts);
    for (int i = 0; i < aperture->numVerts; ++i) {
        R_LocalPointToGlobal(portal->space->modelMatrix, aperture->verts[i].xyz, point);
        if (idMath::Fabs(opening.Distance(point)) > 0.05f) return;
        hull.AddToConvexHull(point, normal, 0.001f);
    }
    if (hull.GetNumPoints() < 3) return;
    idList<idPlane> worldPlanes;
    worldPlanes.Append(opening);
    const idVec3 center = hull.GetCenter();
    for (int i = 0; i < hull.GetNumPoints(); ++i) {
        idPlane side;
        if (!side.FromPoints(view->renderView.vieworg, hull[i].ToVec3(),
            hull[(i+1)%hull.GetNumPoints()].ToVec3())) return;
        if (side.Distance(center) > 0) side = -side;
        worldPlanes.Append(side);
    }
    for (int surfaceIndex = 0; surfaceIndex < view->numDrawSurfs; ++surfaceIndex) {
        drawSurf_t *surf = view->drawSurfs[surfaceIndex];
        const idMaterial *material = surf->material;
        if (!material || material->HasSubview() || surf->space == portal->space ||
            surf->space->weaponDepthHack || surf->space->modelDepthHack != 0) continue;
        bool decal = material->TestMaterialFlag(MF_POLYGONOFFSET);
        for (int stage = 0; !decal && stage < material->GetNumStages(); ++stage)
            decal = material->GetStage(stage)->privatePolygonOffset != 0;
        const srfTriangles_t *source = surf->geo;
        if (!decal || !source || !source->verts || !source->numIndexes) continue;
        idScreenRect overlap = surf->scissorRect; overlap.Intersect(portal->scissorRect);
        if (overlap.IsEmpty()) continue;
        idList<idPlane> planes;
        for (int i = 0; i < worldPlanes.Num(); ++i) {
            idPlane local; R_GlobalPlaneToLocal(surf->space->modelMatrix, worldPlanes[i], local);
            planes.Append(local);
        }
        bool disjoint = false;
        for (int i = 0; i < planes.Num(); ++i)
            if (source->bounds.PlaneDistance(planes[i]) > 0.001f) { disjoint = true; break; }
        if (disjoint) continue;
        idList<idDrawVert> result;
        idList<int> parents;
        bool changed = false;
        for (int index = 0; index < source->numIndexes; index += 3) {
            const int firstResult = result.Num();
            idList<idDrawVert> remaining, fragments;
            for (int k = 0; k < 3; ++k) remaining.Append(source->verts[source->indexes[index+k]]);
            for (int planeIndex = 0; planeIndex < planes.Num() && remaining.Num() >= 3; ++planeIndex) {
                idList<idDrawVert> inside, outside;
                const idPlane &plane = planes[planeIndex];
                for (int k = 0; k < remaining.Num(); ++k) {
                    const idDrawVert &a = remaining[k], &b = remaining[(k+1)%remaining.Num()];
                    const float da = plane.Distance(a.xyz)-0.001f, db = plane.Distance(b.xyz)-0.001f;
                    if (da <= 0) inside.Append(a); else outside.Append(a);
                    if ((da <= 0) != (db <= 0)) {
                        idDrawVert cut; cut.LerpAll(a, b, da/(da-db));
                        inside.Append(cut); outside.Append(cut);
                    }
                }
                for (int k = 1; k+1 < outside.Num(); ++k) {
                    fragments.Append(outside[0]); fragments.Append(outside[k]); fragments.Append(outside[k+1]);
                }
                remaining = inside;
            }
            if (remaining.Num() < 3) {
                for (int k = 0; k < 3; ++k) result.Append(source->verts[source->indexes[index+k]]);
            } else {
                changed = true;
                result.Append(fragments);
            }
            for (int t = firstResult; t < result.Num(); t += 3) parents.Append(index);
        }
        if (!changed) continue;
        srfTriangles_t *tri = (srfTriangles_t *)R_FrameAlloc(sizeof(*tri));
        *tri = *source;
        tri->numVerts = tri->numIndexes = result.Num();
        tri->verts = (idDrawVert *)R_FrameAlloc(result.Num()*sizeof(idDrawVert));
        tri->indexes = (glIndex_t *)R_FrameAlloc(result.Num()*sizeof(glIndex_t));
        tri->bounds.Clear();
        for (int i = 0; i < result.Num(); ++i) {
            tri->verts[i] = result[i]; tri->indexes[i] = i; tri->bounds.AddPoint(result[i].xyz);
        }
        tri->indexCache = NULL;
        tri->ambientCache = result.Num() ? vertexCache.AllocFrameTemp(tri->verts, result.Num()*sizeof(idDrawVert)) : NULL;
        if (!result.Num() || tri->ambientCache) {
            R_ClipPortalDecalInteractions(view, surf, source, tri, parents);
            surf->geo = tri;
        }
    }
}

static void R_PortalBackgroundImage(idImage *image) {
    const byte black[16] = { 0 };
    image->GenerateImage(black, 2, 2, TF_NEAREST, false, TR_CLAMP, TD_HIGH_QUALITY);
}
static portalApertureCommand_t *R_PortalApertureCommand(const viewDef_t *parent, const drawSurf_t *surface,
    const idScreenRect &scissor, idImage *image, bool restore) {
    portalApertureCommand_t *cmd = (portalApertureCommand_t *)R_GetCommandBuffer(sizeof(*cmd));
    cmd->commandId = RC_PORTAL_APERTURE;
    cmd->parent = parent; cmd->surface = surface; cmd->scissor = scissor;
    cmd->image = image; cmd->restore = restore;
    cmd->composite = 0; cmd->textureWidth = cmd->textureHeight = 0;
    return cmd;
}

static portalTargetCommand_t *R_PortalTargetCommand(const viewDef_t *parent, idImage *background, int level, int width, int height, bool begin) {
    portalTargetCommand_t *cmd = (portalTargetCommand_t *)R_GetCommandBuffer(sizeof(*cmd));
    cmd->commandId = RC_PORTAL_TARGET; cmd->parent = parent; cmd->background = background;
    cmd->level = level; cmd->width = width; cmd->height = height; cmd->begin = begin;
    return cmd;
}

// Previous-frame images are isolated by complete portal ancestry and endpoint poses.
// A result is consumed only before its owning ancestor is rendered this frame.
struct portalDeepCache_t {
    unsigned int key;
    int frame, usedFrame, time;
    float fovX, fovY;
    idVec3 eye;
    idMat3 axis;
    idScreenRect rect;
    idImage *image;
};
static portalDeepCache_t portalDeepCache[32];
void R_ClearPortalHistory() {
    for (int i = 0; i < 32; ++i) {
        portalDeepCache[i].key = 0;
        portalDeepCache[i].frame = portalDeepCache[i].usedFrame = -1;
    }
    portalDeepFrame = -1;
}
static unsigned int R_PortalPathKey(const viewDef_t *view) {
    unsigned int hash = 2166136261u;
    for (const viewDef_t *v = view; v; v = v->superView) {
        const drawSurf_t *s = v->subviewSurface;
        if (!s || s->material->GetSubviewClass() != SC_PORTAL) continue;
        const renderEntity_t &e = s->space->entityDef->parms;
        const unsigned char *data = (const unsigned char *)e.origin.ToFloatPtr();
        for (int i = 0; i < sizeof(idVec3); ++i) hash = (hash ^ data[i]) * 16777619u;
        data = (const unsigned char *)e.axis.ToFloatPtr();
        for (int i = 0; i < sizeof(idMat3); ++i) hash = (hash ^ data[i]) * 16777619u;
        if (e.remoteRenderView) {
            data = (const unsigned char *)e.remoteRenderView->vieworg.ToFloatPtr();
            for (int i = 0; i < sizeof(idVec3); ++i) hash = (hash ^ data[i]) * 16777619u;
            data = (const unsigned char *)e.remoteRenderView->viewaxis.ToFloatPtr();
            for (int i = 0; i < sizeof(idMat3); ++i) hash = (hash ^ data[i]) * 16777619u;
        }
        hash = (hash ^ s->space->entityDef->parms.entityNum) * 16777619u;
    }
    hash = (hash ^ view->viewport.x2) * 16777619u;
    hash = (hash ^ view->viewport.y2) * 16777619u;
    const unsigned char *world = (const unsigned char *)&view->renderWorld;
    for (int i = 0; i < sizeof(view->renderWorld); ++i) hash = (hash ^ world[i]) * 16777619u;
    return hash;
}
static int R_PortalCacheSlot(const viewDef_t *view) {
    const unsigned int key = R_PortalPathKey(view);
    int oldest = -1;
    for (int i = 0; i < 32; ++i) {
        if (portalDeepCache[i].usedFrame == tr.frameCount) continue;
        if (portalDeepCache[i].key == key) { oldest = i; break; }
        if (oldest < 0 || portalDeepCache[i].frame < portalDeepCache[oldest].frame) oldest = i;
    }
    if (oldest < 0) return -1;
    portalDeepCache_t &cache = portalDeepCache[oldest];
    if (cache.key != key) cache.frame = -1;
    cache.key = key; cache.usedFrame = tr.frameCount;
    if (!cache.image) cache.image = globalImages->ImageFromFunction(va("_portalHistory%d", oldest), R_PortalBackgroundImage);
    return oldest;
}
static bool R_PortalRepeatFallback(const drawSurf_t *surf, const viewDef_t *next, const idScreenRect &scissor) {
    if (r_portalDeepViews.GetInteger() != 2) return false;
    const viewDef_t *root = tr.viewDef;
    while (root->superView) root = root->superView;
    const float scale = float(root->viewport.x2 - root->viewport.x1 + 1) /
        (tr.viewDef->viewport.x2 - tr.viewDef->viewport.x1 + 1);
    if (Max(scissor.x2 - scissor.x1 + 1, scissor.y2 - scissor.y1 + 1) * scale > 96) return false;
    for (const viewDef_t *v = tr.viewDef; v; v = v->superView) {
        if (!v->subviewSurface || v->subviewSurface->material->GetSubviewClass() != SC_PORTAL ||
            v->subviewSurface->space->entityDef != surf->space->entityDef || v->portalCacheSlot < 0) continue;
        const portalDeepCache_t &cache = portalDeepCache[v->portalCacheSlot];
        if (cache.frame != tr.frameCount - 1 || cache.fovX != root->renderView.fov_x || cache.fovY != root->renderView.fov_y || cache.time > root->renderView.time || root->renderView.time - cache.time > 100 ||
            (cache.eye - root->renderView.vieworg).LengthSqr() > 16 ||
            cache.axis[0] * root->renderView.viewaxis[0] < 0.9998f || cache.axis[1] * root->renderView.viewaxis[1] < 0.9998f) continue;
        if (v->renderView.viewaxis[0] * next->renderView.viewaxis[0] < 0.9998f ||
            v->renderView.viewaxis[1] * next->renderView.viewaxis[1] < 0.9998f) continue;
        idVec3 delta = next->renderView.vieworg - v->renderView.vieworg;
        delta -= v->renderView.viewaxis[0] * (delta * v->renderView.viewaxis[0]);
        if (delta.LengthSqr() > 1.0f) continue;
        portalApertureCommand_t *cmd = R_PortalApertureCommand(tr.viewDef, surf, scissor, cache.image, true);
        cmd->composite = 3; cmd->textureRect = cache.rect;
        if (r_portalTrace.GetBool()) common->Printf("PORTAL_REPEAT_FALLBACK depth %d\n", R_PortalDepth() + 1);
        return true;
    }
    return false;
}

/*
==================
R_GenerateSurfaceSubview
==================
*/
bool	R_GenerateSurfaceSubview( drawSurf_t *drawSurf ) {
	idBounds		ndcBounds;
	viewDef_t		*parms;
	const idMaterial		*shader;

	// for testing the performance hit
	if ( r_skipSubviews.GetBool() ) {
		return false;
	}

	if (r_portalTrace.GetBool()) common->Printf("PORTAL_CANDIDATE entity %d material %s class %d subview %d\n", drawSurf->space->entityDef->parms.entityNum, drawSurf->material->GetName(), drawSurf->material->GetSubviewClass(), tr.viewDef->isSubview ? 1 : 0);
	if ( R_PreciseCullSurface( drawSurf, ndcBounds ) ) {
        if (r_portalTrace.GetBool()) common->Printf("PORTAL_CULL\n");
		return false;
	}

	shader = drawSurf->material;

	// Mirrors and texture subviews retain their cycle guard. A direct portal
	// may legitimately reappear from a different transformed viewpoint; its
	// recursion is bounded by r_portalMaxDepth instead of surface identity.
	for ( parms = (shader->GetSort() == SS_SUBVIEW && shader->GetSubviewClass() == SC_PORTAL)
		? NULL : tr.viewDef ; parms ; parms = parms->superView ) {
		if ( parms->subviewSurface
			&& parms->subviewSurface->geo == drawSurf->geo
			&& parms->subviewSurface->space->entityDef == drawSurf->space->entityDef ) {
			break;
		}
	}
	if ( parms ) {
        if (r_portalTrace.GetBool()) common->Printf("PORTAL_CYCLE\n");
		return false;
	}

	// crop the scissor bounds based on the precise cull
	idScreenRect	scissor;

	idScreenRect	*v = &tr.viewDef->viewport;
	scissor.x1 = v->x1 + (int)( (v->x2 - v->x1 + 1 ) * 0.5f * ( ndcBounds[0][0] + 1.0f ));
	scissor.y1 = v->y1 + (int)( (v->y2 - v->y1 + 1 ) * 0.5f * ( ndcBounds[0][1] + 1.0f ));
	scissor.x2 = v->x1 + (int)( (v->x2 - v->x1 + 1 ) * 0.5f * ( ndcBounds[1][0] + 1.0f ));
	scissor.y2 = v->y1 + (int)( (v->y2 - v->y1 + 1 ) * 0.5f * ( ndcBounds[1][1] + 1.0f ));

	// nudge a bit for safety
	scissor.Expand();

	scissor.Intersect( tr.viewDef->scissor );

	if ( scissor.IsEmpty() ) {
		// cropped out
		return false;
	}

	// DG: r_lockSurfaces needs special treatment
	if ( r_lockSurfaces.GetBool() && tr.viewDef == tr.primaryView ) {
		// we need the scissor for the "real" viewDef (actual camera position etc)
		// so mirrors don't "float around" when looking around with r_lockSurfaces enabled
		// So do the same calculation as before, but with real viewDef (but don't replace
		// calculation above, so the whole mirror or whatever is skipped if not visible in
		// locked view!)
		viewDef_t* origViewDef = tr.viewDef;
		tr.viewDef = &tr.lockSurfacesRealViewDef;
		R_PreciseCullSurface( drawSurf, ndcBounds );

		idScreenRect origScissor = scissor;

		idScreenRect	*v2 = &tr.viewDef->viewport;
		scissor.x1 = v2->x1 + (int)( (v2->x2 - v2->x1 + 1 ) * 0.5f * ( ndcBounds[0][0] + 1.0f ));
		scissor.y1 = v2->y1 + (int)( (v2->y2 - v2->y1 + 1 ) * 0.5f * ( ndcBounds[0][1] + 1.0f ));
		scissor.x2 = v2->x1 + (int)( (v2->x2 - v2->x1 + 1 ) * 0.5f * ( ndcBounds[1][0] + 1.0f ));
		scissor.y2 = v2->y1 + (int)( (v2->y2 - v2->y1 + 1 ) * 0.5f * ( ndcBounds[1][1] + 1.0f ));

		// nudge a bit for safety
		scissor.Expand();

		scissor.Intersect( tr.viewDef->scissor );

		// TBH I'm not 100% happy with how this is handled - you won't get reliable information
		// on what's rendered in a mirror this way. Intersecting with the orig. scissor looks "best".
		// For handling this "properly" we'd need the whole "locked viewDef vs real viewDef" thing
		// for every subview (instead of just once for the primaryView) which would be a lot of
		// work for a corner case...
		scissor.Intersect( origScissor );
		tr.viewDef = origViewDef;
	} // DG end

	// see what kind of subview we are making
	if ( shader->GetSort() != SS_SUBVIEW ) {
		for ( int i = 0 ; i < shader->GetNumStages() ; i++ ) {
			const shaderStage_t	*stage = shader->GetStage( i );
			switch ( stage->texture.dynamic ) {
			case DI_REMOTE_RENDER:
				R_RemoteRender( drawSurf, const_cast<textureStage_t *>(&stage->texture) );
				break;
			case DI_MIRROR_RENDER:
				R_MirrorRender( drawSurf, const_cast<textureStage_t *>(&stage->texture), scissor );
				break;
            case DI_PORTAL_RENDER:
                R_PortalRender( drawSurf, const_cast<textureStage_t *>(&stage->texture) );
                break;
            case DI_SKYBOX_RENDER:
                R_SkyboxRender( drawSurf, const_cast<textureStage_t *>(&stage->texture) );
                break;
			case DI_XRAY_RENDER:
				R_XrayRender( drawSurf, const_cast<textureStage_t *>(&stage->texture), scissor );
				break;
			}
		}
		return true;
	}

	switch ( shader->GetSubviewClass() ) {
		case SC_PORTAL: {
			parms = R_PortalSubviewBySurface( const_cast<drawSurf_t *>( drawSurf ) );
			if ( !parms ) {
				return false;
			}
			if (r_portalTrace.GetBool()) common->Printf("PORTAL_RENDER\n");
			parms->scissor = scissor;
			parms->superView = tr.viewDef;
			parms->subviewSurface = drawSurf;
            const viewDef_t *parent = tr.viewDef;
            int level = 0;
            for (const viewDef_t *view = parent; view; view = view->superView) ++level;
            const int depth = R_PortalDepth();
            if (portalDeepFrame != tr.frameCount) { portalDeepFrame = tr.frameCount; portalDeepViews = 0; }
            const bool reduced = r_portalDeepViews.GetInteger() && depth >= 2;
            if (depth >= r_portalMaxDepth.GetInteger() || (reduced && portalDeepViews >= 12)) {
                const bool reused = R_PortalRepeatFallback(drawSurf, parms, scissor);
                if (r_portalTrace.GetBool()) common->Printf("PORTAL_DEPTH_LIMIT depth %d reused %d\n", depth, reused);
                if (reused) R_ClipDecalsBehindPortal(parent, drawSurf);
                return reused;
            }
            if (reduced) ++portalDeepViews;
            idImage *background = globalImages->ImageFromFunction(
                va("_portalBackground%d", level), R_PortalBackgroundImage);
            if (reduced) {
                const int pw = parent->viewport.x2 - parent->viewport.x1 + 1;
                const int ph = parent->viewport.y2 - parent->viewport.y1 + 1;
                // A whole-view target keeps projection and screen-space material coordinates
                // identical. Its scissor follows the visible portal footprint.
                const int tw = Min(pw, Max(16, pw / 2)), th = Min(ph, Max(16, ph / 2));
                parms->portalCacheSlot = R_PortalCacheSlot(parms);
                idScreenRect whole; whole.Clear(); whole.x1 = whole.y1 = 0;
                whole.x2 = pw - 1; whole.y2 = ph - 1;
                const bool target = level < 32 && R_PortalTargetsAvailable();
                portalTargetCommand_t *targetBegin = NULL;
                if (target) targetBegin = R_PortalTargetCommand(parent, background, level, tw, th, true);
                else R_PortalApertureCommand(parent, NULL, whole, background, false);
                parms->viewport.x1 = parms->viewport.y1 = 0;
                parms->viewport.x2 = tw - 1; parms->viewport.y2 = th - 1;
                parms->scissor.x1 = Max(0, scissor.x1 * tw / pw);
                parms->scissor.y1 = Max(0, scissor.y1 * th / ph);
                parms->scissor.x2 = Min(tw - 1, ((scissor.x2 + 1) * tw + pw - 1) / pw);
                parms->scissor.y2 = Min(th - 1, ((scissor.y2 + 1) * th + ph - 1) / ph);
                R_RenderView(parms);
                if (targetBegin) {
                    // Texture cameras can use a larger scratch viewport than the deep
                    // view itself. Reserve their pixels too, without upscaling the view.
                    for (const emptyCommand_t *c = (const emptyCommand_t *)targetBegin->next; c; c = (const emptyCommand_t *)c->next) {
                        if (c->commandId == RC_DRAW_VIEW) {
                            const viewDef_t *v = ((const drawSurfsCommand_t *)c)->viewDef;
                            targetBegin->width = Max(targetBegin->width, int(v->viewport.x2) + 1);
                            targetBegin->height = Max(targetBegin->height, int(v->viewport.y2) + 1);
                        } else if (c->commandId == RC_COPY_RENDER) {
                            const copyRenderCommand_t *copy = (const copyRenderCommand_t *)c;
                            targetBegin->width = Max(targetBegin->width, copy->x + copy->imageWidth);
                            targetBegin->height = Max(targetBegin->height, copy->y + copy->imageHeight);
                        }
                    }
                }
                idImage *result = parms->portalCacheSlot >= 0 ? portalDeepCache[parms->portalCacheSlot].image :
                    globalImages->ImageFromFunction(va("_portalDeep%d", level), R_PortalBackgroundImage);
                idScreenRect capture; capture.Clear(); capture.x1 = capture.y1 = 0;
                capture.x2 = tw - 1; capture.y2 = th - 1;
                R_PortalApertureCommand(parms, NULL, capture, result, false);
                if (target) R_PortalTargetCommand(parent, background, level, tw, th, false);
                else R_PortalApertureCommand(parent, NULL, whole, background, true)->composite = 1;
                portalApertureCommand_t *compose = R_PortalApertureCommand(parent, drawSurf, scissor, result, true);
                compose->composite = 2; compose->textureWidth = tw; compose->textureHeight = th;
                if (parms->portalCacheSlot >= 0) {
                    portalDeepCache_t &cache = portalDeepCache[parms->portalCacheSlot];
                    const viewDef_t *root = parent; while (root->superView) root = root->superView;
                    cache.frame = tr.frameCount; cache.time = root->renderView.time;
                    cache.eye = root->renderView.vieworg; cache.axis = root->renderView.viewaxis;
                    cache.rect = parms->scissor; cache.fovX = root->renderView.fov_x; cache.fovY = root->renderView.fov_y;
                }
                if (r_portalTrace.GetBool()) common->Printf("PORTAL_DEEP depth %d target %d %d\n", depth + 1, tw, th);
            } else {
                // Bracket the complete subtree so siblings and sky pixels survive.
                R_PortalApertureCommand(parent, drawSurf, scissor, background, false);
                R_RenderView(parms);
                R_PortalApertureCommand(parent, drawSurf, scissor, background, true);
            }
            R_ClipDecalsBehindPortal(parent, drawSurf);
			return true;
		}
		case SC_PORTAL_SKYBOX: {
			if ( !drawSurf->space->entityDef->parms.remoteRenderView ) {
				return false;
			}

			// A portal view needs its own skybox background. Reject only a
			// skybox already in this ancestry, rather than every subview.
			for (const viewDef_t *view = tr.viewDef; view; view = view->superView) {
				const drawSurf_t *surface = view->subviewSurface;
				if (surface && surface->material->GetSubviewClass() == SC_PORTAL_SKYBOX) return false;
			}

			// copy the viewport size from the original
			parms = (viewDef_t *)R_FrameAlloc( sizeof(*parms) );
			if ( !parms ) {
				return false;
			}
			*parms = *tr.viewDef;

			parms->isSubview = true;
			parms->isMirror = false;

			const renderView_t *remoteRenderView = drawSurf->space->entityDef->parms.remoteRenderView;
			parms->renderView.viewID = 0;	// clear to allow player bodies to show up, and suppress view weapons
			parms->initialViewAreaOrigin = remoteRenderView->vieworg;
			parms->renderView.vieworg = remoteRenderView->vieworg;
			// This camera is in the skybox room, not the portal exit's space.
			// Render the parent's entire scissor: all cube faces share this view.
			parms->numClipPlanes = 0;
			if (r_portalTrace.GetBool()) common->Printf("SKYBOX_RENDER entity %d parent_subview %d origin %s\n",
				drawSurf->space->entityDef->parms.entityNum, tr.viewDef->isSubview ? 1 : 0, remoteRenderView->vieworg.ToString());

			parms->superView = tr.viewDef;
			parms->subviewSurface = drawSurf;

			// generate render commands for it
			R_RenderView( parms );
			tr.viewDef->glowSkyView = parms;
			return true;
		}
		case SC_MIRROR:
		default: {
			// issue a new view command
			parms = R_MirrorViewBySurface( drawSurf );
			if ( !parms ) {
				return false;
			}

			parms->scissor = scissor;
			parms->superView = tr.viewDef;
			parms->subviewSurface = drawSurf;

			// triangle culling order changes with mirroring
			parms->isMirror = ( ( (int)parms->isMirror ^ (int)tr.viewDef->isMirror ) != 0 );

			// generate render commands for it
			R_RenderView( parms );

			return true;
		}
	}
}

/*
================
R_GenerateSubViews

If we need to render another view to complete the current view,
generate it first.

It is important to do this after all drawSurfs for the current
view have been generated, because it may create a subview which
would change tr.viewCount.
================
*/
static float R_SubviewDepth( const drawSurf_t *surf ) {
	idVec3 center;
	R_LocalPointToGlobal( surf->space->modelMatrix, surf->geo->bounds.GetCenter(), center );
	return (center - tr.viewDef->renderView.vieworg) * tr.viewDef->renderView.viewaxis[0];
}

bool R_GenerateSubViews( void ) {
	drawSurf_t		*drawSurf;
	int				i;
	bool			subviews;
	const idMaterial		*shader;

	// for testing the performance hit
	if ( r_skipSubviews.GetBool() ) {
		return false;
	}

	subviews = false;

	// Direct subviews share the color buffer; the parent depth pass preserves
	// that color inside their apertures. Draw farther siblings first so a
	// portal behind another cannot overwrite the nearer portal's remote scene.
	// Sort only the subview list, leaving the parent's material order intact.
	idList<drawSurf_t *> directSubviews;
	for ( i = 0; i < tr.viewDef->numDrawSurfs; ++i ) {
		drawSurf_t *surf = tr.viewDef->drawSurfs[i];
		const idMaterial *material = surf->material;
		if (!material || !material->HasSubview() || material->GetSort() != SS_SUBVIEW ||
			material->GetSubviewClass() == SC_PORTAL_SKYBOX) continue;
		int index = directSubviews.Append(surf);
		const float depth = R_SubviewDepth(surf);
		while (index > 0 && R_SubviewDepth(directSubviews[index - 1]) < depth) {
			directSubviews[index] = directSubviews[index - 1];
			--index;
		}
		directSubviews[index] = surf;
	}

	// Texture subviews use the lower-left framebuffer as scratch storage.
	// Capture them before direct views, otherwise a moving mirror/monitor
	// can overwrite a completed portal with its rectangular scratch image.
	// Then draw the sky background before the direct portal apertures.
	for ( int pass = 0; pass < 3; ++pass ) {
		const int count = pass == 2 ? directSubviews.Num() : tr.viewDef->numDrawSurfs;
		for ( i = 0 ; i < count ; i++ ) {
			drawSurf = pass == 2 ? directSubviews[i] : tr.viewDef->drawSurfs[i];
			shader = drawSurf->material;

			if ( !shader || !shader->HasSubview() ) {
				continue;
			}
			const bool background = shader->GetSort() == SS_SUBVIEW && shader->GetSubviewClass() == SC_PORTAL_SKYBOX;
			const int subviewPass = shader->GetSort() != SS_SUBVIEW ? 0 : ( background ? 1 : 2 );
			if ( subviewPass != pass ) {
				continue;
			}

			if ( R_GenerateSurfaceSubview( drawSurf ) ) {
				subviews = true;
				// One background per parent view, not one for the whole frame.
				if (background) break;
			}
		}
	}

	return subviews;
}
