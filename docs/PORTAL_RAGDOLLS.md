# Portal-gun ragdoll traversal

Free ragdolls now use the portal gun's existing aperture collision rules. Their
individual clip models supply ownership for those checks without disabling AF
self-collision. Contacts, translation, and angular sweeps respect the opening;
the full angular sweep must fit. Nearby sleeping ragdolls wake when an open pair
removes their support. Bound, frozen, world-constrained, living AF actors, and
`noPortal` figures remain excluded.

Ownership crosses at the AF root. Before committing, every limb's exit occupancy
and remaining movement are checked. All body poses and their individual linear
and angular velocities receive the same rigid transform. Old contact state is
cleared, gravity is updated, animation/combat representations are refreshed, and
the ordinary portal notification runs. A blocked exit stops inward motion of all
limbs. Portal replacement validates all limbs when clearing an occupant.

The existing NPC clipped-mesh renderer now also accepts free ragdolls, including
bound visual attachments. There is one physical ragdoll and two clipped render
pieces, not two independently simulated corpses. The render pieces participate
in ordinary lighting. No new persistent state or save format is required.

Scope: this extends the player-created portal pair. The legacy transfer path for
campaign portals is unchanged. It does not implement full two-world constraint
solving while a ragdoll straddles an opening: the destination is validated before
ownership transfer, rather than simulating independent destination-side limbs.

## Validation

All playtests use private desktops, isolated profiles, and `s_volume_dB -60`.

- User `ragdoll` save: an 11-body corpse crosses repeatedly between two floor
  portals; lit clipped pieces are drawn on both sides.
- Save/reload during traversal and replacement of an occupied endpoint.
- A run capped at 144 FPS, without fixed-tic rendering.
- `tools/portalgun/tests/ragdoll.cfg`: floor-to-wall transfer, obstruction at the
  exit, and a `noPortal` corpse. Requires the portal lab `input` save and the
  authored `portal_obstacle.ase` fixture installed in the isolated profile.
- Existing live-NPC floor crossing and player floor-clearance regressions,
  including shallow/fast entries and an obstructed exit. Enable `developer 1`
  after each load when invoking the developer-only `portalGun` test commands.

Developer diagnostics: `portalGun ragdolls` prints AF eligibility, body count,
rest state and position. `PORTAL_AF_CROSS` and `PORTAL_AF_BLOCK` identify transfer
results; `g_portalBodyTrace 1` traces the shared clipped rendering path.
