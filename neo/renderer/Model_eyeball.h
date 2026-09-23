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

// Shared eye UV projection, performed before portal mesh clipping.
#ifndef __MODEL_EYEBALL_H__
#define __MODEL_EYEBALL_H__
#define	MAX_EYEBALL_TRIS	10
#define	MAX_EYEBALL_ISLANDS	6

typedef struct {
	int		tris[MAX_EYEBALL_TRIS];
	int		numTris;
	idBounds	bounds;
	idVec3		mid;
} eyeIsland_t;

static void AddTriangleToIsland_r( const srfTriangles_t *tri, int triangleNum, bool *usedList, eyeIsland_t *island ) {
	int		a, b, c;

	usedList[triangleNum] = true;

	// add to the current island
	if ( island->numTris == MAX_EYEBALL_TRIS ) {
		common->Error( "MAX_EYEBALL_TRIS" );
	}
	island->tris[island->numTris] = triangleNum;
	island->numTris++;

	// recurse into all neighbors
	a = tri->indexes[triangleNum*3];
	b = tri->indexes[triangleNum*3+1];
	c = tri->indexes[triangleNum*3+2];

	island->bounds.AddPoint( tri->verts[a].xyz );
	island->bounds.AddPoint( tri->verts[b].xyz );
	island->bounds.AddPoint( tri->verts[c].xyz );

	int	numTri = tri->numIndexes / 3;
	for ( int i = 0 ; i < numTri ; i++ ) {
		if ( usedList[i] ) {
			continue;
		}
		if ( tri->indexes[i*3+0] == a
			|| tri->indexes[i*3+1] == a
			|| tri->indexes[i*3+2] == a
			|| tri->indexes[i*3+0] == b
			|| tri->indexes[i*3+1] == b
			|| tri->indexes[i*3+2] == b
			|| tri->indexes[i*3+0] == c
			|| tri->indexes[i*3+1] == c
			|| tri->indexes[i*3+2] == c ) {
			AddTriangleToIsland_r( tri, i, usedList, island );
		}
	}
}

/*
=====================
R_EyeballDeform

Each eyeball surface should have an separate upright triangle behind it, long end
pointing out the eye, and another single triangle in front of the eye for the focus point.
=====================
*/
static bool R_BakeEyeballSurface( const srfTriangles_t *tri, idList<idDrawVert> &vertices, idList<glIndex_t> &indexes ) {
	int		i, j, k;
	eyeIsland_t	islands[MAX_EYEBALL_ISLANDS];
	int			numIslands;
	bool		triUsed[MAX_EYEBALL_ISLANDS*MAX_EYEBALL_TRIS];


	// separate all the triangles into islands
	int		numTri = tri->numIndexes / 3;
	if ( numTri > MAX_EYEBALL_ISLANDS*MAX_EYEBALL_TRIS ) {
		common->Printf( "R_EyeballDeform: too many triangles in surface" );
		return false;
	}
	memset( triUsed, 0, sizeof( triUsed ) );

	for ( numIslands = 0  ; numIslands < MAX_EYEBALL_ISLANDS ; numIslands++ ) {
		islands[numIslands].numTris = 0;
		islands[numIslands].bounds.Clear();
		for ( i = 0 ; i < numTri ; i++ ) {
			if ( !triUsed[i] ) {
				AddTriangleToIsland_r( tri, i, triUsed, &islands[numIslands] );
				break;
			}
		}
		if ( i == numTri ) {
			break;
		}
	}

	// assume we always have two eyes, two origins, and two targets
	if ( numIslands != 3 ) {
		common->Printf( "R_EyeballDeform: %i triangle islands\n", numIslands );
		return false;
	}

	vertices.SetNum(tri->numVerts);
	for (int v = 0; v < tri->numVerts; ++v) vertices[v] = tri->verts[v];
	indexes.Clear();
	idDrawVert *ac = vertices.Ptr();

	// decide which islands are the eyes and points
	for ( i = 0 ; i < numIslands ; i++ ) {
		islands[i].mid = islands[i].bounds.GetCenter();
	}

	for ( i = 0 ; i < numIslands ; i++ ) {
		eyeIsland_t		*island = &islands[i];

		if ( island->numTris == 1 ) {
			continue;
		}

		// the closest single triangle point will be the eye origin
		// and the next-to-farthest will be the focal point
		idVec3	origin, focus;
		int		originIsland = 0;
		float	dist[MAX_EYEBALL_ISLANDS];
		int		sortOrder[MAX_EYEBALL_ISLANDS];

		for ( j = 0 ; j < numIslands ; j++ ) {
			idVec3	dir = islands[j].mid - island->mid;
			dist[j] = dir.Length();
			sortOrder[j] = j;
			for ( k = j-1 ; k >= 0 ; k-- ) {
				if ( dist[k] > dist[k+1] ) {
					int	temp = sortOrder[k];
					sortOrder[k] = sortOrder[k+1];
					sortOrder[k+1] = temp;
					float	ftemp = dist[k];
					dist[k] = dist[k+1];
					dist[k+1] = ftemp;
				}
			}
		}

		originIsland = sortOrder[1];
		origin = islands[originIsland].mid;

		focus = islands[sortOrder[2]].mid;

		// determine the projection directions based on the origin island triangle
		idVec3	dir = focus - origin;
		dir.Normalize();

		// Island entries are triangle numbers, not offsets into the index buffer.
		const int originIndex = islands[originIsland].tris[0] * 3;
		const idVec3 &p1 = tri->verts[tri->indexes[originIndex+0]].xyz;
		const idVec3 &p2 = tri->verts[tri->indexes[originIndex+1]].xyz;
		const idVec3 &p3 = tri->verts[tri->indexes[originIndex+2]].xyz;

		idVec3	v1 = p2 - p1;
		v1.Normalize();
		idVec3	v2 = p3 - p1;
		v2.Normalize();

		// texVec[0] will be the normal to the origin triangle
		idVec3	texVec[2];

		texVec[0].Cross( v1, v2 );

		texVec[1].Cross( texVec[0], dir );

		for ( j = 0 ; j < 2 ; j++ ) {
			texVec[j] -= dir * ( texVec[j] * dir );
			texVec[j].Normalize();
		}

		// emit these triangles, generating the projected texcoords

		for ( j = 0 ; j < islands[i].numTris ; j++ ) {
			for ( k = 0 ; k < 3 ; k++ ) {
				int	index = islands[i].tris[j] * 3;

				index = tri->indexes[index+k];
				indexes.Append(index);

				ac[index] = tri->verts[index];

				idVec3	local = tri->verts[index].xyz - origin;

				ac[index].st[0] = 0.5 + local * texVec[0];
				ac[index].st[1] = 0.5 + local * texVec[1];
			}
		}
	}

	return true;
}


#endif
