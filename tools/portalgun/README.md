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

Traversal currently supports players only, not props, enemies or projectiles.
The player's speed is rotated through the pair without an arbitrary velocity
boost or position correction. Occupied exits block traversal instead of telefragging.
This is a sandbox experiment: portals can bypass campaign triggers and puzzles.
Not a complete recreation of Valve's portal mechanics.

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
The room is a collision fixture, not a representative lighting scene.
Campaign testing additionally checked slot-1 toggling, wheel cycling with the
Doom weapons, and save/reload of the selected tool.

The `portaltest` campaign save was also checked at 3.63 units/second, including
stopping while straddling the exit and reversing through it. All four crossings
kept the floor height at 448.25 and reported zero teleport position correction.
