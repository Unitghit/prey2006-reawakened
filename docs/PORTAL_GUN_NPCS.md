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

## Ground-level walking entry

The updated portalnpc save exposed a missed source crossing, not an exit collision: Chuck's feet were already below the raised visual portal plane when he walked onto it. The cutout removed support, but the front-to-back test never fired, allowing him to fall beneath the map.

For NPC floor entry only, the crossing plane now sits just below the supporting surface, using the portal's stored surface offset. Player camera timing and non-floor crossing planes remain unchanged. No NPC-specific camera casts or save data are added.

The updated save replay records Chuck transferring twice, then returning to normal floor height. Fifty post-load position samples ranged from 0.25 to 40.56 units above world zero, finishing at 0.25; the pre-fix replay descended to -67.75 without any teleport. The npc_floor fixture now starts at floor contact height (0.25), rather than falling from five units above the opening.

## Continuous NPC presentation

NPC ownership now crosses at the physics hull center. The existing cutout remains valid while that hull straddles the opening. Floor-exit reorientation preserves the transformed center, and the collision-tested outward assistance accounts for the remaining body depth rather than assuming the feet have already emerged.

The player split-mesh builder also handles nearby living NPCs and their bound model attachments. It creates complementary near/far meshes with the existing portal-specific material, tangent, eye-deformation, and shadow handling. There remains one physical actor with one AI, health value and combat model; remote pieces are renderer definitions only. Unrelated entities and scripted portals are not changed.

A transient NPC presentation record preserves the portal-rotated pose after ownership changes, then eases it upright over 250 ms once the body has room to turn. This rotation uses the render interpolation fraction in unlocked mode. The record survives generic view interpolation resets, and is cleared on map shutdown/restore or entity invalidation. It does not add fields to savegames. Saving remains compatible; visual transition history restarts on loading a save.

Validation:
- Updated portalnpc: gradual entry, full emergence and normal final floor height, including an unlocked 144 FPS limit replay with MSAA 4.
- npc_smooth: entry and exit split meshes, intermediate rotation, repeated crossings, and no stale NPC pieces after reload. Captures inspected across entry, emergence and upright recovery.
- npc_floor, npc, blocked, objects and the existing player body_split tests pass.
- Tests are hidden and muted. No duplicate collision entities are spawned.

This is part of the existing optional portal gun behavior and follows the existing g_portalBodySplit comparison toggle; it adds no launcher choice. Navigation does not gain portal route planning. During the visual turn, the physical monster hull remains gravity-aligned for collision stability.
