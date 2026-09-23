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
