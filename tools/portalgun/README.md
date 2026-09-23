# Experimental portal gun

Enable Portal gun in the native launcher. Press 1 again while holding the wrench
(or cycle weapons) to select the portal tool. Primary places blue; secondary
places orange. Both endpoints must be placed before traversal is possible.
No viewmodel is supplied in this mechanics prototype.

The renderer uses the player's retail Prey oval mesh, original UVs and animated
orange/blue portal materials. The importer flattens the free-standing funnel
against the wall and normalizes its opening to 96 by 144 units. It omits the
backside and outer refraction surfaces. Original texture animations remain;
the retail skeletal opening/closing animation is not part of this prototype.

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
coordinate frame and is blocked only while the player's hull straddles an opening,
not merely because the player is standing nearby.

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
