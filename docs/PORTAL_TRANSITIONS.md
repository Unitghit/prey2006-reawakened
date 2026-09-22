# Portal presentation continuity

Portal crossings previously called `InvalidateEntityPresentation`, and the
player's `SetOrientation` cleared the same history again. Unlocked rendering
therefore snapped for a fixed simulation tick at each crossing.

The new `g_interpolatePortals` option defaults to 1. For eligible single-player
first-person crossings, the portal's rigid transform rebases the previous
camera, aim, gravity basis, weapon models, and attached lights. Model-local
weapon skeletons and beam nodes retain their local coordinates. Ordinary world
objects are not transformed. The existing view interpolation then blends in a
single coordinate system. Until the interpolated eye reaches the destination
plane, its rendered view is mapped back to the entrance side.

Portal entity thinking may run after the player's camera/weapon calculation.
A snapshot identifies that stale camera exactly. Presentation transforms that
view temporarily, uses the original camera basis for current weapon-local
poses, and restores the simulation render view after drawing. The next history
capture also rebases those stale poses before the player's next regular tick.
No extra player Think, weapon update, physics tick, or movement easing is added.

Ordinary teleports still invalidate history. Collision recovery that displaces
the exit position, multiple crossings in one tick, unsupported camera modes,
legacy rendering and multiplayer retain conservative fallback behavior. Existing
mode, gravity, distance, and rotation guards still apply. The new history is
transient and is not added to savegames.

Muted isolated tests live in `validation/portal-transition`. The trace-gated
`g_presentationTestForward` injects normal movement commands in single player;
it is inactive without `com_fpsTrace` and resets on map shutdown. `fps-portal.csv`
records camera coordinates, interpolation, and which side is being rendered.
When tracing, a post-draw check verifies player physics position/velocity and
simulation beam nodes are unchanged by presentation.

Validation: the smoothing-disabled round trip recorded nine non-interpolated
render frames across two crossings. With smoothing enabled, the same portal
pair was crossed in both directions at configured caps of 144 and 360 FPS.
All 338/507 traced render frames retained interpolation. Four/thirteen samples
landed in the crossing ticks, covering both entrance and destination sides.
Camera positions mapped to the expected interpolated path within 0.001 world
units; weapon models and attached lights remained presented in those samples.
These caps are test settings, not sustained-FPS benchmark claims.

The deployed smoke test also covers save restores and immediate post-restore
rendering, ordinary setviewpos teleports, third person, spirit/body and gravity
transitions, menus, the previously black decal, and the multi_portals save.
No GL errors or presentation physics/beam-mutation failures were reported.
This is targeted validation of the available saves, not every portal or gravity
arrangement in the campaign.

Initial deployment: `validation/portal-transition-build`. The renderer executable is
byte-identical to the decal-lighting build; the packaged game DLL and symbols
match the newly built source. AllFixes, AlienText, TranslatedText, and 144FPS
launchers use the new build. Existing saves and player settings are preserved.
`g_interpolatePortals 0` offers a comparison with the previous snapping behavior.

## Crossing movement and body visibility follow-up

The follow-up build is `validation/portal-crossing-build`.

The original `AttemptPortal` discarded the movement remaining after intersecting
the portal plane. In the measured crossing, 2.324 units of a 2.880-unit step
were lost, even though velocity remained 180 units/second. Our transformed
camera reached the full endpoint, so the following camera step shrank to
0.550 units while physics caught up. This was a real position discontinuity.

`g_portalPreserveMotion` defaults to 1 for single-player players. It carries
the full endpoint through the same rigid transform used for velocity, sweeps
the remaining exit segment against world/static collision, and retains the
existing solid-exit recovery, gravity, and telefrag handling. A clipped sweep
or recovery displacement invalidates presentation history. Non-player entities
and multiplayer retain the old path. Set the CVar to 0 for a legacy-motion
comparison; it is not archived. This intentionally changes crossing position,
but does not add simulation ticks, alter speed, or ease physical movement.

There was also a first-person visibility mismatch: portal views use view ID 0
to show third-person bodies. During the crossing tick, bound head/world-weapon
render definitions can still be on the entrance side while the interpolated
camera is already on the exit side. Looking through the return portal briefly
exposes those meshes beside the first-person weapon. The local player's normal
first-person suppression is now extended to subviews for that crossing tick.
Renderer copies are restored after Draw, so other actors and normal views of
Tommy through portals remain unaffected outside that brief transition.

Evidence and muted scripts are in `validation/portal-crossing-fix`. The
`attachmentslast` diagnostic of `fpsTestView` reproduces the permitted entity
update ordering that exposed the body/weapon fragment. A temporary paired
render probe captured the same camera with and without local third-person
meshes. `body-attachmentslast-360` shows the fragment; `body-fixed-360` removes
it. The paired-render probe is archived in `probed-prey_game.cpp` and is not
part of the deployed build. Its second render can change the portal-border
bloom, so that border difference is not counted as body geometry evidence.
The trace-gated view/order commands and near-portal frame capture remain
available for isolated regression testing.

The final 144/360-cap movement runs have capture readback disabled to avoid
biasing timing. Both directions retain their expected 2.88-unit post-crossing
horizontal camera step and approximately 180-unit speed. Crossing render
samples retain interpolation. The edge test exercises the existing solid-exit
repositioning fallback; it is deliberately not smoothed through geometry.
The deployed smoke test repeats save loads, immediate post-load rendering,
third person, spirit/body and gravity transitions, menus, black_floor, and
multi_portals. Detailed measurements and package hashes are recorded in the
build manifest. These are targeted save-based checks, not exhaustive coverage
of every portal, gravity arrangement, or multiplayer mode.

The subsequent [alignment correction](PORTAL_ALIGNMENT.md) removes a separate
four-unit bias between the rendered portal and the physical destination. The
current launchers use `validation/portal-alignment-build`.
