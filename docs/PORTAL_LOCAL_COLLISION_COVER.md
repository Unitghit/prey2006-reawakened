# Uneven portal collision covers

Invisible actor-clip terrain can use multiple sloping faces across one portal.
The old code cached the clip face hit by a ray at the opening center. That face
could differ from the one actually supporting the player's current position.
A cached miss also persisted in saves. This left solid invisible cover across
an otherwise valid aperture in either direction.

`RW_PortalCoverPlane` now projects the current collision query onto the support
plane and samples the nearby actor-clip face there. It does not reuse the saved
center-face cache. The ray is bounded (32 units above to 8 below), must hit world
player-clip geometry, and must face within the existing normal tolerance. The
collision manager still requires the exact detected face and excludes solid
geometry. Aperture fit, exit occupancy, and swept movement checks remain active.

Validation on the reported campaign saves:
- `portalstuck1`: old build repeatedly rejected the exit on
  `textures/common/clip_monplaymov_flesh`; new build crosses while walking,
  using the existing bounded 16-unit exit-clearance correction.
- `portalstuck2`: old build stays on the ground indefinitely; new build falls
  through and crosses, with zero landing-position correction.
- Both tests used the original saved placements, not replacement portals.

Additional regression cases: tapered actor-clip shells, clip columns, sloped
ceilings, uneven static ground, blocked exits, floor corners/edges, floor
approach, and reversal. Automated tests use hidden, muted isolated profiles.
No map-specific coordinates or material-name exceptions are used in the fix.

## Partial-entry edge contacts

The subsequent `portalstuck2` save exposed another case: the hull was already
partway into the orange floor opening, with no velocity, but an invisible shell
edge still stopped further descent. Recognizing the shell's top face did not
remove its side/edge contacts.

A shared collision-contact predicate now clips pure player-clip contacts below
the locally sampled cover plane as well as contacts behind the actual portal
support. It is used consistently for swept vertex/edge contacts, resting
contacts, and occupancy tests. Visible solid geometry keeps the original
support-plane rule. The cover only exists during an aperture-validated query;
contacts in front of it still collide normally.

The exact saved overlap now falls through with zero teleport correction.
Regression coverage again includes real blocked exits, nearby clip columns,
tapered shells, sloped ceilings, floor edges/corners, static uneven ground,
floor approach, and reverse travel. No portal repositioning or save migration
is needed.

## Beveled covers at wall exits

The `portalstuck1` campaign placement encountered an invisible player-clip
cover angled about 22 degrees from the visible wall. The former 0.95 normal
dot threshold rejected this cover, blocking the partial exit hull. Covers now
allow up to 30 degrees. The same bounded ray, world-only player-clip filter,
exact plane match, aperture checks, and destination occupancy checks remain.
No scripted portal entity is ignored and no solid geometry exception is added.
The saved placement crosses with zero teleport clearance adjustment.
