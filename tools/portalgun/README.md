# Experimental portal gun

Enable Portal gun in the native launcher. Press 1 again while holding the wrench
(or cycle weapons) to select the portal tool. Primary places blue; secondary
places orange. Both endpoints must be placed before traversal is possible.
An optional first-person Portal gun model can be imported locally as described
below. Without that import the mechanics-only tool remains available.

## Portal first-person model

The local importer uses the installed PC Portal game and the command-line Crowbar
decompiler to convert its gun mesh, weighted skeleton, and ten animation sequences
into Prey's MD5 format. It requires Python with numpy and Pillow:

```
python tools/portalgun/import_viewmodel.py <Portal-install> <Crowbar-CLI-exe> <runtime-base> <temporary-work-folder>
```

Use [Crowbar Command Line](https://github.com/UltraTechX/Crowbar-Command-Line),
tested at revision `0c5950af196fe1edcce55b2b54d3c159490e5db0`, built against .NET
Framework 4.8. This is an external conversion dependency; neither its executable
nor extracted Valve content is included in this repository.

The existing launcher Portal gun option controls the feature. When imported files
are present, selecting the tool in slot 1 displays the model. Draw, idle, firing,
holster and lowered animations use the imported sequences; both shots use Portal's
fire animation and their corresponding sounds. Placement is still controlled by
our existing portal mechanics, not by weapon projectiles or animation events.

Prey's native weapon renderer handles lighting and portal presentation. The shell
and glass now use a dedicated, independently written Phong interaction shader:
the normal-map alpha supplies specular strength, and the exponent texture varies
highlight sharpness from 1 to 150. The shell uses the original material's boost
and three Fresnel controls; the glass has its own controls and a subtle neutral
additive layer over the illuminated chamber. This approximates Source shading
under Prey's lights; it does not reproduce Source's ambient probes, lightwarp,
HDR pipeline, or complete particle system. No dropped
or third-person world model is imported. Keep imported files
installed when loading saves that contain the model; disabling the launcher option
is supported and restores the wrench without changing weapon inventory slots.

The converter inserts a stationary root so Prey's removal of root motion does not
erase Source's camera-relative offsets. The recovered bind pose was checked to
within 0.001 game units. The optional `viewmodel` regression case requires imported
assets and checks both fire inputs, save/reload, disabling and re-enabling.

The importer also reads the original QC attachment locations and adds skinned,
camera-facing glow quads for the body indicator and chamber. A 0.35-unit local
offset keeps their centers clear of the housing under Prey's weapon projection.
The original blue/orange sprite textures and chamber sprite are extracted locally.
Both illuminated parts follow the last shot: primary blue, secondary orange.
The color persists through switching and saves, with a short time-based firing
pulse and gentle idle pulse. These are emissive effects, not extra world lights;
normal depth testing, weapon visibility and portal handling remain active.

Re-run the importer and update the engine/game DLL together. The engine requires
`base/glprogs/portalgunPhong.vfp` (included in source builds). Existing imports
with the old grayscale specular map retain their original shading until imported
again. No new launcher switch is needed; the existing Portal gun option controls
the complete feature. The optional `materials` test case checks both colors,
lit/dark screenshots, save/reload, switching and traversal with the imported gun.

The renderer uses the player's retail Prey oval mesh, original UVs and animated
orange/blue portal materials. The importer flattens the free-standing funnel
against the wall and preserves its original size and proportions: the visible
opening is approximately 76 by 93 units, matching the orange energy portal used
as the `portalsize` reference. Both colors retain the larger 96 by 144 traversal
envelope and existing player corner/foot allowances. Visual sizing does not change
teleport frames, collision clearance, placement support checks or saved positions.
Re-run the importer and restart the game to update existing saved portals too.
The importer omits the backside and outer refraction surfaces.

## Traveling shots and opening effects

Firing captures the world aim and sends a harmless blue/orange visual projectile
at 4,000 units per second. Placement happens on arrival, after a fresh surface and
occupancy check; the previous portal remains usable until then. New same-color
shots supersede older flights. Opposite colors travel independently. Solid objects
that intercept a shot reject it without damage or relocation. Turning away does
not retarget a flight. Disabling the feature or losing its owner cancels it.

Prey's particle system supplies the flight core, fading world-space trail, impact
burst and smaller rejected-hit burst. With a local Portal import, these use the
energy-ball and blue/orange particle textures referenced by Portal's projectile
particle definitions. This is an adaptation, not execution of Source's PCF system.
Without the optional import, the effects use a retail Prey particle texture.

`build_assets.py` now also requires numpy. It flattens the retail skeletal opening
animation by projecting its translation-only joint motion onto the surface plane.
The rim and rendered view aperture grow together at fixed proportions, using the
expanding portion of the retail motion as a timing envelope over approximately
0.45 seconds. The scripted lead-in and overshoot/recoil are omitted. The final
pose exactly matches the static artwork, without an added settling transition.
Collision and traversal clearance are fully open immediately. Floors and ceilings
use the same surface-oriented effect.
Flight state, pending times and opening animation survive saves in the updated
build; existing saves have no pending state and need no conversion.

Update the game DLL, source-built shot definitions/particles/materials, and rerun
both local importers for the complete feature. The existing launcher Portal gun
option controls it. The `shots` regression case covers replacement timing, saved
flights/openings, looking away, superseded shots, rejected surfaces, late obstacles
and cancellation. `portalGun shootblue` / `shootorange` are developer-only test
commands; the older immediate `blue` / `orange` placement commands remain for
physics fixtures.

## Local asset preparation

```
python tools/portalgun/build_assets.py <retail-base-containing-pk4-files> <runtime-base>
```

Run this locally against the user's own Prey installation. Do not distribute the
generated retail-derived ASE and material files. This repository contains only the importer.
Deploy both the updated executable and game DLL: this feature appends a collision
manager interface method, keeping previous virtual slots in their original order.

## Scope

Single player, base game only. Requires the wrench. Placement accepts sufficiently
large flat stationary world surfaces, rejects unsupported openings, overlapping
endpoints and replacement while the player occupies an endpoint. A pair is saved
as ordinary portal entities; selection uses the existing player spawn dictionary,
without extending the saved weapon array. Disabling the option hides existing
endpoints and restores solid-world collision. Endpoints are map-local.

Traversal supports players, native Prey projectiles (including the imported Doom
plasma/rocket projectile classes), and unbound movable props that fit the opening.
Native no-portal flags are respected. AI, vehicles and bound entities remain
excluded from player-placed portals. Existing authored/moving portals retain their
original eligibility and behavior.
The player's speed is rotated through the pair without an arbitrary velocity
boost or position correction. Occupied exits block traversal instead of telefragging.
This is a sandbox experiment: portals can bypass campaign triggers and puzzles.
Not a complete recreation of Valve's portal mechanics.

## Replacement and floor orientation

Each color reuses its existing endpoint: a valid shot removes the old opening
and immediately reconnects the opposite color to the new location. Invalid shots
leave the existing pair intact. Replacement clears crossing history from the old
coordinate frame and is blocked while an eligible entity's hull straddles an
opening, not merely because the player is standing nearby.

Floor and ceiling placements orient the oval along the projected aiming direction.
Straight-down/up shots retain the player's viewing heading. Upright walls continue
to align with local gravity. All placements still require a sufficiently large,
flat static surface.

## Walk-through wall portals

Upright portals within step range of a flat floor align their centers 73 units
above that floor. Existing saved portal-tool endpoints receive this adjustment
once when loaded. Higher placements and floor/ceiling portals are not snapped.
The lower collision opening has flat foot clearance so the rounded artwork does
not force the player's box hull to climb a step. The supporting floor still
participates in collision; traversal does not lift or push the player.

The opening and its teleport frame sit one unit ahead of the supporting wall,
clearing thin decoration layers such as the half-unit-offset grime decal in
`dirtyblueportal`. Collision cutting still uses the real wall plane. The offset
is stored in the existing spawn dictionary and migrated once for older saves;
reloading cannot accumulate additional movement.

Imported portal materials use a depth bias against their supporting wall. The
remote clip plane excludes the coplanar backing face by 0.25 units without
changing the camera or teleport transform. Retail free-standing portals retain
their original materials and clip planes. Re-run the importer after updating from
the initial prototype to generate the adjusted local materials.

## Regression coverage

`test.ps1` runs hidden, muted, isolated-profile tests. It compiles the authored
box-room map and tests primary/secondary placement, wall traversal in both
directions, solid wall outside the oval, save/reload, disabling/re-enabling,
floor-to-wall and floor-to-ceiling momentum, and an enemy blocking the exit.
Repeated replacement tests cover both colors, placement near the previous opening,
angled and vertical floor shots, and traversal of the relocated pair after reload.
The room is a collision fixture, not a representative lighting scene.
Campaign testing additionally checked slot-1 toggling, wheel cycling with the
Doom weapons, and save/reload of the selected tool.

The `portaltest` campaign save was also checked at 3.63 units/second, including
stopping while straddling the exit and reversing through it. All four crossings
kept the floor height at 448.25 and reported zero teleport position correction.

## Entry clearance and collision shells

Player hulls have an eight-unit corner allowance within the oval, plus additional
foot clearance below upright openings. This avoids square hull corners catching
on the visible rim without steering or repositioning the camera. Standing over a
new floor portal seeds the missing crossing immediately, instead of waiting for
an origin already behind the offset plane to cross it again.

A nearby invisible player-clip shell can be registered as a second backing plane.
Only the matched non-solid player-clip surface within 32 units of the wall
and its parallel back face (up to 32 units thick) are removed, and only while
the entity fits the opening. Solid obstructions,
outside-aperture collision and occupied destination protection remain active.
The saved `portalcantgoin` case contains both sloped goo on the approach and such
an invisible clip shell. The latter previously blocked the opening entirely.

Fast projectile sweeps validate the hull where it intersects the portal, even
when a tick spans more than the player's approach band. Projectile teleportation
uses the existing Prey path. Props preserve the rest of their movement step and
rotate angular velocity as well as linear velocity. The access fixture checks
rifle/plasma shots, a movable prop, and entering a floor portal within ten ticks
of placing it directly underfoot. The main fixture includes an invisible clip
shell ahead of one wall, with an outside-aperture solid-wall control.

## Portal-tool traversal refinements

While falling toward an upward-facing linked portal, looking down with little
steering input gently guides the player toward its center. This uses simulation
time and local gravity. Strong steering and fast lateral movement disable it;
it never snaps the player's position or changes ordinary jump height.

Slow upward floor exits receive enough outward speed for up to 24 units of
clearance, reduced when a hull sweep finds limited headroom. Faster exits retain
their transformed velocity. These adjustments apply only to player-created portals.

Placement searches the surrounding surface plane on an eight-unit grid, nearest
first, up to 48 units away. Every candidate must pass the original surface-support
checks. Failed shots leave the previous linked pair intact. Replacing an opening
is deferred while a player, eligible projectile, or movable prop straddles either
endpoint. Props continue to use Prey's native per-entity portal handling; bound
entities and joint-connected groups are not newly supported.

The added fixtures cover guidance and its steering/look/speed controls, low-speed
floor exits, horizontal and downward placement fitting, rejected out-of-range
placement, touching/stacked props, and replacement with an occupied opening.

## Portal edge clearance and placement fitting

Floor-edge collision uses the supporting surface plane, including the one-unit
artwork offset. A hull already clear of the floor can walk away; an embedded hull
still collides with the aperture boundary. `floor_escape` probes both cases,
and `reverse` checks four consecutive crossings with immediate direction changes.
Placement checks the visible oval plus a small margin (39 by 49 unit radii),
including perimeter samples, rather than requiring backing for the larger
traversal envelope. The nearest-position search is bounded to 64 units. Low wall
shots can align upward to a walk-through height when that surface supports it.
Real exit obstructions retain the existing full-hull rejection checks.

Floor entry also recovers an already-partial crossing when contact resolution or a
save places the feet below the portal before its proximity history is initialized.
Recovery requires the player hull to still cross the plane, fit the aperture, and
not be moving outward. Fully backside players are excluded. `floor_partial`
checks both partial recovery and backside rejection; destination obstruction
checks still apply before teleporting.

Static map decoration meshes now receive the same portal collision half-space as
world brushes, transformed into each mesh's local coordinates. Translation,
contact gathering, and contents checks use the same boundary. Movable entities
and trace-model props retain normal collision. `static_exit` checks that geometry
behind an exit does not block the emerging hull, while geometry in front does.
The small test obstacle mesh is independently authored test geometry.

## Continuous ground entry

Ground portals retain the player on the source side until the eye crosses the
portal plane. The feet and body can enter first; source collision keeps the
opening available through that interval. Existing partial-entry saves resume
without forcing an immediate feet-based teleport. A swept eye crossing also
handles fast falls whose final hull has already passed the thin portal trigger.

The emerged part of the player hull is checked in the destination while entering,
so an obstructed exit stops further inward movement before camera transfer.
The blocked view and weapon pose are corrected together. Outward motion can
withdraw from partial entry without teleportation. No fade or artificial entry
impulse is applied; the existing rigid transform preserves crossing velocity.

`floor_continuous` checks partial-entry save/load, outward withdrawal, and both
normal and fast eye-plane crossings. Floor momentum assertions compare the speed
immediately before and after crossing; the extra fall distance before eye entry
naturally increases speed relative to the former feet-triggered transition.

Remote partial-body checks are limited to hulls accepted by the entrance aperture
and below the actual supporting surface. The raised visual plane alone does not
mean a grounded player has entered. This prevents remote geometry from blocking
approach beside a floor portal. `floor_approach` checks walking in from both ends.


Blocked ground entry preserves collision-checked movement along the portal plane,
both during partial entry and at final camera crossing. This lets the player move
away from an obstructed edge instead of gravity repeatedly undoing that movement.
Destination checks use the transformed physics hull, independent of view yaw.
`floor_edge_slide` verifies an obstacle stops entry but allows movement toward
a clear part of the opening, followed by a successful crossing.


Portal occupancy tests filter individual contacts at or behind the supporting
plane, including edge/plane intersections on polygons spanning both sides.
This prevents hidden wall-trim corners from catching the un-emerged portion of
the player. Geometry in front remains solid. `floor_corner` checks rotated
static trim spanning the portal plane and a corresponding foreground obstacle.


Partial ground entry can resolve a shallow world overlap of at most two units
plus a quarter-unit clearance margin. The correction must point toward the
opening center and lie along its plane. Both the swept source hull and the
emerged destination hull must be clear. Only velocity pushing into the contact
is removed; falling and other tangential motion are preserved. Larger overlaps
and movable obstacles retain ordinary blocking behavior. `floor_clearance` tests
a shallow exit-floor contact and a deeper obstruction.


## Surface fitting

Placement requires a flat central ellipse with radii 19.5 by 24.5 units, one
quarter of the former 39 by 49 area. The full footprint still needs world
backing, but its outer portion may recess up to eight units behind the cutout
plane. Protrusions and unsupported openings are rejected. A conservative oval
prism checks solid clearance across the full window, including static objects.
Near-floor wall centers use 71 units instead of 73; existing saved endpoints
keep their placement. Nearby fitting remains bounded to the original search.
`surface_fit` verifies a small raised patch with recessed surround, a solid
window obstruction, and lower wall placement. Clearance fixture offsets follow
the new wall height so they retain the same shallow/deep penetration amounts.

