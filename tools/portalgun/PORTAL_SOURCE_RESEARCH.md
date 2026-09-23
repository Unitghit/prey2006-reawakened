# Portal traversal research

Reviewed 2026-09-22. Reference: SonicEraZoR/Portal-Base, revision `c4584551916acfb1e9583d54587ac84be48c9768`.
Research only: no engine behavior changed in this pass. Downloaded reference files
remain under the local validation directory; this document contains observations
and implementation proposals, not copied implementation code.

## Reference limits

This is a community adaptation to Source SDK 2013, not proof of the exact behavior
of the installed retail binary. Its README documents modified camera reorientation,
placement timing, and unresolved displacement/physics issues. Use retail Portal
as the behavioral comparison. The repository carries the Source SDK license;
this investigation does not establish permission to transplant its implementation
into our engine. Proposed changes below are independent Prey implementations.

## Observed mechanics

### Conditional floor assistance

`FunnelIntoPortal` and its `AirMove` caller provide actual movement assistance.
It applies to linked, upward-facing portals while looking down and falling fast
enough, within a limited height and footprint. Large requested horizontal movement
suppresses it; a separate high-horizontal-speed branch also takes precedence.
The routine adjusts requested horizontal movement toward the portal center. Very
near the opening it zeroes horizontal velocity. Thus it is neither unconditional
suction nor strictly momentum-preserving. The comments and actual conditions should
not be confused: the small-input threshold suppresses assistance on strong input.

Source: [movement, lines 177-350](https://github.com/SonicEraZoR/Portal-Base/blob/c4584551916acfb1e9583d54587ac84be48c9768/sp/src/game/shared/portal/portal_gamemovement.cpp#L177).

Our implementation has immediate underfoot entry and eight-unit corner clearance,
but no approach guidance. A gravity-relative, limited assist could improve falling
near the rim. Respect strong steering and fast intentional motion; use simulation
delta time. Do not copy the reference's last-moment horizontal cancellation blindly,
particularly with our bunny-hop modes. Tune distances against our player hull and
oval dimensions rather than importing Source units unchanged.

### Crossing, floor exits, and orientation

The reference checks portal ownership, teleport eligibility, the entity center's
side of the plane, and intersection with the hole. It transforms player centers
with an origin offset, rather than simply treating feet as the crossing point.
Some transitions temporarily force crouching. Exit velocity is rotated, then
floor-exit minimums and a global speed ceiling are applied. Camera reorientation
chooses pitch/roll behavior based on facing and portal orientation.

Source: [crossing and teleportation, lines 804-1162](https://github.com/SonicEraZoR/Portal-Base/blob/c4584551916acfb1e9583d54587ac84be48c9768/sp/src/game/server/portal/prop_portal.cpp#L804).

Our origin-based crossings and gravity-aware presentation already differ. Switching
to center-based teleporting alone could reintroduce camera jumps: hull, camera,
collision aperture and traversal transforms must stay coordinated. First test
low-speed wall-to-floor and floor-to-floor exits for immediate re-entry. A bounded
outward assist is a possible remedy; preserve higher existing speed. Keep ordinary
jump height unchanged. A global Source-style speed cap is not recommended for our
existing movement modes.

### Placement refinement

`VerifyPortalPlacement` checks backing surface, motion and materials, then invokes
fitting around the linked portal and surrounding edges. Placement searches adjust
the opening rather than demanding that the initial aim point already fit perfectly.
It explicitly rejects surfaces with linear or angular velocity.

Source: [placement, lines 1158 onward](https://github.com/SonicEraZoR/Portal-Base/blob/c4584551916acfb1e9583d54587ac84be48c9768/sp/src/game/server/portal/portal_placement.cpp#L1158).

Our placement checks a supported oval and tries a few upward shifts, with a fixed
160-unit endpoint separation. A bounded two-dimensional search in the surface plane
would help near floors, corners and neighboring portals. Rank valid candidates by
distance from the aim point; verify the whole aperture and approach clearance.
Use aperture overlap rather than center distance alone. Preserve the old portal if
all candidates fail, and retain our one-unit visual/teleport attachment offset.

### Collision during partial passage

The simulator maintains clipped local geometry and transformed destination geometry,
as well as ownership and physics representations of entities near an opening.
Movement sweeps test the linked environment too. Hole overlap uses collision shapes,
not just a player-origin distance. Physics representations are not additional
independent gameplay objects.

Sources: [simulator](https://github.com/SonicEraZoR/Portal-Base/blob/c4584551916acfb1e9583d54587ac84be48c9768/sp/src/game/shared/portal/PortalSimulation.cpp#L407),
[linked physics](https://github.com/SonicEraZoR/Portal-Base/blob/c4584551916acfb1e9583d54587ac84be48c9768/sp/src/game/shared/portal/PortalSimulation.cpp#L1291),
[movement traces](https://github.com/SonicEraZoR/Portal-Base/blob/c4584551916acfb1e9583d54587ac84be48c9768/sp/src/game/shared/portal/portal_util_shared.cpp#L979).

This is the largest architectural gap. We cut backing collision for a fitting hull,
then sweep/check the destination during teleportation. We do not have a complete
remote collision environment while half a body is through. For Prey, investigate
splitting a hull at the portal plane and testing the transformed remote portion,
with an explicit per-entity crossing state. Start with player/static geometry;
physics proxies for interacting props are a larger subsequent project. Preserve
solid obstruction checks and avoid duplicate damage, triggers or physics impulses.

### Replacement and moving portals

`NewLocation` releases entity ownership before relocation. Simulator `MoveTo`
records intersecting entities, updates transforms, rebuilds collision, and handles
entities affected by losing the old opening. This is useful lifecycle guidance,
not evidence that player shots should attach to moving objects.

Sources: [replacement](https://github.com/SonicEraZoR/Portal-Base/blob/c4584551916acfb1e9583d54587ac84be48c9768/sp/src/game/server/portal/prop_portal.cpp#L2074),
[simulator relocation](https://github.com/SonicEraZoR/Portal-Base/blob/c4584551916acfb1e9583d54587ac84be48c9768/sp/src/game/shared/portal/PortalSimulation.cpp#L200).

We clear old crossing history and reject replacement while the player straddles
an opening, but the guard does not yet account for occupying props. Add entity-aware
replacement handling before permitting replacement around partially entered props.
Keep Prey's authored moving doors on their native path. Any later integration must
update both portal transforms, collision and crossing history in the same simulation
step; test translation, rotation, reversal and save/reload while moving.

## Suggested implementation order and acceptance tests

1. Conditional floor guidance: centered and off-center falls; looking away; strong
   counter-steering; fast strafes; local gravity changes; compare multiple render rates.
2. Safe floor exits: walking-speed entry, two floor portals, repeated falls, low
   ceilings and occupied exits. No changes to ordinary jumps or high-speed momentum.
3. Placement fitting: corners, low walls, adjacent portals, thin clip layers and
   failed shots. Failed placement must leave the previous pair usable.
4. Replacement ownership: a prop halfway through, a projectile in flight, nearby
   player, repeated same-color shots, and save/reload. No duplication or deleted props.
5. Partial-body collision: obstacles immediately beyond the exit, diagonal crossing,
   large props and contacts on both sides. Scope initially to the player portal pair.
6. Authored moving-portal regressions: preserve existing campaign behavior; do not
   assume Source's world-up or static-surface restrictions fit Prey's gravity system.

Keep all automated playtests hidden, muted and isolated. Any new optional controls
belong in the current settings launcher, not the older preset BATs.
