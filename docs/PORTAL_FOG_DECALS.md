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
