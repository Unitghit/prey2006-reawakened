# Lighter illumination through portals

`g_portalLighter 1` (default) creates up to four temporary destination-side
copies of the local player's lighter. This is single-player, one-hop lighting;
copied lights cannot create further copies.

The portal transforms the light origin and axis. The original radius, material,
color parameters and timing remain intact, retaining the retail falloff and
flicker. Four planes through the virtual source and aperture edges, plus the
destination plane, restrict illumination. Dedicated variants of the six stock
interaction shaders evaluate these planes per fragment, using model-local
position in texcoord 7 and fragment constants 22–26. This preserves depth-prepass
triangle coverage instead of clipping/retriangulating geometry. Ordinary lights
use their unchanged shaders; portal-view clipping still uses plane 0.

Both interaction creation and view-light traversal exclude first-person
depth-hacked models and entities associated with the owner's view ID, including
the player's body and weapon attachments. Entity numbers alone are deliberately
not used: static world render entities default to entity number zero, which also
identifies the single-player player. Existing weapon-light blending is
independent of these copies.

Handles are reused, disabled after each rendered player view, and released at
game shutdown. Turning off the lighter or the feature leaves no active copies.
Save restoration clears the transient fields. The engine/game interface version
is 17: deploy the matching executable and game DLL together; save serialization
has not changed.

Limits:

- The aperture uses the portal's rectangular local Y/Z bounds. Irregular
  apertures are approximated by those bounds.
- Copies do not cast shadows. The retail `lights/playerlighter` material also
  specifies `noshadows`; this does not introduce destination-object occlusion.
- At most four eligible portals are processed in entity order, with no recursion.
- Render demos do not preserve the additional transient clip fields.

Validation artifacts and muted test scripts are in the parent workspace's
`validation/portal-lighter` directory; the world-surface exclusion and per-pixel
clipping corrections are tested in `validation/portal-lighter-refine`.
Static on/off captures show additional
destination lighting. Crossing tests exercise both directions at 144 FPS;
the smoke test includes saving/reloading with the lighter on, existing portal
saves, menus, spirit mode and third person. The isolated test command
`fpsTestView lighteron|lighteroff` requires `com_fpsTrace` and is disabled in
multiplayer.

The initial geometric clipping changed triangle rasterization between the depth
prepass and additive lighting pass. The per-pixel version removes that source
of depth-equality artifacts. It does not enable shadow casting: the lighter and
its copies still retain their no-shadow behavior. Static world surfaces now
receive the light without weakening the owner view-ID/weapon exclusions.

The handoff follows the rendered eye's side of the portal, rather than the
offset flame's side. The actual light and copies use the same camera-relative
presentation transform and restore the simulation light after drawing. At the
aperture plane, the clipping-cone apex stays at least 0.1 units on the entrance
side to avoid degenerate or inverted planes; the shading origin keeps its true
transformed position. Handoff tests are in `validation/portal-lighter-handoff`.
