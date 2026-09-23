# Portal-gun shots through linked portals

Portal shots now follow open, teleport-enabled scripted portals and player-made
portals. A front-facing plane/aperture test chooses the nearest crossing before
a solid obstacle. Player portals use their visible oval; scripted portals use
their native portal bounds. Closed and visual-only portals are not traversed.

The eye ray targets the first opening so the muzzle offset does not shift the
aim beyond it. At crossing, the shot uses the native portal transform for its
position and direction, acquires its next segment, and resets its trail origin.
Unused simulation-tick time carries into the next segment. Flight state remains
in saved spawn dictionaries, so saving either before or after a crossing works.
Placement uses the final segment and only moves the selected endpoint on impact.
New remote endpoints query local gravity at their destination.

Total travel remains bounded by the original 8192-unit budget, with at most
eight crossings. A cycle dissipates without moving either endpoint. The current
portal transforms are checked as the shot travels; moving/replaced/closed portals
are not blindly followed using an old stored link.

Validation in hidden, muted test profiles:
- Shooting through an existing player portal places beyond its linked exit.
- Scripted portal traversal, followed by a save/reload after crossing.
- Two scripted portals in one shot, including orange fire.
- Shots outside the player portal oval stay in the original room.
- A deliberate portal loop stops after eight crossings without placement.
- Existing flight/opening saves, aim retention, superseded shots, obstructions,
  replacement, movable-occupant clearance, and blocked-exit checks.

The older aim-retention test fires directly into an existing orange portal. Its
expected result is now the wall beyond the linked blue exit, not the entrance
wall. The test still checks that looking away cannot retarget the flying shot.
No new launcher setting or save format is needed.
