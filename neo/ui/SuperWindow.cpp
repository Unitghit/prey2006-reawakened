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

#include "Window.h"
#include "UserInterfaceLocal.h"
#include "SuperWindow.h"

void hhSuperWindow::CommonInit(void)
{
	idWindow::CommonInit();

	barMat.Reset();
	sideMat.Reset();
	cornerSize = idVec2(-1.0f, -1.0f);
	edgeSize = idVec2(-1.0f, -1.0f);
	margins.Zero();
}

hhSuperWindow::hhSuperWindow(idDeviceContext *d, idUserInterfaceLocal *g) : idWindow(d, g)
{
	dc = d;
	gui = g;
	CommonInit();
}

hhSuperWindow::hhSuperWindow(idUserInterfaceLocal *g) : idWindow(g)
{
	gui = g;
	CommonInit();
}

bool hhSuperWindow::ParseInternalVar(const char *_name, idParser *src)
{
	if (idStr::Icmp(_name, "leftMat") == 0) {
		idStr materialName;
		ParseString(src, materialName);
		sideMat.name = materialName;
		return true;
	}
	if (idStr::Icmp(_name, "cornerSize") == 0)
	{
		cornerSize[0] = src->ParseFloat();
		src->ExpectTokenString(",");
		cornerSize[1] = src->ParseFloat();
		return true;
	}
	if (idStr::Icmp(_name, "edgeSize") == 0)
	{
		edgeSize[0] = src->ParseFloat();
		src->ExpectTokenString(",");
		edgeSize[1] = src->ParseFloat();
		return true;
	}
	if (idStr::Icmp(_name, "margins") == 0)
	{
		margins[0] = src->ParseFloat();
		src->ExpectTokenString(",");
		margins[1] = src->ParseFloat();
		src->ExpectTokenString(",");
		margins[2] = src->ParseFloat();
		src->ExpectTokenString(",");
		margins[3] = src->ParseFloat();
		return true;
	}

	return idWindow::ParseInternalVar(_name, src);
}

idWinVar * hhSuperWindow::GetWinVarByName(const char *_name, bool fixup, drawWin_t **owner)
{
	if (idStr::Icmp(_name, "leftMat") == 0) {
		return &sideMat.name;
	}
	if (idStr::Icmp(_name, "topMat") == 0)
	{
		return &barMat.middle.name;
	}
	if (idStr::Icmp(_name, "cornerMat") == 0)
	{
		return &barMat.left.name;
	}

	return idWindow::GetWinVarByName(_name, fixup, owner);
}

void hhSuperWindow::PostParse(void)
{
	idWindow::PostParse();

	barMat.Setup(cornerSize[0]);
	sideMat.Setup();
}

// Frame geometry adapted from openPREY (themuffinator).
static void DrawRetailSuperWindowFrame( idDeviceContext *dc, const idRectangle &drawRect, const idMaterial *cornerMat, const idMaterial *sideMat, const idMaterial *topMat, float cornerWidth, float cornerHeight, float edgeSizeX, float edgeSizeY, const idVec4 &frameColor ) {
	if ( dc == NULL || drawRect.w <= 0.0f || drawRect.h <= 0.0f ) {
		return;
	}

	cornerWidth = idMath::Fabs( cornerWidth );
	cornerHeight = idMath::Fabs( cornerHeight );
	edgeSizeX = idMath::Fabs( edgeSizeX );
	edgeSizeY = idMath::Fabs( edgeSizeY );

	const bool drawTopSegments = ( drawRect.h > cornerHeight );

	if ( cornerMat != NULL && cornerWidth > 0.0f && cornerHeight > 0.0f ) {
		if ( drawTopSegments ) {
			dc->DrawMaterial( drawRect.x, drawRect.y, cornerWidth, cornerHeight, cornerMat, frameColor );
			dc->DrawMaterial( drawRect.x + drawRect.w - cornerWidth, drawRect.y, -cornerWidth, cornerHeight, cornerMat, frameColor );
		}

		dc->DrawMaterial( drawRect.x, drawRect.y + drawRect.h - cornerHeight, cornerWidth, -cornerHeight, cornerMat, frameColor );
		dc->DrawMaterial( drawRect.x + drawRect.w - cornerWidth, drawRect.y + drawRect.h - cornerHeight, -cornerWidth, -cornerHeight, cornerMat, frameColor );
	}

	const float horizontalX = drawRect.x + cornerWidth;
	const float horizontalW = drawRect.w - cornerWidth - cornerWidth;
	if ( topMat != NULL && edgeSizeY > 0.0f && horizontalW > 0.0f ) {
		if ( drawTopSegments ) {
			dc->DrawMaterial( horizontalX, drawRect.y, horizontalW, edgeSizeY, topMat, frameColor );
		}

		dc->DrawMaterial( horizontalX, drawRect.y + drawRect.h - edgeSizeY, horizontalW, -edgeSizeY, topMat, frameColor );
	}

	const float verticalY = drawRect.y + cornerHeight;
	const float verticalH = drawRect.h - cornerHeight - cornerHeight;
	if ( sideMat != NULL && edgeSizeX > 0.0f && verticalH > 0.0f ) {
		dc->DrawMaterial( drawRect.x, verticalY, edgeSizeX, verticalH, sideMat, frameColor );
		dc->DrawMaterial( drawRect.x + drawRect.w - edgeSizeX, verticalY, -edgeSizeX, verticalH, sideMat, frameColor );
	}
}

void hhSuperWindow::DrawBackground( const idRectangle &bounds ) {
    idRectangle inset = bounds;
    inset.x += margins[0];
    inset.y += margins[2];
    inset.w -= margins[0] + margins[1];
    inset.h -= margins[2] + margins[3];
    if ( inset.w > 0.0f && inset.h > 0.0f ) {
        idWindow::DrawBackground( inset );
    }
}

void hhSuperWindow::Draw( int time, float x, float y ) {
    const idMaterial *corner = barMat.left.material;
    const float cw = cornerSize[0] >= 0.0f ? cornerSize[0] : ( corner ? corner->GetImageWidth() : 16.0f );
    const float ch = cornerSize[1] >= 0.0f ? cornerSize[1] : ( corner ? corner->GetImageHeight() : 16.0f );
    const float ew = edgeSize[0] >= 0.0f ? edgeSize[0] : 4.0f;
    const float eh = edgeSize[1] >= 0.0f ? edgeSize[1] : 4.0f;
    DrawRetailSuperWindowFrame( dc, drawRect, corner, sideMat.material, barMat.middle.material, cw, ch, ew, eh, matColor );
    idWindow::Draw( time, x, y );
}
