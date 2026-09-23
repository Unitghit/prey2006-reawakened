# Portal replacement with movable occupants

A partially entered bugbomb held the old pair-wide occupancy guard indefinitely,
so either firing mode rejected otherwise valid placements. Replacement now
tries to clear movable occupants before moving an endpoint.

For every overlapping eligible moveable, find the nearest tested outward move,
then bounded lateral alternatives (up to 48 units sideways and 32 additional
units outward). Each route is swept against collision. A truncated sweep is
usable only if the full hull has already cleared the opening. The final hull
must also be clear with portal collision cutouts disabled, without making portal
sensor volumes solid. Planned destinations cannot overlap one another.

All moves are validated before any object is moved. Objects keep their identity,
health, angular velocity, and tangential/outgoing linear velocity; only inward
velocity is removed. Physics wakes after repositioning. Players and projectiles
still prevent replacement while straddling an opening, and an object with no
safe path still blocks replacement rather than being deleted or forced into a
wall. No save format or launcher setting changes are required.

Validation:
- `noportal`: the saved hhPod bugbomb clears and either portal color can be
  replaced (independent reload for each color).
- `replacement_occupants`: movable wall occupant clears; an obstructed occupant
  and a straddling player retain the guard.
- Existing object traversal, repeated replacement, projectile/opening sequences,
  blocked exits, and reverse travel remain covered by hidden, muted tests.
