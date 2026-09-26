# NPC traversal through portal gun portals

Living NPCs using monster physics now participate in the portal gun's existing collision cutout and teleportation path. The previous entity filter admitted players, projectiles and movable props only, so an NPC could walk across the solid floor beneath a portal.

This is part of the existing optional portal gun feature. It preserves authored `noPortal` restrictions, bound-entity exclusions, the aperture fit check, and exit collision validation. Dead actors/ragdolls and other physics types are not newly enabled. Scripted portal behavior is unchanged. AI does not gain portal-aware path planning; this allows physical traversal when an actor reaches or falls into the opening.

Validation:
- User's `portalnpc` save: Dalton crosses the floor pair twice and then continues walking; no engine error.
- Isolated Hunter: falls into a floor portal, exits a wall portal exactly once, remains at full health and resumes movement.
- Automated `npc` fixture also checks authored noPortal and blocked exit restrictions.
- Existing input, objects, blocked, replacement_occupants, floor and through_portals cases pass.

All tests use isolated hidden sessions with s_volume_dB -60. No save format changes or retail assets are included.

## Floor exit correction

The initial traversal test proved teleportation but did not prove that an NPC emerged above the destination floor. A floor-to-floor transform turned the NPC hull upside down; monster movement then tried to restore gravity alignment on the next tick. NPCs also did not receive floor-exit lift assistance.

Gun-portal NPCs now adopt their destination gravity orientation during floor-exit validation, before committing the teleport. Slow floor exits receive collision-tested lift sufficient for 24 units of rise; existing higher outward velocity remains intact. Player assistance is unchanged. Non-floor exits keep the previous orientation behavior.

The updated `portalnpc` replay shows Dalton at the orange destination, upright with his feet 21 units above the floor, followed by walking away and landing at floor height. The `npc_floor` fixture checks full-body emergence and repeated floor-pair traversal, rather than using a teleport log alone as evidence of success. The original wall-exit, noPortal and blocked-exit NPC checks remain part of the regression run.
