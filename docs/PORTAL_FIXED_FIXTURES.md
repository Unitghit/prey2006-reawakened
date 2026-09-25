# Portal traversal through fixed wall fixtures

The retail `portalstuck` save placed paired portals over roadhouse windows. The wall cutout worked, but the window woodwork used a separate rigid-body collision hull (`nodrop` and `noimpact`) and remained solid behind the opening. A destination probe at the player's feet also missed the invisible wall cover when the transformed feet were slightly below the floor.

The cutout now applies to the hidden portion of an unbound, non-damageable moveable fixture only while it is at rest, non-pushable, at its authored position, and has no linear or angular velocity. Ordinary props and characters retain their collision. The same plane/contact clipping preserves solid geometry in front of the opening. Wall-cover probes use the query hull's transformed center rather than its foot origin across translation, motion, contacts, and contents checks.

No portal placement, step-height, or teleport clearance limits were increased. No save format change is needed.

Validation:
- Retail `portalstuck`: walking through each direction succeeds; the lower exit uses the existing six-unit step correction.
- New `fixed_fixture`: hidden fixed fixture allows passage; a fixture in front and an ordinary loose prop still block it.
- Existing `blocked`, `objects`, `tapered_shell`, `clip_column`, `floor_partial`, `sloped_ceiling`, and `replacement_occupants` pass.

Retail save data is not distributed.
