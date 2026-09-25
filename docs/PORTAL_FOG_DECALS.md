# Portal fog and decal occlusion

Direct portal views now clip fog/blend-light geometry at the destination plane.
Fog distance and volume-entry fade are evaluated from the ray's intersection
with that plane, rather than including the invisible segment between the
virtual camera and the exit. This prevents dense fog behind the exit from
filling the entire portal. Main-view fog and visible fog beyond the exit remain.
The generated ARB fragment program is recreated after a context reset; systems
without fragment-program support retain the existing distance calculation.

Polygon-offset surfaces (including per-stage offsets) are clipped against the
visible portal aperture and its backing half-space before the parent view is
drawn. Only the part behind the opening is removed. This avoids depth-bias
leakage without hiding foreground decals or the portions outside the opening.
Clipping uses frame-owned geometry and does not modify shared models. Nonplanar
apertures and surfaces without CPU geometry retain their existing behavior.

Validation:
- `portalred`: red portal fill removed, surrounding red mist retained.
- Own `decal_mask` fixture: old renderer draws green through the opening; new
  renderer removes that portion while preserving outside green and foreground red.
- Run `tools/portalgun/test.ps1` with cases `input,decal_mask` (as a PowerShell
  string array). Inspect `decal_clean.tga` and `decal_mask.tga` in the isolated
  profile. Fixture assets are independently authored, with no retail content.
- Campaign checks: nested portals, portal skybox, close weapon view, distant
  aperture mask. Tests use hidden windows and muted isolated profiles.

The user did not have a saved decal example, so campaign decal variants beyond
these material/geometry paths still need normal playtesting.

## Lit polygon-offset surfaces

`lightbug` exposed dark, view-dependent speckling on the airplane wall beside a
portal. Aperture subtraction had rebuilt its ambient/depth triangles while its
light interactions still used the original geometry. The resulting depth-equal
lighting test disagreed along the newly triangulated surface.

Track the source triangle of each clipped triangle and rebuild matching light
interaction draws using precisely the same clipped vertices and indices. Keep
each light's original triangle subset, shader parameters, scissor, and shadow
chains. All replacements are frame-owned and limited to the current view;
cached interaction geometry and other views remain unchanged. Subsequent portal
cuts operate on the updated geometry consistently.

Validated with hidden, muted `lightbug` static and turning captures, plus the
portal decal-mask and deep-view regression fixtures.
