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
generated retail-derived ASE files. This repository contains only the importer.
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

## Regression coverage

`test.ps1` runs hidden, muted, isolated-profile tests. It compiles the authored
box-room map and tests primary/secondary placement, wall traversal in both
directions, solid wall outside the oval, save/reload, disabling/re-enabling,
floor-to-wall and floor-to-ceiling momentum, and an enemy blocking the exit.
The room is a collision fixture, not a representative lighting scene.
Campaign testing additionally checked slot-1 toggling, wheel cycling with the
Doom weapons, and save/reload of the selected tool.
