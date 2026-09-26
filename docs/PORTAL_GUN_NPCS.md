# NPC traversal through portal gun portals

Living NPCs using monster physics now participate in the portal gun's existing collision cutout and teleportation path. The previous entity filter admitted players, projectiles and movable props only, so an NPC could walk across the solid floor beneath a portal.

This is part of the existing optional portal gun feature. It preserves authored `noPortal` restrictions, bound-entity exclusions, the aperture fit check, and exit collision validation. Dead actors/ragdolls and other physics types are not newly enabled. Scripted portal behavior is unchanged. AI does not gain portal-aware path planning; this allows physical traversal when an actor reaches or falls into the opening.

Validation:
- User's `portalnpc` save: Dalton crosses the floor pair twice and then continues walking; no engine error.
- Isolated Hunter: falls into a floor portal, exits a wall portal exactly once, remains at full health and resumes movement.
- Automated `npc` fixture also checks authored noPortal and blocked exit restrictions.
- Existing input, objects, blocked, replacement_occupants, floor and through_portals cases pass.

All tests use isolated hidden sessions with s_volume_dB -60. No save format changes or retail assets are included.
